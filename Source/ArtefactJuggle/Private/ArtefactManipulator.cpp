#include "ArtefactManipulator.h"
#include "ArtefactBase.h"
#include "ArtefactJuggleCharacter.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

UArtefactManipulator::UArtefactManipulator()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Run AFTER character movement and skeletal mesh animations have updated for the frame
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	SetIsReplicatedByDefault(true);

	CurrentPosition = EArtifactPosition::Forward;
	InterpSpeed = 10.0f;
	bIsControllingArtefact = false;
	bIsGonnaPutDownArtefact = false;
	bHasNeutralizedInput = true;
	EquippedArtefact = nullptr;

	CurrentLocalOffset = FVector::ZeroVector;
	CurrentLocalRotation = FRotator::ZeroRotator;
}

void UArtefactManipulator::BeginPlay()
{
	Super::BeginPlay();
}

void UArtefactManipulator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsGonnaPutDownArtefact) return;

	if (EquippedArtefact != nullptr && EquippedArtefact->GetRootComponent())
	{
		ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return;

		USceneComponent* ArtefactRoot = EquippedArtefact->GetRootComponent();
		FTransform MeshWorldTransform = OwnerCharacter->GetMesh()->GetComponentTransform();

		// 1. Get target slot transform in World Space, then convert to Character Mesh's Local Space
		FTransform TargetWorldTransform = GetTargetSlotTransform(CurrentPosition);
		FTransform TargetLocalTransform = TargetWorldTransform.GetRelativeTransform(MeshWorldTransform);

		// 2. Interpolate ONLY the local slot transition (zero lag when character walks or jumps)
		CurrentLocalOffset = FMath::VInterpTo(CurrentLocalOffset, TargetLocalTransform.GetLocation(), DeltaTime, InterpSpeed);
		CurrentLocalRotation = FMath::RInterpTo(CurrentLocalRotation, TargetLocalTransform.GetRotation().Rotator(), DeltaTime, InterpSpeed);

		// 3. Convert back to World Space and apply directly to the artefact
		FTransform FinalLocalTransform(CurrentLocalRotation, CurrentLocalOffset);
		FTransform FinalWorldTransform = FinalLocalTransform * MeshWorldTransform;

		ArtefactRoot->SetWorldLocationAndRotation(FinalWorldTransform.GetLocation(), FinalWorldTransform.GetRotation().Rotator());
	}
}

void UArtefactManipulator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UArtefactManipulator, EquippedArtefact);
	DOREPLIFETIME(UArtefactManipulator, CurrentPosition);
}

void UArtefactManipulator::SetIsControllingArtefact(bool bNewState)
{
	bIsControllingArtefact = bNewState;
}

void UArtefactManipulator::HandleDirectionInput(FVector2D InputVector, FVector CameraForwardVector)
{
	// SAFETY CHECK 1: Ensure we are in control mode and have a valid character
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!bIsControllingArtefact || !OwnerCharacter) return;

	// FIX: Clear and robust deadzone check on the raw input vector length.
	// If the player releases the keys/stick, unlock the input system immediately and abort.
	if (InputVector.SizeSquared() < (0.5f * 0.5f))
	{
		bHasNeutralizedInput = true;
		return;
	}

	// SAFETY CHECK 2: Prevent input spamming if the player hasn't returned to neutral yet
	if (!bHasNeutralizedInput) return;

	// Lock input immediately for exactly ONE clean grid step
	bHasNeutralizedInput = false;

	EArtifactPosition NewTargetPosition = CurrentPosition;

	// 1. IS THE INPUT DOMINANTLY VERTICAL OR HORIZONTAL ON SCREEN?
	if (FMath::Abs(InputVector.Y) > FMath::Abs(InputVector.X))
	{
		// --- VERTICAL SCREEN MOVEMENT (W / S or Stick Up/Down) ---
		// W (InputVector.Y > 0) means "Move UP on screen". S means "Move DOWN on screen".
		bool bPressUpOnScreen = (InputVector.Y > 0.0f);

		if (bPressUpOnScreen)
		{
			switch (CurrentPosition)
			{
			case EArtifactPosition::Down:
				NewTargetPosition = EArtifactPosition::Forward;
				break;

			case EArtifactPosition::Forward:
				NewTargetPosition = EArtifactPosition::Up;
				break;

			default:
				NewTargetPosition = EArtifactPosition::Up;
				break;
			}
		}
		else // Pressing Down (S)
		{
			switch (CurrentPosition)
			{
			case EArtifactPosition::Up:
				NewTargetPosition = EArtifactPosition::Forward;
				break;

			case EArtifactPosition::Forward:
				NewTargetPosition = EArtifactPosition::Down;
				break;

			default:
				NewTargetPosition = EArtifactPosition::Down;
				break;
			}
		}
	}
	else
	{
		// --- HORIZONTAL SCREEN MOVEMENT (D / A or Stick Right/Left) ---
		// D (InputVector.X > 0) means "Move RIGHT on screen". A means "Move LEFT on screen".
		bool bPressRightOnScreen = (InputVector.X > 0.0f);

		// Get the clean, horizontal control rotation from the controller (ignoring pitch/camera tilt)
		FVector CamRight = FVector::ZeroVector;
		if (AController* CharacterController = OwnerCharacter->GetController())
		{
			const FRotator ControlRotation = CharacterController->GetControlRotation();
			const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

			// Get a perfect, flat world right vector from the view matrix
			CamRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		}
		else
		{
			// Fallback if controller is missing
			CamRight = OwnerCharacter->GetActorRightVector();
		}

		// Get the character's physical right vector
		FVector CharRight = OwnerCharacter->GetActorRightVector();

		// Check if the camera's screen-right aligns with the character's physical right side
		float CameraSideDot = FVector::DotProduct(CamRight, CharRight);
		bool bCharRightIsScreenRight = (CameraSideDot > 0.0f);

		if (bPressRightOnScreen)
		{
			if (bCharRightIsScreenRight)
			{
				// Looking from behind: Screen Right = Character Right
				switch (CurrentPosition)
				{
				case EArtifactPosition::Left:
					NewTargetPosition = EArtifactPosition::Forward;
					break;

				case EArtifactPosition::Forward:
					NewTargetPosition = EArtifactPosition::Right;
					break;

				default:
					NewTargetPosition = EArtifactPosition::Right;
					break;
				}
			}
			else
			{
				// Looking from front: Screen Right = Character Left
				switch (CurrentPosition)
				{
				case EArtifactPosition::Right:
					NewTargetPosition = EArtifactPosition::Forward;
					break;

				case EArtifactPosition::Forward:
					NewTargetPosition = EArtifactPosition::Left;
					break;

				default:
					NewTargetPosition = EArtifactPosition::Left;
					break;
				}
			}
		}
		else // Pressing Left (A)
		{
			if (bCharRightIsScreenRight)
			{
				// Looking from behind: Screen Left = Character Left
				switch (CurrentPosition)
				{
				case EArtifactPosition::Right:
					NewTargetPosition = EArtifactPosition::Forward;
					break;

				case EArtifactPosition::Forward:
					NewTargetPosition = EArtifactPosition::Left;
					break;

				default:
					NewTargetPosition = EArtifactPosition::Left;
					break;
				}
			}
			else
			{
				// Looking from front: Screen Left = Character Right
				switch (CurrentPosition)
				{
				case EArtifactPosition::Left:
					NewTargetPosition = EArtifactPosition::Forward;
					break;

				case EArtifactPosition::Forward:
					NewTargetPosition = EArtifactPosition::Right;
					break;

				default:
					NewTargetPosition = EArtifactPosition::Right;
					break;
				}
			}
		}
	}

	// NETWORK OPTIMIZATION: Only send RPC to server if the position actually changed
	if (NewTargetPosition != CurrentPosition)
	{
		CurrentPosition = NewTargetPosition;

		// Update locally for instant visual responsiveness, then notify the server
		Server_ChangePosition(NewTargetPosition);
	}
}

void UArtefactManipulator::Server_ChangePosition_Implementation(EArtifactPosition NewPosition)
{
	CurrentPosition = NewPosition;

	if (EquippedArtefact)
	{
		EquippedArtefact->SetArtefactPosition(NewPosition);
	}
}

void UArtefactManipulator::Server_DropArtefact_Implementation(AArtefactBase* ArtefactToDrop, FVector FinalLocation, FRotator FinalRotation)
{
	if (ArtefactToDrop == nullptr) return;

	AArtefactJuggleCharacter* CharOwner = Cast<AArtefactJuggleCharacter>(GetOwner());
	if (!CharOwner) return;

	// 1. Stop manipulator interpolation immediately
	EquippedArtefact = nullptr;
	bIsControllingArtefact = false;

	CharOwner->StopArtefactJuggle();
	CharOwner->CurrentArtefact = nullptr;
	CharOwner->bIsHoldingArtefact = false;

	// 2. Activate ground state (collisions + server physics)
	ArtefactToDrop->SetArtefactPosition(EArtifactPosition::None);

	// 3. Teleport physics body cleanly to the exact drop location and upright rotation
	if (UPrimitiveComponent* ArtefactRoot = Cast<UPrimitiveComponent>(ArtefactToDrop->GetRootComponent()))
	{
		ArtefactToDrop->SetActorLocationAndRotation(FinalLocation, FinalRotation, false, nullptr, ETeleportType::TeleportPhysics);
		ArtefactRoot->WakeAllRigidBodies();
	}

	// 4. Tell all clients to snap immediately to this exact location (prevents uneven interpolation on spectators).
	ArtefactToDrop->Multicast_OnDropped(FinalLocation, FinalRotation);

	ResetCurrentPosition();

	// 5. Clear network ownership
	ArtefactToDrop->SetOwner(nullptr);
}

FTransform UArtefactManipulator::GetTargetSlotTransform(EArtifactPosition Position) const
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return FTransform::Identity;

	FString ComponentName;
	switch (Position)
	{
		case EArtifactPosition::Forward:
			ComponentName = "SlotForward";
			break;
		case EArtifactPosition::Left:
			ComponentName = "SlotLeft";
			break;
		case EArtifactPosition::Right:
			ComponentName = "SlotRight";
			break;
		case EArtifactPosition::Up:
			ComponentName = "SlotUp";
			break;
		case EArtifactPosition::Down:
			ComponentName = "SlotDown";
			break;
		default:
			return FTransform::Identity;
	}

	TArray<USceneComponent*> FoundChildren;
	OwnerCharacter->GetMesh()->GetChildrenComponents(true, FoundChildren);

	for (USceneComponent* Comp : FoundChildren)
	{
		if (Comp && Comp->GetName().StartsWith(ComponentName))
		{
			// RETURNERA VÄRLDSTRANSFORMEN: Ger oss exakt den punkt du ser i Blueprint-editorn
			return Comp->GetComponentTransform();
		}
	}

	return FTransform::Identity;
}

void UArtefactManipulator::ResetCurrentPosition()
{
	CurrentPosition = EArtifactPosition::Forward;
	bIsGonnaPutDownArtefact = false;
	bHasNeutralizedInput = true;

	// Snap local offset directly to SlotForward so it doesn't fly across the room on initial pickup
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (OwnerCharacter->GetMesh())
		{
			FTransform MeshWorldTransform = OwnerCharacter->GetMesh()->GetComponentTransform();
			FTransform ForwardWorldTransform = GetTargetSlotTransform(EArtifactPosition::Forward);
			FTransform ForwardLocalTransform = ForwardWorldTransform.GetRelativeTransform(MeshWorldTransform);

			CurrentLocalOffset = ForwardLocalTransform.GetLocation();
			CurrentLocalRotation = ForwardLocalTransform.GetRotation().Rotator();
		}
	}

	if (EquippedArtefact != nullptr)
	{
		EquippedArtefact->SetArtefactPosition(EArtifactPosition::Forward);
	}
}
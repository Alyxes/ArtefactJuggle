#include "ArtefactManipulator.h"
#include "ArtefactBase.h"
#include "ArtefactJuggleCharacter.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

UArtefactManipulator::UArtefactManipulator()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);

	CurrentPosition = EArtifactPosition::Forward;
	InterpSpeed = 10.0f;
	bIsControllingArtefact = false;
	bIsGonnaPutDownArtefact = false;
	bHasNeutralizedInput = true;
	EquippedArtefact = nullptr;
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
		USceneComponent* ArtefactRoot = EquippedArtefact->GetRootComponent();

		// 1. Hämta den exakta mål-transformationen i VÄRLDEN (World Space)
		FTransform TargetWorldTransform = GetTargetSlotTransform(CurrentPosition);

		// 2. Hämta stenens nuvarande position och rotation i VÄRLDEN
		FVector CurrentWorldLoc = ArtefactRoot->GetComponentLocation();
		FRotator CurrentWorldRot = ArtefactRoot->GetComponentRotation();

		// 3. Interpolera mjukt i World Space
		FVector NewWorldLoc = FMath::VInterpTo(CurrentWorldLoc, TargetWorldTransform.GetLocation(), DeltaTime, InterpSpeed);
		FRotator NewWorldRot = FMath::RInterpTo(CurrentWorldRot, TargetWorldTransform.GetRotation().Rotator(), DeltaTime, InterpSpeed);

		// 4. Applicera direkt i världen (Detta struntar helt i om karaktärens mesh är omskalad!)
		ArtefactRoot->SetWorldLocationAndRotation(NewWorldLoc, NewWorldRot);
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

		if (EquippedArtefact)
			EquippedArtefact->CurrentPosition = NewTargetPosition;

		Server_ChangePosition(NewTargetPosition);
	}
}

void UArtefactManipulator::Server_ChangePosition_Implementation(EArtifactPosition NewPosition)
{
	CurrentPosition = NewPosition;

	if (EquippedArtefact)
	{
		EquippedArtefact->CurrentPosition = NewPosition;
	}
}

void UArtefactManipulator::Server_DropArtefact_Implementation(AArtefactBase* ArtefactToDrop, FVector FinalLocation, FRotator FinalRotation)
{
	if (ArtefactToDrop == nullptr) return;

	// Fetch the character (owner of the component).
	AArtefactJuggleCharacter* CharOwner = Cast<AArtefactJuggleCharacter>(GetOwner());
	if (!CharOwner) return;

	// FIX 1: Turn off the control states IMMEDIATELY so TickComponent stops interpolating this frame!
	EquippedArtefact = nullptr;
	bIsControllingArtefact = false;

	CharOwner->StopArtefactJuggle();
	CharOwner->CurrentArtefact = nullptr;
	CharOwner->bIsHoldingArtefact = false;

	// FIX 2: Perform DETACH first before we touch any positions or network movement replication
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	ArtefactToDrop->DetachFromActor(DetachRules);

	// FIX 3: Force the root component (Mesh) to clear any active interpolation offsets
	if (UPrimitiveComponent* ArtefactRoot = Cast<UPrimitiveComponent>(ArtefactToDrop->GetRootComponent()))
	{
		// Set both Actor and Component rotation to the straight upright target rotation
		ArtefactToDrop->SetActorRotation(FinalRotation);
		ArtefactRoot->SetWorldRotation(FinalRotation);

		// Restore collision channels
		ArtefactRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	// FIX 4: Apply the exact world location on the server and enable movement replication LAST
	ArtefactToDrop->SetActorLocation(FinalLocation);
	// ArtefactToDrop->SetReplicateMovement(true);

	ResetCurrentPosition();

	// 5. Reset network ownership and state
	ArtefactToDrop->SetOwner(nullptr);
	ArtefactToDrop->CurrentPosition = EArtifactPosition::None;
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

	if (EquippedArtefact != nullptr)
	{
		EquippedArtefact->CurrentPosition = EArtifactPosition::Forward;
	}
}
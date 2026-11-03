// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArtefactJuggleCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ArtefactJuggle.h"
#include "ArtefactManipulator.h"
#include "ArtefactBase.h"

AArtefactJuggleCharacter::AArtefactJuggleCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	bIsModifierPressed = false;
}

void AArtefactJuggleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AArtefactJuggleCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AArtefactJuggleCharacter::FinishedJuggleMove);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AArtefactJuggleCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AArtefactJuggleCharacter::Look);
	}
	else
	{
		UE_LOG(LogArtefactJuggle, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AArtefactJuggleCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	// If the player is actively controlling the artefact, freeze WASD movement
	if (bIsModifierPressed)
	{
		UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
		if (Manipulator && FollowCamera)
		{
			// Get the camera's forward vector to determine screen space direction
			FVector CameraForward = FollowCamera->GetForwardVector();
			
			// Pass both the input vector AND the camera direction to the manipulator
			Manipulator->HandleDirectionInput(MovementVector, CameraForward);
		}
		return;
	}

	DoMove(MovementVector.X, MovementVector.Y);
}

void AArtefactJuggleCharacter::FinishedJuggleMove()
{
	if (!bIsModifierPressed) return;

	UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
	if (Manipulator)
	{
		Manipulator->bHasNeutralizedInput = true;
	}
}
void AArtefactJuggleCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AArtefactJuggleCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AArtefactJuggleCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AArtefactJuggleCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AArtefactJuggleCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AArtefactJuggleCharacter::PickUpArtefact(AArtefactBase* ArtefactToPickUp)
{
	// Säkerhetsspärr: Avbryt om stenen är tom eller om vi redan bär på något
	if (ArtefactToPickUp == nullptr || bIsHoldingArtefact)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("C++: PickUp failed - Invalid or already holding."));
		return;
	}

	UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
	if (Manipulator)
	{
		// 1. Tilldela stenen till manipulatorn och karaktärens referens
		Manipulator->EquippedArtefact = ArtefactToPickUp;
		CurrentArtefact = ArtefactToPickUp; // Er befintliga backup-referens

		UPrimitiveComponent* ArtefactRoot = Cast<UPrimitiveComponent>(ArtefactToPickUp->GetRootComponent());
		if (ArtefactRoot)
		{
			ArtefactRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
			ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		}

		// 3. Hitta SlotForward dynamiskt bland Mesh-komponentens barn (era Blueprint Scene Components)
		USceneComponent* MeshComponent = GetMesh();
		USceneComponent* TargetSlot = nullptr;

		if (MeshComponent)
		{
			TArray<USceneComponent*> FoundChildren;
			MeshComponent->GetChildrenComponents(true, FoundChildren);

			for (USceneComponent* Child : FoundChildren)
			{
				if (Child && Child->GetName() == TEXT("SlotForward"))
				{
					TargetSlot = Child;
					break;
				}
			}
		}

		// Använd SlotForward om den hittades, annars karaktärens Mesh som säkerhetsmarginal
		USceneComponent* AttachParent = TargetSlot ? TargetSlot : MeshComponent;

		if (TargetSlot == nullptr)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("C++: TergetSlot = nullptr!"));

		// 4. ATTACH TO CHARACTER: Flytta och limma stenen direkt till sloten
		FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget, // Roterar och flyttar stenen direkt till slotens nollpunkt
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			false
		);
		ArtefactToPickUp->AttachToComponent(AttachParent, AttachRules);

		bIsHoldingArtefact = true;

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("C++: Artefact Attached to SlotForward successfully!"));
	}
}
void AArtefactJuggleCharacter::DropArtefact()
{
	// 1. SAFETY CHECK: Abort if we are not holding an artefact or the reference is invalid
	if (!bIsHoldingArtefact || CurrentArtefact == nullptr) return;

	// 2. PRE-COMPUTE GROUND TRACE CONDITIONS FIRST
	// Start from the character's reliable center point (immune to being inside walls)
	FVector CharacterCenter = GetActorLocation();
	FVector ForwardDirection = GetActorForwardVector();

	// Max distance we prefer to place the stone (64 units forward)
	float TargetDistance = 64.0f;
	FVector HorizontalTraceEnd = CharacterCenter + (ForwardDirection * TargetDistance);

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);
	TraceParams.AddIgnoredActor(CurrentArtefact);

	// 3. HORIZONTAL RAYCAST: Check for walls or obstacles in front of the character center
	FHitResult WallHit;
	FVector FinalHorizontalLocation = HorizontalTraceEnd;

	if (GetWorld()->LineTraceSingleByChannel(WallHit, CharacterCenter, HorizontalTraceEnd, ECC_Visibility, TraceParams))
	{
		// Obstacle found! Pull the placement point back to the wall surface, minus a small margin (e.g., 5 units)
		FinalHorizontalLocation = WallHit.Location - (ForwardDirection * 5.0f);
	}

	// 4. VERTICAL RAYCAST: Detect the floor starting from our safe horizontal location
	float MaxDropDepth = 200.0f; // Maximum distance allowed to drop into a pit or slope
	FVector GroundTraceStart = FinalHorizontalLocation;
	FVector GroundTraceEnd = GroundTraceStart - FVector(0.0f, 0.0f, MaxDropDepth);

	FHitResult GroundHit;
	if (!GetWorld()->LineTraceSingleByChannel(GroundHit, GroundTraceStart, GroundTraceEnd, ECC_Visibility, TraceParams))
	{
		// BLOCKED MECHANIC: No ground detected within 200 units. Abort the drop entirely!
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("C++: Cannot drop here - Ground too far down or abyss!"));
		return;
	}

	FRotator CharacterRotation = GetActorRotation();
	FRotator PutdownRotation = FRotator(0.0f, CharacterRotation.Yaw - 90.0f, 0.0f);
	CurrentArtefact->SetActorRotation(PutdownRotation);

	// 5. CALCULATE ARTEFACT DIMENSIONS: Get the real-world bounding box to snap perfectly to the floor
	float ArtefactHalfHeight = 0.0f;
	UStaticMeshComponent* ArtefactMesh = CurrentArtefact->FindComponentByClass<UStaticMeshComponent>();
	if (ArtefactMesh)
	{
		// Native C++ uses the built-in 'Bounds' property, which is already in world-space
		// BoxExtent.Z is the exact half-height of the mesh, taking scale/rotation into account!
		ArtefactHalfHeight = ArtefactMesh->Bounds.BoxExtent.Z;
	}

	// Calculate final world position sitting flat on the detected ground
	FVector FinalDropLocation = GroundHit.Location;
	FinalDropLocation.Z += ArtefactHalfHeight;

	// 6. SUCCESSFUL DROP EXECUTED BELOW: Disconnect from character now that the path is verified safe
	StopArtefactJuggle();

	FDetachmentTransformRules DetachRules(
		EDetachmentRule::KeepWorld,
		EDetachmentRule::KeepWorld,
		EDetachmentRule::KeepWorld,
		true
	);
	CurrentArtefact->DetachFromActor(DetachRules);

	// Apply final location.
	CurrentArtefact->SetActorLocation(FinalDropLocation);
	

	// Re-enable query collision for puzzle overlapping, keeping physics simulation false for absolute stability
	UPrimitiveComponent* ArtefactRoot = Cast<UPrimitiveComponent>(CurrentArtefact->GetRootComponent());
	if (ArtefactRoot)
	{
		ArtefactRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	// Clear references in the manipulator and character
	UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
	if (Manipulator)
	{
		Manipulator->EquippedArtefact = nullptr;
		Manipulator->bIsControllingArtefact = false;
		Manipulator->ResetCurrentPosition();
	}

	CurrentArtefact = nullptr;
	bIsHoldingArtefact = false;

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("C++: Artefact Dropped safely on ground!"));
}

void AArtefactJuggleCharacter::StartArtefactJuggle()
{
	// Only activate control if there actually is an artefact.
	if (bIsHoldingArtefact && CurrentArtefact)
	{
		bIsModifierPressed = true;

		UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
		if (Manipulator)
		{
			// Tell the manipulator that we started to control the artefact.
			Manipulator->bIsControllingArtefact = true;
			// Set the control neutralizer to false, to avoid having the artefact move immediately when you press the shiftArtefact-button.
			// if ()
			Manipulator->bHasNeutralizedInput = false;
		}
	}
}

void AArtefactJuggleCharacter::StopArtefactJuggle()
{
	bIsModifierPressed = false;

	UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
	if (Manipulator)
	{
		// Shutting off controlling mode.
		Manipulator->bIsControllingArtefact = false;
		Manipulator->bHasNeutralizedInput = true;
	}
}
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
#include "Net/UnrealNetwork.h"

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

	bReplicates = true;
	SetReplicateMovement(true);
}

void AArtefactJuggleCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// This macro tells Unreal Engine to replicate these properties from server -> clients whenever they change.
	DOREPLIFETIME(AArtefactJuggleCharacter, CurrentArtefact);
	DOREPLIFETIME(AArtefactJuggleCharacter, bIsHoldingArtefact);
	DOREPLIFETIME(AArtefactJuggleCharacter, bIsModifierPressed);
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
	bIsPressingMove = true;

	FVector2D MovementVector = Value.Get<FVector2D>();

	// If the player is actively controlling the artefact, freeze movement.
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
	bIsPressingMove = false;

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

void AArtefactJuggleCharacter::PickUpArtefact_Implementation(AArtefactBase* ArtefactToPickUp)
{
	// Säkerhetsspärr: Avbryt om stenen är tom eller om vi redan bär på något
	if (ArtefactToPickUp == nullptr || bIsHoldingArtefact || ArtefactToPickUp->GetOwner() != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("C++: PickUp failed - Invalid or already holding."));
		return;
	}
	
	UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
	if (Manipulator)
	{
		// 1. Tilldela stenen till manipulatorn och karaktärens referens
		Manipulator->EquippedArtefact = ArtefactToPickUp;

		// Försök att stoppa Manipulators tick från att interpolera artefakten när den sen ska sättas ner.
		Manipulator->bIsGonnaPutDownArtefact = false;
		CurrentArtefact = ArtefactToPickUp; // Er befintliga backup-referens

		ArtefactToPickUp->SetReplicateMovement(false);

		UPrimitiveComponent* ArtefactRoot = Cast<UPrimitiveComponent>(ArtefactToPickUp->GetRootComponent());
		if (ArtefactRoot)
		{
			ArtefactRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
			ArtefactRoot->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		}

		// 2. SÄTT STARTLÄGET DIREKT I DATA
		ArtefactToPickUp->CurrentPosition = EArtifactPosition::Forward;

		// 3. PROFESSIONELL ATTACHMENT:
		// Istället för den tunga text-loopen, fäster vi stenen direkt till karaktärens Mesh!
		FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			false
		);
		ArtefactToPickUp->AttachToComponent(GetMesh(), AttachRules);

		bIsHoldingArtefact = true;

		ArtefactToPickUp->SetOwner(this);
		ArtefactToPickUp->CurrentPosition = EArtifactPosition::Forward;
		Manipulator->ResetCurrentPosition();

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("C++: Artefact Attached to SlotForward successfully!"));
	}
}
void AArtefactJuggleCharacter::DropArtefact()
{
	// 1. SAFETY CHECK: Abort if we are not holding an artefact or the reference is invalid
	if (!bIsHoldingArtefact || CurrentArtefact == nullptr) return;

	UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
	if (Manipulator)
	{
		Manipulator->bIsGonnaPutDownArtefact = true;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("C++: Manipulator component missing!"));
		return;
	}

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
		Manipulator->bIsGonnaPutDownArtefact = false;
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("C++: Cannot drop here - Ground too far down or abyss!"));
		return;
	}

	FRotator CharacterRotation = GetActorRotation();
	FRotator PutdownRotation = FRotator(0.0f, CharacterRotation.Yaw - 90.0f, 0.0f);
	// CurrentArtefact->SetActorRotation(PutdownRotation);

	// 5. CALCULATE ARTEFACT DIMENSIONS: Get the real-world bounding box to snap perfectly to the floor
	float ArtefactHalfHeight = 0.0f;
	UStaticMeshComponent* ArtefactMesh = CurrentArtefact->FindComponentByClass<UStaticMeshComponent>();
	if (ArtefactMesh && ArtefactMesh->GetStaticMesh())
	{
		// 1. Hämta storleken på 3D-modellen i dess helt oroterade grundläge
		FVector LocalExtent = ArtefactMesh->GetStaticMesh()->GetExtendedBounds().BoxExtent;

		// 2. Multiplicera med den unika Z-skalan som du satt i editorn
		float LocalScaleZ = ArtefactMesh->GetComponentScale().Z;

		// 3. Nu har vi en helt konstant halvhöjd för när stenen står upprätt!
		ArtefactHalfHeight = LocalExtent.Z * LocalScaleZ;
	}

	// Calculate final world position sitting flat on the detected ground
	FVector FinalDropLocation = GroundHit.Location;
	FinalDropLocation.Z += ArtefactHalfHeight;

	// 6. CALL SERVER FUNCTION TO EXECUTE THE DROP: This ensures the server validates and replicates the drop to all clients
	if (Manipulator)
	{
		// Call the server function with both the final drop location and the desired rotation
		Manipulator->Server_DropArtefact(CurrentArtefact, FinalDropLocation, PutdownRotation);
	}
	else
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("C++: Cannot drop artefact - Manipulator component missing!"));
}

void AArtefactJuggleCharacter::StartArtefactJuggle()
{
	// Only activate control if there actually is an artefact.
	if (bIsHoldingArtefact && CurrentArtefact != nullptr)
	{
		bIsModifierPressed = true;

		UArtefactManipulator* Manipulator = FindComponentByClass<UArtefactManipulator>();
		if (Manipulator)
		{
			// Tell the manipulator that we started to control the artefact.
			Manipulator->bIsControllingArtefact = true;

			if (bIsPressingMove)
				Manipulator->bHasNeutralizedInput = false;
			else
				Manipulator->bHasNeutralizedInput = true;
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
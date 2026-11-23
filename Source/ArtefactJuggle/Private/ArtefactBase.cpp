#include "ArtefactBase.h"
#include "Net/UnrealNetwork.h" // Livsviktigt makro-bibliotek för nätverksreplikering (DOREPLIFETIME)

// Konstruktorn: Här bygger vi upp objektets grundkomponenter i minnet
AArtefactBase::AArtefactBase()
{
    // Slå på Tick om du vill att denna Actor ska köra logik varje bildruta (kan stängas av senare för prestanda)
    PrimaryActorTick.bCanEverTick = true;

    // 1. SÄTT UPP NÄTVERK: Berätta för motorn att detta objekt är viktigt över nätverket
    bReplicates = true;
    SetReplicateMovement(true); // Synkar automatiskt position/fysik över nätverket när den är lös

    // 2. SKAPA KOMPONENT: Skapa vår Static Mesh i minnet
    ArtefactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtefactMesh"));

    // Sätt meshen som rotkomponent (objektets absoluta mittpunkt i världen)
    RootComponent = ArtefactMesh;

    // Sätt startläget till None eftersom ingen håller i den när den spawnas på marken
    CurrentPosition = EArtifactPosition::None;

    bIsBeingCarried = false;
}

void AArtefactBase::BeginPlay()
{
    Super::BeginPlay();
}

void AArtefactBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority() && !bIsBeingCarried && ArtefactMesh && ArtefactMesh->IsSimulatingPhysics())
    {
        FVector CurrentVel = ArtefactMesh->GetPhysicsLinearVelocity();

        // Break X and Y only.
        float HorizontalDamping = 20.0f;
        CurrentVel.X = FMath::FInterpTo(CurrentVel.X, 0.0f, DeltaTime, HorizontalDamping);
        CurrentVel.Y = FMath::FInterpTo(CurrentVel.Y, 0.0f, DeltaTime, HorizontalDamping);

        ArtefactMesh->SetPhysicsLinearVelocity(CurrentVel);
    }

    if (ArtefactMesh->IsGravityEnabled() && GravityMultiplier != 1.0f)
    {
        float WorldGravityZ = GetWorld()->GetGravityZ();
        float ExtraGravityZ = WorldGravityZ * (GravityMultiplier - 1.0f);

        ArtefactMesh->AddForce(FVector(0.0f, 0.0f, ExtraGravityZ), NAME_None, true);
    }
}

void AArtefactBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AArtefactBase, CurrentPosition);
    DOREPLIFETIME(AArtefactBase, bIsBeingCarried);
}
void AArtefactBase::OnRep_CurrentPosition()
{
	// Runs automatically on all clients when the server changes CurrentPosition.
    FString RoleStr = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    FString PosStr = UEnum::GetValueAsString(CurrentPosition);
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("%s OnRep_CurrentPosition -> %s"), *RoleStr, *PosStr));

	UpdateCollisionBasedOnState();
}

void AArtefactBase::OnRep_bIsBeingCarried()
{
    // Runs automatically on all clients when the server changes bIsBeingCarried.

    /*FString RoleStr = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    FString PosStr = UEnum::GetValueAsString(CurrentPosition);
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, FString::Printf(TEXT("%s OnRep_bIsBeingCarried: %d | CurrentPos: %s"), *RoleStr, bIsBeingCarried, *PosStr));

    if (!bIsBeingCarried)
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        FRotator UprightRot(0.0f, GetActorRotation().Yaw, 0.0f);
        SetActorRotation(UprightRot, ETeleportType::TeleportPhysics);
    }*/

    ApplyPhysicsBehaviour();
}

void AArtefactBase::OnPickedUp()
{
    // Here, any physics behaviour that is applied when it begins, and when it's dropped will be nullified so that it
    // moves freely with the character and isn't affected by gravity or any other forces or zones while being carried.
    // Until it should be.
    if (HasAuthority() && !bIsBeingCarried)
    {
        bIsBeingCarried = true;
        OnRep_bIsBeingCarried();
    }
}

void AArtefactBase::OnDropped()
{
    if (HasAuthority() && bIsBeingCarried)
    {
        bIsBeingCarried = false;
        OnRep_bIsBeingCarried();
    }
}

void AArtefactBase::Multicast_OnDropped_Implementation(FVector DropLocation, FRotator DropRotation)
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorLocationAndRotation(DropLocation, DropRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AArtefactBase::SetXYRotationLocked()
{
    if (ArtefactMesh)
    {
        // Get a reference to the component's BodyInstance.
        FBodyInstance* BodyInst = ArtefactMesh->GetBodyInstance();
        if (BodyInst)
        {
            BodyInst->bLockXRotation = true;
            BodyInst->bLockYRotation = true;
            BodyInst->bLockZRotation = false;
            // This is important to ensure that the physics engine recognizes the changes immediately.
            BodyInst->SetDOFLock(EDOFMode::SixDOF);
        }
    }
}

void AArtefactBase::HandleStandSteady()
{
    if (ArtefactMesh && !bIsBeingCarried)
	{
		ArtefactMesh->SetSimulatePhysics(HasAuthority());
		ArtefactMesh->SetEnableGravity(HasAuthority());

		if (HasAuthority())
		{
			ArtefactMesh->SetLinearDamping(0.0f);
			ArtefactMesh->SetAngularDamping(20.0f);
			SetXYRotationLocked();
			ArtefactMesh->WakeRigidBody(); // <-- Tvingar Chaos-fysiken att börja falla direkt!
		}
	}
}

void AArtefactBase::HandleSimulatePhysics()
{
    if (ArtefactMesh)
    {
        if (bIsBeingCarried)
        {
            // Only for if there will be specific things that this type need set when being carried.
        }
        else
        {
            ArtefactMesh->SetSimulatePhysics(HasAuthority());
            ArtefactMesh->SetEnableGravity(HasAuthority());
            ArtefactMesh->SetLinearDamping(0.05f);
            ArtefactMesh->SetAngularDamping(0.05f);
        }
    }
}

void AArtefactBase::HandleHoverStatic()
{
    if (ArtefactMesh)
    {
        if (bIsBeingCarried)
        {
            // Only for if there will be specific things that this type need set when being carried.
        }
        else
        {
            ArtefactMesh->SetSimulatePhysics(false);
            ArtefactMesh->SetEnableGravity(false);
            ArtefactMesh->SetLinearDamping(20.0f);
            ArtefactMesh->SetAngularDamping(20.0f);
            SetXYRotationLocked();
        }
    }
}



void AArtefactBase::HandleHoverDynamic()
{
    if (ArtefactMesh)
    {
        if (bIsBeingCarried)
        {
            // Only for if there will be specific things that this type need set when being carried.
        }
        else
        {
            ArtefactMesh->SetSimulatePhysics(true); // Maybe?
            ArtefactMesh->SetEnableGravity(false);
            ArtefactMesh->SetLinearDamping(1.0f);
            ArtefactMesh->SetAngularDamping(20.0f);
            SetXYRotationLocked();
        }
    }
}

void AArtefactBase::ApplyPhysicsBehaviour()
{
    UpdateCollisionBasedOnState();

    if (bIsBeingCarried)
    {
        ArtefactMesh->SetSimulatePhysics(false);
        ArtefactMesh->SetEnableGravity(false);

		// If the artefact is being carried, these are the damping values that all types will have while being carried. They are the minimum needed for carrying around an artefact.
        ArtefactMesh->SetLinearDamping(0.0f);
        ArtefactMesh->SetAngularDamping(0.0f);

        // The code below might not be needed, I have added it as a code example, to know how to affect the rotation if I need to.
		// These settings doesn't actually seem to affect the rotation of the artefact while being carried, so any artefacts that
        // should have specific rotation settings could just have that and never need to change them. Probably.

		// Get a reference to the component's BodyInstance.
        FBodyInstance* BodyInst = ArtefactMesh->GetBodyInstance();
        if (BodyInst)
        {
            BodyInst->bLockXRotation = false;
            BodyInst->bLockYRotation = false;
            BodyInst->bLockZRotation = false;

			// This is important to ensure that the physics engine recognizes the changes immediately.
            BodyInst->SetDOFLock(EDOFMode::SixDOF);
        }
    }

    switch (StandAloneBehaviour)
    {
        case EArtefactPhysicsBehaviour::StandSteady:
            HandleStandSteady();
            break;
        case EArtefactPhysicsBehaviour::SimulatePhysics:
            HandleSimulatePhysics();
            break;
        case EArtefactPhysicsBehaviour::HoverStatic:
            HandleHoverStatic();
            break;
        case EArtefactPhysicsBehaviour::HoverDynamic:
            HandleHoverDynamic();
            break;
    }
}

void AArtefactBase::UpdateCollisionBasedOnState()
{
    if (!ArtefactMesh) return;

    if (CurrentPosition == EArtifactPosition::None)
    {
        ArtefactMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ArtefactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        bCollisionIsDisabled = false;
    }
    else
    {
        ArtefactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
        ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
        bCollisionIsDisabled = true;
    }
}
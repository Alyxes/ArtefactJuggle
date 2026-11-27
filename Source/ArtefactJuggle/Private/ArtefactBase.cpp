#include "ArtefactBase.h"
#include "Net/UnrealNetwork.h"

AArtefactBase::AArtefactBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// Enable network replication and movement replication by default (since it starts on the ground)
	bReplicates = true;
	SetReplicateMovement(true);

	ArtefactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtefactMesh"));
	RootComponent = ArtefactMesh;

	CurrentPosition = EArtifactPosition::None;
}

void AArtefactBase::BeginPlay()
{
	Super::BeginPlay();

	// Ensure initial collision and replication settings match the starting state
	UpdateStateConfiguration();
}

void AArtefactBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AArtefactBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArtefactBase, CurrentPosition);
}

void AArtefactBase::SetArtefactPosition(EArtifactPosition NewPosition)
{
	// Only the server (or Standalone player) is allowed to change the authoritative state
	if (!HasAuthority()) return;

	if (CurrentPosition != NewPosition)
	{
		CurrentPosition = NewPosition;

		// Manually call OnRep on the server because C++ only triggers OnRep automatically on clients
		OnRep_CurrentPosition();
	}
}

void AArtefactBase::OnRep_CurrentPosition()
{
	// Runs on both the server (via SetArtefactPosition) and all clients (via replication)
	UpdateStateConfiguration();
}

void AArtefactBase::UpdateStateConfiguration()
{
	if (!ArtefactMesh) return;

	if (CurrentPosition == EArtifactPosition::None)
	{
		ArtefactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

		// Only the server runs physics simulation; clients receive movement via replication.
		if (HasAuthority())
		{
			ArtefactMesh->SetSimulatePhysics(true);
			ArtefactMesh->WakeAllRigidBodies();
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("C++ State: On Ground (Collision & Movement Rep ON)"));
		}
	}
	else
	{
		ArtefactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);

		// Turn off physics on both server and client so gravity doesn't pull it out of the player's hands.
		ArtefactMesh->SetSimulatePhysics(false);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("C++ State: Held (Collision & Movement Rep OFF)"));
		}
	}
}

void AArtefactBase::Multicast_OnDropped_Implementation(FVector DropLocation, FRotator DropRotation)
{
	// On clients (spectators), snap immediately to the server's authoritative drop location
	// so Unreal's network smoothing doesn't make the stone glide unevenly from the previous player's hand.
	if (!HasAuthority())
	{
		SetActorLocationAndRotation(DropLocation, DropRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
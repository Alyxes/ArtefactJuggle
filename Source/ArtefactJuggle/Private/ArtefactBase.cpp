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
}

void AArtefactBase::BeginPlay()
{
    Super::BeginPlay();
}

void AArtefactBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// NÄTVERKS-REGISTRERING: Här talar vi om för UE vilka variabler som ska skickas över nätet
void AArtefactBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Detta makro säger: "Skicka CurrentPosition från Servern till alla Klienter så fort den ändras"
    DOREPLIFETIME(AArtefactBase, CurrentPosition);
}
void AArtefactBase::OnRep_CurrentPosition()
{
	// Körs AUTOMATISKT på alla klienter när servern ändrar CurrentPosition!
	UpdateCollisionBasedOnState();
}

void AArtefactBase::UpdateCollisionBasedOnState()
{
	if (!ArtefactMesh) return;

	if (CurrentPosition == EArtifactPosition::None)
	{
		SetReplicateMovement(true);

		if (bCollisionIsDisabled)
		{
			/*ArtefactMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			ArtefactMesh->SetCollisionResponseToAllChannels(ECR_Block);*/

			ArtefactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
			ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

			bCollisionIsDisabled = false;

			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("C++ Fysik: Kollision påslagen"));
		}
	}
	else
	{
		SetReplicateMovement(false);

		if (!bCollisionIsDisabled)
		{
			// ArtefactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			ArtefactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
			ArtefactMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);

			bCollisionIsDisabled = true;

			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("C++ Fysik: Kollision avslagen"));
		}
	}
}
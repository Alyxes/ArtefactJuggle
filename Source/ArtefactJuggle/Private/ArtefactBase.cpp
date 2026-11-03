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

// 3. NÄTVERKS-REGISTRERING: Här talar vi om för UE vilka variabler som ska skickas över nätet
void AArtefactBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Detta makro säger: "Skicka CurrentPosition från Servern till alla Klienter så fort den ändras"
    DOREPLIFETIME(AArtefactBase, CurrentPosition);
}
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtefactBase.generated.h"

// Vår nätverkssäkrade enum för de 5 fasta riktningarna samt ett tomt läge (None)
UENUM(BlueprintType)
enum class EArtifactPosition : uint8
{
    Forward   UMETA(DisplayName = "Forward"),
    Left      UMETA(DisplayName = "Left"),
    Right     UMETA(DisplayName = "Right"),
    Up        UMETA(DisplayName = "Up"),
    Down      UMETA(DisplayName = "Down"),
    None      UMETA(DisplayName = "None")
};

UCLASS()
class ARTEFACTJUGGLE_API AArtefactBase : public AActor
{
    GENERATED_BODY()

public:
    // Konstruktor: Här sätter vi startinställningar för klassen
    AArtefactBase();

protected:
    // Körs en gång när spelet startar eller objektet spawnas
    virtual void BeginPlay() override;

public:
    // Körs varje frame
    virtual void Tick(float DeltaTime) override;

    // Denna funktion krävs av Unreal för att registrera nätverksreplikerade variabler
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Den fysiska stenen (mesh) som syns i världen
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Artefact|Components")
    UStaticMeshComponent* ArtefactMesh;

    // Variabeln som håller koll på var stenen är just nu (Replicated = nätverkssäkrad)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Artefact|State")
    EArtifactPosition CurrentPosition;
};
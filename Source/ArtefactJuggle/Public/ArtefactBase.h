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

UENUM(BlueprintType)
enum class EArtefactPhysicsBehaviour : uint8
{
    StandSteady        UMETA(DisplayName = "Stand Steady", ToolTip = "The artefact will be put down on the ground, standing, and not have any physics behaviour."),
    SimulatePhysics    UMETA(DisplayName = "Simulate Physics", ToolTip = "The artefact will simulate physics when put down, rolling if round, falling over edges if put too close to them."),
    HoverStatic        UMETA(DisplayName = "Hover Static", ToolTip = "The artefact will hover in the air, in front of the player, and not fall or be moved by any force."),
    HoverDynamic       UMETA(DisplayName = "Hover Dynamic", ToolTip = "The artefact will hover in the air, in front of the player, but it will slowly descend if they stand on it, and can be moved by surrounding forces, wind, magnetism, etc.")
};

UENUM(BlueprintType)
enum class EArtefactDeflectionType : uint8
{
    Reflect             UMETA(DisplayName = "Reflect"),
    Block               UMETA(DisplayName = "Block"),
	StickyForAll		UMETA(DisplayName = "Sticky For All"),
	StickyBluntOnly     UMETA(DisplayName = "Sticky Blunt Only"),
    NoShielding         UMETA(DisplayName = "No Shielding")
};

UCLASS(Blueprintable, BlueprintType)
class ARTEFACTJUGGLE_API AArtefactBase : public AActor
{
    GENERATED_BODY()

public:
    // Konstruktor: Här sätter vi startinställningar för klassen
    AArtefactBase();

protected:
    // Körs en gång när spelet startar eller objektet spawnas
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnRep_CurrentPosition();

    UFUNCTION()
    void OnRep_bIsBeingCarried();

    // En hjälpfunktion för att hålla koden modulär och ren
    void UpdateCollisionBasedOnState();

private:

    bool bCollisionIsDisabled = false;

public:

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Artefact|Components", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* ArtefactMesh;

	// Variable that keeps track of the current position of the artefact, OnRep_CurrentPosition makes sure that the variable is updated on clients when it changes on the server.
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPosition, BlueprintReadOnly, Category = "Artefact|State")
    EArtifactPosition CurrentPosition;

    // Physical behaviour of the artefact when it is not carried.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artefact|Stats")
	EArtefactPhysicsBehaviour StandAloneBehaviour = EArtefactPhysicsBehaviour::StandSteady;

    // How this artefact deflects (or not) projectiles hitting it while carried.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artefact|Stats")
	EArtefactDeflectionType DeflectionType = EArtefactDeflectionType::Block;

    // Used to make the player slower or faster while carrying this artefact. < 1 = slower, > 1 = faster.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artefact|Stats", meta = (ClampMin = "0.05", ClampMax = "3.0"))
	float MovementSpeedMultiplier = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_bIsBeingCarried, BlueprintReadOnly, Category = "Artefact|State")
	bool bIsBeingCarried;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artefact|Stats")
    float GravityMultiplier = 1.0f;

public:
    // Körs varje frame
    virtual void Tick(float DeltaTime) override;

    // Denna funktion krävs av Unreal för att registrera nätverksreplikerade variabler
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void ApplyPhysicsBehaviour();

    void OnPickedUp();

    void OnDropped();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnDropped(FVector DropLocation, FRotator DropRotation);

private:

    void HandleStandSteady();
    void HandleSimulatePhysics();
    void HandleHoverStatic();
	void HandleHoverDynamic();

    void SetXYRotationLocked();
};
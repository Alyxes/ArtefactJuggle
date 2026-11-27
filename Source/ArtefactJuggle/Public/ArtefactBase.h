#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtefactBase.generated.h"

// Network-safe enum for the 5 fixed slots plus an empty state (None)
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
	AArtefactBase();

protected:
	virtual void BeginPlay() override;

	// Automatically called on clients when CurrentPosition replicates, and manually called on the server
	UFUNCTION()
	void OnRep_CurrentPosition();

	// Central helper function that updates collision and movement replication based on CurrentPosition
	void UpdateStateConfiguration();

public:
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server-authoritative setter: Call this whenever the artefact changes state (picked up, moved slot, or dropped)
	UFUNCTION(BlueprintCallable, Category = "Artefact|State")
	void SetArtefactPosition(EArtifactPosition NewPosition);

	UFUNCTION(BlueprintPure, Category = "Artefact|State")
	EArtifactPosition GetArtefactPosition() const { return CurrentPosition; }

	// Broadcasts an instant teleport to all clients when dropped so spectators don't see uneven network smoothing
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDropped(FVector DropLocation, FRotator DropRotation);

	// The physical mesh visible in the world
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Artefact|Components")
	UStaticMeshComponent* ArtefactMesh;

protected:
	// Replicated state variable. Only modified via SetArtefactPosition on the server.
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPosition, BlueprintReadOnly, Category = "Artefact|State")
	EArtifactPosition CurrentPosition;
};
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtefactManipulator.generated.h"

// Framåtdeklaration av både klassen och enumen.
class AArtefactBase;
enum class EArtifactPosition : uint8;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ARTEFACTJUGGLE_API UArtefactManipulator : public UActorComponent
{
	GENERATED_BODY()

public:
	UArtefactManipulator();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SetIsControllingArtefact(bool bNewState);

	void HandleDirectionInput(FVector2D InputVector, FVector CameraForwardVector);

	UFUNCTION(BlueprintCallable)
	void ResetCurrentPosition();

	UFUNCTION(Server, Reliable)
	void Server_ChangePosition(EArtifactPosition NewPosition);

	UFUNCTION(Server, Reliable)
	void Server_DropArtefact(class AArtefactBase* ArtefactToDrop, FVector FinalLocation, FRotator FinalRotation);

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Artefact")
	class AArtefactBase* EquippedArtefact;

	// UPROPERTY(Replicated, BlueprintReadWrite, Category = "Artefact")
	bool bHasNeutralizedInput;

	UPROPERTY(BlueprintReadWrite, Category = "Artefact")
	bool bIsControllingArtefact;

	UPROPERTY(BlueprintReadWrite, Category = "Artefact")
	bool bIsGonnaPutDownArtefact;

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Artefact")
	EArtifactPosition CurrentPosition;

	UPROPERTY(EditAnywhere, Category = "Artefact Settings")
	float InterpSpeed;

	FTransform GetTargetSlotTransform(EArtifactPosition Position) const;
};
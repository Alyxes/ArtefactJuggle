#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// Dynamic multicast delegate to allow Blueprint classes (like the HUD/Character) to listen to health changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChangedSignature, float, HealthPercent);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ARTEFACTJUGGLE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

protected:
	virtual void BeginPlay() override;

	// The replicated health value. ReplicatedUsing ensures clients execute OnRep_Health when the server updates this value.
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Health")
	float Health;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth;

	// Automatically called on clients upon network replication
	UFUNCTION()
	void OnRep_Health();

public:
	// Blueprint event that your character or HUD will bind to
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChangedSignature OnHealthChanged;

	// Function to apply damage (Executes ONLY on the server!)
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Health")
	void TakeDamage(float DamageAmount);
};
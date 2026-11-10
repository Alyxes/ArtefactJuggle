#include "HealthComponent.h"
#include "Net/UnrealNetwork.h" // Essential for DOREPLIFETIME to function!

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Tells Unreal that this component should communicate across the network
	SetIsReplicatedByDefault(true);

	MaxHealth = 100.f;
	Health = MaxHealth;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize health to maximum upon game start
	Health = MaxHealth;
}

// Mandatory Unreal function to register WHICH properties should replicate over the network
void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Registers the Health variable for network replication from server -> client
	DOREPLIFETIME(UHealthComponent, Health);
}

// This function is called on the server when a client takes damage. It ensures that only the server can modify the health value.
void UHealthComponent::TakeDamage(float DamageAmount)
{
	// Safety check: Only execute damage logic if running on the server
	if (!GetOwner()->HasAuthority()) return;

	if (Health <= 0.f) return;

	Health = FMath::Clamp(Health - DamageAmount, 0.f, MaxHealth);

	float HealthPercent = Health / MaxHealth;
	// Broadcast here only sends info to the server's local instance, so the server's HUD can update.
	OnHealthChanged.Broadcast(HealthPercent);
}

// Executes automatically on CLIENTS when the server changes the Health value
void UHealthComponent::OnRep_Health()
{
	float HealthPercent = Health / MaxHealth;

	// Broadcast only sends info to the local client, in this case the player who owns this character, so their HUD can update.
	OnHealthChanged.Broadcast(HealthPercent);
}
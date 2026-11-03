// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ArtefactJuggleCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AArtefactJuggleCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	// Keeps track of if the player is pushing the modifier-button.
	UPROPERTY(BlueprintReadWrite, Category = "Artefact Juggle")
	bool bIsModifierPressed = false;

	// Keeps track of if the player is holding an artefact.
	UPROPERTY(BlueprintReadWrite, Category = "Artefact Juggle")
	bool bIsHoldingArtefact = false;

public:

	/** Constructor */
	AArtefactJuggleCharacter();

	UPROPERTY(BlueprintReadWrite, Category = "Artefact Juggle")
	AArtefactBase* CurrentArtefact;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void FinishedJuggleMove();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	// MÅL 1 & 2: Funktion som ENBART sköter upplockning och fastsättning (Attach)
	UFUNCTION(BlueprintCallable, Category = "Artefact Juggle")
	void PickUpArtefact(AArtefactBase* ArtefactToPickUp);

	UFUNCTION(BlueprintCallable, Category = "Artefact Juggle")
	void DropArtefact();

	// MÅL 3: Aktiverar modifieraren och fryser WASD (Kopplas till IA_ShiftToggle)
	UFUNCTION(BlueprintCallable, Category = "Artefact Juggle")
	void StartArtefactJuggle(); // Vi tar bort argumentet härifrån!

	// Släpper modifieraren och ger tillbaka WASD-kontrollen
	UFUNCTION(BlueprintCallable, Category = "Artefact Juggle")
	void StopArtefactJuggle();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};


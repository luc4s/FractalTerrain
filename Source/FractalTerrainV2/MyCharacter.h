// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class ATerrain;
class AStaticMeshActor;

class CharacterEventListener {
public:
	virtual void OnSelect(AActor* selected) {};
};

UCLASS()
class FRACTALTERRAINV2_API AMyCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	// Sets default values for this character's properties
	AMyCharacter();

protected:
	enum ActionMode {
		BREAK_BLOCKS = 1,
		SPAWN_OBJECT = 2,
		SELECT_OBJECT = 3,
	};

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void SetGhost(const FString &resName);

	void DispatchEvent();

	ATerrain* m_terrain;
	bool m_hasMoved;

	ActionMode m_actionMode;

	// Used when placing object
	AStaticMeshActor* m_ghost;
	FString m_ghostBlueprintResName;

	// Used for selection
	AActor* m_selected;

	TArray<CharacterEventListener*> m_listeners;
	TArray<CharacterEventListener*> m_listenersToRemove;

public:
	void AddSelectListener(CharacterEventListener* listener);
	void RemoveSelectListener(CharacterEventListener* listener);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION()
	void SetActionMode(int mode);

	// Handles input for moving forward and backward.
	UFUNCTION()
	void MoveForward(float Value);

	// Handles input for moving right and left.
	UFUNCTION()
	void MoveRight(float Value);

	UFUNCTION()
	void Scroll(float value);

	UFUNCTION()
	void Hit();

	// Sets jump flag when key is pressed.
	UFUNCTION()
	void StartJump();

	// Clears jump flag when key is released.
	UFUNCTION()
	void StopJump();
};

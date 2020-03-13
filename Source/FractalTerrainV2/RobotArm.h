// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyCharacter.h"

#include "RobotArm.generated.h"


UENUM(BlueprintType)
enum class Axe : uint8 {
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y"),
	Z UMETA(DisplayName = "Z")
};

UCLASS()
class FRACTALTERRAINV2_API ARobotArm : public AActor, public CharacterEventListener
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	ARobotArm();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void OnSelect(AActor* selected) override;

	UFUNCTION(BlueprintNativeEvent, Category = "RobotArm")
	void GetBones(TArray<int> &indices, TArray<Axe> &axes);
	void GetBones_Implementation(TArray<int>& indices, TArray<Axe>& axes) {};

	UFUNCTION(BlueprintCallable, Category = "RobotArm")
	bool SetBoneAnglesIK(const FVector& target, float range = 10);

	UFUNCTION(BlueprintCallable, Category = "RobotArm")
	void SetSettingUp(bool value);

	TArray<Axe> BoneAxes;
	TArray<int> BoneIndices;

	AActor* Src;
	AActor* Dst;
};

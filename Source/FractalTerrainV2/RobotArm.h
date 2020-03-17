// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyCharacter.h"

#include "Components/TimelineComponent.h"

#include "RobotArm.generated.h"


class UCurveFloat;

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

private:
	UFUNCTION()
	void HandleProgress(float Value);

	UFUNCTION()
	void HandleTimelineEnd();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void OnSelect(AActor* selected) override;

	UFUNCTION(BlueprintNativeEvent, Category = "RobotArm")
	void GetBones(TArray<int> &indices);
	void GetBones_Implementation(TArray<int>& indices) {};

	/**
		Add a node to this arm path.
	 */
	UFUNCTION(BlueprintCallable, Category = "RobotArm")
	bool AddPathNode(const FVector& target, const FRotator& orientation);

	/**
		Put the arm in setup mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "RobotArm")
	void SetSettingUp(bool value);

	UFUNCTION(BlueprintImplementableEvent, Category = "RobotArm")
	UObject* GetRessource(FName name);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotArm")
	UCurveFloat* curve;

	FTimeline Timeline;

	TArray<int> BoneIndices;
	TArray<FName> BoneNames;
	EAxis::Type BoneAxes[6];

	AActor* Src;
	AActor* Dst;

	TArray< TArray<float> > PathNodes;
};

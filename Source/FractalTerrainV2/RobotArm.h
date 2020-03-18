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
	enum GripAction {
		NOTHING,
		OPEN,
		CLOSE
	};

	UFUNCTION()
	void HandleProgress(float Value);

	UFUNCTION()
	void HandleTimelineEnd();

	/**
		Calculates end angle to minimize travel distance.
	 */
	void ShortestAngle(float startAngle, float& endAngle) const {
		float diff = abs(endAngle - startAngle);
		if (abs(endAngle + 360 - startAngle) < diff)
			endAngle += 360;
		else if (abs(endAngle - 360 - startAngle) < diff)
			endAngle -= 360;
	}

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void OnSelect(AActor* selected) override;

	/**
		Callbacks for timeline events
	*/
	UFUNCTION()
	void HandleTryPick();
	UFUNCTION()
	void HandleTryDrop();
	UFUNCTION(BlueprintImplementableEvent, Category = "RobotArm")
	void OnPick();
	UFUNCTION(BlueprintImplementableEvent, Category = "RobotArm")
	void OnDrop();

	UFUNCTION(BlueprintImplementableEvent, Category = "RobotArm")
	bool ReadyToPick();

	UFUNCTION(BlueprintImplementableEvent, Category = "RobotArm")
	bool ReadyToDrop();



	UFUNCTION(BlueprintNativeEvent, Category = "RobotArm")
	void GetBones(TArray<int> &indices);
	void GetBones_Implementation(TArray<int>& indices) {};

	/**
		Add a node to this arm path.
	 */
	bool AddPathNode(const FVector& target, const FRotator& orientation, GripAction gripAction);

	/**
		Removes last added path node.
	 */
	void PopPathNode() {
		if (PathNodes.Num() > 0)
			PathNodes.Pop();
	}
	/**
		Put the arm in setup mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "RobotArm")
	void SetSettingUp(bool value);

	UFUNCTION(BlueprintImplementableEvent, Category = "RobotArm")
	UObject* GetRessource(FName name);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotArm")
	float ArmSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotArm")
	FName EffectorBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotArm")
	UCurveFloat* curve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotArm")
	AActor* Src;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotArm")
	AActor* Dst;

	FTimeline Timeline;

	bool WaitDrop;
	bool WaitPick;

	TArray<int> BoneIndices;
	TArray<FName> BoneNames;
	EAxis::Type BoneAxes[6];

	TArray< TArray<float> > PathNodes;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningDrill.generated.h"

UCLASS()
class FRACTALTERRAINV2_API AMiningDrill : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMiningDrill();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	float m_timeAcc;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;


	UPROPERTY(EditAnywhere)
	AActor *m_terrain;

};

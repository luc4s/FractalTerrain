// Fill out your copyright notice in the Description page of Project Settings.


#include "MiningDrill.h"

// Sets default values
AMiningDrill::AMiningDrill()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	m_timeAcc = 0;
}

// Called when the game starts or when spawned
void AMiningDrill::BeginPlay()
{
	Super::BeginPlay();
	m_terrain = nullptr;
}

// Called every frame
void AMiningDrill::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	m_timeAcc += DeltaTime;
	if (m_timeAcc > 1) {
		m_timeAcc = 0;

	}
}


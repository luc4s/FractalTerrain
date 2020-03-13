// Fill out your copyright notice in the Description page of Project Settings.


#include "RobotArm.h"
#include "Kismet/GameplayStatics.h"

#include "Components/PoseableMeshComponent.h"

#include <cmath>

// Sets default values
ARobotArm::ARobotArm() : Src(NULL), Dst(NULL)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickInterval(1000);
}

// Called when the game starts or when spawned
void ARobotArm::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ARobotArm::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void ARobotArm::SetSettingUp(bool value) {
	GetBones(BoneIndices, BoneAxes);
	AMyCharacter* character = Cast<AMyCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	character->SetActionMode(-1);
	character->AddSelectListener(this);
}

void ARobotArm::OnSelect(AActor *selected) {
	UE_LOG(LogTemp, Warning, TEXT("Selected: %s"), *AActor::GetDebugName(selected));
	if (!Src) {
		Src = selected;
		SetBoneAnglesIK(Src->GetActorLocation());
		return;
	}

	Dst = selected;
	SetBoneAnglesIK(Dst->GetActorLocation());

	AMyCharacter* character = Cast<AMyCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	character->RemoveSelectListener(this);
	character->SetActionMode(1);
}

bool ARobotArm::SetBoneAnglesIK(const FVector& target, float range) {
	//*
	TArray<float> angles;

	TArray<FTransform> transforms;
	UPoseableMeshComponent* armMesh = Cast<UPoseableMeshComponent>(RootComponent);
	if (!armMesh) {
		UE_LOG(LogTemp, Error, TEXT("Could not find skeletal mesh."));
		return false;
	}

	if (BoneAxes[0] != Axe::Z) {
		UE_LOG(LogTemp, Error, TEXT("Base must be able to rotate around Z axis"));
		return false;
	}

	for (int idx : BoneIndices) {
		transforms.Add(armMesh->GetBoneTransform(idx));
		angles.Add(0.0f);
	}

	const FName baseName = armMesh->GetBoneName(BoneIndices[0]);
	const FName effectorName = armMesh->GetBoneName(BoneIndices.Last());
	const EBoneSpaces::Type space = EBoneSpaces::ComponentSpace;

	// Align base toward target
	FVector targetVec = target - armMesh->GetBoneLocationByName(baseName, EBoneSpaces::WorldSpace);
	FVector fwdVec = GetActorForwardVector();
	float angle = acos(targetVec.CosineAngle2D(fwdVec)) * (180 / PI);
	if (FVector::CrossProduct(fwdVec, targetVec).Z < 0)
		angle = -angle;


	FRotator baseRot = armMesh->GetBoneRotationByName(baseName, space);
	baseRot.Yaw = angle;
	armMesh->SetBoneRotationByName("Sphere", baseRot, space);

	//FRotator botRot(armMesh->GetBoneRotationByName("Middle", EBoneSpaces::Type::ComponentSpace));
	//botRot.Pitch += 45;
	//armMesh->SetBoneRotationByName("Middle", botRot, EBoneSpaces::Type::ComponentSpace);
	

	FVector targetBS;
	//targetBS.Z += 10; // Temp fix not to stick arm into belt

	FRotator targetRot;
	armMesh->TransformToBoneSpace("Bottom", target, FRotator(), targetBS, targetRot);
	//targetBS.Y = 0;

	UE_LOG(LogTemp, Warning, TEXT("TARGET = %s"), *targetBS.ToString());

	TArray<FName> names;
	for (size_t i = 1; i < BoneIndices.Num() - 1; ++i)
		names.Add(armMesh->GetBoneName(BoneIndices[i]));

	for (int i = names.Num() - 1; i >= 0; --i) {
		const FName &boneName = names[i];
		const FVector effectorPos = armMesh->GetBoneLocationByName(effectorName, space);
		const FVector jointPos = armMesh->GetBoneLocationByName(boneName, space);

//		UE_LOG(LogTemp, Warning, TEXT("BonePos: %s"), *jointPos.ToString());
		const FVector v1 = effectorPos - jointPos;
		const FVector v2 = targetBS - jointPos;
		UE_LOG(LogTemp, Warning, TEXT("%s, j = %s"), *boneName.ToString(), *jointPos.ToString());
		//UE_LOG(LogTemp, Warning, TEXT("v1 = %s v2 = %s"), *v1.ToString(), *v2.ToString());
		const float dot = FVector::DotProduct(v1, v2);
		const float sizesProd = v1.Size() * v2.Size();
		float angle = acos(dot / sizesProd) * (180 / PI);

		FVector CP = FVector::CrossProduct(v1, v2);
		//UE_LOG(LogTemp, Warning, TEXT("Angle = %f, CP = %s"), angle, *CP.ToString());
		//if (FVector::CrossProduct(v1, v2).Y < 0)
		//	angle = -angle;

		FRotator rotation = armMesh->GetBoneRotationByName(boneName, space);
		rotation.Pitch = angle;
		armMesh->SetBoneRotationByName(boneName, rotation, space);
	}
	return true;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "RobotArm.h"
#include "Kismet/GameplayStatics.h"

#include "Components/PoseableMeshComponent.h"

#include <cmath>

// Sets default values
ARobotArm::ARobotArm() : Src(NULL), Dst(NULL), CurrentNode(0)
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
	GetBones(BoneIndices);
	AMyCharacter* character = Cast<AMyCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	character->SetActionMode(-1);
	character->AddSelectListener(this);
}

void ARobotArm::OnSelect(AActor *selected) {
	if (!selected->ActorHasTag("Belt"))
		return;

	FVector target = selected->GetActorLocation();
	FRotator orientation = selected->GetActorRotation();
	target.Z += 30;
	if (!Src) {
		Src = selected;
		AddPathNode(target, orientation);
		return;
	}

	// Compute intermediate for shortest path interpolation
	AddPathNode((Src->GetActorLocation() + target) / 2, (Src->GetActorRotation() + orientation) * 0.5);

	// Final node
	Dst = selected;
	AddPathNode(target, orientation);

	// Remove selection listener
	AMyCharacter* character = Cast<AMyCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	character->RemoveSelectListener(this);
	character->SetActionMode(1);
}

bool ARobotArm::AddPathNode(const FVector& target, const FRotator &orientation) {
	//*

	UPoseableMeshComponent* armMesh = Cast<UPoseableMeshComponent>(RootComponent);
	if (!armMesh) {
		UE_LOG(LogTemp, Error, TEXT("Could not find skeletal mesh."));
		return false;
	}

	const FVector actorLocation = GetActorLocation();
	const FName baseName = armMesh->GetBoneName(BoneIndices[0]);
	const EBoneSpaces::Type localSpace = EBoneSpaces::ComponentSpace;
	const FVector armBasePos = armMesh->GetBoneLocationByName(baseName, EBoneSpaces::WorldSpace);

	TArray<FName> names;
	for (size_t i = 1; i < BoneIndices.Num(); ++i)
		names.Add(armMesh->GetBoneName(BoneIndices[i]));

	const float armLength = FVector::Distance(
		armMesh->GetBoneLocationByName(names[0], localSpace),
		armMesh->GetBoneLocationByName(names[1], localSpace)
	);
	const float dist = FVector::Distance(
		armBasePos,
		target
	);

	// Target cannot be reached
	if (dist > 3 * armLength)
		return false;

	// Compute angle to orient arm toward target
	// Init vectors
	FVector targetV = target - actorLocation;
	targetV.Normalize();
	const FVector rightV = GetActorRightVector();

	// Compute angle in degrees
	float angle = acos(targetV.CosineAngle2D(rightV)) * 180.f / PI;
	if (FVector::CrossProduct(targetV, rightV).Z > 0)
		angle = -angle;

	FRotator baseRot = armMesh->GetBoneRotationByName(baseName, localSpace);
	baseRot.Yaw = angle;
	armMesh->SetBoneRotationByName(baseName, baseRot, localSpace);

	// Compute joints angle
	const float heightDiff = target.Z - armBasePos.Z;
	float outerAngle = -atan(heightDiff / dist) * (180.f / PI);

	float innerAngle;
	const float term = (dist - armLength) / (-2 * armLength);
	if (dist <= armLength)
		innerAngle = acos(term);
	else
		innerAngle = asin(-term);
	
	innerAngle = (innerAngle * 180.f / PI) + 90.f;
	outerAngle += 90.f - (180.f - innerAngle);

	// Apply rotations
	const float angles[5] = {
		outerAngle,
		outerAngle + 180 - innerAngle,
		outerAngle + 2 * (180 - innerAngle),
		-90,
		orientation.Yaw
	};

	const EAxis::Type axes[5] = {
		EAxis::X,
		EAxis::X,
		EAxis::X,
		EAxis::Y,
		EAxis::Z
	};

	float acc = 0;
	for (size_t i = 0; i < 5; ++i) {
		FRotator rotation = armMesh->GetBoneRotationByName(names[i], localSpace);
		rotation.SetComponentForAxis(axes[i], angles[i]);
		rotation.Normalize();
		armMesh->SetBoneRotationByName(names[i], rotation, localSpace);
	}

	return true;
}

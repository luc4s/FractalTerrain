// Fill out your copyright notice in the Description page of Project Settings.


#include "RobotArm.h"
#include "Kismet/GameplayStatics.h"

#include "Components/PoseableMeshComponent.h"

#include <cmath>

// Sets default values
ARobotArm::ARobotArm() :
	Src(NULL),
	Dst(NULL),
	BoneAxes{
		EAxis::Z,
		EAxis::X,
		EAxis::X,
		EAxis::X,
		EAxis::Y,
		EAxis::Z
	}
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
	Timeline.TickTimeline(DeltaTime);
}

void ARobotArm::SetSettingUp(bool value) {
	GetBones(BoneIndices);

	UPoseableMeshComponent* armMesh = Cast<UPoseableMeshComponent>(RootComponent);
	if (!armMesh) {
		UE_LOG(LogTemp, Error, TEXT("Could not find skeletal mesh."));
		return;
	}

	for (size_t i = 0; i < BoneIndices.Num(); ++i)
		BoneNames.Add(armMesh->GetBoneName(BoneIndices[i]));

	AMyCharacter* character = Cast<AMyCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	character->SetActionMode(-1);
	character->AddSelectListener(this);
}

void ARobotArm::OnSelect(AActor *selected) {
	if (!selected->ActorHasTag("Belt"))
		return;

	FVector target = selected->GetActorLocation();
	target.Z += 30;
	FRotator orientation = selected->GetActorRotation();
	if (!Src) {
		Src = selected;
		AddPathNode(target, orientation);
		target.Z += 30; // TODO: Remove this offset and handle target position properly
		AddPathNode(target, orientation);
		return;
	}

	// Final node
	Dst = selected;
	target.Z += 30; // TODO: Remove this offset and handle target position properly
	AddPathNode(target, orientation);
	target.Z -= 30;
	AddPathNode(target, orientation);

	// Remove selection listener
	AMyCharacter* character = Cast<AMyCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	character->RemoveSelectListener(this);
	character->SetActionMode(1);

	// Setup timeline
	if (!curve) {
		UE_LOG(LogTemp, Warning, TEXT("No curve for timeline!"));
		return;
	}

	FOnTimelineFloat onProgressCallback;
	FOnTimelineEventStatic onTimelineFinishedCallback;

	onProgressCallback.BindUFunction(this, FName("HandleProgress"));
	onTimelineFinishedCallback.BindUFunction(this, FName("HandleTimelineEnd"));

	Timeline.AddInterpFloat(curve, onProgressCallback);
	Timeline.SetTimelineFinishedFunc(onTimelineFinishedCallback);
	Timeline.SetLooping(false);
	Timeline.PlayFromStart();
}

bool ARobotArm::AddPathNode(const FVector& target, const FRotator &orientation) {
	UPoseableMeshComponent* armMesh = Cast<UPoseableMeshComponent>(RootComponent);

	const FVector actorLocation = GetActorLocation();
	const FName baseName = BoneNames[0];
	const EBoneSpaces::Type localSpace = EBoneSpaces::ComponentSpace;
	const FVector armBasePos = armMesh->GetBoneLocationByName(baseName, EBoneSpaces::WorldSpace);

	const float armLength = FVector::Distance(
		armMesh->GetBoneLocationByName(BoneNames[1], localSpace),
		armMesh->GetBoneLocationByName(BoneNames[2], localSpace)
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

	if (PathNodes.Num() > 0) {
		float lastAngle = PathNodes[PathNodes.Num() - 1][0];
		float diff = abs(angle - lastAngle);
		if (abs(angle + 360 - lastAngle) < diff)
			angle += 360;
		else if (abs(angle - 360 - lastAngle) < diff)
			angle -= 360;
	}
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
	const TArray<float> angles({
		angle,
		outerAngle,
		outerAngle + 180 - innerAngle,
		outerAngle + 2 * (180 - innerAngle),
		-90,
		orientation.Yaw
	});

	for (size_t i = 1; i < 6; ++i) {
		FRotator rotation = armMesh->GetBoneRotationByName(BoneNames[i], localSpace);
		rotation.SetComponentForAxis(BoneAxes[i], angles[i]);
		rotation.Normalize();
		armMesh->SetBoneRotationByName(BoneNames[i], rotation, localSpace);
	}

	PathNodes.Add(angles);
	return true;
}

void ARobotArm::HandleProgress(float Value) {
	const size_t stateCount = PathNodes.Num() - 1;
	const float stateTime = 1.f / stateCount;
	const float progress = Value / stateTime;
	const size_t currentState = progress;
	const size_t nextState = currentState + 1;
	if (nextState >= PathNodes.Num())
		return;

	const float interp = (progress - currentState);
	UE_LOG(LogTemp, Warning, TEXT("%f"), interp);

	UPoseableMeshComponent* arm = Cast<UPoseableMeshComponent>(RootComponent);
	for (size_t i = 0; i < BoneNames.Num(); ++i) {
		const float s0 = PathNodes[currentState][i];
		const float s1 = PathNodes[nextState][i];
		const float angle = s0 * (1 - interp) + s1 * interp;
		//if (i == 1)
		//	UE_LOG(LogTemp, Warning, TEXT("[%f;%f] = %f"), s0, s1, angle);

		FRotator rotation = arm->GetBoneRotationByName(BoneNames[i], EBoneSpaces::ComponentSpace);
		rotation.SetComponentForAxis(BoneAxes[i], angle);
		//rotation.Normalize();
		arm->SetBoneRotationByName(BoneNames[i], rotation, EBoneSpaces::ComponentSpace);
	}
}

void ARobotArm::HandleTimelineEnd() {
	static bool reverse = false;
	if (reverse) {
		Timeline.PlayFromStart();
		reverse = false;
	}
	else {
		Timeline.ReverseFromEnd();
		reverse = true;
	}
}
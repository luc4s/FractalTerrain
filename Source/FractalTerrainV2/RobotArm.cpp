// Fill out your copyright notice in the Description page of Project Settings.


#include "RobotArm.h"
#include "Kismet/GameplayStatics.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SphereComponent.h"

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
	},
	ArmSpeed(50)
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
	if (!selected->ActorHasTag("ArmCanPick"))
		return;

	const USphereComponent* pickUpSlot = Cast<USphereComponent>(
		selected->GetComponentsByTag(USphereComponent::StaticClass(), "PickUpLoc")[0]);
	const FVector pickUpLoc = pickUpSlot->GetComponentLocation();
	const FRotator pickUpRot = pickUpSlot->GetComponentRotation();
	
	const USphereComponent* waitSlot = Cast<USphereComponent>(
		selected->GetComponentsByTag(USphereComponent::StaticClass(), "WaitLoc")[0]);
	const FVector waitLoc = waitSlot->GetComponentLocation();
	const FRotator waitRot = waitSlot->GetComponentRotation();

	if (!Src) {
		if (AddPathNode(pickUpLoc, pickUpRot)) {
			if (AddPathNode(waitLoc, waitRot))
				Src = selected;
			else
				PopPathNode();
		}
		return;
	}

	// Source and Destination canot be the same
	if (selected == Src)
		return;

	// Dest
	if (!Dst) {
		if (AddPathNode(waitLoc, pickUpRot)) {
			if (AddPathNode(pickUpLoc, waitRot))
				Dst = selected;
			else
				PopPathNode();
		}
	}

	// Adjust arm speed depending on distance
	// TODO: Improve this as it looks weird
	const float dist = FVector::Dist(Src->GetActorLocation(), Dst->GetActorLocation());
	const float rate = ArmSpeed / dist;
	Timeline.SetPlayRate(rate);

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

bool ARobotArm::AddPathNode(const FVector& actualTarget, const FRotator &orientation) {
	UPoseableMeshComponent* armMesh = Cast<UPoseableMeshComponent>(RootComponent);

	const FVector actorLocation = GetActorLocation();
	const FName baseName = BoneNames[0];
	const EBoneSpaces::Type localSpace = EBoneSpaces::ComponentSpace;
	const EBoneSpaces::Type worldSpace = EBoneSpaces::WorldSpace;
	const FVector armBasePos = armMesh->GetBoneLocationByName(baseName, worldSpace);

	const float armLength = FVector::Distance(
		armMesh->GetBoneLocationByName(BoneNames[1], localSpace),
		armMesh->GetBoneLocationByName(BoneNames[2], localSpace)
	);

	// Adjuste target to take arm effector offset into account
	FVector offset = armMesh->GetBoneLocation(EffectorBoneName, worldSpace) - armMesh->GetBoneLocation(BoneNames[5], worldSpace);
	FVector target = actualTarget;
	target.Z += abs(offset.Z);
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

	float headOrientation = orientation.Yaw;
	if (PathNodes.Num() > 0) {
		const float lastBaseAngle = PathNodes[PathNodes.Num() - 1][0];
		ShortestAngle(lastBaseAngle, angle);

		const float lastHeadAngle = PathNodes[PathNodes.Num() - 1][5];
		ShortestAngle(lastHeadAngle, headOrientation);
	}
	FRotator baseRot = armMesh->GetBoneRotationByName(baseName, localSpace);
	baseRot.Yaw = angle;
	armMesh->SetBoneRotationByName(baseName, baseRot, localSpace);

	// Compute joints angle
	// Height difference
	const float heightDiff = target.Z - armBasePos.Z;
	float outerAngle = -atan(heightDiff / dist) * (180.f / PI);

	// Inner and outer angles (of trapezoid)
	float innerAngle;
	const float term = (dist - armLength) / (-2 * armLength);
	if (dist <= armLength)
		innerAngle = acos(term);
	else
		innerAngle = asin(-term);
	
	innerAngle = (innerAngle * 180.f / PI) + 90.f;
	outerAngle += 90.f - (180.f - innerAngle);

	// Create node
	const TArray<float> angles({
		angle,
		outerAngle,
		outerAngle + 180 - innerAngle,
		outerAngle + 2 * (180 - innerAngle),
		-90,
		headOrientation
	});
	PathNodes.Add(angles);

	for (size_t i = 1; i < 6; ++i) {
		FRotator rotation = armMesh->GetBoneRotationByName(BoneNames[i], localSpace);
		rotation.SetComponentForAxis(BoneAxes[i], angles[i]);
		rotation.Normalize();
		armMesh->SetBoneRotationByName(BoneNames[i], rotation, localSpace);
	}

	return true;
}

void ARobotArm::HandleProgress(float Value) {
	const size_t stateCount = PathNodes.Num() - 1;
	const float stateTime = 1.f / stateCount;
	const float progress = Value / stateTime;
	const size_t currentState = progress;
	const size_t nextState = currentState + 1;
	if (nextState > stateCount)
		return;

	const float interp = (progress - currentState);

	UPoseableMeshComponent* arm = Cast<UPoseableMeshComponent>(RootComponent);
	for (size_t i = 0; i < BoneNames.Num(); ++i) {
		const float s0 = PathNodes[currentState][i];
		const float s1 = PathNodes[nextState][i];
		const float angle = s0 * (1 - interp) + s1 * interp;

		FRotator rotation = arm->GetBoneRotationByName(BoneNames[i], EBoneSpaces::ComponentSpace);
		rotation.SetComponentForAxis(BoneAxes[i], angle);
		arm->SetBoneRotationByName(BoneNames[i], rotation, EBoneSpaces::ComponentSpace);
	}
}

void ARobotArm::HandleTimelineEnd() {
	const float playbackPos = Timeline.GetPlaybackPosition();
	if (playbackPos == curve->GetFloatValue(0))
		Timeline.PlayFromStart();
	else 
		Timeline.ReverseFromEnd();
}
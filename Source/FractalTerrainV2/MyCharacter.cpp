// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"

#include "Terrain.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/World.h"
#include "CollisionQueryParams.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

#include "Engine/StaticMeshActor.h"


// Sets default values
AMyCharacter::AMyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	m_terrain = nullptr;
	m_hasMoved = false;
	m_actionMode = BREAK_BLOCKS;
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	TArray<AActor*> actors;
	UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), ATerrain::StaticClass(), actors);
	if (actors.Num() != 1) {
		UE_LOG(LogTemp, Error, TEXT("Unable to get terrain object!"));
	}
	m_terrain = static_cast<ATerrain*>(actors[0]);
	if (!m_terrain) {
		UE_LOG(LogTemp, Error, TEXT("NO TERRAIN ATTACHED TO CHARACTER"));
		return;
	}
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (m_hasMoved) {
		const FVector aLoc = GetActorLocation();
		const FIntVector chkLoc = m_terrain->worldToChunkCoords(
			aLoc.X,
			aLoc.Y,
			aLoc.Z);

		for (int i = -1; i <= 1; ++i) {
			for (int j = -1; j <= 1; ++j) {
				for (int k = -1; k <= 1; ++k) {
					const int64 chkIdx = m_terrain->getChunkIndex(
						chkLoc.X + i,
						chkLoc.Y + j,
						chkLoc.Z + k);
					m_terrain->loadChunk(chkIdx);
				}
			}
		}
	}

	if (m_actionMode == SPAWN_OBJECT && m_ghost) {
		APlayerCameraManager* cameraMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
		const FVector forward = cameraMgr->GetCameraRotation().Vector();
		const FVector start = cameraMgr->GetCameraLocation();
		const FVector end = start + (forward * 1000);
		FIntVector blockCoords;
		FVector coords;
		if (m_terrain->Raycast(start, end, blockCoords, coords)) {
			FVector spawnCoord = m_terrain->getBlockSideCoord(blockCoords);
			// For depth fighting purposes
			spawnCoord.Z += 1;
			m_ghost->SetActorRelativeLocation(spawnCoord);
		}
	}
}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up "movement" bindings.
	PlayerInputComponent->BindAxis("MoveForward", this, &AMyCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMyCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Yaw", this, &AMyCharacter::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Pitch", this, &AMyCharacter::AddControllerPitchInput);

	PlayerInputComponent->BindAxis("Scroll", this, &AMyCharacter::Scroll);

	// Setup action bindings
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AMyCharacter::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AMyCharacter::StopJump);

	// Mouse click
	PlayerInputComponent->BindAction("Hit", IE_Pressed, this, &AMyCharacter::Hit);

	// Keyboard (1...9)
	for (int i = 1; i < 5; ++i) {
		FString name = FString::Printf(TEXT("Action_%d"), i);
		FInputActionBinding actionBinding(FName(*name) , IE_Pressed);
		FInputActionHandlerSignature actionHandler;
		actionHandler.BindUFunction(this, FName("SetActionMode"), i);
		actionBinding.ActionDelegate = actionHandler;
		PlayerInputComponent->AddActionBinding(actionBinding);
	}
	//PlayerInputComponent->BindAction("Action_1", IE_Pressed, this, &AMyCharacter::SetActionMode);
}

void AMyCharacter::MoveForward(float Value)
{
	// Find out which way is "forward" and record that the player wants to move that way.
	//FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::X);
	APlayerCameraManager* cameraMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	const FVector Direction = cameraMgr->GetCameraRotation().Vector();
	AddMovementInput(Direction, Value);
	m_hasMoved = true;
}

void AMyCharacter::MoveRight(float Value)
{
	// Find out which way is "right" and record that the player wants to move that way.
	FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::Y);
	AddMovementInput(Direction, Value);
	m_hasMoved = true;
}

void AMyCharacter::Scroll(float value) {
	if (m_actionMode == SPAWN_OBJECT && m_ghost && value != 0) {
		FRotator rot(0, value * -90, 0);
		m_ghost->AddActorLocalRotation(rot);
	}
}

void AMyCharacter::StartJump()
{
	bPressedJump = true;
}

void AMyCharacter::StopJump()
{
	bPressedJump = false;
}

void AMyCharacter::Hit()
{
	APlayerCameraManager* cameraMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	const FVector forward = cameraMgr->GetCameraRotation().Vector();
	const FVector start = cameraMgr->GetCameraLocation();
	const FVector end = start + (forward * 1000);
	FIntVector blockCoords;
	FVector coords;

	switch (m_actionMode) {
	case BREAK_BLOCKS:
		if (m_terrain->Raycast(start, end, blockCoords, coords))
			m_terrain->PopBlock(blockCoords);
		break;
	case SPAWN_OBJECT:
		if (m_ghost) {
			UObject* SpawnActor = Cast<UObject>(
				StaticLoadObject(UObject::StaticClass(), NULL, *m_ghostBlueprintResName));
			UBlueprint* GeneratedBP = Cast<UBlueprint>(SpawnActor);
			FVector loc = m_ghost->GetActorLocation();
			FRotator rot = m_ghost->GetActorRotation();
			UWorld* World = GetWorld();
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			AActor *actor  = World->SpawnActor<AActor>(GeneratedBP->GeneratedClass, loc, rot, SpawnParams);
			if (actor) {
				actor->DisableComponentsSimulatePhysics();
				actor->DispatchBeginPlay();
			}
		}
		break;
	case SELECT_OBJECT:
		struct FHitResult hitResult;
		struct FCollisionQueryParams queryParams;
		queryParams.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(
			hitResult,
			start,
			end,
			ECC_WorldStatic,
			queryParams)) {
			m_selected = hitResult.Actor.Get();
			DispatchEvent();
		}
		break;
	}
}

void AMyCharacter::SetGhost(const FString &resName) {
	UMaterialInterface* material = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/Game/Materials/Ghost.Ghost")));

	UStaticMesh* staticMesh = Cast<UStaticMesh>(
		StaticLoadObject(UObject::StaticClass(), NULL, *resName));

	m_ghost = (AStaticMeshActor*)GetWorld()->SpawnActor(AStaticMeshActor::StaticClass());
	m_ghost->SetMobility(EComponentMobility::Movable);
	m_ghost->SetActorEnableCollision(false);
	UStaticMeshComponent* staticMeshComp = m_ghost->GetStaticMeshComponent();

	staticMeshComp->SetStaticMesh(staticMesh);
	const int numMaterials = staticMeshComp->GetNumMaterials();
	for (size_t i = 0; i < numMaterials; ++i)
		staticMeshComp->SetMaterial(i, material);
}

void AMyCharacter::SetActionMode(int mode) {
	UE_LOG(LogTemp, Warning, TEXT("CHANGE MODE %d"), mode);

	if (m_ghost) {
		m_ghost->Destroy();
		m_ghost = NULL;
	}

	switch (mode) {
	case -1:
		m_selected = NULL;
		m_actionMode = SELECT_OBJECT;
		break;
	case 2:
		SetGhost(TEXT("StaticMesh'/Game/Assets/foreuse_v3.foreuse_v3'"));
		m_ghostBlueprintResName = TEXT("Blueprint'/Game/Actors/Foreuse.Foreuse'");
		m_actionMode = SPAWN_OBJECT;
		break;
	case 3:
		SetGhost(TEXT("StaticMesh'/Game/Assets/Belt/belt_straight.belt_straight'"));
		m_ghostBlueprintResName = TEXT("Blueprint'/Game/Actors/Belt.Belt'");
		m_actionMode = SPAWN_OBJECT;
		break;
	case 4:
		SetGhost(TEXT("StaticMesh'/Game/Assets/Arm/Arm_SM.Arm_SM'"));
		m_ghostBlueprintResName = TEXT("Blueprint'/Game/Actors/RobotArm_BP.RobotArm_BP'");
		m_actionMode = SPAWN_OBJECT;
		break;
	default:
		m_actionMode = BREAK_BLOCKS;
		break;
	}
}

void AMyCharacter::AddSelectListener(CharacterEventListener* listener) {
	m_listeners.Add(listener);
}

void AMyCharacter::RemoveSelectListener(CharacterEventListener* listener) {
	m_listenersToRemove.Add(listener);
}

void AMyCharacter::DispatchEvent() {
	for (auto listener : m_listenersToRemove)
		m_listeners.Remove(listener);

	m_listenersToRemove.Empty();
	for (auto listener : m_listeners)
		listener->OnSelect(m_selected);
}
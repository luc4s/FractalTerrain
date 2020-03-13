// Fill out your copyright notice in the Description page of Project Settings.


#include "Terrain.h"
#include "MyCharacter.h"
#include "MyGameInstance.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "Math/BigInt.h"

#include "DrawDebugHelpers.h"

#include <cmath>

static void AddFace(
	size_t dir,
	const openvdb::Coord::Int32 *coordPtr,
	TArray<FVector> &vertices,
	TArray<int32> &triangles,
	TArray<FVector> &normals) {

	const FVector normalList[6] = {
		FVector(-1, 0, 0),
		FVector(1, 0, 0),
		FVector(0, -1, 0),
		FVector(0,  1, 0),
		FVector(0, 0, -1),
		FVector(0, 0,  1)
	};

	const int32 indices[6][6] = {
		{ 0, 2, 1, 2, 3, 1 },
		{ 1, 3, 2, 1, 2, 0 },
		{ 1, 3, 2, 1, 2, 0 },
		{ 0, 2, 1, 2, 3, 1 },
		{ 0, 2, 1, 2, 3, 1 },
		{ 1, 3, 2, 1, 2, 0 },
	};

	const int32_t x = coordPtr[0];
	const int32_t y = coordPtr[1];
	const int32_t z = coordPtr[2];
	const FVector vertexList[8] = {
		FVector(x, y, z),
		FVector(x, y, z + 1),
		FVector(x, y + 1, z),
		FVector(x, y + 1, z + 1),
		FVector(x + 1, y, z),
		FVector(x + 1, y, z + 1),
		FVector(x + 1, y + 1, z),
		FVector(x + 1, y + 1, z + 1)
	};

	const size_t faceVertices[6][4] = {
		{ 0, 1, 2, 3 },
		{ 4, 5, 6, 7 },
		{ 0, 1, 4, 5 },
		{ 2, 3, 6, 7 },
		{ 0, 2, 4, 6 },
		{ 1, 3, 5, 7 }
	};

	const size_t vIndex = vertices.Num();
	for (size_t i = 0; i < 4; ++i) {
		normals.Add(normalList[dir]);
		vertices.Add(vertexList[faceVertices[dir][i]]);
	}

	// Triangles
	for (size_t i = 0; i < 6; ++i)
		triangles.Add(vIndex + indices[dir][i]);
}

// Sets default values
ATerrain::ATerrain(){
	PrimaryActorTick.bCanEverTick = true;
	VoxelSize = 100;
	ChunkSize = 32;
	DbgChunkLoadRange = 3;
	m_chunkWorldSize = ChunkSize * VoxelSize;

	openvdb::initialize();
	m_grid = openvdb::FloatGrid::create();

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Terrain"));

	const int size = static_cast<int>(ChunkSize);
	m_heightMapBuilder.SetSourceModule(m_groundNoiseModule);
	m_heightMapBuilder.SetDestNoiseMap(m_heightMap);
	m_heightMapBuilder.SetDestSize(1 + size * 2, 1 + size * 2);

	GroundMaterial = nullptr;
	CoalOreMaterial = nullptr;
	HeightFactor = 20.0;
}

bool ATerrain::Raycast(const FVector& start, const FVector& end, FIntVector& blockCoords, FVector &impactCoords) {
	bool found = false;
	struct FHitResult hitResult;
	//TODO: Fix this, as intersection may not be the closest
	for (auto& entry : m_chunks) {
		UProceduralMeshComponent* mesh = entry.Value;
		struct FHitResult tempHitRes;
		if (mesh->LineTraceComponent(tempHitRes, start, end, FCollisionQueryParams::DefaultQueryParam)) {
			if (!found || tempHitRes.Distance < hitResult.Distance) {
				found = true;
				hitResult = tempHitRes;
				continue;
			}
		}
	}
	if (found) {
		impactCoords.X = hitResult.ImpactPoint.X;
		impactCoords.Y = hitResult.ImpactPoint.Y;
		impactCoords.Z = hitResult.ImpactPoint.Z;
		const FVector impact = hitResult.ImpactPoint / VoxelSize;
		blockCoords.X = std::floor(impact.X);
		blockCoords.Y = std::floor(impact.Y);
		blockCoords.Z = std::floor(impact.Z);
		if (hitResult.Normal.X > 0) --blockCoords.X;
		else if (hitResult.Normal.Y > 0) --blockCoords.Y;
		else if (hitResult.Normal.Z > 0) --blockCoords.Z;
	}
	return found;
}

float ATerrain::PopBlock(const FIntVector& coord) {
	openvdb::Coord voxel(coord.X, coord.Y, coord.Z);

	// Draw debug block
	/*
	const FVector center = FVector(voxel.x(), voxel.y(), voxel.z()) * VoxelSize + FVector(VoxelSize / 2.0);
	const FVector extent = FVector(VoxelSize / 2.0) * 1.1;
	FlushPersistentDebugLines(GetWorld());
	DrawDebugBox(GetWorld(), center, extent, FColor(255, 0, 0), false, 3);
	//*/

	openvdb::FloatGrid::Accessor accessor = m_grid->getAccessor();
	const float voxelType = accessor.getValue(voxel);
	accessor.setValue(voxel, 1);

	openvdb::Coord voxels[6] = {
		voxel.offsetBy(0, 0, -1),
		voxel.offsetBy(0, 0,  1),
		voxel.offsetBy(0, -1, 0),
		voxel.offsetBy(0,  1, 0),
		voxel.offsetBy(1, 0, 0),
		voxel.offsetBy(-1, 0, 0),
	};

	const int64 chunkId = getChunkIndex(
		std::floor(coord.X / ChunkSize),
		std::floor(coord.Y / ChunkSize),
		std::floor(coord.Z / ChunkSize));

	if (coord.X % ChunkSize == 0) {
		m_dirtyChunks.Add(getChunkIndex(
			std::floor(coord.X / ChunkSize) - 1,
			std::floor(coord.Y / ChunkSize),
			std::floor(coord.Z / ChunkSize)));
	} else if ((coord.X + 1) % ChunkSize == 0) {
		m_dirtyChunks.Add(getChunkIndex(
			std::floor(coord.X / ChunkSize) + 1,
			std::floor(coord.Y / ChunkSize),
			std::floor(coord.Z / ChunkSize)));
	}
	if (coord.Y % ChunkSize == 0) {
		m_dirtyChunks.Add(getChunkIndex(
			std::floor(coord.X / ChunkSize),
			std::floor(coord.Y / ChunkSize) - 1,
			std::floor(coord.Z / ChunkSize)));
	} else if ((coord.Y + 1) % ChunkSize == 0) {
		m_dirtyChunks.Add(getChunkIndex(
			std::floor(coord.X / ChunkSize),
			std::floor(coord.Y / ChunkSize) + 1,
			std::floor(coord.Z / ChunkSize)));
	}
	if (coord.Z % ChunkSize == 0) {
		m_dirtyChunks.Add(getChunkIndex(
			std::floor(coord.X / ChunkSize),
			std::floor(coord.Y / ChunkSize),
			std::floor(coord.Z / ChunkSize) - 1));
	} else if ((coord.Z + 1) % ChunkSize == 0) {
		m_dirtyChunks.Add(getChunkIndex(
			std::floor(coord.X / ChunkSize),
			std::floor(coord.Y / ChunkSize),
			std::floor(coord.Z / ChunkSize) + 1));
	}

	m_dirtyChunks.Add(chunkId);

	//UE_LOG(LogTemp, Warning, TEXT("POPPED %f"), voxelType);
	return voxelType;
}

int ATerrain::GetBlockType(const FIntVector& coord) {
	openvdb::Coord voxel(coord.X, coord.Y, coord.Z);
	openvdb::FloatGrid::Accessor accessor = m_grid->getAccessor();
	return (int)accessor.getValue(voxel);
}

// Called when the game starts or when spawned
void ATerrain::BeginPlay()
{
	Super::BeginPlay();
	VoxelSize = Cast<UMyGameInstance>(GetGameInstance())->GetWorldUnitSize();
	m_chunkWorldSize = ChunkSize * VoxelSize;

	openvdb::FloatGrid::Ptr grid = m_grid;

	TBigInt<64> idx(getChunkIndex(0, 0, -1));
	const short range = DbgChunkLoadRange;
	const short preloadRange = range + 1;

	// Preload some chunks around starting zone
	for (short i = -preloadRange; i < preloadRange; ++i) {
		for (short j = -preloadRange; j < preloadRange; ++j) {
			for (short k = -preloadRange; k < preloadRange; ++k) {
				const uint64 chkIdx = getChunkIndex(i, j, k);
				preloadChunk(chkIdx);
			}
		}
	}
	for (short i = -range; i < range; ++i) {
		for (short j = -range; j < range; ++j) {
			for (short k = -range; k < range; ++k) {
				const uint64 chkIdx = getChunkIndex(i, j, k);
				loadChunk(chkIdx);
			}
		}
	}

	RootComponent->SetRelativeScale3D(FVector(VoxelSize));

	// Teleport player to ground
	const int size = static_cast<int>(ChunkSize);
	AActor* player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const float playerHeight = player->GetComponentsBoundingBox().GetSize().Z + 10;
	const float spawnHeight = (m_heightMap.GetValue(0, 0) * HeightFactor * VoxelSize) + playerHeight;
	FVector spawnLoc = player->GetActorLocation();
	spawnLoc.SetComponentForAxis(EAxis::Z, spawnHeight);
	player->SetActorLocation(spawnLoc, false, nullptr, ETeleportType::ResetPhysics);
}

void ATerrain::Tick(float delta) {
	for (size_t i = 0; i < m_dirtyChunks.Num(); ++i) {
		loadChunk(m_dirtyChunks[i]);
	}
	m_dirtyChunks.Empty();
}

UProceduralMeshComponent* ATerrain::getChunk(uint64 index) {
	if (!m_chunks.Contains(index)) {
		loadChunk(index);
	}
	return m_chunks[index];
}

int64 ATerrain::getChunkIndex(short x, short y, short z) const {
	TBigInt<64> result(z & 0xFFFF);
	//UE_LOG(LogTemp, Warning, TEXT("====> %s"), *result.ToString());
	result.ShiftLeft(16);
	result.BitwiseOr(TBigInt<64>(y & 0xFFFF));
	result.ShiftLeft(16);
	result.BitwiseOr(TBigInt<64>(x & 0xFFFF));
	return result.ToInt();
}

void ATerrain::preloadChunk(int64 index) {
	// Check not preloaded already
	if (m_chunks.Contains(index)) return;

	// Check content exists
	const FIntVector chunkCoords = getChunkCoords(index);
	const FIntVector chunkVoxel = chunkCoords * ChunkSize;
	openvdb::FloatGrid::Accessor accessor = m_grid->getAccessor();
	const openvdb::Coord chunkSample(chunkVoxel.X, chunkVoxel.Y, chunkVoxel.Z);

	const float value = accessor.getValue(chunkSample);
	if (value == 0) {
		const FVector center = VoxelSize * (FVector(chunkVoxel.X, chunkVoxel.Y, chunkVoxel.Z) + FVector(ChunkSize / 2.0));
		const FVector extent = VoxelSize * FVector(ChunkSize / 2.0) * 0.9;

		// Compute fractal (generate noise)
		const int size = static_cast<int>(ChunkSize);

		const int Ox = chunkVoxel.X;
		const int Oy = chunkVoxel.Y;
		const int Oz = chunkVoxel.Z;
		const int chkWidth = Ox + size - 1;
		const int chkDepth = Oy + size - 1;
		const int chkHeight = Oz + size - 1;
		// First fill with air
		const openvdb::CoordBBox chunk(Ox, Oy, Oz, chkWidth, chkDepth, chkHeight);
		m_grid->fill(chunk, 1.0, false);

		for (int i = Ox; i <= chkWidth; ++i) {
			for (int j = Oy; j <= chkDepth; ++j) {
				const int height = m_groundNoiseModule
					.GetValue(
						i / 128.0,
						j / 128.0,
						0) * HeightFactor;

				const openvdb::Coord lowest(i, j, Oz);
				const openvdb::Coord level(i, j, height >= chkHeight ? chkHeight : height);

				// Fill voxel grid
				const openvdb::CoordBBox gndBbox(lowest, level);
				m_grid->fill(gndBbox, 2.0, true);

				for (int k = -Oz; k <= height; ++k) {
					if (m_oreNoiseModule.GetValue(i / 32.0, j / 32.0, k / 32.0) > 0.5) {
						const openvdb::Coord crd(i, j, k);
						accessor.setValue(crd, 3.0);
					}
				}
			}
		}
		// Optimize grid sparseness
		m_grid->pruneGrid();
	}
	else
		UE_LOG(LogTemp, Warning, TEXT("Chunk at %s already generated"), *chunkCoords.ToString());


	// Create mesh
	UProceduralMeshComponent* mesh = NewObject<UProceduralMeshComponent>(this);
	mesh->RegisterComponent();
	if (mesh == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("Couldn't create mesh!"));
		return;
	}

	const FVector chunkLocation = FVector(chunkVoxel.X, chunkVoxel.Y, chunkVoxel.Z) / 2;

	mesh->bUseComplexAsSimpleCollision = true;
	mesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	m_chunks.Add(index, mesh);
}

void ATerrain::loadChunk(int64 index) {
	if (!m_chunks.Contains(index)) {
		preloadChunk(index);
	}
	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector> normals;

	TArray<FVector2D> uv;
	TArray<FLinearColor> colors;
	TArray<FProcMeshTangent> tangents;

	const FIntVector chunkVoxel = getChunkCoords(index) * ChunkSize;
	const int Ox = chunkVoxel.X;
	const int Oy = chunkVoxel.Y;
	const int Oz = chunkVoxel.Z;

	const int size = static_cast<int>(ChunkSize);
	const openvdb::CoordBBox chunk(Ox, Oy, Oz, Ox + size - 1, Oy + size - 1, Oz + size - 1);

	UProceduralMeshComponent* mesh = m_chunks[index];

	processChunk(chunk, vertices, triangles, normals, 2);
	mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv, colors, tangents, true);

	processChunk(chunk, vertices, triangles, normals, 3);
	mesh->CreateMeshSection_LinearColor(1, vertices, triangles, normals, uv, colors, tangents, true);
	
	mesh->SetMaterial(0, GroundMaterial);
	mesh->SetMaterial(1, CoalOreMaterial);
}

void ATerrain::processChunk(const openvdb::CoordBBox& bbox, TArray<FVector>& vertices, TArray<int32>& triangles, TArray<FVector>& normals, int voxelType) {
	openvdb::FloatGrid::Accessor accessor = m_grid->getAccessor();

	for (openvdb::CoordBBox::ZYXIterator it = bbox.beginZYX(); it; ++it) {
		const openvdb::Coord coord = *it;
		const float value = accessor.getValue(coord);

		if (value != voxelType)
			continue;

		const bool nx = 1 == accessor.getValue(coord.offsetBy(-1, 0, 0));
		const bool px = 1 == accessor.getValue(coord.offsetBy(1, 0, 0));

		const bool ny = 1 == accessor.getValue(coord.offsetBy(0, -1, 0));
		const bool py = 1 == accessor.getValue(coord.offsetBy(0, 1, 0));

		const bool nz = 1 == accessor.getValue(coord.offsetBy(0, 0, -1));
		const bool pz = 1 == accessor.getValue(coord.offsetBy(0, 0, 1));

		if (nx || px || ny || py || nz || pz) {
			// Create faces
			if (nx) AddFace(0, coord.asPointer(), vertices, triangles, normals);
			if (px) AddFace(1, coord.asPointer(), vertices, triangles, normals);

			if (ny) AddFace(2, coord.asPointer(), vertices, triangles, normals);
			if (py) AddFace(3, coord.asPointer(), vertices, triangles, normals);

			if (nz) AddFace(4, coord.asPointer(), vertices, triangles, normals);
			if (pz) AddFace(5, coord.asPointer(), vertices, triangles, normals);
		}
	}
}

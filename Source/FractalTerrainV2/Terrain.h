// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#pragma warning ( push )
#pragma warning ( disable: 4668 )
#pragma warning ( disable: 4211 )
#pragma warning ( disable: 4146 )

#include <openvdb/openvdb.h>

#include <noise/noise.h>
#include <noise/noiseutils.h>

#pragma warning ( pop )


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Terrain.generated.h"

class UProceduralMeshComponent;
class UMaterial;


UCLASS()
class FRACTALTERRAINV2_API ATerrain : public AActor
{
	GENERATED_BODY()
	
public:	
	enum Side {
		TOP,
		BOTTOM,
		LEFT,
		RIGHT,
		FRONT,
		BACK
	};
	// Sets default values for this actor's properties
	ATerrain();

	/*
		Performs collision testing on the terrain
	*/
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool Raycast(const FVector& start, const FVector& end, FIntVector& blockCoords, FVector &coords);

	/*
		Pops a block from the terrain and remesh
	*/
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	float PopBlock(const FIntVector &coords);

	/*
		Returns the block type at given coordinates
	*/
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	int GetBlockType(const FIntVector& coord);

	/*
		Returns chunk index at given chunk coordinates
	*/
	int64 getChunkIndex(short x, short y, short z) const;

	/*
		Get back chunk coordinates from index
	*/
	FIntVector getChunkCoords(int64 index) const {
		return FIntVector(
			(short)(index & 0xFFFF),
			(short)((index >> 16) & 0xFFFF),
			(short)((index >> 32)));
	};

	/*
		Given world coordinates, translates them to chunk coordinates.
	*/
	FIntVector worldToChunkCoords(float x, float y, float z) const {
		return FIntVector(
			std::floor(x / m_chunkWorldSize),
			std::floor(y / m_chunkWorldSize),
			std::floor(z / m_chunkWorldSize));
	};

	void preloadChunk(int64 index);
	void loadChunk(int64 index);

	/*
		Returns the world coordinate in the middle of the block given face.
	*/
	FVector getBlockSideCoord(FIntVector block, Side side = TOP) const {
		const float halfSize = VoxelSize / 2.0;
		FVector coord(block * VoxelSize);
		coord += FVector(halfSize);
		switch (side) {
		case TOP:
			coord.Z += halfSize;
			break;
		default:
			UE_LOG(LogTemp, Error, TEXT("Side coord not implemented!"));
		}
		return coord;
	}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UProceduralMeshComponent *getChunk(uint64 index);

	void processChunk(
		const openvdb::CoordBBox& bbox, 
		TArray<FVector>& vertices,
		TArray<int32>& triangles,
		TArray<FVector>& normals,
		int voxelType);


	//UProceduralMeshComponent *m_mesh;
	openvdb::FloatGrid::Ptr m_grid;

	TArray<int64> m_dirtyChunks;
	TMap<int64, UProceduralMeshComponent*> m_chunks;


	noise::module::Perlin m_groundNoiseModule;
	noise::module::Perlin m_oreNoiseModule;
	noise::utils::NoiseMap m_heightMap;
	noise::utils::NoiseMapBuilderPlane m_heightMapBuilder;

	float m_chunkWorldSize;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float VoxelSize;

	UPROPERTY(EditAnywhere)
	uint32 ChunkSize;

	UPROPERTY(EditAnywhere)
	uint32 DbgChunkLoadRange;

	UPROPERTY(EditAnywhere)
	float HeightFactor;

	UPROPERTY(EditAnywhere)
	UMaterialInterface *GroundMaterial;

	UPROPERTY(EditAnywhere)
	UMaterialInterface *CoalOreMaterial;
};

#pragma once
#include"raylib.h"
#include<cstdint>
#include"raymath.h"
#include"BlockTypes.h"


class World;

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 64;
const int CHUNK_DEPTH = 16;

struct ChunkNeighborData
{
	uint8_t left[CHUNK_HEIGHT][CHUNK_DEPTH]; // x - 1 neighbor
	uint8_t right[CHUNK_HEIGHT][CHUNK_DEPTH];
	uint8_t back[CHUNK_WIDTH][CHUNK_HEIGHT];
	uint8_t front[CHUNK_WIDTH][CHUNK_HEIGHT]; // z + 1 neighbor
	bool hasLeft, hasRight, hasBack, hasFront;
};

class Chunk
{
public:
	// small int (0-255); enough to rep diff block types and small enough
	// that chunk doesnt eat much memory. 0 = air, 1 = solid
	uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];

	Mesh mesh;
	Mesh waterMesh;
	bool meshReady;
	bool meshDirty;

	Chunk();
	void Fill(int worldX, int worldZ);
	void BuildMesh();
	void BuildMeshData(const ChunkNeighborData& neighbors);	// safe to call on background thread
	void UploadMeshData();	// must be called on main thread
	void Draw(Vector3 position, Material& mat);
	void DrawWater(Vector3 position, Material& mat);

private:
	bool IsAir(int x, int y, int z, const ChunkNeighborData& neighbors);
	float VertexAO(bool side1, bool side2, bool corner); // ambient occlusion
};
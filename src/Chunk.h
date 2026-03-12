#pragma once
#include"raylib.h"
#include<cstdint>
#include"raymath.h"


const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 16;
const int CHUNK_DEPTH = 16;

class Chunk
{
public:
	// small int (0-255); enough to rep diff block types and small enough
	// that chunk doesnt eat much memory. 0 = air, 1 = solid
	uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];

	Mesh mesh;
	bool meshDirty;

	Chunk();
	void Fill(int worldX, int worldZ);
	void BuildMesh();
	void Draw(Vector3 position);

private:
	bool IsAir(int x, int y, int z);

};
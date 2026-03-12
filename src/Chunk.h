#pragma once
#include"raylib.h"
#include<cstdint>

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 16;
const int CHUNK_DEPTH = 16;

class Chunk
{
public:
	// small int (0-255); enough to rep diff block types and small enough
	// that chunk doesnt eat much memory. 0 = air, 1 = solid
	uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];

	Chunk();
	void Fill();
	void Draw();

private:


};
#pragma once
#include"Chunk.h"
#include<stdint.h>

const int WORLD_WIDTH = 4; // x
const int WORLD_DEPTH = 4; // z

class World
{
public:
	Chunk chunks[WORLD_WIDTH][WORLD_DEPTH];

	World();
	void Draw();
	uint8_t GetBlock(int x, int y, int z);

private:


};
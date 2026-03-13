#include"World.h"

World::World()
{
	for (int x = 0; x < WORLD_WIDTH; x++)
	{
		for (int z = 0; z < WORLD_DEPTH; z++)
		{
			chunks[x][z].Fill(x, z);
		}
	}
}

void World::Draw()
{
	for (int x = 0; x < WORLD_WIDTH; x++)
	{
		for (int z = 0; z < WORLD_DEPTH; z++)
		{
			Vector3 pos =
			{
				(float)(x * CHUNK_WIDTH),
				0.f,
				(float)(z * CHUNK_DEPTH)
			};
			chunks[x][z].Draw(pos);
		}
	}
}

uint8_t World::GetBlock(int x, int y, int z)
{
	// figure out which chunk the coord is in
	int chunkX = x / CHUNK_WIDTH;
	int chunkZ = z / CHUNK_DEPTH;

	// out of world bounds
	if (chunkX < 0 || chunkX >= WORLD_WIDTH) { return BLOCK_STONE; }
	if (chunkZ < 0 || chunkZ >= WORLD_DEPTH) { return BLOCK_STONE; }
	if (y < 0)								 { return BLOCK_STONE; }
	if (y >= CHUNK_HEIGHT)					 { return BLOCK_AIR; }

	// local coords in chunk
	int lx = x % CHUNK_WIDTH;
	int lz = z % CHUNK_DEPTH;

	return chunks[chunkX][chunkZ].blocks[lx][y][lz];
}
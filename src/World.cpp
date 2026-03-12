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
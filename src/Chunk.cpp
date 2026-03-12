#include"Chunk.h"
#include"external/stb_perlin.h"

Chunk::Chunk()
{
	// initialize all blocks to air
	for (int x = 0; x < CHUNK_WIDTH; x++)
	{
		for (int y = 0; y < CHUNK_HEIGHT; y++)
		{
			for (int z = 0; z < CHUNK_DEPTH; z++)
			{
				blocks[x][y][z] = 0;
			}
		}
	}
}

void Chunk::Fill()
{
	for (int x = 0; x < CHUNK_WIDTH; x++)
	{
		for (int z = 0; z < CHUNK_DEPTH; z++)
		{
			float noise = stb_perlin_noise3(x * 0.1f, 0, z * 0.1f, 0, 0, 0);
			int height = (int)((noise + 1.f) * 0.5f * 8) + 4;

			for (int y = 0; y < CHUNK_HEIGHT; y++)
			{
				blocks[x][y][z] = (y <= height) ? 1 : 0;
			}
		}
	}
}

void Chunk::Draw()
{
	for (int x = 0; x < CHUNK_WIDTH; x++)
	{
		for (int y = 0; y < CHUNK_HEIGHT; y++)
		{
			for (int z = 0; z < CHUNK_DEPTH; z++)
			{
				if (blocks[x][y][z] == 0) { continue; }

				DrawCube({ x + 0.5f, y + 0.5f, z + 0.5f }, 1.f, 1.f, 1.f, GREEN);
				DrawCubeWires({ x + 0.5f, y + 0.5f, z + 0.5f }, 1.f, 1.f, 1.f, DARKGREEN);
			}
		}
	}
}
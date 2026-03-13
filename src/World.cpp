#include"World.h"
#include<cmath>


World::World()
{
	
}


void World::LoadChunk(int cx, int cz)
{
	ChunkCoord coord = { cx, cz };
	if (chunks.find(coord) != chunks.end()) { return; } // already loaded

	auto chunk = std::make_unique<Chunk>();
	chunk->Fill(cx, cz);
	chunks[coord] = std::move(chunk);
}

void World::UnloadChunk(int cx, int cz)
{
	chunks.erase({ cx, cz });
}

void World::Update(Vector3 playerPos)
{
	int playerChunkX = (int)floor(playerPos.x / CHUNK_WIDTH);
	int playerChunkZ = (int)floor(playerPos.z / CHUNK_DEPTH);

	// load chunks within view distance
	for (int x = playerChunkX - VIEW_DISTANCE; x <= playerChunkX + VIEW_DISTANCE; x++)
		for (int z = playerChunkZ - VIEW_DISTANCE; z <= playerChunkZ + VIEW_DISTANCE; z++)
			LoadChunk(x, z);

	// unload chunks outside view distance
	std::vector<ChunkCoord> toUnload;
	for (auto& pair : chunks)
	{
		int dx = abs(pair.first.x - playerChunkX);
		int dz = abs(pair.first.z - playerChunkZ);
		if (dx > VIEW_DISTANCE || dz > VIEW_DISTANCE)
			toUnload.push_back(pair.first);
	}
	for (auto& coord : toUnload)
		chunks.erase(coord);
}

void World::Draw()
{
	for (auto& pair : chunks)
	{
		pair.second->Draw({
			(float)(pair.first.x * CHUNK_WIDTH),
			0.0f,
			(float)(pair.first.z * CHUNK_DEPTH)
			});
	}
}

uint8_t World::GetBlock(int x, int y, int z)
{
	// figure out which chunk the coord is in
	int chunkX = (int)floor((float)x / CHUNK_WIDTH);
	int chunkZ = (int)floor((float)z / CHUNK_DEPTH);

	auto it = chunks.find({ chunkX, chunkZ });
	if (it == chunks.end()) { return BLOCK_STONE; }
	if (y < 0)				{ return BLOCK_STONE; }
	if (y >= CHUNK_HEIGHT)	{ return BLOCK_AIR; }

	// local coords in chunk
	int lx = x - chunkX * CHUNK_WIDTH;
	int lz = z - chunkZ * CHUNK_DEPTH;

	return it->second->blocks[lx][y][lz];
}
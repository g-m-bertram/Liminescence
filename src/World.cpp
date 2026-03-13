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

RaycastResult World::Raycast(Vector3 origin, Vector3 direction, float maxDistance)
{
	RaycastResult result = {};
	result.hit = false;

	direction = Vector3Normalize(direction);

	float stepSize = 0.05f;
	Vector3 pos = origin;

	int lastAirX = 0, lastAirY = 0, lastAirZ = 0;

	for (float dist = 0; dist < maxDistance; dist += stepSize)
	{
		pos = Vector3Add(origin, Vector3Scale(direction, dist));

		int bx = (int)floor(pos.x);
		int by = (int)floor(pos.y);
		int bz = (int)floor(pos.z);

		uint8_t block = GetBlock(bx, by, bz);

		if (block != BLOCK_AIR)
		{
			result.hit = true;
			result.x = bx;
			result.y = by;
			result.z = bz;
			result.nx = lastAirX;
			result.ny = lastAirY;
			result.nz = lastAirZ;
			return result;
		}

		lastAirX = bx;
		lastAirY = by;
		lastAirZ = bz;
	}

	return result;
}

void World::SetBlock(int x, int y, int z, uint8_t block)
{
	int chunkX = (int)floor((float)x / CHUNK_WIDTH);
	int chunkZ = (int)floor((float)z / CHUNK_DEPTH);

	auto it = chunks.find({ chunkX, chunkZ });
	if (it == chunks.end()) { return; }
	if (y < 0 || y >= CHUNK_HEIGHT) { return; }

	int lx = x - chunkX * CHUNK_WIDTH;
	int lz = z - chunkZ * CHUNK_DEPTH;

	it->second->blocks[lx][y][lz] = block;
	it->second->meshDirty = true;
}
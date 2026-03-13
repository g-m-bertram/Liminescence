#include"World.h"
#include<cmath>


World::World()
{
	running = true;
	genThread = std::thread(&World::ChunkGenThread, this);
}

World::~World()
{
	running = false;
	if (genThread.joinable())
		genThread.join();
}

void World::ChunkGenThread()
{
	while (running)
	{
		ChunkCoord coord;
		bool hasWork = false;

		{
			std::lock_guard<std::mutex> lock(loadQueueMutex);
			if (!loadQueue.empty())
			{
				coord = loadQueue.front();
				loadQueue.pop();
				hasWork = true;
			}
		}

		if (!hasWork)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		// check if already loaded
		{
			std::lock_guard<std::mutex> lock(chunksMutex);
			if (chunks.find(coord) != chunks.end()) { continue; }
		}

		// generate chunk data and build mesh data on background thread
		auto chunk = std::make_unique<Chunk>();
		chunk->Fill(coord.x, coord.z);
		chunk->BuildMeshData();

		{
			std::lock_guard<std::mutex> lock(readyQueueMutex);
			readyQueue.push({coord, std::move(chunk)});
		}
	}
}

void World::LoadChunk(int cx, int cz)
{
	ChunkCoord coord = { cx, cz };
	
	{
		std::lock_guard<std::mutex> lock(chunksMutex);
		if (chunks.find(coord) != chunks.end()) { return; }
	}

	std::lock_guard<std::mutex> lock(loadQueueMutex);
	loadQueue.push(coord);
}

void World::UnloadChunk(int cx, int cz)
{
	std::lock_guard<std::mutex> lock(chunksMutex);
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

	// upload ready chunks on main thread
	{
		std::lock_guard<std::mutex> lock(readyQueueMutex);
		while (!readyQueue.empty())
		{
			auto& [coord, chunk] = readyQueue.front();

			// re-upload mesh on main thread
			chunk->UploadMeshData();

			std::lock_guard<std::mutex> chunksLock(chunksMutex);
			chunks[coord] = std::move(chunk);
			readyQueue.pop();
		}
	}

	// unload chunks outside view distance
	std::vector<ChunkCoord> toUnload;
	{
		std::lock_guard<std::mutex> lock(chunksMutex);
		for (auto& pair : chunks)
		{
			int dx = abs(pair.first.x - playerChunkX);
			int dz = abs(pair.first.z - playerChunkZ);
			if (dx > VIEW_DISTANCE || dz > VIEW_DISTANCE)
				toUnload.push_back(pair.first);
		}
	}

	for (auto& coord : toUnload)
		UnloadChunk(coord.x, coord.z);
}

void World::Draw()
{
	for (auto& pair : chunks)
	{
		std::lock_guard<std::mutex> lock(chunksMutex);
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

	std::lock_guard<std::mutex> lock(chunksMutex);
	auto it = chunks.find({ chunkX, chunkZ });
	if (it == chunks.end()) { return; }
	if (y < 0 || y >= CHUNK_HEIGHT) { return; }

	int lx = x - chunkX * CHUNK_WIDTH;
	int lz = z - chunkZ * CHUNK_DEPTH;

	it->second->blocks[lx][y][lz] = block;
	it->second->BuildMesh();
}
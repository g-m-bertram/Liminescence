#include"World.h"
#include<cmath>


World::World() : loadQueue([this](const ChunkCoord& a, const ChunkCoord& b)
	{
		int ax = a.x * CHUNK_WIDTH - (int)playerPos.x;
		int az = a.z * CHUNK_DEPTH - (int)playerPos.z;
		int bx = b.x * CHUNK_WIDTH - (int)playerPos.x;
		int bz = b.z * CHUNK_DEPTH - (int)playerPos.z;
		return (ax * ax + az * az) > (bx * bx + bz * bz);
	})
{
	playerPos = { 0,0,0 };
	running = true;
	chunkMaterial = LoadMaterialDefault();
	// shader initialization
	fogShader = LoadShader("assets/shaders/chunk.vs", "assets/shaders/chunk.fs");
	int fogColorLoc =	GetShaderLocation(fogShader, "fogColor");
	int fogStartLoc =	GetShaderLocation(fogShader, "fogStart");
	int fogEndLoc =		GetShaderLocation(fogShader, "fogEnd");
	float fogColor[4] = { 0.5f, 0.7f, 1.f, 1.f }; // skyblue
	float fogStart =	(VIEW_DISTANCE - 1) * CHUNK_WIDTH * 0.8f;
	float fogEnd =		VIEW_DISTANCE * CHUNK_WIDTH * 0.9f;
	SetShaderValue(fogShader, fogColorLoc,	fogColor,	SHADER_UNIFORM_VEC4);
	SetShaderValue(fogShader, fogStartLoc,	&fogStart,	SHADER_UNIFORM_FLOAT);
	SetShaderValue(fogShader, fogEndLoc,	&fogEnd,	SHADER_UNIFORM_FLOAT);
	chunkMaterial.shader = fogShader;

	genThread = std::thread(&World::ChunkGenThread, this);
}

World::~World()
{
	// stop thread first
	running = false;
	if (genThread.joinable())
		genThread.join();

	// clear queues
	{
		std::lock_guard<std::mutex> lock(readyQueueMutex);
		while (!readyQueue.empty()) { readyQueue.pop(); }
	}

	// unload gpu resources
	UnloadMaterial(chunkMaterial);

	// clear chunks
	{
		std::lock_guard<std::mutex> lock(chunksMutex);
		chunks.clear();
	}
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
				coord = loadQueue.top();
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
		// no locks needed as this chunk isn't in the map yet
		auto chunk = std::make_unique<Chunk>();
		chunk->Fill(coord.x, coord.z);

		// take neighbor snapshot while holding the lock
		ChunkNeighborData neighbors;
		{
			std::lock_guard<std::mutex> lock(chunksMutex);
			neighbors = GetNeighborData(coord.x, coord.z);
		}
		//chunk->BuildMeshData(this, coord.x, coord.z);		<-- old; before GetNeighborData()
		// build mesh data with snapshot; no locks needed
		chunk->BuildMeshData(neighbors);

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
	this->playerPos = playerPos;

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
			{
				std::lock_guard<std::mutex> chunksLock(chunksMutex);
				chunks[coord] = std::move(chunk);

				// rebuild neighbors so their edges reflect the new chunk
				auto rebuildNeighbor = [&](int nx, int nz)
				{
					auto n = chunks.find({ nx, nz });
					if (n != chunks.end())
					{
						ChunkNeighborData nNeighbors = GetNeighborData(nx, nz);
						n->second->BuildMeshData(nNeighbors);
						n->second->UploadMeshData();
					}
				};

				rebuildNeighbor(coord.x - 1, coord.z);
				rebuildNeighbor(coord.x + 1, coord.z);
				rebuildNeighbor(coord.x, coord.z - 1);
				rebuildNeighbor(coord.x, coord.z + 1);
			}

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
	int camPosLoc = GetShaderLocation(fogShader, "cameraPosition");
	SetShaderValue(fogShader, camPosLoc, &playerPos, SHADER_UNIFORM_VEC3);

	std::lock_guard<std::mutex> lock(chunksMutex);
	for (auto& pair : chunks)
	{
		pair.second->Draw({
			(float)(pair.first.x * CHUNK_WIDTH),
			0.0f,
			(float)(pair.first.z * CHUNK_DEPTH)
			},
			chunkMaterial);
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

	{
		std::lock_guard<std::mutex> lock(chunksMutex);
		auto it = chunks.find({ chunkX, chunkZ });
		if (it == chunks.end()) { return; }
		if (y < 0 || y >= CHUNK_HEIGHT) { return; }

		int lx = x - chunkX * CHUNK_WIDTH;
		int lz = z - chunkZ * CHUNK_DEPTH;

		it->second->blocks[lx][y][lz] = block;

		// rebuild affected chunk
		ChunkNeighborData neighbors = GetNeighborData(chunkX, chunkZ);
		it->second->BuildMeshData(neighbors);
		it->second->UploadMeshData();

		// rebuild neighbors if on edge of chunk
		auto rebuildNeighbor = [&](int nx, int nz)
		{
			auto n = chunks.find({ nx, nz });
			if (n != chunks.end())
			{
				ChunkNeighborData nNeighbors = GetNeighborData(nx, nz);
				n->second->BuildMeshData(nNeighbors);
				n->second->UploadMeshData();
			}
		};

		if (lx == 0) { rebuildNeighbor(chunkX - 1, chunkZ); }
		if (lx == CHUNK_WIDTH - 1) { rebuildNeighbor(chunkX + 1, chunkZ); }
		if (lz == 0) { rebuildNeighbor(chunkX, chunkZ - 1); }
		if (lz == CHUNK_DEPTH - 1) { rebuildNeighbor(chunkX, chunkZ + 1); }
	}
}

ChunkNeighborData World::GetNeighborData(int chunkX, int chunkZ)
{
	ChunkNeighborData neighbors = {};

	auto copyEdge = [&](int cx, int cz, auto copyFunc)
	{
			auto it = chunks.find({cx, cz});
			if (it == chunks.end()) { return false; }
			copyFunc(it->second.get());
			return true;
	};

	// left neighbor (x - 1), copy its right edge (x = CHUNK_WIDTH - 1)
	neighbors.hasLeft = copyEdge(chunkX - 1, chunkZ, [&](Chunk* c)
		{
			for (int y = 0; y < CHUNK_HEIGHT; y++)
			{
				for (int z = 0; z < CHUNK_DEPTH; z++)
				{
					neighbors.left[y][z] = c->blocks[CHUNK_WIDTH - 1][y][z];
				}
			}
		});

	// right neighbor (x + 1), copy its left edge (x = 0)
	neighbors.hasRight = copyEdge(chunkX + 1, chunkZ, [&](Chunk* c)
		{
			for (int y = 0; y < CHUNK_HEIGHT; y++)
			{
				for (int z = 0; z < CHUNK_DEPTH; z++)
				{
					neighbors.right[y][z] = c->blocks[0][y][z];
				}
			}
		});

	// back neighbor (z - 1), copy its front edge (z = CHUNK_DEPTH - 1)
	neighbors.hasBack = copyEdge(chunkX, chunkZ - 1, [&](Chunk* c)
		{
			for (int x = 0; x < CHUNK_WIDTH; x++)
			{
				for (int y = 0; y < CHUNK_HEIGHT; y++)
				{
					neighbors.back[x][y] = c->blocks[x][y][CHUNK_DEPTH - 1];
				}
			}
		});

	// front neighbor (z + 1), copy its back edge (z = 0)
	neighbors.hasFront = copyEdge(chunkX, chunkZ + 1, [&](Chunk* c)
		{
			for (int x = 0; x < CHUNK_WIDTH; x++)
			{
				for (int y = 0; y < CHUNK_HEIGHT; y++)
				{
					neighbors.front[x][y] = c->blocks[x][y][0];
				}
			}
		});

	return neighbors;
}
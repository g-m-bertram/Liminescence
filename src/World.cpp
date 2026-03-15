#include"World.h"
#include<cmath>
#include"rlgl.h"
#include<algorithm>

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
	
	// chunk shader initialization
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
	chunkMaterial = LoadMaterialDefault();
	chunkMaterial.shader = fogShader;

	// water shader initialization
	waterShader = LoadShader("assets/shaders/water.vs", "assets/shaders/water.fs");
	int wFogColorLoc =	GetShaderLocation(waterShader, "fogColor");
	int wFogStartLoc =	GetShaderLocation(waterShader, "fogStart");
	int wFogEndLoc =	GetShaderLocation(waterShader, "fogEnd");
	SetShaderValue(waterShader, wFogColorLoc,	fogColor,	SHADER_UNIFORM_VEC4);
	SetShaderValue(waterShader, wFogStartLoc,	&fogStart,	SHADER_UNIFORM_FLOAT);
	SetShaderValue(waterShader, wFogEndLoc,		&fogEnd,	SHADER_UNIFORM_FLOAT);
	waterMaterial = LoadMaterialDefault();
	waterMaterial.shader = waterShader;


	int threadCount = 1;
	//int threadCount = std::thread::hardware_concurrency();
	//threadCount = std::max(1, (int)threadCount - 1); // leave one core for main thread
	for (int i = 0; i < threadCount; i++)
		genThreads.emplace_back(&World::ChunkGenThread, this);
}

World::~World()
{
	// stop threads first
	running = false;
	for (auto& t : genThreads)
		if (t.joinable())
			t.join();

	// clear queues
	{
		std::lock_guard<std::mutex> lock(readyQueueMutex);
		while (!readyQueue.empty()) { readyQueue.pop(); }
	}

	// unload gpu resources
	UnloadMaterial(chunkMaterial);
	UnloadMaterial(waterMaterial);

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
		int uploadsThisFrame = 0;
		while (!readyQueue.empty() && uploadsThisFrame < 2)
		{
			auto& [coord, chunk] = readyQueue.front();

			chunk->UploadMeshData();
			// re-upload mesh on main thread
			{
				std::lock_guard<std::mutex> chunksLock(chunksMutex);
				chunks[coord] = std::move(chunk);

				// rebuild neighbors so their edges reflect the new chunk
				/*auto rebuildNeighbor = [&](int nx, int nz)
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
				rebuildNeighbor(coord.x, coord.z + 1);*/
			}
			uploadsThisFrame++;
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

void World::Draw(Camera3D& camera)
{
	int camPosLoc = GetShaderLocation(fogShader, "cameraPosition");
	SetShaderValue(fogShader, camPosLoc, &playerPos, SHADER_UNIFORM_VEC3);

	int wCamPosLoc = GetShaderLocation(waterShader, "cameraPosition");
	SetShaderValue(waterShader, wCamPosLoc, &playerPos, SHADER_UNIFORM_VEC3);

	std::vector<std::pair<float, ChunkCoord>> waterChunks;

	{
		std::lock_guard<std::mutex> lock(chunksMutex);
		
		// draw solid chunks first
		for (auto& pair : chunks)
		{
			if (!IsChunkVisible(pair.first.x, pair.first.z, camera)) { continue; }
			pair.second->Draw({
				(float)(pair.first.x * CHUNK_WIDTH),
				0.0f,
				(float)(pair.first.z * CHUNK_DEPTH)
				},
				chunkMaterial);
		}

		// collect water chunks while holding lock
		for (auto& pair : chunks)
		{
			if (!IsChunkVisible(pair.first.x, pair.first.z, camera)) { continue; }
			float dx = pair.first.x * CHUNK_WIDTH - playerPos.x;
			float dz = pair.first.z * CHUNK_DEPTH - playerPos.z;
			waterChunks.push_back({ dx * dx + dz * dz, pair.first });
		}
	}

	// sort and draw water outside lock
	std::sort(waterChunks.begin(), waterChunks.end(),
		[](auto& a, auto& b) {return a.first > b.first; });

	// draw water with blending
	BeginBlendMode(BLEND_ALPHA);
	rlDisableDepthMask(); // prevents water from blocking solid geometry behind it

	{
		std::lock_guard<std::mutex> lock(chunksMutex);
		for (auto& [dist, coord] : waterChunks)
		{
			auto it = chunks.find(coord);
			if (it == chunks.end()) { continue; }
			it->second->DrawWater(
				{ (float)(coord.x * CHUNK_WIDTH),
				0.0f,
				(float)(coord.z * CHUNK_DEPTH)},
				waterMaterial);
		}
	}

	rlEnableDepthMask();
	EndBlendMode();
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

		if (lx == 0)				{ rebuildNeighbor(chunkX - 1, chunkZ); }
		if (lx == CHUNK_WIDTH - 1)	{ rebuildNeighbor(chunkX + 1, chunkZ); }
		if (lz == 0)				{ rebuildNeighbor(chunkX, chunkZ - 1); }
		if (lz == CHUNK_DEPTH - 1)	{ rebuildNeighbor(chunkX, chunkZ + 1); }
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


bool World::IsChunkVisible(int chunkX, int chunkZ, Camera3D& camera)
{
	// get chunk center in worldspace
	float centerX = chunkX * CHUNK_WIDTH + CHUNK_WIDTH * 0.5f;
	float centerY = CHUNK_HEIGHT * 0.5f;
	float centerZ = chunkZ * CHUNK_DEPTH + CHUNK_DEPTH * 0.5f;

	// radius that encompasses the whole chunk
	float radius = Vector3Length({ (float)CHUNK_WIDTH, (float)CHUNK_HEIGHT, (float)CHUNK_DEPTH }) * 0.5f;
	
	// use raylib's sphere frustum check
	Matrix matProj = MatrixPerspective(camera.fovy,
					(float)GetScreenWidth() / GetScreenHeight(),
					0.01f, 1000.f);
	Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
	Matrix matVP = MatrixMultiply(matView, matProj);

	// check all 6 frustum planes
	Vector4 planes[6];
	// left
	planes[0] = { matVP.m3 + matVP.m0, matVP.m7 + matVP.m4, matVP.m11 + matVP.m8, matVP.m15 + matVP.m12};
	// right
	planes[1] = { matVP.m3 - matVP.m0, matVP.m7 - matVP.m4, matVP.m11 - matVP.m8, matVP.m15 - matVP.m12 };
	// bottom
	planes[2] = { matVP.m3 + matVP.m1, matVP.m7 + matVP.m5, matVP.m11 + matVP.m9, matVP.m15 + matVP.m13 };
	// top
	planes[3] = { matVP.m3 - matVP.m1, matVP.m7 - matVP.m5, matVP.m11 - matVP.m9, matVP.m15 - matVP.m13 };
	// near
	planes[4] = { matVP.m3 + matVP.m2, matVP.m7 + matVP.m6, matVP.m11 + matVP.m10, matVP.m15 + matVP.m14 };
	// far
	planes[5] = { matVP.m3 - matVP.m2, matVP.m7 - matVP.m6, matVP.m11 - matVP.m10, matVP.m15 - matVP.m14 };
	
	for (int i = 0; i < 6; i++)
	{
		float len = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
		planes[i] = { planes[i].x / len, planes[i].y / len, planes[i].z / len, planes[i].w / len};
	
		float dist = planes[i].x * centerX + planes[i].y * centerY + planes[i].z * centerZ + planes[i].w;
		if (dist < -radius) { return false; }
	}

	return true;
}
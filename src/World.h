#pragma once
#include<stdint.h>
#include<unordered_map>
#include<memory>
#include<thread>
#include<mutex>
#include<queue>
#include"Chunk.h"

/*
mutex is a synchronization primitive in C++ used to protect shared data 
from being simultaneously accessed by multiple threads. It ensures 
that only one thread can access a critical section of code at a time
*/


const int VIEW_DISTANCE = 4;


struct ChunkCoord
{
	// chunk coords, not world coords 
	// (e.g. chunk (2, 3) is the chunk that starts at (32, 48) world coords
	int x, z; 
	bool operator==(const ChunkCoord& other) const
	{
		return x == other.x && z == other.z;
	}
};

struct ChunkCoordHash
{
	// need to tell program how to hash our custom ChunkCoord struct
	// ^ (... < 16) combines x and z into a single number using XOR and bitshift
	//	without the shift, (2, 3) and (3, 2) would produce the same hash

	size_t operator()(const ChunkCoord& coord) const
	{
		return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.z) << 16);
	}
};

struct RaycastResult
{
	bool hit;
	int x, y, z;	// block that was hit
	int nx, ny, nz; // last air block before hit
};


class World
{
public:
	// unordered_map<key, value, hash>
	// unique_ptr is useful because Chunk objects are large and we want to
	//	dynamically create and destroy them at runtime.
	//	automatically freed from memory when removed from map
	std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;

	World();
	~World();
	void Update(Vector3 playerPos);
	void Draw();
	uint8_t GetBlock(int x, int y, int z);

	RaycastResult Raycast(Vector3 origin, Vector3 direction, float maxDistance);
	void SetBlock(int x, int y, int z, uint8_t block);

private:
	void LoadChunk(int cx, int cz);
	void UnloadChunk(int cx, int cz);
	void ChunkGenThread();

	std::thread genThread;
	std::mutex chunksMutex;

	std::queue<ChunkCoord> loadQueue;
	std::mutex loadQueueMutex;

	std::queue<std::pair<ChunkCoord, std::unique_ptr<Chunk>>> readyQueue;
	std::mutex readyQueueMutex;

	bool running;
};
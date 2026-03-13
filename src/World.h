#pragma once
#include<stdint.h>
#include<unordered_map>
#include<memory>
#include"Chunk.h"


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


class World
{
public:
	// unordered_map<key, value, hash>
	// unique_ptr is useful because Chunk objects are large and we want to
	//	dynamically create and destroy them at runtime.
	//	automatically freed from memory when removed from map
	std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;

	World();
	void Update(Vector3 playerPos);
	void Draw();
	uint8_t GetBlock(int x, int y, int z);

private:
	void LoadChunk(int cx, int cz);
	void UnloadChunk(int cx, int cz);

};
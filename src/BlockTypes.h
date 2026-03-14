#pragma once
#include"raylib.h"
#include<stdint.h>

enum BlockType : uint8_t
{
	BLOCK_AIR = 0,
	BLOCK_STONE,
	BLOCK_DIRT,
	BLOCK_GRASS,
	BLOCK_WATER
};

inline Color GetBlockColor(BlockType type)
{
	switch (type)
	{
		case BLOCK_GRASS:	return { 86, 125, 70, 255 };
		case BLOCK_DIRT:	return { 121, 85, 58, 255 };
		case BLOCK_STONE:	return { 120, 120, 120, 255 };
		case BLOCK_WATER:	return { 20, 100, 180, 255 };
		default:			return PINK;
	}
}
#include"Chunk.h"
#include"external/stb_perlin.h"
#include<vector>
#include"World.h"

Chunk::Chunk()
{
	meshDirty = true;
	mesh = {};

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

bool Chunk::IsAir(int x, int y, int z, const ChunkNeighborData& neighbors)
{
	// in bounds - check local blocks
	if (x >= 0 && x < CHUNK_WIDTH &&
		y >- 0 && y < CHUNK_HEIGHT &&
		z >= 0 && z < CHUNK_DEPTH)
		return blocks[x][y][z] == 0;

	// out of bounds - query world
	if (y < 0)				{ return false; }
	if (y >= CHUNK_HEIGHT)	{ return true; }

	// out of bounds - check neighbor snapshot
	if (x < 0 && neighbors.hasLeft)
		return neighbors.left[y][z] == BLOCK_AIR;
	if (x >= CHUNK_WIDTH && neighbors.hasRight)
		return neighbors.right[y][z] == BLOCK_AIR;
	if (z < 0 && neighbors.hasBack)
		return neighbors.back[x][y] == BLOCK_AIR;
	if (z >= CHUNK_DEPTH && neighbors.hasFront)
		return neighbors.front[x][y] == BLOCK_AIR;

	return true;
}

void Chunk::Fill(int worldX, int worldZ)
{
	for (int x = 0; x < CHUNK_WIDTH; x++)
	{
		for (int z = 0; z < CHUNK_DEPTH; z++)
		{
			float nx = (worldX * CHUNK_WIDTH + x) * 0.1f;
			float nz = (worldZ * CHUNK_DEPTH + z) * 0.1f;
			float noise = stb_perlin_noise3(nx, 0, nz, 0, 0, 0);
			int height = (int)((noise + 1.f) * 0.5f * 8) + 4;

			for (int y = 0; y < CHUNK_HEIGHT; y++)
			{
				if (y > height)
					blocks[x][y][z] = BLOCK_AIR;
				else if (y == height)
					blocks[x][y][z] = BLOCK_GRASS;
				else if (y >= height - 3)
					blocks[x][y][z] = BLOCK_DIRT;
				else
					blocks[x][y][z] = BLOCK_STONE;
			}
		}
	}

	meshDirty = true;
}

void Chunk::BuildMesh()
{
	ChunkNeighborData neighbors = {};
	BuildMeshData(neighbors);
	UploadMeshData();
}

void Chunk::BuildMeshData(const ChunkNeighborData& neighbors)
{
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<unsigned char> colors;
	std::vector<unsigned short> indices;

	unsigned short index = 0;

	auto addFace = [&]
	(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal, Color color)
		{
			// triangle 1
			vertices.insert(vertices.end(), { a.x, a.y, a.z });
			vertices.insert(vertices.end(), { b.x, b.y, b.z });
			vertices.insert(vertices.end(), { c.x, c.y, c.z });
			// triangle 2
			vertices.insert(vertices.end(), { a.x, a.y, a.z });
			vertices.insert(vertices.end(), { c.x, c.y, c.z });
			vertices.insert(vertices.end(), { d.x, d.y, d.z });

			for (int i = 0; i < 6; i++) // find normals of each face
			{
				normals.insert(normals.end(), { normal.x, normal.y, normal.z });
				colors.insert(colors.end(), { color.r, color.g, color.b, color.a });
			}

			indices.insert
			(indices.end(),
				{ index,
				(unsigned short)(index + 1),
				(unsigned short)(index + 2),
				(unsigned short)(index + 3),
				(unsigned short)(index + 4),
				(unsigned short)(index + 5) }
			);
			index += 6;
		};

	for (int x = 0; x < CHUNK_WIDTH; x++)
	{
		for (int y = 0; y < CHUNK_HEIGHT; y++)
		{
			for (int z = 0; z < CHUNK_DEPTH; z++)
			{
				if (blocks[x][y][z] == BLOCK_AIR) { continue; }

				Color color = GetBlockColor((BlockType)blocks[x][y][z]);

				float x0 = x, x1 = x + 1;
				float y0 = y, y1 = y + 1;
				float z0 = z, z1 = z + 1;

				// top
				if (IsAir(x, y + 1, z, neighbors))
					addFace({ x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 }, { x0, y1, z0 }, { 0, 1, 0 }, color);
				// bottom
				if (IsAir(x, y - 1, z, neighbors))
					addFace({ x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 }, { 0, -1, 0 }, color);
				// front
				if (IsAir(x, y, z + 1, neighbors))
					addFace({ x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { 0, 0, 1 }, color);
				// back
				if (IsAir(x, y, z - 1, neighbors))
					addFace({ x1, y0, z0 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { 0, 0, -1 }, color);
				// right
				if (IsAir(x + 1, y, z, neighbors))
					addFace({ x1, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { 1, 0, 0 }, color);
				// left
				if (IsAir(x - 1, y, z, neighbors))
					addFace({ x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, { -1, 0, 0 }, color);
			}
		}
	}

	if (mesh.vboId != nullptr) { UnloadMesh(mesh); }

	mesh = {};
	mesh.triangleCount = vertices.size() / 9;
	mesh.vertexCount = vertices.size() / 3;

	mesh.vertices = (float*)MemAlloc(vertices.size() * sizeof(float));
	mesh.normals = (float*)MemAlloc(normals.size() * sizeof(float));
	mesh.colors = (unsigned char*)MemAlloc(colors.size() * sizeof(unsigned char));
	mesh.indices = (unsigned short*)MemAlloc(indices.size() * sizeof(unsigned short));

	memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));
	memcpy(mesh.normals, normals.data(), normals.size() * sizeof(float));
	memcpy(mesh.colors, colors.data(), colors.size() * sizeof(unsigned char));
	memcpy(mesh.indices, indices.data(), indices.size() * sizeof(unsigned short));

	meshDirty = false;
}

void Chunk::UploadMeshData()
{
	UploadMesh(&mesh, false);
	meshDirty = false;
}

void Chunk::Draw(Vector3 position)
{
	if (mesh.vertexCount == 0) { return; }

	Material mat = LoadMaterialDefault();
	DrawMesh(mesh, mat, MatrixTranslate(position.x, position.y, position.z));
	UnloadMaterial(mat);
}
#include"Chunk.h"
#include"external/stb_perlin.h"
#include<vector>
#include"World.h"


const int MAX_HEIGHT = 40;
const int MIN_HEIGHT = 8;
const int SEA_LEVEL = 24;


Chunk::Chunk()
{
	meshDirty = true;
	meshReady = false;
	memset(&mesh, 0, sizeof(Mesh));
	memset(&waterMesh, 0, sizeof(Mesh));

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
		y >= 0 && y < CHUNK_HEIGHT &&
		z >= 0 && z < CHUNK_DEPTH)
		return blocks[x][y][z] == BLOCK_AIR;

	// out of bounds - query world
	if (y < 0)				{ return false; }
	if (y >= CHUNK_HEIGHT)	{ return true; }

	// out of bounds - check neighbor snapshot
	if (x < 0 && neighbors.hasLeft && z >= 0 && z < CHUNK_DEPTH)
		return neighbors.left[y][z] == BLOCK_AIR;
	if (x >= CHUNK_WIDTH && neighbors.hasRight && z >= 0 && z < CHUNK_DEPTH)
		return neighbors.right[y][z] == BLOCK_AIR;
	if (z < 0 && neighbors.hasBack && x >= 0 && x < CHUNK_WIDTH)
		return neighbors.back[x][y] == BLOCK_AIR;
	if (z >= CHUNK_DEPTH && neighbors.hasFront && x >= 0 && x < CHUNK_WIDTH)
		return neighbors.front[x][y] == BLOCK_AIR;

	if (x < 0 && !neighbors.hasLeft)			{ return false; }
	if (x >= CHUNK_WIDTH && !neighbors.hasRight){ return false; }
	if (z < 0 && !neighbors.hasBack)			{ return false; }
	if (z >= CHUNK_DEPTH && !neighbors.hasFront){ return false; }

	return true;
}

void Chunk::Fill(int worldX, int worldZ)
{
	for (int x = 0; x < CHUNK_WIDTH; x++)
	{
		for (int z = 0; z < CHUNK_DEPTH; z++)
		{
			float nx = (worldX * CHUNK_WIDTH + x);
			float nz = (worldZ * CHUNK_DEPTH + z);

			// layer multiple octaves
			float noise = 0.f;
			float amplitude = 1.f;
			float frequency = 0.01f; // lower->broader smoother hills, higher->jagged terrain
			float maxAmplitude = 0.f;
			for (int i = 0; i < 4; i++)
			{
				noise += stb_perlin_noise3(nx * frequency, 0, nz * frequency, 0, 0, 0) * amplitude;
				maxAmplitude += amplitude;
				amplitude *= 0.7f;	// called persistence; lower vals make fine detail less prominent
				frequency *= 2.f;	// called lacunarity; higher vals make each octave finer
			}

			// normalize to -1...1
			noise /= maxAmplitude;

			int height = (int)((noise + 1.f) * 0.5f * MAX_HEIGHT) + MIN_HEIGHT;

			for (int y = 0; y < CHUNK_HEIGHT; y++)
			{
				if (y > height && y <= SEA_LEVEL)
					blocks[x][y][z] = BLOCK_WATER;
				else if (y > height)
					blocks[x][y][z] = BLOCK_AIR;
				else if (y == height)
					blocks[x][y][z] = y >= SEA_LEVEL ? BLOCK_GRASS : BLOCK_DIRT;
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
	std::vector<float> vertices; // solids
	std::vector<float> normals;
	std::vector<unsigned char> colors;
	std::vector<unsigned short> indices;
	unsigned short index = 0;

	std::vector<float> wVertices; // water
	std::vector<float> wNormals;
	std::vector<unsigned char> wColors;
	std::vector<unsigned short> wIndices;
	unsigned short wIndex = 0;

	auto addFace = [&]
	(Vector3 a, Vector3 b, Vector3 c, Vector3 d, 
		Vector3 normal, Color color, bool water,
		float ao0, float ao1, float ao2, float ao3)
		{
			auto& v = water ? wVertices : vertices;
			auto& n = water ? wNormals : normals;
			auto& co = water ? wColors : colors;
			auto& ind = water ? wIndices : indices;
			unsigned short& idx = water ? wIndex : index;

			// triangle 1
			v.insert(v.end(), { a.x, a.y, a.z });
			v.insert(v.end(), { b.x, b.y, b.z });
			v.insert(v.end(), { c.x, c.y, c.z });
			// triangle 2
			v.insert(v.end(), { a.x, a.y, a.z });
			v.insert(v.end(), { c.x, c.y, c.z });
			v.insert(v.end(), { d.x, d.y, d.z });

			float aos[4] = { ao0, ao2, ao2, ao3 };
			// triangle 1: a, b, c = indices 0, 1, 2
			// triangle 2: a, c, d = indices 0, 2, 3
			int triIndices[6] = { 0, 1, 2, 0, 2, 3 };
			for (int i = 0; i < 6; i++) // find normals of each face
			{
				float ao = aos[triIndices[i]];
				n.insert(n.end(), { normal.x, normal.y, normal.z });
				co.insert(co.end(), { 
					(unsigned char)(color.r * ao), 
					(unsigned char)(color.g * ao), 
					(unsigned char)(color.b * ao), 
					color.a});
			}

			ind.insert(ind.end(),
				{ idx,
				(unsigned short)(idx + 1),
				(unsigned short)(idx + 2),
				(unsigned short)(idx + 3),
				(unsigned short)(idx + 4),
				(unsigned short)(idx + 5) });
			idx += 6;
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

				bool isWater = blocks[x][y][z] == BLOCK_WATER;

				auto IsAirForSolid = [&](int x, int y, int z) -> bool
				{
					if (x >= 0 && x < CHUNK_WIDTH &&
						y >= 0 && y < CHUNK_HEIGHT &&
						z >= 0 && z < CHUNK_DEPTH)
						{ return blocks[x][y][z] == BLOCK_AIR || blocks[x][y][z] == BLOCK_WATER; }

					if (y < 0) { return false; }
					if (y >= CHUNK_HEIGHT) { return true; }

					if (x < 0 && neighbors.hasLeft && z >= 0 && z < CHUNK_DEPTH)
						return neighbors.left[y][z] == BLOCK_AIR || neighbors.left[y][z] == BLOCK_WATER;
					if (x >= CHUNK_WIDTH && neighbors.hasRight && z >= 0 && z < CHUNK_DEPTH)
						return neighbors.right[y][z] == BLOCK_AIR || neighbors.right[y][z] == BLOCK_WATER;
					if (z < 0 && neighbors.hasBack && x >= 0 && x < CHUNK_WIDTH)
						return neighbors.back[x][y] == BLOCK_AIR || neighbors.back[x][y] == BLOCK_WATER;
					if (z >= CHUNK_DEPTH && neighbors.hasFront && x >= 0 && x < CHUNK_WIDTH)
						return neighbors.front[x][y] == BLOCK_AIR || neighbors.front[x][y] == BLOCK_WATER;

					return true;
				};

				if (isWater)
				{
					Color sideColor = { 20, 80, 160, 255 };

					if (IsAir(x, y + 1, z, neighbors))
						addFace({ x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 }, { x0, y1, z0 }, { 0, 1, 0 }, color, true, 1.f, 1.f, 1.f, 1.f);
					
					// side faces ony where adjacent to air
					if (IsAir(x, y, z + 1, neighbors))
						addFace({ x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { 0, 0, 1 }, sideColor, false, 1.f, 1.f, 1.f, 1.f);
					if (IsAir(x, y, z - 1, neighbors))
						addFace({ x1, y0, z0 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { 0, 0, -1 }, sideColor, false, 1.f, 1.f, 1.f, 1.f);
					if (IsAir(x + 1, y, z, neighbors))
						addFace({ x1, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { 1, 0, 0 }, sideColor, false, 1.f, 1.f, 1.f, 1.f);
					if (IsAir(x - 1, y, z, neighbors))
						addFace({ x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, { -1, 0, 0 }, sideColor, false, 1.f, 1.f, 1.f, 1.f);
				}
				else
				{
					// top
					if (IsAirForSolid(x, y + 1, z))
					{
						bool s1, s2, c; // sides and corner
						float ao0, ao1, ao2, ao3;

						s1 = !IsAirForSolid(x - 1, y + 1, z); s2 = !IsAirForSolid(x, y + 1, z + 1); c = !IsAirForSolid(x - 1, y + 1, z + 1);
						ao0 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y + 1, z); s2 = !IsAirForSolid(x, y + 1, z + 1); c = !IsAirForSolid(x + 1, y + 1, z + 1);
						ao1 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y + 1, z); s2 = !IsAirForSolid(x, y + 1, z - 1); c = !IsAirForSolid(x + 1, y + 1, z - 1);
						ao2 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y + 1, z); s2 = !IsAirForSolid(x, y + 1, z - 1); c = !IsAirForSolid(x - 1, y + 1, z - 1);
						ao3 = VertexAO(s1, s2, c);
						addFace({ x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 }, { x0, y1, z0 }, { 0, 1, 0 }, color, false, ao0, ao1, ao2, ao3);
					}


					// bottom
					if (IsAirForSolid(x, y - 1, z))
					{
						bool s1, s2, c;
						float ao0, ao1, ao2, ao3;

						s1 = !IsAirForSolid(x - 1, y - 1, z); s2 = !IsAirForSolid(x, y - 1, z - 1); c = !IsAirForSolid(x - 1, y - 1, z - 1);
						ao0 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y - 1, z); s2 = !IsAirForSolid(x, y - 1, z - 1); c = !IsAirForSolid(x + 1, y - 1, z - 1);
						ao1 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y - 1, z); s2 = !IsAirForSolid(x, y - 1, z + 1); c = !IsAirForSolid(x + 1, y - 1, z + 1);
						ao2 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y - 1, z); s2 = !IsAirForSolid(x, y - 1, z + 1); c = !IsAirForSolid(x - 1, y - 1, z + 1);
						ao3 = VertexAO(s1, s2, c);
						addFace({ x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 }, { 0, -1, 0 }, color, false, ao0, ao1, ao2, ao3);
					}


					// front
					if (IsAirForSolid(x, y, z + 1))
					{
						bool s1, s2, c;
						float ao0, ao1, ao2, ao3;

						s1 = !IsAirForSolid(x - 1, y, z + 1); s2 = !IsAirForSolid(x, y - 1, z + 1); c = !IsAirForSolid(x - 1, y - 1, z + 1);
						ao0 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y, z + 1); s2 = !IsAirForSolid(x, y - 1, z + 1); c = !IsAirForSolid(x + 1, y - 1, z + 1);
						ao1 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y, z + 1); s2 = !IsAirForSolid(x, y + 1, z + 1); c = !IsAirForSolid(x + 1, y + 1, z + 1);
						ao2 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y, z + 1); s2 = !IsAirForSolid(x, y + 1, z + 1); c = !IsAirForSolid(x - 1, y + 1, z + 1);
						ao3 = VertexAO(s1, s2, c);
						addFace({ x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { 0, 0, 1 }, color, false, ao0, ao1, ao2, ao3);
					}


					// back
					if (IsAirForSolid(x, y, z - 1))
					{
						bool s1, s2, c;
						float ao0, ao1, ao2, ao3;

						s1 = !IsAirForSolid(x + 1, y, z - 1); s2 = !IsAirForSolid(x, y - 1, z - 1); c = !IsAirForSolid(x + 1, y - 1, z - 1);
						ao0 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y, z - 1); s2 = !IsAirForSolid(x, y - 1, z - 1); c = !IsAirForSolid(x - 1, y - 1, z - 1);
						ao1 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y, z - 1); s2 = !IsAirForSolid(x, y + 1, z - 1); c = !IsAirForSolid(x - 1, y + 1, z - 1);
						ao2 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y, z - 1); s2 = !IsAirForSolid(x, y + 1, z - 1); c = !IsAirForSolid(x + 1, y + 1, z - 1);
						ao3 = VertexAO(s1, s2, c);
						addFace({ x1, y0, z0 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { 0, 0, -1 }, color, false, ao0, ao1, ao2, ao3);
					}


					// right
					if (IsAirForSolid(x + 1, y, z))
					{
						bool s1, s2, c;
						float ao0, ao1, ao2, ao3;

						s1 = !IsAirForSolid(x + 1, y, z + 1); s2 = !IsAirForSolid(x + 1, y - 1, z); c = !IsAirForSolid(x + 1, y - 1, z + 1);
						ao0 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y, z - 1); s2 = !IsAirForSolid(x + 1, y - 1, z); c = !IsAirForSolid(x + 1, y - 1, z - 1);
						ao1 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y, z - 1); s2 = !IsAirForSolid(x + 1, y + 1, z); c = !IsAirForSolid(x + 1, y + 1, z - 1);
						ao2 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x + 1, y, z + 1); s2 = !IsAirForSolid(x + 1, y + 1, z); c = !IsAirForSolid(x + 1, y + 1, z + 1);
						ao3 = VertexAO(s1, s2, c);
						addFace({ x1, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { 1, 0, 0 }, color, false, ao0, ao1, ao2, ao3);
					}


					// left
					if (IsAirForSolid(x - 1, y, z))
					{
						bool s1, s2, c;
						float ao0, ao1, ao2, ao3;

						s1 = !IsAirForSolid(x - 1, y, z - 1); s2 = !IsAirForSolid(x - 1, y - 1, z); c = !IsAirForSolid(x - 1, y - 1, z - 1);
						ao0 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y, z + 1); s2 = !IsAirForSolid(x - 1, y - 1, z); c = !IsAirForSolid(x - 1, y - 1, z + 1);
						ao1 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y, z + 1); s2 = !IsAirForSolid(x - 1, y + 1, z); c = !IsAirForSolid(x - 1, y + 1, z + 1);
						ao2 = VertexAO(s1, s2, c);
						s1 = !IsAirForSolid(x - 1, y, z - 1); s2 = !IsAirForSolid(x - 1, y + 1, z); c = !IsAirForSolid(x - 1, y + 1, z - 1);
						ao3 = VertexAO(s1, s2, c);
						addFace({ x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, { -1, 0, 0 }, color, false, ao0, ao1, ao2, ao3);
					}
				}

			}
		}
	}

	if (indices.size() > 60000)
		TraceLog(LOG_WARNING, "Chunk at high index count: %d indices", (int)indices.size());

	// build solid mesh
	if (meshReady && mesh.vboId != nullptr) { UnloadMesh(mesh); }

	mesh = {};
	mesh.triangleCount = vertices.size() / 9;
	mesh.vertexCount = vertices.size() / 3;

	mesh.vertices = (float*)MemAlloc(			vertices.size() * sizeof(float));
	mesh.normals =	(float*)MemAlloc(			normals.size() * sizeof(float));
	mesh.colors =	(unsigned char*)MemAlloc(	colors.size() * sizeof(unsigned char));
	mesh.indices =	(unsigned short*)MemAlloc(	indices.size() * sizeof(unsigned short));

	memcpy(mesh.vertices,	vertices.data(),vertices.size() * sizeof(float));
	memcpy(mesh.normals,	normals.data(), normals.size() * sizeof(float));
	memcpy(mesh.colors,		colors.data(),	colors.size() * sizeof(unsigned char));
	memcpy(mesh.indices,	indices.data(), indices.size() * sizeof(unsigned short));

	// build solid mesh
	if (meshReady && waterMesh.vboId != nullptr) { UnloadMesh(waterMesh); }

	waterMesh = {};
	waterMesh.triangleCount = wVertices.size() / 9;
	waterMesh.vertexCount = wVertices.size() / 3;

	waterMesh.vertices = (float*)MemAlloc(			wVertices.size() * sizeof(float));
	waterMesh.normals =	(float*)MemAlloc(			wNormals.size() * sizeof(float));
	waterMesh.colors =	(unsigned char*)MemAlloc(	wColors.size() * sizeof(unsigned char));
	waterMesh.indices =	(unsigned short*)MemAlloc(	wIndices.size() * sizeof(unsigned short));

	memcpy(waterMesh.vertices,	wVertices.data(),	wVertices.size() * sizeof(float));
	memcpy(waterMesh.normals,	wNormals.data(),	wNormals.size() * sizeof(float));
	memcpy(waterMesh.colors,	wColors.data(),		wColors.size() * sizeof(unsigned char));
	memcpy(waterMesh.indices,	wIndices.data(),	wIndices.size() * sizeof(unsigned short));

	// set flag
	meshDirty = false;
}

void Chunk::UploadMeshData()
{
	UploadMesh(&mesh, false);
	if (waterMesh.vertexCount > 0)
		UploadMesh(&waterMesh, false);
	meshReady = true;
}

void Chunk::Draw(Vector3 position, Material& mat)
{
	if (!meshReady || mesh.vertexCount == 0) { return; }

	DrawMesh(mesh, mat, MatrixTranslate(position.x, position.y, position.z));
}

void Chunk::DrawWater(Vector3 position, Material& mat)
{
	if (!meshReady 
		|| waterMesh.vertexCount == 0 
		|| waterMesh.vboId == nullptr
		|| waterMesh.vboId[0] == 0) 
	{ return; }

	// SetShaderValue() needs to be called while shader is active in order for 
	// uniform variable to be picked up correctly. hence Begin/EndShaderMode()
	BeginShaderMode(mat.shader); 
	int chunkOffsetLoc = GetShaderLocation(mat.shader, "chunkOffset");
	float offset[2] = { position.x, position.z };
	SetShaderValue(mat.shader, chunkOffsetLoc, offset, SHADER_UNIFORM_VEC2);
	EndShaderMode();
	DrawMesh(waterMesh, mat, MatrixTranslate(position.x, position.y, position.z));
}

float Chunk::VertexAO(bool side1, bool side2, bool corner)
{
	if (side1 && side2) { return 0.6; } // darkest
	return 1.f - (side1 + side2 + corner) * 0.15f;
}
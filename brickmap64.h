#pragma once

#include "windowgl.h"
#include "obj.h"
#include "math.h"

struct BrickmapNode{
	DWORD mask_low = 0;
	DWORD mask_high = 0;
	DWORD offset = 0xFFFFFFFF;	//Index wo die Brickmap zu finden ist TODO Muss evtl auf 64 Bit erweitert werden
};

struct AABB{
	fvec3 min;
	fvec3 max;
};

constexpr DWORD textureRepeated(Image& image, float u, float v)noexcept{
	u = u - floor(u);
	v = v - floor(v);
	WORD u1 = u*(image.width-1);
	WORD v1 = v*(image.height-1);
	return image.data[v1*image.width+u1];
}

bool testAxis(fvec3 axis, const AABB& aabb, const fvec3& p0, const fvec3& p1, const fvec3& p2){
    float minA = std::numeric_limits<float>::max();
    float maxA = -std::numeric_limits<float>::max();

    for(int i = 0; i < 8; ++i){
        fvec3 p;

        p.x = (i & 1) ? aabb.max.x : aabb.min.x;
        p.y = (i & 2) ? aabb.max.y : aabb.min.y;
        p.z = (i & 4) ? aabb.max.z : aabb.min.z;

        float proj = dot(p, axis);

        minA = std::min(minA, proj);
        maxA = std::max(maxA, proj);
    }

    float minB = std::numeric_limits<float>::max();
    float maxB = -std::numeric_limits<float>::max();

    float proj = dot(p0, axis);
    minB = std::min(minB, proj);
    maxB = std::max(maxB, proj);
	proj = dot(p1, axis);
    minB = std::min(minB, proj);
    maxB = std::max(maxB, proj);
	proj = dot(p2, axis);
    minB = std::min(minB, proj);
    maxB = std::max(maxB, proj);

    return !(maxA < minB || maxB < minA);
}

bool triangleAABBIntersection(const fvec3& p0, const fvec3& p1, const fvec3& p2, const AABB& aabb){
	fvec3 f0 = {p0.x - p1.x, p0.y - p1.y, p0.z - p1.z};
	fvec3 f1 = {p0.x - p2.x, p0.y - p2.y, p0.z - p2.z};
	fvec3 f2 = {p1.x - p2.x, p1.y - p2.y, p1.z - p2.z};
	fvec3 normal = cross(f0, f1);
	if(!testAxis(normal, aabb, p0, p1, p2)) return false;
	if(!testAxis({1, 0, 0}, aabb, p0, p1, p2)) return false;
	if(!testAxis({0, 1, 0}, aabb, p0, p1, p2)) return false;
	if(!testAxis({0, 0, 1}, aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f0, {1, 0, 0}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f0, {0, 1, 0}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f0, {0, 0, 1}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f1, {1, 0, 0}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f1, {0, 1, 0}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f1, {0, 0, 1}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f2, {1, 0, 0}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f2, {0, 1, 0}), aabb, p0, p1, p2)) return false;
	if(!testAxis(cross(f2, {0, 0, 1}), aabb, p0, p1, p2)) return false;
	return true;
}

struct BarycentricCoords{
	float u;
	float v;
	float w;
};

BarycentricCoords getBarycentricFromPoint(const fvec3& p, Triangle& tri) {
    fvec3 p21 = {tri.points[2].x - tri.points[1].x, tri.points[2].y - tri.points[1].y, tri.points[2].z - tri.points[1].z};
    fvec3 p20 = {tri.points[2].x - tri.points[0].x, tri.points[2].y - tri.points[0].y, tri.points[2].z - tri.points[0].z};
    fvec3 normal = normalize(cross(p21, p20));
    
    fvec3 diff = {p.x - tri.points[0].x, p.y - tri.points[0].y, p.z - tri.points[0].z};
    float dist = dot(normal, diff);
    fvec3 projP = {p.x - normal.x * dist, p.y - normal.y * dist, p.z - normal.z * dist};

    fvec3 v0 = {tri.points[1].x - tri.points[0].x, tri.points[1].y - tri.points[0].y, tri.points[1].z - tri.points[0].z};
    fvec3 v1 = {tri.points[2].x - tri.points[0].x, tri.points[2].y - tri.points[0].y, tri.points[2].z - tri.points[0].z};
    fvec3 v2 = {projP.x - tri.points[0].x, projP.y - tri.points[0].y, projP.z - tri.points[0].z};

    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = 1.f / (d00 * d11 - d01 * d01);
    float v = (d11 * d20 - d01 * d21) * denom;
    float w = (d00 * d21 - d01 * d20) * denom;
    float u = 1 - v - w;

    return {clamp(u, 0.f, 1.f), clamp(v, 0.f, 1.f), clamp(w, 0.f, 1.f)};
}

void calculateBrickmapFromMesh(SSBO& low_lod, SSBO& high_lod, DWORD dx, DWORD dy, DWORD dz, TriangleModel* models, DWORD modelCount)noexcept{
    float minX = std::numeric_limits<float>::max(), minY = std::numeric_limits<float>::max(), minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max(), maxY = -std::numeric_limits<float>::max(), maxZ = -std::numeric_limits<float>::max();

	for(DWORD i=0; i < modelCount; ++i){
		const TriangleModel& model = models[i];
		for(DWORD j=0; j < model.triangleCount; ++j){
			const Triangle& tri = model.triangles[j];
			for(int k=0; k < 3; ++k){
				const fvec3& point = tri.points[k];
				minX = min(minX, point.x);
				minY = min(minY, point.y);
				minZ = min(minZ, point.z);
				maxX = max(maxX, point.x);
				maxY = max(maxY, point.y);
				maxZ = max(maxZ, point.z);
			}
		}
	}

    float modelSizeX = maxX - minX;
    float modelSizeY = maxY - minY;
    float modelSizeZ = maxZ - minZ;

	DWORD dx4 = std::ceil((float)dx/4);
	DWORD dy4 = std::ceil((float)dy/4);
	DWORD dz4 = std::ceil((float)dz/4);
	std::vector<BrickmapNode> low_brickmap(dx4 * dy4 * dz4);
	std::vector<DWORD> voxel_data;

	DWORD non_air_voxels = 0;

	for(DWORD i=0; i < modelCount; ++i){
		const TriangleModel& model = models[i];
		for(DWORD j = 0; j < model.triangleCount; ++j){
			fvec3 point1 = model.triangles[j].points[0];
			fvec3 point2 = model.triangles[j].points[1];
			fvec3 point3 = model.triangles[j].points[2];

			point1.x = (point1.x - minX) * (dx-1) / modelSizeX;
			point1.y = (point1.y - minY) * (dy-1) / modelSizeY;
			point1.z = (point1.z - minZ) * (dz-1) / modelSizeZ;

			point2.x = (point2.x - minX) * (dx-1) / modelSizeX;
			point2.y = (point2.y - minY) * (dy-1) / modelSizeY;
			point2.z = (point2.z - minZ) * (dz-1) / modelSizeZ;

			point3.x = (point3.x - minX) * (dx-1) / modelSizeX;
			point3.y = (point3.y - minY) * (dy-1) / modelSizeY;
			point3.z = (point3.z - minZ) * (dz-1) / modelSizeZ;

			float minSDFX = max(min(point1.x, min(point2.x, point3.x))-1, 0.f);
			float minSDFY = max(min(point1.y, min(point2.y, point3.y))-1, 0.f);
			float minSDFZ = max(min(point1.z, min(point2.z, point3.z))-1, 0.f);
			float maxSDFX = min(max(point1.x, max(point2.x, point3.x))+1, (float)dx-1);
			float maxSDFY = min(max(point1.y, max(point2.y, point3.y))+1, (float)dy-1);
			float maxSDFZ = min(max(point1.z, max(point2.z, point3.z))+1, (float)dz-1);

			for(int x = minSDFX; x <= maxSDFX; ++x){
				for(int y = minSDFY; y <= maxSDFY; ++y){
					for(int z = minSDFZ; z <= maxSDFZ; ++z){
						DWORD brickmap_index = z/4 * dy4 * dx4 + y/4 * dx4 + x/4;
						DWORD brickmap_entry_index = (z % 4) * 16 + (y % 4) * 4 + (x % 4);
						DWORD marked_idx = brickmap_index * 64 + brickmap_entry_index;
						fvec3 voxel_center = {(x + 0.5f) * modelSizeX / (dx-1) + minX, (y + 0.5f) * modelSizeY / (dy-1) + minY, (z + 0.5f) * modelSizeZ / (dz-1) + minZ};

						if(triangleAABBIntersection(point1, point2, point3, {{(float)x, (float)y, (float)z}, {(float)(x+1), (float)(y+1), (float)(z+1)}})){
							BarycentricCoords coords = getBarycentricFromPoint(voxel_center, model.triangles[j]);

							if(low_brickmap[brickmap_index].offset == 0xFFFFFFFF){
								low_brickmap[brickmap_index].offset = voxel_data.size();
								voxel_data.resize(voxel_data.size() + 64, 0);
							}

							float u1 = model.attributesBuffer[j * model.attributesCount * 3 ];
							float v1 = model.attributesBuffer[j * model.attributesCount * 3 + 1];
							float u2 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount];
							float v2 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount + 1];
							float u3 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount * 2];
							float v3 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount * 2 + 1];
							float u = coords.u * u1 + coords.v * u2 + coords.w * u3;
							float v = coords.u * v1 + coords.v * v2 + coords.w * v3;
							DWORD idx = low_brickmap[brickmap_index].offset + brickmap_entry_index;
							if(model.material){
								voxel_data[idx] = textureRepeated(model.material->textures[0], u, v);
								voxel_data[idx] = A(voxel_data[idx], 128);
							}
							else voxel_data[idx] = RGBA(255, 255, 255, 128);
							non_air_voxels++;
							if(brickmap_entry_index < 32) low_brickmap[brickmap_index].mask_low |= (1u << brickmap_entry_index);
							else low_brickmap[brickmap_index].mask_high |= (1u << (brickmap_entry_index - 32));
						}
					}
				}
			}
		}
	}
	low_lod.clear();
	low_lod.append(low_brickmap.data(), low_brickmap.size() * sizeof(BrickmapNode));
	std::cout << "Brickmap Größe: " << memoryUsageToHuman(low_brickmap.size() * sizeof(BrickmapNode)) << std::endl;
	high_lod.clear();
	high_lod.append(voxel_data.data(), voxel_data.size() * sizeof(DWORD));
	std::cout << "Voxeldaten Größe: " << memoryUsageToHuman(voxel_data.size() * sizeof(DWORD)) << std::endl;
	std::cout << "Nicht leere Voxel: " << non_air_voxels << std::endl;
}

/*
	Speichert jeden Eintrag im 64-Tree
	- Äste sind Einträge mit index != 0xFFFFFFFF
	- Äste ohne Blätter sind Einträge mit children_mask == 0
	- Blätter sind Einträge mit children_mask == 0 und index != 0xFFFFFFFF
	- Kinder werden packed gespeichert, sprich es wird nur so viel Speicher allokiert wie es auch Kinder gibt
	- Ein Kind kann dann gefunden werden per Basepointer + popcount der children_mask

	TODO
	- Algorithmus der den Baum neu anordnet für bessere Speicherlokalität
*/

struct ivec3{
	int x;
	int y;
	int z;
};

struct Tree64{
	static const uint32_t INVALID_NODE = 0xFFFFFFFF;

	struct Node{
		uint64_t children_mask;
		uint32_t index;			// TODO Kann evtl. auf 64 Bit erweitert werden
	};

	struct BuildNode{
		uint32_t children_pointer[64];

		BuildNode(){
			for(uint8_t i=0; i < 64; ++i) children_pointer[i] = INVALID_NODE;
		}
	};

	std::vector<Node> tree_data;
	std::vector<BuildNode> build_tree_data;
	std::vector<uint32_t> voxel_data;
	uint8_t depth;
	
	Tree64(uint8_t tree_depth = 5){
		assert(depth >= 1);
		depth = tree_depth;
		tree_data.push_back({0, 0});
		build_tree_data.push_back(BuildNode());
	}

	// TODO Implementieren
	// void clear(){}
	
	void make_sparse(std::vector<Node>& tree)noexcept{
		// ...
		build_tree_data.clear();
	}

	void insert_voxel(ivec3 position, uint32_t voxel)noexcept{
		assert(build_tree_data.size() > 0);

		uint32_t current_index = 0;
		for(uint8_t i=0; i < depth; ++i){
			uint32_t shift = (depth - i - 1) * 2;
			uint32_t x = (position.x >> shift) & 0b11;
			uint32_t y = (position.y >> shift) & 0b11;
			uint32_t z = (position.z >> shift) & 0b11;

			uint32_t index = x | (y << 2) | (z << 4);
			if(build_tree_data[current_index].children_pointer[index] == INVALID_NODE){
				build_tree_data.push_back(BuildNode());
				build_tree_data[current_index].children_pointer[index] = build_tree_data.size() - 1;
			}
			current_index = build_tree_data[current_index].children_pointer[index];
		}
		if(build_tree_data[current_index].children_pointer[0] == INVALID_NODE){
			voxel_data.push_back(voxel);
			build_tree_data[current_index].children_pointer[0] = voxel_data.size() - 1;
		}else{
			uint32_t voxel_data_index = build_tree_data[current_index].children_pointer[0];
			voxel_data[voxel_data_index] = voxel;
		}
	}

	// TODO Implementieren
	// void remove_voxel(){}

	void add_mesh(TriangleModel* models, DWORD modelCount)noexcept{
		float minX = std::numeric_limits<float>::max(), minY = std::numeric_limits<float>::max(), minZ = std::numeric_limits<float>::max();
		float maxX = -std::numeric_limits<float>::max(), maxY = -std::numeric_limits<float>::max(), maxZ = -std::numeric_limits<float>::max();

		for(DWORD i=0; i < modelCount; ++i){
			const TriangleModel& model = models[i];
			for(DWORD j=0; j < model.triangleCount; ++j){
				const Triangle& tri = model.triangles[j];
				for(int k=0; k < 3; ++k){
					const fvec3& point = tri.points[k];
					minX = min(minX, point.x);
					minY = min(minY, point.y);
					minZ = min(minZ, point.z);
					maxX = max(maxX, point.x);
					maxY = max(maxY, point.y);
					maxZ = max(maxZ, point.z);
				}
			}
		}

		float modelSizeX = maxX - minX;
		float modelSizeY = maxY - minY;
		float modelSizeZ = maxZ - minZ;

		float voxelVolumeSize = std::pow(4, depth);

		float modelScaleFactor = max(modelSizeX, max(modelSizeY, modelSizeZ));
		float invModelScaleFactor = 1.f / modelScaleFactor;

		DWORD non_air_voxels = 0;

		for(DWORD i=0; i < modelCount; ++i){
			const TriangleModel& model = models[i];
			for(DWORD j = 0; j < model.triangleCount; ++j){
				fvec3 point1 = model.triangles[j].points[0];
				fvec3 point2 = model.triangles[j].points[1];
				fvec3 point3 = model.triangles[j].points[2];

				point1.x = (point1.x - minX) * voxelVolumeSize * invModelScaleFactor;
				point1.y = (point1.y - minY) * voxelVolumeSize * invModelScaleFactor;
				point1.z = (point1.z - minZ) * voxelVolumeSize * invModelScaleFactor;

				point2.x = (point2.x - minX) * voxelVolumeSize * invModelScaleFactor;
				point2.y = (point2.y - minY) * voxelVolumeSize * invModelScaleFactor;
				point2.z = (point2.z - minZ) * voxelVolumeSize * invModelScaleFactor;

				point3.x = (point3.x - minX) * voxelVolumeSize * invModelScaleFactor;
				point3.y = (point3.y - minY) * voxelVolumeSize * invModelScaleFactor;
				point3.z = (point3.z - minZ) * voxelVolumeSize * invModelScaleFactor;

				float minSDFX = max(min(point1.x, min(point2.x, point3.x))-1, 0.f);
				float minSDFY = max(min(point1.y, min(point2.y, point3.y))-1, 0.f);
				float minSDFZ = max(min(point1.z, min(point2.z, point3.z))-1, 0.f);
				float maxSDFX = min(max(point1.x, max(point2.x, point3.x))+1, voxelVolumeSize-1);
				float maxSDFY = min(max(point1.y, max(point2.y, point3.y))+1, voxelVolumeSize-1);
				float maxSDFZ = min(max(point1.z, max(point2.z, point3.z))+1, voxelVolumeSize-1);

				for(int x = minSDFX; x <= maxSDFX; ++x){
					for(int y = minSDFY; y <= maxSDFY; ++y){
						for(int z = minSDFZ; z <= maxSDFZ; ++z){
							fvec3 voxel_center_mesh = {(x + 0.5f) * modelSizeX / (voxelVolumeSize-1) + minX, (y + 0.5f) * modelSizeY / (voxelVolumeSize-1) + minY, (z + 0.5f) * modelSizeZ / (voxelVolumeSize-1) + minZ};

							if(triangleAABBIntersection(point1, point2, point3, {{(float)x, (float)y, (float)z}, {(float)(x+1), (float)(y+1), (float)(z+1)}})){
								BarycentricCoords coords = getBarycentricFromPoint(voxel_center_mesh, model.triangles[j]);

								float u1 = model.attributesBuffer[j * model.attributesCount * 3 ];
								float v1 = model.attributesBuffer[j * model.attributesCount * 3 + 1];
								float u2 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount];
								float v2 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount + 1];
								float u3 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount * 2];
								float v3 = model.attributesBuffer[j * model.attributesCount * 3 + model.attributesCount * 2 + 1];
								float u = coords.u * u1 + coords.v * u2 + coords.w * u3;
								float v = coords.u * v1 + coords.v * v2 + coords.w * v3;

								DWORD voxel_data = RGBA(255, 255, 255, 128);
								if(model.material){
									voxel_data = textureRepeated(model.material->textures[0], u, v);
									voxel_data = A(voxel_data, 128);
								}
								non_air_voxels += 1;
								insert_voxel({x, y, z}, voxel_data);
							}
						}
					}
				}
			}
		}
		std::cout << "Mesh hat " << non_air_voxels << " voxel belegt." << std::endl;
	}
};

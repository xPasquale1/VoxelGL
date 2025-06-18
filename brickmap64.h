#pragma once

#include "windowgl.h"
#include "obj.h"

struct BrickmapNode{
	DWORD mask_low = 0;
	DWORD mask_high = 0;
	DWORD offset = 0xFFFFFFFF;	//Index wo die Brickmap zu finden ist TODO Muss evtl auf 64 Bit erweitert werden
};

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
						fvec3 pos = {x * modelSizeX / (dx-1) + minX, y * modelSizeY / (dy-1) + minY, z * modelSizeZ / (dz-1) + minZ};

						DistanceInfo dstInfo = pointToTriangleDistance(pos, model.triangles[j]);

						#ifdef ACCURATEMESHTOVOXEL
						if(dstInfo.distance < 0.2){	//TODO < 0.2 ist nicht akkurat, ich tippe mal das kommt daher, dass die distance basierend auf der mesh größe relative zur sdf größe berechnet werden muss
						#else
						if(dstInfo.distance < 2.0){	//TODO < 2.0 ist nicht akkurat
						#endif
							if(low_brickmap[brickmap_index].offset == 0xFFFFFFFF){
								low_brickmap[brickmap_index].offset = voxel_data.size();
								voxel_data.resize(voxel_data.size() + 64, 0);
							}
							float u1 = model.attributesBuffer[j*model.attributesCount*3];
							float v1 = model.attributesBuffer[j*model.attributesCount*3+1];
							float u2 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount];
							float v2 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount+1];
							float u3 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount*2];
							float v3 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount*2+1];
							float u = dstInfo.u * u1 + dstInfo.v * u2 + dstInfo.w * u3;
							float v = dstInfo.u * v1 + dstInfo.v * v2 + dstInfo.w * v3;
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

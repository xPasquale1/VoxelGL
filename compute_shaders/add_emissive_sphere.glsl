#version 430 core

#define PI 3.14159265359

layout(binding = 1, rgba8ui) uniform uimage3D sdfData;
layout(binding = 2, r8ui) uniform uimage3D sdfData8;
uniform ivec3 sdfSize;
uniform vec3 camPos;
uniform mat3 camRot;

struct HitData{
    bool didHit;
    vec3 color;
    vec3 position;
    vec3 normal;
};

HitData trace(vec3 origin, vec3 dir){
    HitData ret;
    ret.didHit = false;
    vec3 normal = vec3(0, 1, 0);

    ivec3 voxel_pos = (ivec3(origin) >> 3) << 3;
    vec3 inv_dir = 1.0 / dir;
    vec3 pos = origin;
    ivec3 step_dir = ivec3(
        int(sign(dir.x)),
        int(sign(dir.y)),
        int(sign(dir.z))
    );
    vec3 step_dir_f = vec3(step_dir);
    int scale = 8;
    ivec3 side_offset = ivec3(step(0.0, dir));
    ivec3 block8 = voxel_pos/8;
    ivec3 sidePos = voxel_pos + side_offset * scale;
    vec3 side_dist = (sidePos - pos) * inv_dir;

    for(int i=0; i < 1024; ++i){
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) return ret;
        if(scale == 8){
            block8 = voxel_pos/8;
            uint sdf_data = imageLoad(sdfData8, block8).r;
            if(sdf_data > 0){
                scale = 1;
                voxel_pos = ivec3(pos);
                continue;
            }
        }else{
            if(block8 != voxel_pos/8){
                scale = 8;
                voxel_pos = (voxel_pos >> 3) << 3;
                continue;
            }
            uvec4 sdf_data = imageLoad(sdfData, voxel_pos);
            if(sdf_data.a > 0){
                ret.didHit = true;
                ret.color = vec3(sdf_data.rgb)/255.0;
                ret.position = pos;
                ret.normal = normal;
                return ret;
            }
        }
        
        sidePos = voxel_pos + side_offset * scale;
        side_dist = (sidePos - pos) * inv_dir;

        float tmin;
        if(side_dist.x < side_dist.y && side_dist.x < side_dist.z){
            tmin = side_dist.x;
            voxel_pos.x += step_dir.x * scale;
            normal = vec3(-step_dir_f.x, 0, 0);
            pos.x = sidePos.x + step_dir_f.x*0.0001;
            pos.y += tmin * dir.y;
            pos.z += tmin * dir.z;
        }else if(side_dist.y < side_dist.z){
            tmin = side_dist.y;
            voxel_pos.y += step_dir.y * scale;
            normal = vec3(0, -step_dir_f.y, 0);
            pos.x += tmin * dir.x;
            pos.y = sidePos.y + step_dir_f.y*0.0001;
            pos.z += tmin * dir.z;
        }else{
            tmin = side_dist.z;
            voxel_pos.z += step_dir.z * scale;
            normal = vec3(0, 0, -step_dir_f.z);
            pos.x += tmin * dir.x;
            pos.y += tmin * dir.y;
            pos.z = sidePos.z + step_dir_f.z*0.0001;
        }
    }
    return ret;
}

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main(){
    //Raycast in die Szene
    HitData primary_hit_data = trace(camPos, camRot * vec3(0, 0, 1));
    if(primary_hit_data.didHit){
        ivec3 place_position = ivec3(primary_hit_data.position + primary_hit_data.normal*0.5);
        for(int z=-6; z <= 6; ++z){
            for(int y=-6; y <= 6; ++y){
                for(int x=-6; x < 6; ++x){
                    ivec3 pos = place_position + ivec3(x, y, z);
                    if(any(lessThan(pos, ivec3(0))) || any(greaterThanEqual(pos, ivec3(sdfSize)))) continue;
                    if(distance(pos, place_position) <= 6){
                        imageStore(sdfData, pos, uvec4(255, 255, 255, 255));
                        imageStore(sdfData8, pos/8, uvec4(1, 1, 1, 1));
                    }
                }
            }
        }
    }
}

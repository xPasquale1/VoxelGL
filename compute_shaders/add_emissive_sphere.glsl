#version 430 core

#define PI 3.14159265359

uniform ivec3 sdfSize;
uniform vec3 camPos;
uniform mat3 camRot;


vec4 decode_color(uint color){
    return vec4(
        float((color >> 16) & 0xFFu),
        float((color >> 8) & 0xFFu),
        float(color & 0xFFu),
        float((color >> 24) & 0xFFu)
    ) / 255.0;
}

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

    ivec3 voxel_pos = (ivec3(origin) >> 2) << 2;
    vec3 inv_dir = 1.0 / dir;
    vec3 pos = origin;
    ivec3 step_dir = ivec3(
        int(sign(dir.x)),
        int(sign(dir.y)),
        int(sign(dir.z))
    );
    vec3 step_dir_f = vec3(step_dir);
    int scale = 4;
    ivec3 side_offset = ivec3(step(0.0, dir));
    ivec3 block4 = voxel_pos/4;
    ivec3 sidePos = voxel_pos + side_offset * scale;
    vec3 side_dist = (sidePos - pos) * inv_dir;
    ivec3 sdfSize4 = (sdfSize + 3) / 4;

    BrickmapNode node;

    for(int i=0; i < 1024; ++i){
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) return ret;
        if(scale == 4){
            block4 = voxel_pos/4;
            uint brick_idx = block4.z * sdfSize4.y * sdfSize4.x + block4.y * sdfSize4.x + block4.x;
            node = brickmap_data[brick_idx];
            if((node.mask_low | node.mask_high) != 0){
                scale = 1;
                voxel_pos = ivec3(pos);
                continue;
            }
        }else{
            if(voxel_pos/4 != block4){
                scale = 4;
                voxel_pos = (voxel_pos >> 2) << 2;
                continue;
            }
            uint local_x = voxel_pos.x % 4;
            uint local_y = voxel_pos.y % 4;
            uint local_z = voxel_pos.z % 4;
            uint bit_index = local_z * 16 + local_y * 4 + local_x;
            bool bit_set = (bit_index < 32) ? ((node.mask_low >> bit_index) & 1u) == 1u : ((node.mask_high >> (bit_index - 32)) & 1u) == 1u;
            if(bit_set){
                uint data_index = node.offset + bit_index;
                uint color = voxel_data[data_index];
                ret.didHit = true;
                ret.color = decode_color(color);
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
    ivec3 sdfSize4 = (sdfSize + 3) / 4;
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
                        uint brick_idx = pos.z/4 * sdfSize4.y * sdfSize4.x + pos.y/4 * sdfSize4.x + pos.x/4;
                        uint local_x = pos.x % 4;
                        uint local_y = pos.y % 4;
                        uint local_z = pos.z % 4;
                        uint bit_index = local_z * 16 + local_y * 4 + local_x;
                        bool bit_set = (bit_index < 32) ? ((node.mask_low >> bit_index) & 1u) == 1u : ((node.mask_high >> (bit_index - 32)) & 1u) == 1u;
                        if(brickmap_data[brick_idx].offset == 0xFFFFFFFFu){
                            
                        }
                        
                        imageStore(sdfData, pos, uvec4(255, 255, 255, 255));
                        imageStore(sdfData8, pos/8, uvec4(1, 1, 1, 1));
                    }
                }
            }
        }
    }
}

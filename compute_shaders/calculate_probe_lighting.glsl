#version 430 core

#define PI 3.14159265359

uniform ivec3 gi_probe_size;
uniform ivec3 sdfSize;

struct GIProbe{
    vec4 light[6];
};

layout(std430, binding = 0) writeonly buffer GIProbeData{
    GIProbe gi_data[];
};

layout(std430, binding = 1) readonly buffer GIProbeData2{
    GIProbe gi_data2[];
};

struct BrickmapNode{
    uint mask_low;
    uint mask_high;
    uint offset;
};

layout(std430, binding = 2) readonly buffer Brickmap{
    BrickmapNode brickmap_data[];
};

layout(std430, binding = 3) readonly buffer Voxel{
    uint voxel_data[];
};


vec4 decode_color(uint color){
    return vec4(
        float((color >> 16) & 0xFFu),
        float((color >> 8) & 0xFFu),
        float(color & 0xFFu),
        float((color >> 24) & 0xFFu)
    ) / 255.0;
}

uint rng_state;
float random(){
    rng_state = 1664525U * rng_state + 1013904223U;
    return float(rng_state)/4294967295.;
}

vec3 randomHemisphereVector(vec3 normal){
    float rand = random();
    float theta = 2. * PI * rand;
    rand = random();
    float phi = 2. * PI * rand;

    float x = sin(theta) * cos(phi);
    float y = sin(theta) * sin(phi);
    float z = cos(theta);

    vec3 randomVec = vec3(x, y, z);

    if(dot(randomVec, normal) < 0.0) randomVec = -randomVec;

    return randomVec;
}

struct HitData{
    bool didHit;
    vec4 color;
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
    int scale = 4;
    ivec3 side_offset = ivec3(step(0.0, dir));
    ivec3 block4 = voxel_pos/4;
    ivec3 sidePos = voxel_pos + side_offset * scale;
    vec3 side_dist = (sidePos - pos) * inv_dir;
    ivec3 sdfSize4 = (sdfSize + 3) / 4;

    BrickmapNode node;

    for(int i=0; i < 2048; ++i){
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) return ret;
        if(scale == 4){
            block4 = voxel_pos/4;
            uint brick_idx = block4.z * sdfSize4.y * sdfSize4.x + block4.y * sdfSize4.x + block4.x;
            node = brickmap_data[brick_idx];
            if((node.mask_low | node.mask_high) != 0){
                scale = 1;
                ivec3 brickBase = block4 * 4;
                vec3 brickBaseF = vec3(brickBase);
                vec3 localF = pos - brickBaseF;
                ivec3 localI = ivec3(floor(localF));
                localI = clamp(localI, ivec3(0), ivec3(3));
                voxel_pos = brickBase + localI;
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
            normal = vec3(-step_dir.x, 0, 0);
            pos.x = sidePos.x;
            pos.y += tmin * dir.y;
            pos.z += tmin * dir.z;
        }else if(side_dist.y < side_dist.z){
            tmin = side_dist.y;
            voxel_pos.y += step_dir.y * scale;
            normal = vec3(0, -step_dir.y, 0);
            pos.x += tmin * dir.x;
            pos.y = sidePos.y;
            pos.z += tmin * dir.z;
        }else{
            tmin = side_dist.z;
            voxel_pos.z += step_dir.z * scale;
            normal = vec3(0, 0, -step_dir.z);
            pos.x += tmin * dir.x;
            pos.y += tmin * dir.y;
            pos.z = sidePos.z;
        }
    }
    return ret;
}

HitData traceMax(vec3 origin, vec3 dir, float max_distance){
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
    int scale = 4;
    ivec3 side_offset = ivec3(step(0.0, dir));
    ivec3 block4 = voxel_pos/4;
    ivec3 sidePos = voxel_pos + side_offset * scale;
    vec3 side_dist = (sidePos - pos) * inv_dir;
    ivec3 sdfSize4 = (sdfSize + 3) / 4;

    BrickmapNode node;

    for(int i=0; i < 2048; ++i){
        if(distance(pos, origin) > max_distance) return ret;
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) return ret;
        if(scale == 4){
            block4 = voxel_pos/4;
            uint brick_idx = block4.z * sdfSize4.y * sdfSize4.x + block4.y * sdfSize4.x + block4.x;
            node = brickmap_data[brick_idx];
            if((node.mask_low | node.mask_high) != 0){
                scale = 1;
                ivec3 brickBase = block4 * 4;
                vec3 brickBaseF = vec3(brickBase);
                vec3 localF = pos - brickBaseF;
                ivec3 localI = ivec3(floor(localF));
                localI = clamp(localI, ivec3(0), ivec3(3));
                voxel_pos = brickBase + localI;
                continue;
            }
        }else{
            if(distance(pos, origin) > max_distance) return ret;
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
            normal = vec3(-step_dir.x, 0, 0);
            pos.x = sidePos.x;
            pos.y += tmin * dir.y;
            pos.z += tmin * dir.z;
        }else if(side_dist.y < side_dist.z){
            tmin = side_dist.y;
            voxel_pos.y += step_dir.y * scale;
            normal = vec3(0, -step_dir.y, 0);
            pos.x += tmin * dir.x;
            pos.y = sidePos.y;
            pos.z += tmin * dir.z;
        }else{
            tmin = side_dist.z;
            voxel_pos.z += step_dir.z * scale;
            normal = vec3(0, 0, -step_dir.z);
            pos.x += tmin * dir.x;
            pos.y += tmin * dir.y;
            pos.z = sidePos.z;
        }
    }
    return ret;
}

#define GI_SAMPLES 128
const vec3 directions[6] = vec3[6](
    vec3(1, 0, 0),
    vec3(-1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, -1, 0),
    vec3(0, 0, 1),
    vec3(0, 0, -1)
);

const vec3 sky_dir = normalize(vec3(0.25, 1.0, 0.1));
const vec3 sky_color = vec3(0.53, 0.8, 0.92);

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
void main(){
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= gi_probe_size.x*gi_probe_size.y*gi_probe_size.z) return;

    rng_state = idx;
    ivec3 probe_coord = ivec3(
        idx % gi_probe_size.x,
        (idx / gi_probe_size.x) % gi_probe_size.y,
        idx / (gi_probe_size.x * gi_probe_size.y)
    );
    vec3 position = (vec3(probe_coord) + 0.5) * vec3(sdfSize) / vec3(gi_probe_size);

    for(int d=0; d < 6; ++d){
        vec3 dir = directions[d];
        vec3 indirect_light = vec3(0);
        gi_data[idx].light[d] = vec4(0);
        for(int i=0; i < GI_SAMPLES; ++i){
            vec3 sample_direction = randomHemisphereVector(dir);
            HitData gi_sample_data = trace(position, sample_direction);
            if(gi_sample_data.didHit){

                if(gi_sample_data.color.a > 0.9){
                    indirect_light += vec3(1);
                    continue;
                }

                vec3 indirect_light_2 = vec3(0);
                vec3 gi_grid_pos = (gi_sample_data.position / vec3(sdfSize)) * vec3(gi_probe_size) - 0.5;
                ivec3 base = ivec3(floor(gi_grid_pos));
                vec3 frac = gi_grid_pos - vec3(base);
                float total_weight = 0;
                for(int z=0; z <= 1; ++z)
                for(int y=0; y <= 1; ++y)
                for(int x=0; x <= 1; ++x){
                    ivec3 offset = ivec3(x, y, z);
                    ivec3 probe_pos_grid = base + offset;

                    if(any(lessThan(probe_pos_grid, ivec3(0))) || any(greaterThanEqual(probe_pos_grid, gi_probe_size))) continue;

                    float wx = (x == 0) ? 1.0 - frac.x : frac.x;
                    float wy = (y == 0) ? 1.0 - frac.y : frac.y;
                    float wz = (z == 0) ? 1.0 - frac.z : frac.z;
                    float weight = wx * wy * wz;

                    vec3 world_pos = (vec3(probe_pos_grid) + 0.5) * vec3(sdfSize) / vec3(gi_probe_size);
                    HitData hit_data = traceMax(gi_sample_data.position + gi_sample_data.normal * 0.0001, normalize(world_pos - gi_sample_data.position), distance(world_pos, gi_sample_data.position));
                    if(hit_data.didHit) continue;

                    uint probe_idx = probe_pos_grid.z * gi_probe_size.y * gi_probe_size.x + probe_pos_grid.y * gi_probe_size.x + probe_pos_grid.x;
                    total_weight += weight;

                    for(int d=0; d < 6; ++d){
                        float n_dot_dir = dot(gi_sample_data.normal, directions[d]);
                        indirect_light_2 += max(gi_data2[probe_idx].light[d].rgb * n_dot_dir * weight, vec3(0));
                    }
                }
                if(total_weight > 0.0) indirect_light_2 /= total_weight;
                indirect_light += indirect_light_2 * 0.8;   //TODO Faktor zum "ausklingen"?

                HitData gi_sample_direct_light_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.0001, sky_dir);
                if(gi_sample_direct_light_data.didHit == false) indirect_light += gi_sample_data.color.rgb * 2 * dot(dir, sample_direction);
            }else{
                indirect_light += sky_color * 2 * dot(dir, sample_direction);
            }
        }
        gi_data[idx].light[d].rgb = indirect_light / float(GI_SAMPLES);
    }
}

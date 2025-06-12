#version 430 core

#define PI 3.14159265359

uniform ivec3 gi_probe_size;
uniform sampler3D sdfData;
uniform sampler3D sdfData4;
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

    for(int i=0; i < 1024; ++i){
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) return ret;
        if(scale == 4){
            block4 = voxel_pos/4;
            float sdf_data = texelFetch(sdfData4, block4, 0).r;
            if(sdf_data > 0.1){
                scale = 1;
                voxel_pos = ivec3(pos);
                continue;
            }
        }else{
            if(block4 != voxel_pos/4){
                scale = 4;
                voxel_pos = (voxel_pos>>2)<<2;
                continue;
            }
            vec4 sdf_data = texelFetch(sdfData, voxel_pos, 0);
            if(sdf_data.a > 0.1){
                ret.didHit = true;
                ret.color = sdf_data.rgb;
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
    vec3 step_dir_f = vec3(step_dir);
    int scale = 4;
    ivec3 side_offset = ivec3(step(0.0, dir));
    ivec3 block4 = voxel_pos/4;
    ivec3 sidePos = voxel_pos + side_offset * scale;
    vec3 side_dist = (sidePos - pos) * inv_dir;

    for(int i=0; i < 1024; ++i){
        if(distance(pos, origin) > max_distance) return ret;
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) return ret;
        if(scale == 4){
            block4 = voxel_pos/4;
            float sdf_data = texelFetch(sdfData4, block4, 0).r;
            if(sdf_data > 0.1){
                scale = 1;
                voxel_pos = ivec3(pos);
                continue;
            }
        }else{
            if(distance(pos, origin) > max_distance) return ret;
            if(block4 != voxel_pos/4){
                scale = 4;
                voxel_pos = (voxel_pos>>2)<<2;
                continue;
            }
            vec4 sdf_data = texelFetch(sdfData, voxel_pos, 0);
            if(sdf_data.a > 0.1){
                ret.didHit = true;
                ret.color = sdf_data.rgb;
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
                    HitData hit_data = traceMax(gi_sample_data.position + gi_sample_data.normal * 0.01, normalize(world_pos - gi_sample_data.position), distance(world_pos, gi_sample_data.position));
                    if(hit_data.didHit) continue;

                    uint probe_idx = probe_pos_grid.z * gi_probe_size.y * gi_probe_size.x + probe_pos_grid.y * gi_probe_size.x + probe_pos_grid.x;
                    total_weight += weight;

                    for(int d=0; d < 6; ++d){
                        float n_dot_dir = max(dot(gi_sample_data.normal, directions[d]), 0.0);
                        indirect_light_2 += gi_data2[probe_idx].light[d].rgb * n_dot_dir * weight * 0.8;
                    }
                }
                if(total_weight > 0.0) indirect_light_2 /= total_weight;
                indirect_light += indirect_light_2 * 0.95;   //TODO Faktor zum "ausklingen"?

                HitData gi_sample_direct_light_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.01, sky_dir);
                if(gi_sample_direct_light_data.didHit == false) indirect_light += gi_sample_data.color * 2 * dot(dir, sample_direction);
            }else{
                indirect_light += sky_color * 2 * dot(dir, sample_direction);
            }
        }
        gi_data[idx].light[d].rgb = indirect_light / float(GI_SAMPLES);
    }
}

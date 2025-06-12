#version 430 core

#define PI 3.14159265359

uniform ivec3 gi_probe_size;
uniform sampler3D sdfData;
uniform sampler3D sdfData4;
uniform ivec3 sdfSize;

struct GIProbe{
    vec4 light[6];
};

layout(std430, binding = 0) buffer GIProbeData{
    GIProbe gi_data[];
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

#define GI_SAMPLES 64
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
                HitData gi_sample_direct_light_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.01, sky_dir);
                if(gi_sample_direct_light_data.didHit == false) indirect_light += gi_sample_data.color * dot(dir, sample_direction);
            }else{
                indirect_light += sky_color * 2 * dot(dir, sample_direction);
            }
        }
        gi_data[idx].light[d].rgb = indirect_light / float(GI_SAMPLES);
    }
}

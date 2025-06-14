#version 430 core

precision highp float;

#define PI 3.14159265359

in vec4 gl_FragCoord;

uniform int gi_enabled;
uniform int gi_second_bounce;
uniform vec3 camPos;
uniform mat3 camRot;
layout(binding = 1, rgba8ui) uniform uimage3D sdfData;
layout(binding = 2, r8ui) uniform uimage3D sdfData8;
layout(location = 0) out vec3 albedo;
layout(location = 1) out vec4 lighting;
uniform vec2 windowSize;
uniform ivec3 sdfSize;
uniform ivec3 gi_probe_size;

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
    vec4 color;
    vec3 position;
    vec3 normal;
};

//TODO Ist schon viel besser wie zuvor, aber approximiert noch immer bei Übergängen der LODs

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
                ret.color = vec4(sdf_data)/255.0;
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
        if(distance(pos, origin) > max_distance) return ret;
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
            if(distance(pos, origin) > max_distance) return ret;
            if(block8 != voxel_pos/8){
                scale = 8;
                voxel_pos = (voxel_pos >> 3) << 3;
                continue;
            }
            uvec4 sdf_data = imageLoad(sdfData, voxel_pos);
            if(sdf_data.a > 0){
                ret.didHit = true;
                ret.color = vec4(sdf_data)/255.0;
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

#define GI_SAMPLES 8
#define GI_SAMPLES2 8

const vec3 sky_dir = normalize(vec3(0.25, 1.0, 0.1));
const vec3 sky_color = vec3(0.53, 0.8, 0.92);
float ambient = 0.005;

vec3 direct_light = vec3(0);
vec3 indirect_light = vec3(0);

const vec3 directions[6] = vec3[6](
    vec3(1, 0, 0),
    vec3(-1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, -1, 0),
    vec3(0, 0, 1),
    vec3(0, 0, -1)
);

void main(){
    float aspect = float(windowSize.x)/float(windowSize.y);
    vec2 uv = gl_FragCoord.xy/windowSize*2.0-vec2(1.0);
    uv.x *= aspect;
    
    //Raycast in die Szene
    HitData primary_hit_data = trace(camPos, camRot * normalize(vec3(uv, 1.3)));
    if(primary_hit_data.didHit){

        //Direktes Licht
        HitData direct_light_data = trace(primary_hit_data.position + primary_hit_data.normal * 0.01, sky_dir);
        if(direct_light_data.didHit == false) direct_light = primary_hit_data.color.rgb * max(dot(primary_hit_data.normal, sky_dir), 0);

        rng_state = uint(fract(sin(dot(gl_FragCoord.xy/vec2(1920, 1080), vec2(12.9898, 78.233))) * 43758.5453123) * 1000.0);

        //GI
        //BRDF und PDF und NdotL werden vernächlässigt, da:
        //die Verteilung cosinus weighted ist -> daher kein NdotL nötig
        //BRDF = albedo/PI
        //PDF = 1/PI
        //BRDF/PDF = albedo * PI/PI = albedo
        if(gi_enabled > 0){
            for(int i=0; i < GI_SAMPLES; ++i){
                vec3 sample_direction = normalize(primary_hit_data.normal + normalize(vec3(random()*2-1, random()*2-1, random()*2-1)));
                HitData gi_sample_data = trace(primary_hit_data.position + primary_hit_data.normal * 0.01, sample_direction);
                if(gi_sample_data.didHit){

                    if(gi_sample_data.color.a > 0.9){
                        indirect_light += vec3(1);
                        continue;
                    }
                    if(gi_second_bounce > 0){
                        vec3 indirect_light_2 = vec3(0);
                        for(int i=0; i < GI_SAMPLES2; ++i){
                            vec3 sample_direction = normalize(gi_sample_data.normal + normalize(vec3(random()*2-1, random()*2-1, random()*2-1)));
                            HitData gi_2_sample_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.01, sample_direction);
                            if(gi_2_sample_data.didHit){
                                if(gi_2_sample_data.color.a > 0.9){
                                    indirect_light_2 += vec3(1);
                                    continue;
                                }
                                HitData gi_sample_2_direct_light_data = trace(gi_2_sample_data.position + gi_2_sample_data.normal * 0.01, sky_dir);
                                if(gi_sample_2_direct_light_data.didHit == false) indirect_light_2 += gi_2_sample_data.color.rgb;
                            }else{
                                indirect_light_2 += sky_color;
                            }
                        }
                        indirect_light += indirect_light_2 / float(GI_SAMPLES2);
                    }

                    HitData gi_sample_direct_light_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.01, sky_dir);
                    if(gi_sample_direct_light_data.didHit == false) indirect_light += gi_sample_data.color.rgb;
                }else{
                    indirect_light += sky_color;
                }
            }
            indirect_light *= primary_hit_data.color.rgb;
            indirect_light /= float(GI_SAMPLES);
        }else{
            vec3 gi_grid_pos = (primary_hit_data.position / vec3(sdfSize)) * vec3(gi_probe_size) - 0.5;
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
                HitData hit_data = traceMax(primary_hit_data.position + primary_hit_data.normal * 0.01, normalize(world_pos - primary_hit_data.position), distance(world_pos, primary_hit_data.position));
                if(hit_data.didHit) continue;

                uint probe_idx = probe_pos_grid.z * gi_probe_size.y * gi_probe_size.x + probe_pos_grid.y * gi_probe_size.x + probe_pos_grid.x;
                total_weight += weight;

                for(int d=0; d < 6; ++d){
                    float n_dot_dir = max(dot(primary_hit_data.normal, directions[d]), 0.0);
                    indirect_light += gi_data[probe_idx].light[d].rgb * n_dot_dir * weight;
                }
            }
            if(total_weight > 0.0) indirect_light /= total_weight;
        }

        // lighting.rgb = direct_light;
        // lighting.rgb = indirect_light;
        lighting.rgb = direct_light + indirect_light * primary_hit_data.color.rgb + primary_hit_data.color.rgb * ambient;
        // lighting.rgb = primary_hit_data.color;
        return;
    }
    lighting.rgb = sky_color;
}

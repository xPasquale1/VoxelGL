#version 330 core

precision highp float;

#define PI 3.14159265359

in vec4 gl_FragCoord;

uniform int gi_enabled;
uniform vec3 camPos;
uniform mat3 camRot;
uniform sampler3D sdfData;
uniform sampler3D sdfData4;
uniform sampler3D sdfData8;
layout(location = 0) out vec3 albedo;
layout(location = 1) out vec4 lighting;
uniform vec2 windowSize;
uniform ivec3 sdfSize;

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

vec3 cosineSampleHemisphere(vec3 normal){
    float u1 = random();
    float u2 = random();
    
    float r = sqrt(u1);
    float theta = 2.0 * PI * u2;

    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0 - u1);

    vec3 tangent = normalize(cross(normal, abs(normal.x) > 0.1 ? vec3(0,1,0) : vec3(1,0,0)));
    vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * x + bitangent * y + normal * z);
}

struct HitData{
    bool didHit;
    vec3 color;
    vec3 position;
    vec3 normal;
};

//TODO Ist schon viel besser wie zuvor, aber approximiert noch immer bei Übergängen der LODs

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

#define GI_SAMPLES 8

const vec3 sky_dir = normalize(vec3(0.25, 1.0, 0.1));
const vec3 sky_color = vec3(0.53, 0.8, 0.92);
const float ambient = 0.005;

vec3 direct_light = vec3(0);
vec3 indirect_light = vec3(0);

void main(){
    float aspect = float(windowSize.x)/float(windowSize.y);
    vec2 uv = gl_FragCoord.xy/windowSize*2.0-vec2(1.0);
    uv.x *= aspect;
    
    //Raycast in die Szene
    HitData primary_hit_data = trace(camPos, camRot * normalize(vec3(uv, 1.5)));
    if(primary_hit_data.didHit){

        //Direktes Licht
        HitData direct_light_data = trace(primary_hit_data.position + primary_hit_data.normal * 0.01, sky_dir);
        if(direct_light_data.didHit == false) direct_light = primary_hit_data.color * max(dot(primary_hit_data.normal, sky_dir), 0);

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
                    HitData gi_sample_direct_light_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.01, sky_dir);
                    if(gi_sample_direct_light_data.didHit == false) indirect_light += gi_sample_data.color;
                }else{
                    indirect_light += sky_color;
                }
            }
            indirect_light *= primary_hit_data.color;
            indirect_light /= float(GI_SAMPLES);
        }

        // lighting.rgb = direct_light;
        // lighting.rgb = indirect_light;
        lighting.rgb = direct_light + indirect_light + primary_hit_data.color * ambient;
        // lighting.rgb = primary_hit_data.color;
        return;
    }
    lighting.rgb = sky_color;
}

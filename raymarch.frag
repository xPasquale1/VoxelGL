#version 330 core

precision highp float;

#define PI 3.14159265359

in vec4 gl_FragCoord;

uniform vec3 camPos;
uniform mat3 camRot;
uniform sampler3D sdfData;
uniform sampler3D sdfData4;
uniform sampler3D sdfData8;
layout(location = 0) out vec3 albedo;
layout(location = 1) out vec4 lighting;
uniform vec2 windowSize;
uniform ivec3 sdfSize;

//TODO Der ganze Code hat so viele "Fixes"...
//Man sollte nicht per 0.0005 in den nächsten Voxel steppen, sondern einfach die Position zusätzlich als Mittelpunkt im Voxel speichern

vec3 intersect(vec3 position, vec3 dir, out vec3 normal, float scale){
    float eps = 0.0001;
    if(dir.x < 0.0 && abs(fract(position.x)) < eps) position.x -= eps;
    if(dir.y < 0.0 && abs(fract(position.y)) < eps) position.y -= eps;
    if(dir.z < 0.0 && abs(fract(position.z)) < eps) position.z -= eps;

    vec3 minBox = floor(position/scale)*scale;
    vec3 maxBox = minBox + vec3(scale);
    vec3 tv;

    if(dir.x < 0.0) tv.x = (minBox.x-position.x)/dir.x;
    else tv.x = (maxBox.x-position.x)/dir.x;

    if(dir.y < 0.0) tv.y = (minBox.y-position.y)/dir.y;
    else tv.y = (maxBox.y-position.y)/dir.y;

    if(dir.z < 0.0) tv.z = (minBox.z-position.z)/dir.z;
    else tv.z = (maxBox.z-position.z)/dir.z;
    
    normal = vec3(-sign(dir.x), 0, 0);
    float t = tv.x;
    if(tv.y < t){
        t = tv.y;
        normal = vec3(0, -sign(dir.y), 0);
    }
    if(tv.z < t){
        t = tv.z;
        normal = vec3(0, 0, -sign(dir.z));
    }
    return position + dir*(t+0.0005);
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

HitData trace(vec3 position, vec3 dir, float maxLength){
    vec3 startPos = position;
    vec3 normal = vec3(0);
    HitData ret;
    ret.didHit = false;
    for(int i=0; i < 512; ++i){
        if(distance(startPos, position) > maxLength) return ret;
        if(any(lessThan(position, vec3(0))) || any(greaterThanEqual(position, vec3(sdfSize)))) return ret;
        ivec3 lowresBlock8 = ivec3(position/8);
        vec4 sdfInfo8 = texelFetch(sdfData8, lowresBlock8, 0);
        if(sdfInfo8.r > 0.1){
            for(int j=0; j < 64; ++j){
                if(distance(startPos, position) > maxLength) return ret;
                if(ivec3(position/8) != lowresBlock8) break;
                ivec3 lowresBlock4 = ivec3(position/4);
                vec4 sdfInfo4 = texelFetch(sdfData4, lowresBlock4, 0);
                if(sdfInfo4.r > 0.1){
                    for(int k=0; k < 16; k++){
                        if(distance(startPos, position) > maxLength) return ret;
                        if(ivec3(position/4) != lowresBlock4) break;
                        vec4 sdfInfo = texelFetch(sdfData, ivec3(position), 0);
                        if(sdfInfo.a > 0.1){
                            ret.didHit = true;
                            ret.color = sdfInfo.rgb;
                            ret.position = position;
                            ret.normal = normal;
                            return ret;
                        }
                        position = intersect(position, dir, normal, 1.0);
                    }
                    continue;
                }
                position = intersect(position, dir, normal, 4.0);
            }
            continue;
        }
        position = intersect(position, dir, normal, 8.0);
    }
    return ret;
}

#define GI_SAMPLES 16

const vec3 sky_dir = normalize(vec3(0.25, 1.0, 0.1));
const vec3 sky_color = vec3(0.53, 0.8, 0.92);

vec3 direct_light = vec3(0);
vec3 indirect_light = vec3(0);

void main(){
    float aspect = float(windowSize.x)/float(windowSize.y);
    vec2 uv = gl_FragCoord.xy/windowSize*2.0-vec2(1.0);
    uv.x *= aspect;
    
    //Raycast in die Szene
    HitData primary_hit_data = trace(camPos, camRot * normalize(vec3(uv, 1.5)), 10000);
    if(primary_hit_data.didHit){

        //Direktes Licht
        HitData direct_light_data = trace(primary_hit_data.position + primary_hit_data.normal * 0.01, sky_dir, 10000);
        if(direct_light_data.didHit == false) direct_light = primary_hit_data.color * max(dot(primary_hit_data.normal, sky_dir), 0);

        rng_state = uint(fract(sin(dot(gl_FragCoord.xy/vec2(1920, 1080), vec2(12.9898, 78.233))) * 43758.5453123) * 1000.0);

        //GI
        //BRDF und PDF und NdotL werden vernächlässigt, da:
        //die Verteilung cosinus weighted ist -> daher kein NdotL nötig
        //BRDF = albedo/PI
        //PDF = 1/PI
        //BRDF/PDF = albedo * PI/PI = albedo
        for(int i=0; i < GI_SAMPLES; ++i){
            vec3 sample_direction = normalize(primary_hit_data.normal + normalize(vec3(random()*2-1, random()*2-1, random()*2-1)));
            HitData gi_sample_data = trace(primary_hit_data.position + primary_hit_data.normal * 0.01, sample_direction, 10000);
            if(gi_sample_data.didHit){
                HitData gi_sample_direct_light_data = trace(gi_sample_data.position + gi_sample_data.normal * 0.01, sky_dir, 10000);
                if(gi_sample_direct_light_data.didHit == false) indirect_light += gi_sample_data.color;
            }else{
                indirect_light += sky_color;
            }
        }
        indirect_light *= primary_hit_data.color;
        indirect_light /= float(GI_SAMPLES);

        // lighting.rgb = direct_light;
        // lighting.rgb = indirect_light;
        lighting.rgb = direct_light + indirect_light;
        return;
    }
    albedo = sky_color;
}

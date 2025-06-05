#version 330 core

precision highp float;

#define PI 3.14159265359

in vec4 gl_FragCoord;

uniform vec3 camPos;
uniform mat3 camRot;
uniform sampler3D sdfData;
uniform sampler3D sdfDataLow;
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

bool trace(vec3 position, vec3 dir, out vec3 hitPos, out vec3 normal, float maxLength){
    vec3 startPos = position;
    for(int i=0; i < 512; ++i){
        if(distance(startPos, position) > maxLength) return false;
        if(any(lessThan(position, vec3(0))) || any(greaterThanEqual(position, vec3(sdfSize)))) return false;
        ivec3 lowresBlock = ivec3(position/4);
        vec4 sdfInfo = texelFetch(sdfDataLow, lowresBlock, 0);
        if(sdfInfo.r > 0.1){
            for(int j=0; j < 16; j++){
                if(distance(startPos, position) > maxLength) return false;
                if(ivec3(position/4) != lowresBlock) break;
                sdfInfo = texelFetch(sdfData, ivec3(position), 0);
                if(sdfInfo.a > 0.1){
                    hitPos = position;
                    return true;
                }
                position = intersect(position, dir, normal, 1.0);
            }
            continue;
        }
        position = intersect(position, dir, normal, 4.0);
    }
    return false;
}

#define GI_SAMPLES 16
#define GI_BOUNCES 2

void main(){
    float aspect = float(windowSize.x)/float(windowSize.y);
    vec2 uv = gl_FragCoord.xy/windowSize*2.0-vec2(1.0);
    uv.x *= aspect;

    vec3 position = camPos;
    vec3 dir = normalize(vec3(uv, 1.5));
    dir = camRot * dir;
    
    vec3 hitPos;
    vec3 normal;
    if(trace(position, dir, hitPos, normal, 10000)){
        albedo = texelFetch(sdfData, ivec3(hitPos), 0).rgb;

        vec3 reflPos = hitPos;
        vec3 reflDir = dir;
        vec3 reflNormal = normal;
        position = hitPos;
        dir = normalize(vec3(0.25, 1.0, 0.1));
        position += normal * 0.01;
        vec3 direct_light = vec3(0);
        if(trace(position, dir, hitPos, normal, 10000)) direct_light = albedo * 0.0;
        else direct_light = albedo * dot(normal, reflDir);

        rng_state = uint(fract(sin(dot(gl_FragCoord.xy/vec2(1920, 1080), vec2(12.9898, 78.233))) * 43758.5453123) * 1000.0);

        vec3 indirect_light = vec3(0);
        float irradiance = 0;
        for(int i=0; i < GI_SAMPLES; ++i){
            dir = randomHemisphereVector(reflNormal);
            position = reflPos + reflNormal * 0.01;
            if(trace(position, dir, hitPos, normal, 10000)){
                position = hitPos + normal * 0.01;
                dir = normalize(vec3(0.25, 1.0, 0.1));
                if(!trace(position, dir, hitPos, normal, 10000)){
                    indirect_light += dot(dir, reflNormal) * texelFetch(sdfData, ivec3(hitPos), 0).rgb;
                }
            }
        }
        indirect_light *= 2.0 / float(GI_SAMPLES) * albedo;

        lighting.rgb = direct_light + indirect_light;
        return;
    }
    albedo = vec3(0, 0.3, 1.0);
}

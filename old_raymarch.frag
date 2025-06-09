// vec3 intersect(vec3 position, vec3 dir, out vec3 normal, float scale){
//     float eps = 0.0001;
//     if(dir.x < 0.0 && abs(fract(position.x)) < eps) position.x -= eps;
//     if(dir.y < 0.0 && abs(fract(position.y)) < eps) position.y -= eps;
//     if(dir.z < 0.0 && abs(fract(position.z)) < eps) position.z -= eps;

//     vec3 minBox = floor(position/scale)*scale;
//     vec3 maxBox = minBox + vec3(scale);
//     vec3 tv;

//     if(dir.x < 0.0) tv.x = (minBox.x-position.x)/dir.x;
//     else tv.x = (maxBox.x-position.x)/dir.x;

//     if(dir.y < 0.0) tv.y = (minBox.y-position.y)/dir.y;
//     else tv.y = (maxBox.y-position.y)/dir.y;

//     if(dir.z < 0.0) tv.z = (minBox.z-position.z)/dir.z;
//     else tv.z = (maxBox.z-position.z)/dir.z;
    
//     normal = vec3(-sign(dir.x), 0, 0);
//     float t = tv.x;
//     if(tv.y < t){
//         t = tv.y;
//         normal = vec3(0, -sign(dir.y), 0);
//     }
//     if(tv.z < t){
//         t = tv.z;
//         normal = vec3(0, 0, -sign(dir.z));
//     }
//     return position + dir*(t+0.0005);
// }

// HitData trace(vec3 position, vec3 dir, float maxLength){
//     vec3 startPos = position;
//     vec3 normal = vec3(0);
//     HitData ret;
//     ret.didHit = false;
//     for(int i=0; i < 256; ++i){
//         if(distance(startPos, position) > maxLength) return ret;
//         if(any(lessThan(position, vec3(0))) || any(greaterThanEqual(position, vec3(sdfSize)))) return ret;
//         ivec3 lowresBlock8 = ivec3(position/8);
//         vec4 sdfInfo8 = texelFetch(sdfData8, lowresBlock8, 0);
//         if(sdfInfo8.r > 0.1){
//             for(int j=0; j < 8; ++j){
//                 if(distance(startPos, position) > maxLength) return ret;
//                 if(ivec3(position/8) != lowresBlock8) break;
//                 ivec3 lowresBlock4 = ivec3(position/4);
//                 vec4 sdfInfo4 = texelFetch(sdfData4, lowresBlock4, 0);
//                 if(sdfInfo4.r > 0.1){
//                     for(int k=0; k < 8; k++){
//                         if(distance(startPos, position) > maxLength) return ret;
//                         if(ivec3(position/4) != lowresBlock4) break;
//                         vec4 sdfInfo = texelFetch(sdfData, ivec3(position), 0);
//                         if(sdfInfo.a > 0.1){
//                             ret.didHit = true;
//                             ret.color = sdfInfo.rgb;
//                             ret.position = position;
//                             ret.normal = normal;
//                             return ret;
//                         }
//                         sdf1_traces += 1;
//                         position = intersect(position, dir, normal, 1.0);
//                     }
//                     continue;
//                 }
//                 sdf4_traces += 1;
//                 position = intersect(position, dir, normal, 4.0);
//             }
//             continue;
//         }
//         sdf8_traces += 1;
//         position = intersect(position, dir, normal, 8.0);
//     }
//     return ret;
// }

HitData trace(vec3 origin, vec3 dir, float maxLength){
    HitData ret;
    ret.didHit = false;

    ivec3 voxel_pos = ivec3(floor(origin));
    ivec3 step = ivec3(sign(dir));
    vec3 inv_dir = 1.0 / dir;
    vec3 tDelta = abs(inv_dir);

    vec3 nextVoxelBoundary = vec3(voxel_pos) + vec3(
        step.x > 0 ? 1.0 : 0.0,
        step.y > 0 ? 1.0 : 0.0,
        step.z > 0 ? 1.0 : 0.0
    );

    vec3 tMax = (nextVoxelBoundary - origin) * inv_dir;

    for(int i = 0; i < 1024; ++i){
        if(any(lessThan(voxel_pos, ivec3(0))) || any(greaterThanEqual(voxel_pos, sdfSize))) break;
        vec4 sdf_data = texelFetch(sdfData4, voxel_pos, 0);
        if(sdf_data.a > 0.1){
            ret.didHit = true;
            ret.color = sdf_data.rgb;
            ret.position = origin + dir * min(min(tMax.x, tMax.y), tMax.z);
            return ret;
        }
        
        if(tMax.x < tMax.y){
            if(tMax.x < tMax.z){
                voxel_pos.x += step.x;
                tMax.x += tDelta.x;
            }else{
                voxel_pos.z += step.z;
                tMax.z += tDelta.z;
            }
        }else{
            if(tMax.y < tMax.z){
                voxel_pos.y += step.y;
                tMax.y += tDelta.y;
            }else{
                voxel_pos.z += step.z;
                tMax.z  += tDelta.z;
            }
        }
    }
    return ret;
}

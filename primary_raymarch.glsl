#version 430 core

struct VoxelHashmapEntry{
    uint idx_low;   //TODO Muss noch auf 64bit erweitert werden
    uint normal;    
};

layout(std430, binding = 0) buffer VoxelHashmap{
    VoxelHashmapEntry hashmap[];
};
uniform uint hashmap_size_low;

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

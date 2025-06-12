#pragma once

#include "util.h"

//TODO Hier kann man betimmt noch mehr templaten, vorallen die Vektoren, da man mit Templates aj auch cool Größen und so angeben kann

#define PI 3.14159265359

struct ivec2{
	int x;
	int y;
};

struct fvec2{
	float x;
	float y;
};

struct fvec3{
    float x;
    float y;
    float z;
};

struct fvec4{
	float x;
	float y;
	float z;
	float w;
};

constexpr float distance(ivec2& a, ivec2& b)noexcept{return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));}
constexpr float distance(float x1, float y1, float x2, float y2)noexcept{return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));}
constexpr float distance(const fvec2& a, const fvec2& b)noexcept{return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));}

constexpr float length(const fvec3& vec)noexcept{return sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);}
constexpr float length(const fvec2& vec)noexcept{return sqrt(vec.x*vec.x + vec.y*vec.y);}

#define MULTIPLIER 1664525
#define INCREMENT 1013904223
unsigned long _rand;
unsigned long nextrand()noexcept{
    _rand = (MULTIPLIER*_rand+INCREMENT);
    return _rand;
}

constexpr fvec3 normalize(const fvec3& vec)noexcept{
	const float invLength = 1.f/length(vec);
	return {vec.x*invLength, vec.y*invLength, vec.z*invLength};
}

constexpr fvec2 normalize(const fvec2& vec)noexcept{
	const float invLength = 1.f/length(vec);
	return {vec.x*invLength, vec.y*invLength};
}

constexpr float dot(const fvec3& a, const fvec3& b)noexcept{return (a.x * b.x + a.y * b.y + a.z * b.z);}
constexpr float dot(const fvec2& a, const fvec2& b)noexcept{return (a.x * b.x + a.y * b.y);}
constexpr float dot(fvec2& a, fvec2& b)noexcept{return (a.x * b.x + a.y * b.y);}
constexpr float cross(fvec2 a, fvec2 b)noexcept{return (a.x * b.y - a.y * b.x);}
constexpr float cross(fvec2& a, fvec2& b)noexcept{return (a.x * b.y - a.y * b.x);}
constexpr fvec3 cross(fvec3& a, fvec3& b)noexcept{return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};}

constexpr float deg2rad(float deg)noexcept{return deg*PI/180;}
constexpr float rad2deg(float rad)noexcept{return rad*180/PI;}

template <typename T>
constexpr T min(T a, T b)noexcept{
//	return a*(a<b)+b*(b<=a);
	return a < b ? a : b;
}
template <typename T>
constexpr T max(T a, T b)noexcept{
//	return a*(a>b)+b*(b>=a);
	return a > b ? a : b;
}

template <typename T>
constexpr T clamp(T val, T minVal, T maxVal)noexcept{
	if(val < minVal) return minVal;
	if(val > maxVal) return maxVal;
	return val;
}

//Interpoliere die Werte A und B linear für einen Punkt C
constexpr float interpolateLinear(float valA, float valB, fvec2 posA, fvec2 posB, fvec2 posC)noexcept{
	float valDiff = valB-valA;
	return valA + (valDiff*(posC.x-posA.x)+valDiff*(posC.y-posA.y))/((posB.x-posA.x)+(posB.y-posA.y));
}

constexpr float interpolateLinear(float start, float end, float t)noexcept{
    return start+(end-start)*t;
}

constexpr WORD interpolateLinear(WORD start, WORD end, float t)noexcept{
    return start+(end-start)*t;
}

constexpr SWORD sign(SWORD x)noexcept{return (x > 0) - (x < 0);}

//Testet ob die float Zahl negative ist und gibt entweder -1 oder 1 zurück
constexpr float negSign(const float val)noexcept{
	DWORD buffer = 0b00111111100000000000000000000000;	//Binäre Darstellung einer float 1
	buffer |= ((*(DWORD*)&val)&0b1000'0000'0000'0000'0000'0000'0000'0000);
	return *(float*)&buffer;
}

constexpr void mult3x3Mat(float* matA, float* matB, float* dstMat)noexcept{
	for(int row=0; row < 3; row++){
        for(int col=0; col < 3; col++){
            float sum = 0;
            for(int k=0; k < 3; k++){
                sum += matA[row * 3 + k] * matB[k * 3 + col];
            }
            dstMat[row * 3 + col] = sum;
        }
    }
}

constexpr void mult4x4Mat(float* matA, float* matB, float* dstMat)noexcept{
	for(int row=0; row < 4; row++){
        for(int col=0; col < 4; col++){
            float sum = 0;
            for(int k=0; k < 4; k++){
                sum += matA[row * 4 + k] * matB[k * 4 + col];
            }
            dstMat[row * 4 + col] = sum;
        }
    }
}

constexpr fvec3 lerp(const fvec3& a, const fvec3& b, const float t)noexcept{
	const fvec3 ba = {b.x - a.x, b.y - a.y, b.z - a.z};
	return {a.x + t*ba.x, a.y + t*ba.y, a.z + t*ba.z};
}

fvec3 closestPointOnLineSegment(const fvec3& point, const fvec3& start, const fvec3& end)noexcept{
	const fvec3 diff = {end.x - start.x, end.y - start.y, end.z - start.z};
	float t = dot(point, diff)/dot(diff, diff);
    return lerp(start, end, clamp(t, 0.f, 1.f));
}

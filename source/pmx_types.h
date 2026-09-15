/*
pmx_types.h

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMX ReaderとScene Loaderで共有する
PMXデータ構造を定義する。
*/

#ifndef GPT_MMD_TOOLS_PMX_TYPES_H__
#define GPT_MMD_TOOLS_PMX_TYPES_H__

#include "c4d.h"

#include <vector>

struct PMXVertex
{
	Vector position;
	Vector normal;
	Vector uv;

	UChar weightType;
	Int32 boneIndices[4];
	Float32 boneWeights[4];
	UChar weightCount;

	Vector sdefC;
	Vector sdefR0;
	Vector sdefR1;

	PMXVertex()
	{
		weightType = 0;
		weightCount = 0;

		for (Int32 i = 0; i < 4; ++i)
		{
			boneIndices[i] = -1;
			boneWeights[i] = 0.0f;
		}

		sdefC = Vector(0.0);
		sdefR0 = Vector(0.0);
		sdefR1 = Vector(0.0);
	}
};


// ============================================================
// PMX IK Link
// ============================================================

struct PMXIKLink
{
	Int32 boneIndex;
	Bool hasLimit;
	Vector minimum;
	Vector maximum;

	PMXIKLink()
	{
		boneIndex = -1;
		hasLimit = false;
		minimum = Vector(0.0);
		maximum = Vector(0.0);
	}
};


// ============================================================
// PMX Bone
// ============================================================

struct PMXBone
{
	String name;
	String nameUniversal;

	Vector position;
	Int32 parentIndex;
	Int32 deformLayer;
	UInt16 flags;

	Int32 tailBoneIndex;
	Vector tailPositionOffset;

	Int32 appendBoneIndex;
	Float32 appendWeight;

	Vector fixedAxis;
	Vector localXAxis;
	Vector localZAxis;

	Int32 externalParentKey;

	Int32 ikTargetBoneIndex;
	Int32 ikLoopCount;
	Float32 ikLoopAngleLimit;
	std::vector<PMXIKLink> ikLinks;

	PMXBone()
	{
		position = Vector(0.0);
		parentIndex = -1;
		deformLayer = 0;
		flags = 0;
		tailBoneIndex = -1;
		tailPositionOffset = Vector(0.0);
		appendBoneIndex = -1;
		appendWeight = 0.0f;
		fixedAxis = Vector(0.0);
		localXAxis = Vector(1.0, 0.0, 0.0);
		localZAxis = Vector(0.0, 0.0, 1.0);
		externalParentKey = 0;
		ikTargetBoneIndex = -1;
		ikLoopCount = 0;
		ikLoopAngleLimit = 0.0f;
	}
};


// ============================================================
// PMX Texture
// ============================================================

struct PMXTexture
{
	String path;
};


// ============================================================
// PMX Material
// ============================================================

struct PMXMaterial
{
	String name;
	String nameUniversal;

	Vector diffuse;
	Float32 diffuseAlpha;

	Vector specular;
	Float32 specularPower;

	Vector ambient;

	UChar drawFlags;

	Vector edgeColor;
	Float32 edgeAlpha;
	Float32 edgeSize;

	Int32 textureIndex;
	Int32 sphereTextureIndex;

	UChar sphereMode;

	UChar toonFlag;
	Int32 toonTextureIndex;

	String memo;

	Int32 faceVertexCount;
	Int32 polygonCount;

	Int32 polygonStart;
};


// ============================================================
// PMX Reader
// ============================================================

#endif

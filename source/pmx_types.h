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

struct PMXVertex
{
	Vector position;
	Vector normal;
	Vector uv;
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

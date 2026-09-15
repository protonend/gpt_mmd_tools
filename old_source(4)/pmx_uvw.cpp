/*
pmx_uvw.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMX UVW / Normal / Phong処理を担当する。

STEP 09：
PMXReader::CreateNormalTag()
PMXReader::CreateUVW()
PMXReader::CreateSmoothTag()
をPMXReader本体から分離。

既存のPMX処理内容・生成結果は変更しない。

Cinema 4D R19 / Visual Studio 2015
*/

#include "c4d.h"
#include "pmx_reader.h"
#include "pmx_uvw.h"

#include <vector>


// ============================================================
// Create Normal Tag
// ============================================================

Bool PMXReader::CreateNormalTag(
	PolygonObject* object,
	const std::vector<PMXVertex>& vertices,
	const std::vector<Int32>& indices
)
{
	const Int32 polygonCount =
		static_cast<Int32>(
			indices.size() / 3
			);


	NormalTag* normalTag =
		NormalTag::Alloc(
			polygonCount
		);


	if (!normalTag)
		return false;


	NormalHandle handle =
		normalTag->GetDataAddressW();


	if (!handle)
	{
		NormalTag::Free(normalTag);

		return false;
	}


	for (
		Int32 i = 0;
		i < polygonCount;
		++i
		)
	{
		const Int32 base =
			i * 3;


		const Vector& normalA =
			vertices[
				static_cast<size_t>(
					indices[
						static_cast<size_t>(base)
					]
					)
			].normal;


		const Vector& normalB =
			vertices[
				static_cast<size_t>(
					indices[
						static_cast<size_t>(base + 1)
					]
					)
			].normal;


		const Vector& normalC =
			vertices[
				static_cast<size_t>(
					indices[
						static_cast<size_t>(base + 2)
					]
					)
			].normal;


		NormalStruct normalData;


		normalData.a = normalA;
		normalData.b = normalB;
		normalData.c = normalC;
		normalData.d = normalC;


		NormalTag::Set(
			handle,
			i,
			normalData
		);
	}


	object->InsertTag(
		normalTag
	);


	GePrint(
		"PMX NORMAL TAG : CREATED"
	);


	return true;
}


// ============================================================
// Create UVW
// ============================================================

Bool PMXReader::CreateUVW(
	PolygonObject* object,
	const std::vector<PMXVertex>& vertices,
	const std::vector<Int32>& indices
)
{
	const Int32 polygonCount =
		static_cast<Int32>(
			indices.size() / 3
			);


	UVWTag* uvwTag =
		UVWTag::Alloc(
			polygonCount
		);


	if (!uvwTag)
		return false;


	for (
		Int32 i = 0;
		i < polygonCount;
		++i
		)
	{
		const Int32 base =
			i * 3;


		const Vector& uvA =
			vertices[
				static_cast<size_t>(
					indices[
						static_cast<size_t>(base)
					]
					)
			].uv;


		const Vector& uvB =
			vertices[
				static_cast<size_t>(
					indices[
						static_cast<size_t>(base + 1)
					]
					)
			].uv;


		const Vector& uvC =
			vertices[
				static_cast<size_t>(
					indices[
						static_cast<size_t>(base + 2)
					]
					)
			].uv;


		UVWStruct uvwData;


		uvwData.a = uvA;
		uvwData.b = uvB;
		uvwData.c = uvC;
		uvwData.d = uvC;


		uvwTag->SetSlow(
			i,
			uvwData
		);
	}


	object->InsertTag(
		uvwTag
	);


	GePrint(
		"PMX UVW : CREATED"
	);


	return true;
}


// ============================================================
// Create Smooth / Phong Tag
// ============================================================

Bool PMXReader::CreateSmoothTag(
	PolygonObject* object
)
{
	if (!object)
		return false;


	BaseTag* phongTag =
		object->MakeTag(
			Tphong
		);


	if (!phongTag)
	{
		GePrint(
			"PMX PHONG TAG : MAKE FAILED"
		);

		return false;
	}


	GePrint(
		"PMX PHONG TAG : CREATED"
	);


	return true;
}
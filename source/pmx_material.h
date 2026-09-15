 /*
pmx_material.h

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMXマテリアル関連処理をPMXReaderから分離する。

分離対象：
CreateC4DMaterial()
BuildTextureFilename()
BuildRelativeTextureFilename()
CreateBitmapShader()
CreateMaterialSelection()
CreateMaterialTag()
SetupMaterial()

Cinema 4D R19 / Visual Studio 2015
*/

#ifndef GPT_MMD_TOOLS_PMX_MATERIAL_H__
#define GPT_MMD_TOOLS_PMX_MATERIAL_H__

#include "c4d.h"
#include "pmx_types.h"

#include <vector>


class PMXReader
{
public:

	BaseMaterial* CreateC4DMaterial(
		const PMXMaterial& pmxMaterial,
		Int32 materialIndex
	);

	Filename BuildTextureFilename(
		const Filename& pmxFilename,
		const String& texturePath
	);

	Filename BuildRelativeTextureFilename(
		const String& texturePath
	);

	BaseShader* CreateBitmapShader(
		BaseMaterial* material,
		const Filename& absoluteTextureFile,
		const Filename& relativeTextureFile,
		const PMXMaterial& pmxMaterial
	);

	Bool CreateMaterialSelection(
		PolygonObject* object,
		const PMXMaterial& material,
		Int32 materialIndex,
		Int32 polygonOffset,
		String& selectionName
	);

	Bool CreateMaterialTag(
		PolygonObject* object,
		BaseMaterial* material,
		const PMXMaterial& pmxMaterial,
		Int32 materialIndex,
		Int32 polygonOffset
	);

	Bool SetupMaterial(
		PolygonObject* object,
		BaseDocument* doc,
		const Filename& filename,
		const std::vector<PMXTexture>& textures,
		const PMXMaterial& pmxMaterial,
		Int32 materialIndex,
		Int32 polygonOffset
	);
};


#endif
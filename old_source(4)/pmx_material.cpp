/*
pmx_material.cpp

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

STEP 10-B：
Material処理分離。

Cinema 4D R19 / Visual Studio 2015

注意：
PMXReaderのクラス宣言はpmx_reader.hのみ。
このファイルではPMXReaderを再宣言しない。
*/

#include "c4d.h"
#include "pmx_reader.h"

#include <vector>


// ============================================================
// Create C4D Material
// ============================================================

BaseMaterial* PMXReader::CreateC4DMaterial(
	const PMXMaterial& pmxMaterial,
	Int32 materialIndex
)
{
	BaseMaterial* material =
		BaseMaterial::Alloc(
			Mmaterial
		);


	if (!material)
		return nullptr;


	String materialName =
		pmxMaterial.name;


	if (materialName == String())
	{
		materialName =
			String("PMX Material ") +
			String::IntToString(materialIndex);
	}


	material->SetName(
		materialName
	);


	material->SetParameter(
		DescID(MATERIAL_COLOR_COLOR),
		GeData(pmxMaterial.diffuse),
		DESCFLAGS_SET_0
	);


	// ========================================================
	// Texture Preview Size
	// ========================================================

	material->SetParameter(
		DescID(MATERIAL_PREVIEWSIZE),
		GeData(MATERIAL_PREVIEWSIZE_NO_SCALE),
		DESCFLAGS_SET_0
	);


	GePrint(
		"PMX MATERIAL PREVIEW SIZE : NO SCALE"
	);


	GePrint(
		"PMX C4D MATERIAL : CREATED"
	);


	GePrint(
		"  MATERIAL INDEX = " +
		String::IntToString(materialIndex)
	);


	GePrint(
		"  MATERIAL NAME = " +
		materialName
	);


	return material;
}


// ============================================================
// Build Texture Filename
// ============================================================

Filename PMXReader::BuildTextureFilename(
	const Filename& pmxFilename,
	const String& texturePath
)
{
	Filename textureFile =
		pmxFilename.GetDirectory();


	textureFile +=
		texturePath;


	return textureFile;
}


// ============================================================
// Build Relative Texture Filename
// ============================================================

Filename PMXReader::BuildRelativeTextureFilename(
	const String& texturePath
)
{
	Filename relativeTextureFile;


	relativeTextureFile.SetString(
		texturePath
	);


	return relativeTextureFile;
}


// ============================================================
// Create Bitmap Shader
// ============================================================

BaseShader* PMXReader::CreateBitmapShader(
	BaseMaterial* material,
	const Filename& absoluteTextureFile,
	const Filename& relativeTextureFile,
	const PMXMaterial& pmxMaterial
)
{
	if (!material)
		return nullptr;


	if (!GeFExist(
		absoluteTextureFile,
		false
	))
	{
		GePrint(
			"PMX BITMAP : FILE NOT FOUND"
		);

		GePrint(
			"  ABSOLUTE PATH = " +
			absoluteTextureFile.GetString()
		);

		return nullptr;
	}


	// ========================================================
	// Texture Alpha Detection
	// ========================================================

	Bool textureHasAlpha = false;


	BaseBitmap* bitmap =
		BaseBitmap::Alloc();


	if (bitmap)
	{
		const IMAGERESULT imageResult =
			bitmap->Init(
				absoluteTextureFile
			);


		if (imageResult == IMAGERESULT_OK)
		{
			BaseBitmap* alphaChannel =
				bitmap->GetInternalChannel();


			if (alphaChannel)
			{
				textureHasAlpha = true;
			}
		}


		BaseBitmap::Free(
			bitmap
		);
	}


	if (textureHasAlpha)
	{
		GePrint(
			"PMX BITMAP ALPHA : FOUND"
		);
	}
	else
	{
		GePrint(
			"PMX BITMAP ALPHA : NOT FOUND"
		);
	}


	BaseShader* bitmapShader =
		BaseShader::Alloc(
			Xbitmap
		);


	if (!bitmapShader)
		return nullptr;


	if (!bitmapShader->SetParameter(
		DescID(BITMAPSHADER_FILENAME),
		GeData(relativeTextureFile),
		DESCFLAGS_SET_0
	))
	{
		BaseShader::Free(bitmapShader);

		return nullptr;
	}


	// ========================================================
	// Color
	// ========================================================

	if (!material->SetParameter(
		DescID(MATERIAL_COLOR_SHADER),
		GeData(bitmapShader),
		DESCFLAGS_SET_0
	))
	{
		BaseShader::Free(bitmapShader);

		return nullptr;
	}


	// ========================================================
	// PMX diffuse alpha
	// ========================================================

	const Float alpha =
		static_cast<Float>(
			pmxMaterial.diffuseAlpha
			);


	Vector alphaColor(
		alpha,
		alpha,
		alpha
	);


	material->SetParameter(
		DescID(MATERIAL_ALPHA_COLOR),
		GeData(alphaColor),
		DESCFLAGS_SET_0
	);


	// ========================================================
	// Alpha mode
	// ========================================================

	if (textureHasAlpha)
	{
		if (!material->SetParameter(
			DescID(MATERIAL_USE_ALPHA),
			GeData(true),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		if (!material->SetParameter(
			DescID(MATERIAL_ALPHA_SHADER),
			GeData(bitmapShader),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		if (!material->SetParameter(
			DescID(MATERIAL_ALPHA_IMAGEALPHA),
			GeData(true),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		GePrint(
			"PMX BITMAP ALPHA MODE : TEXTURE"
		);
	}
	else if (alpha < 0.999999f)
	{
		if (!material->SetParameter(
			DescID(MATERIAL_USE_ALPHA),
			GeData(true),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		if (!material->SetParameter(
			DescID(MATERIAL_ALPHA_IMAGEALPHA),
			GeData(false),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		GePrint(
			"PMX BITMAP ALPHA MODE : MATERIAL"
		);
	}
	else
	{
		if (!material->SetParameter(
			DescID(MATERIAL_USE_ALPHA),
			GeData(false),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		if (!material->SetParameter(
			DescID(MATERIAL_ALPHA_IMAGEALPHA),
			GeData(false),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


		GePrint(
			"PMX BITMAP ALPHA MODE : NONE"
		);
	}


	material->InsertShader(
		bitmapShader
	);


	GePrint(
		"PMX BITMAP SHADER : CREATED"
	);


	GePrint(
		"  ABSOLUTE FILE = " +
		absoluteTextureFile.GetString()
	);


	GePrint(
		"  STORED FILE = " +
		relativeTextureFile.GetString()
	);


	return bitmapShader;
}


// ============================================================
// Create Material Selection
// ============================================================

Bool PMXReader::CreateMaterialSelection(
	PolygonObject* object,
	const PMXMaterial& material,
	Int32 materialIndex,
	Int32 polygonOffset,
	String& selectionName
)
{
	if (!object)
		return false;


	SelectionTag* selectionTag =
		SelectionTag::Alloc(
			Tpolygonselection
		);


	if (!selectionTag)
		return false;


	selectionName =
		material.name;


	if (selectionName == String())
	{
		selectionName =
			String("PMX Material ") +
			String::IntToString(materialIndex);
	}


	selectionTag->SetName(
		selectionName
	);


	BaseSelect* selection =
		selectionTag->GetBaseSelect();


	if (!selection)
	{
		SelectionTag::Free(selectionTag);

		return false;
	}


	for (
		Int32 i = 0;
		i < material.polygonCount;
		++i
		)
	{
		selection->Select(
			polygonOffset + i
		);
	}


	object->InsertTag(
		selectionTag
	);


	return true;
}


// ============================================================
// Create Material Tag
// ============================================================

Bool PMXReader::CreateMaterialTag(
	PolygonObject* object,
	BaseMaterial* material,
	const PMXMaterial& pmxMaterial,
	Int32 materialIndex,
	Int32 polygonOffset
)
{
	if (!object)
		return false;


	if (!material)
		return false;


	TextureTag* textureTag =
		TextureTag::Alloc();


	if (!textureTag)
		return false;


	textureTag->SetMaterial(
		material
	);


	textureTag->SetParameter(
		DescID(TEXTURETAG_PROJECTION),
		GeData(TEXTURETAG_PROJECTION_UVW),
		DESCFLAGS_SET_0
	);


	String selectionName;


	if (!CreateMaterialSelection(
		object,
		pmxMaterial,
		materialIndex,
		polygonOffset,
		selectionName
	))
	{
		TextureTag::Free(textureTag);

		return false;
	}


	if (!textureTag->SetParameter(
		DescID(TEXTURETAG_RESTRICTION),
		GeData(selectionName),
		DESCFLAGS_SET_0
	))
	{
		TextureTag::Free(textureTag);

		return false;
	}


	object->InsertTag(
		textureTag
	);


	return true;
}


// ============================================================
// Setup Material
// ============================================================

Bool PMXReader::SetupMaterial(
	PolygonObject* object,
	BaseDocument* doc,
	const Filename& filename,
	const std::vector<PMXTexture>& textures,
	const PMXMaterial& pmxMaterial,
	Int32 materialIndex,
	Int32 polygonOffset
)
{
	if (!object)
		return false;


	BaseMaterial* material =
		CreateC4DMaterial(
			pmxMaterial,
			materialIndex
		);


	if (!material)
		return false;


	if (pmxMaterial.textureIndex >= 0 &&
		pmxMaterial.textureIndex <
		static_cast<Int32>(
			textures.size()
			))
	{
		const String& texturePath =
			textures[
				static_cast<size_t>(
					pmxMaterial.textureIndex
					)
			].path;


		if (texturePath != String())
		{
			Filename absoluteTextureFile =
				BuildTextureFilename(
					filename,
					texturePath
				);


			Filename relativeTextureFile =
				BuildRelativeTextureFilename(
					texturePath
				);


			if (!CreateBitmapShader(
				material,
				absoluteTextureFile,
				relativeTextureFile,
				pmxMaterial
			))
			{
				GePrint(
					"PMX BITMAP : NOT LINKED"
				);
			}
		}
	}


	doc->InsertMaterial(
		material
	);


	if (!CreateMaterialTag(
		object,
		material,
		pmxMaterial,
		materialIndex,
		polygonOffset
	))
	{
		return false;
	}


	return true;
}
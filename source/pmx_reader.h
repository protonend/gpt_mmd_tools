/*
pmx_reader.h

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMXバイナリの読み込み、PMXデータ検証、
C4Dジオメトリ・マテリアル生成を担当する
PMXReaderクラスを宣言する。
*/

#ifndef GPT_MMD_TOOLS_PMX_READER_H__
#define GPT_MMD_TOOLS_PMX_READER_H__

#include "c4d.h"
#include "pmx_types.h"
#include "pmx_dialog.h"

#include <vector>

class PMXReader
{
private:

BaseFile* _file;

	UChar _encoding;
	UChar _additionalUV;

	UChar _vertexIndexSize;
	UChar _textureIndexSize;
	UChar _materialIndexSize;
	UChar _boneIndexSize;
	UChar _morphIndexSize;
	UChar _rigidIndexSize;

	Float32 _version;

	UInt64 _readOffset;

	String _stage;


public:

	private:

	void SetStage(
		const String& stage
	);
	String GetOffsetString() const;
	Bool ReaderFailed(
		const String& reason
	);
	Bool Open(
		const Filename& filename
	);
	Bool ReadBytes(
		void* buffer,
		Int32 size
	);
	Bool ReadUChar(
		UChar& value
	);
	Bool ReadInt16(
		Int16& value
	);
	Bool ReadInt32(
		Int32& value
	);
	Bool ReadUInt16(
		UInt16& value
	);
	Bool ReadUInt32(
		UInt32& value
	);
	Bool ReadFloat32(
		Float32& value
	);
	Bool ReadVector3(
		Vector& value
	);
	Bool ReadAdditionalUV();
	Bool IsValidIndexSize(
		UChar size
	);
	Bool ReadIndex(
		UChar indexSize,
		Int32& value
	);
	Bool VertexReadFailed(
		Int32 vertexIndex,
		const String& stage,
		UChar weightType,
		Bool weightTypeValid
	);
	Bool ReadPMXString(
		String* result = nullptr
	);
	Bool ReadHeader();
	Bool ReadModelInfo();
	Bool ReadVertices(
		std::vector<PMXVertex>& vertices
	);
	Bool ReadFaces(
		const std::vector<PMXVertex>& vertices,
		std::vector<Int32>& indices
	);
	Bool ValidateFaces(
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices
	);
	Bool ReadTextures(
		std::vector<PMXTexture>& textures
	);
	Bool ReadMaterials(
		std::vector<PMXMaterial>& materials
	);
	Bool ValidateMaterials(
		const std::vector<PMXMaterial>& materials,
		Int32 geometryPolygonCount
	);
	Bool CreateNormalTag(
		PolygonObject* object,
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices
	);
	Bool CreateUVW(
		PolygonObject* object,
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices
	);
	Bool CreateSmoothTag(
		PolygonObject* object
	);
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
	PolygonObject* BuildCombinedObject(
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices,
		Float scale
	);
	PolygonObject* BuildMaterialObject(
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices,
		const PMXMaterial& material,
		Float scale
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

public:

	PMXReader();
	~PMXReader();
	public:

	Bool Load(
		const Filename& filename,
		BaseDocument* doc,
		const PMXImportSettings& settings
	);

};

#endif

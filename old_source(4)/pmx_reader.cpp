/*
pmx_reader.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
STEP 10のPMXReaderクラス本体。
PMX読み込み・検証・C4Dオブジェクト生成処理を
STEP 09のPMX読込を維持したまま、
PMX Vertex Morphを追加する。

分担：
pmx_vertex.cpp  -> Vertex
pmx_face.cpp    -> Face / Face Validation
pmx_uvw.cpp     -> Normal / UVW / Phong
pmx_material.cpp-> Material / Texture / Material Tag
pmx_bone.cpp    -> Bone
pmx_morph.cpp   -> Morph

このファイルはPMX Reader本体と
Object Build / Load制御を担当する。

Cinema 4D R19 / Visual Studio 2015
*/

#include "c4d.h"
#include "pmx_reader.h"
#include "pmx_bone.h"
#include "pmx_morph.h"

#include <vector>


// ============================================================
// Constructor
// ============================================================

PMXReader::PMXReader()
{
	_file = nullptr;

	_encoding = 0;
	_additionalUV = 0;

	_vertexIndexSize = 0;
	_textureIndexSize = 0;
	_materialIndexSize = 0;
	_boneIndexSize = 0;
	_morphIndexSize = 0;
	_rigidIndexSize = 0;

	_version = 0.0f;

	_readOffset = 0;

	_stage = String();
}


// ============================================================
// Destructor
// ============================================================

PMXReader::~PMXReader()
{
	if (_file)
	{
		_file->Close();

		BaseFile::Free(
			_file
		);

		_file = nullptr;
	}
}


// ============================================================
// Stage
// ============================================================

void PMXReader::SetStage(
	const String& stage
)
{
	_stage = stage;
}


// ============================================================
// Offset
// ============================================================

String PMXReader::GetOffsetString() const
{
	return String::IntToString(
		static_cast<Int32>(
			_readOffset
			)
	);
}


// ============================================================
// Reader Failed
// ============================================================

Bool PMXReader::ReaderFailed(
	const String& reason
)
{
	GePrint(
		"PMX READER : FAILED"
	);

	GePrint(
		"  STAGE = " +
		_stage
	);

	GePrint(
		"  OFFSET = " +
		GetOffsetString()
	);

	GePrint(
		"  REASON = " +
		reason
	);

	return false;
}


// ============================================================
// Open
// ============================================================

Bool PMXReader::Open(
	const Filename& filename
)
{
	SetStage(
		String("OPEN")
	);

	GePrint(
		"PMX FILE PATH : " +
		filename.GetString()
	);

	if (!GeFExist(
		filename,
		false
	))
	{
		GePrint(
			"PMX FILE EXIST : FALSE"
		);

		return false;
	}

	GePrint(
		"PMX FILE EXIST : TRUE"
	);

	_file =
		BaseFile::Alloc();

	if (!_file)
	{
		GePrint(
			"PMX FILE OPEN : BASEFILE ALLOC FAILED"
		);

		return false;
	}

	if (!_file->Open(
		filename,
		FILEOPEN_READ,
		FILEDIALOG_NONE
	))
	{
		GePrint(
			"PMX FILE OPEN : FAILED"
		);

		BaseFile::Free(
			_file
		);

		_file = nullptr;

		return false;
	}

	_readOffset = 0;

	GePrint(
		"PMX FILE OPEN : OK"
	);

	return true;
}


// ============================================================
// Read Bytes
// ============================================================

Bool PMXReader::ReadBytes(
	void* buffer,
	Int32 size
)
{
	if (!_file)
		return false;

	if (!buffer)
		return false;

	if (size <= 0)
		return false;

	const Int readSize =
		_file->ReadBytes(
			buffer,
			size
		);

	if (readSize !=
		static_cast<Int>(size))
	{
		GePrint(
			"PMX READ BYTES FAILED"
		);

		GePrint(
			"  STAGE = " +
			_stage
		);

		GePrint(
			"  OFFSET = " +
			GetOffsetString()
		);

		GePrint(
			"  REQUEST = " +
			String::IntToString(
				size
			)
		);

		GePrint(
			"  ACTUAL = " +
			String::IntToString(
				static_cast<Int32>(
					readSize
					)
			)
		);

		return false;
	}

	_readOffset +=
		static_cast<UInt64>(
			size
			);

	return true;
}


// ============================================================
// Primitive Readers
// ============================================================

Bool PMXReader::ReadUChar(
	UChar& value
)
{
	return ReadBytes(
		&value,
		sizeof(UChar)
	);
}

Bool PMXReader::ReadInt16(
	Int16& value
)
{
	return ReadBytes(
		&value,
		sizeof(Int16)
	);
}

Bool PMXReader::ReadInt32(
	Int32& value
)
{
	return ReadBytes(
		&value,
		sizeof(Int32)
	);
}

Bool PMXReader::ReadUInt16(
	UInt16& value
)
{
	return ReadBytes(
		&value,
		sizeof(UInt16)
	);
}

Bool PMXReader::ReadUInt32(
	UInt32& value
)
{
	return ReadBytes(
		&value,
		sizeof(UInt32)
	);
}

Bool PMXReader::ReadFloat32(
	Float32& value
)
{
	return ReadBytes(
		&value,
		sizeof(Float32)
	);
}


// ============================================================
// Vector
// ============================================================

Bool PMXReader::ReadVector3(
	Vector& value
)
{
	Float32 x;
	Float32 y;
	Float32 z;

	if (!ReadFloat32(
		x
	))
		return false;

	if (!ReadFloat32(
		y
	))
		return false;

	if (!ReadFloat32(
		z
	))
		return false;

	value =
		Vector(
			x,
			y,
			z
		);

	return true;
}


// ============================================================
// Additional UV
// ============================================================

Bool PMXReader::ReadAdditionalUV()
{
	Float32 x;
	Float32 y;
	Float32 z;
	Float32 w;

	if (!ReadFloat32(
		x
	))
		return false;

	if (!ReadFloat32(
		y
	))
		return false;

	if (!ReadFloat32(
		z
	))
		return false;

	if (!ReadFloat32(
		w
	))
		return false;

	return true;
}


// ============================================================
// Index Size
// ============================================================

Bool PMXReader::IsValidIndexSize(
	UChar size
)
{
	if (size == 1)
		return true;

	if (size == 2)
		return true;

	if (size == 4)
		return true;

	return false;
}


// ============================================================
// Read Index
// ============================================================

Bool PMXReader::ReadIndex(
	UChar indexSize,
	Int32& value
)
{
	if (indexSize == 1)
	{
		UChar v;

		if (!ReadUChar(
			v
		))
			return false;

		if (v == 0xFF)
			value = -1;
		else
			value =
			static_cast<Int32>(
				v
				);

		return true;
	}

	if (indexSize == 2)
	{
		UInt16 v;

		if (!ReadUInt16(
			v
		))
			return false;

		if (v == 0xFFFF)
			value = -1;
		else
			value =
			static_cast<Int32>(
				v
				);

		return true;
	}

	if (indexSize == 4)
	{
		UInt32 v;

		if (!ReadUInt32(
			v
		))
			return false;

		if (v == 0xFFFFFFFFu)
			value = -1;
		else
			value =
			static_cast<Int32>(
				v
				);

		return true;
	}

	return false;
}


// ============================================================
// Vertex Read Failed
// ============================================================

Bool PMXReader::VertexReadFailed(
	Int32 vertexIndex,
	const String& stage,
	UChar weightType,
	Bool weightTypeValid
)
{
	GePrint(
		"PMX VERTEX READ FAILED"
	);

	GePrint(
		"  VERTEX = " +
		String::IntToString(
			vertexIndex
		)
	);

	GePrint(
		"  STAGE = " +
		stage
	);

	GePrint(
		"  OFFSET = " +
		GetOffsetString()
	);

	if (weightTypeValid)
	{
		GePrint(
			"  WEIGHT TYPE = " +
			String::IntToString(
				static_cast<Int32>(
					weightType
					)
			)
		);
	}

	return false;
}


// ============================================================
// PMX String
// ============================================================

Bool PMXReader::ReadPMXString(
	String* result
)
{
	Int32 byteLength;

	if (!ReadInt32(
		byteLength
	))
	{
		return false;
	}

	if (byteLength < 0)
		return false;

	if (byteLength == 0)
	{
		if (result)
			*result = String();

		return true;
	}

	if (
		(byteLength & 1) != 0 &&
		_encoding == 0
		)
	{
		return false;
	}

	std::vector<UChar> buffer;

	try
	{
		buffer.resize(
			static_cast<size_t>(
				byteLength + 2
				)
		);
	}
	catch (...)
	{
		return false;
	}

	for (
		Int32 i = 0;
		i < byteLength + 2;
		++i
		)
	{
		buffer[
			static_cast<size_t>(
				i
				)
		] = 0;
	}

	if (!ReadBytes(
		&buffer[0],
		byteLength
	))
	{
		return false;
	}

	if (!result)
		return true;

	if (_encoding == 0)
	{
		const Int32 charCount =
			byteLength / 2;

		std::vector<Utf16Char> utf16;

		try
		{
			utf16.resize(
				static_cast<size_t>(
					charCount + 1
					)
			);
		}
		catch (...)
		{
			return false;
		}

		for (
			Int32 i = 0;
			i < charCount;
			++i
			)
		{
			const UChar lo =
				buffer[
					static_cast<size_t>(
						i * 2
						)
				];

			const UChar hi =
				buffer[
					static_cast<size_t>(
						i * 2 + 1
						)
				];

			utf16[
				static_cast<size_t>(
					i
					)
			] =
				static_cast<Utf16Char>(
					static_cast<UInt32>(
						lo
						) |
						(
							static_cast<UInt32>(
								hi
								)
							<< 8
							)
					);
		}

		utf16[
			static_cast<size_t>(
				charCount
				)
		] = 0;

		*result =
			String(
				&utf16[0],
				charCount
			);

		return true;
	}

	*result =
		String(
			reinterpret_cast<const Char*>(
				&buffer[0]
				),
			STRINGENCODING_UTF8
		);

	return true;
}


// ============================================================
// Header
// ============================================================

Bool PMXReader::ReadHeader()
{
	SetStage(
		String("HEADER")
	);

	UChar signature[4];

	if (!ReadBytes(
		signature,
		4
	))
	{
		return ReaderFailed(
			String("SIGNATURE READ")
		);
	}

	if (
		signature[0] != 'P' ||
		signature[1] != 'M' ||
		signature[2] != 'X' ||
		signature[3] != ' '
		)
	{
		return ReaderFailed(
			String("INVALID PMX SIGNATURE")
		);
	}

	GePrint(
		"PMX SIGNATURE : OK"
	);

	if (!ReadFloat32(
		_version
	))
	{
		return ReaderFailed(
			String("VERSION READ")
		);
	}

	UChar headerSize;

	if (!ReadUChar(
		headerSize
	))
	{
		return ReaderFailed(
			String("HEADER SIZE READ")
		);
	}

	if (headerSize < 8)
	{
		return ReaderFailed(
			String("HEADER SIZE < 8")
		);
	}

	std::vector<UChar> headerData;

	try
	{
		headerData.resize(
			static_cast<size_t>(
				headerSize
				)
		);
	}
	catch (...)
	{
		return ReaderFailed(
			String("HEADER BUFFER ALLOC")
		);
	}

	if (!ReadBytes(
		&headerData[0],
		headerSize
	))
	{
		return ReaderFailed(
			String("HEADER DATA READ")
		);
	}

	_encoding =
		headerData[0];

	_additionalUV =
		headerData[1];

	_vertexIndexSize =
		headerData[2];

	_textureIndexSize =
		headerData[3];

	_materialIndexSize =
		headerData[4];

	_boneIndexSize =
		headerData[5];

	_morphIndexSize =
		headerData[6];

	_rigidIndexSize =
		headerData[7];

	if (_encoding != 0 &&
		_encoding != 1)
	{
		return ReaderFailed(
			String("INVALID ENCODING")
		);
	}

	if (_additionalUV > 4)
	{
		return ReaderFailed(
			String("INVALID ADDITIONAL UV COUNT")
		);
	}

	if (!IsValidIndexSize(
		_vertexIndexSize
	))
	{
		return ReaderFailed(
			String("INVALID VERTEX INDEX SIZE")
		);
	}

	if (!IsValidIndexSize(
		_textureIndexSize
	))
	{
		return ReaderFailed(
			String("INVALID TEXTURE INDEX SIZE")
		);
	}

	if (!IsValidIndexSize(
		_materialIndexSize
	))
	{
		return ReaderFailed(
			String("INVALID MATERIAL INDEX SIZE")
		);
	}

	if (!IsValidIndexSize(
		_boneIndexSize
	))
	{
		return ReaderFailed(
			String("INVALID BONE INDEX SIZE")
		);
	}

	if (!IsValidIndexSize(
		_morphIndexSize
	))
	{
		return ReaderFailed(
			String("INVALID MORPH INDEX SIZE")
		);
	}

	if (!IsValidIndexSize(
		_rigidIndexSize
	))
	{
		return ReaderFailed(
			String("INVALID RIGID INDEX SIZE")
		);
	}

	GePrint(
		"PMX VERSION : " +
		String::FloatToString(
			static_cast<Float>(
				_version
				)
		)
	);

	GePrint(
		"PMX HEADER SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				headerSize
				)
		)
	);

	GePrint(
		"PMX ENCODING : " +
		String::IntToString(
			static_cast<Int32>(
				_encoding
				)
		)
	);

	GePrint(
		"PMX ADDITIONAL UV : " +
		String::IntToString(
			static_cast<Int32>(
				_additionalUV
				)
		)
	);

	GePrint(
		"PMX VERTEX INDEX SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				_vertexIndexSize
				)
		)
	);

	GePrint(
		"PMX TEXTURE INDEX SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				_textureIndexSize
				)
		)
	);

	GePrint(
		"PMX MATERIAL INDEX SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				_materialIndexSize
				)
		)
	);

	GePrint(
		"PMX BONE INDEX SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				_boneIndexSize
				)
		)
	);

	GePrint(
		"PMX MORPH INDEX SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				_morphIndexSize
				)
		)
	);

	GePrint(
		"PMX RIGID INDEX SIZE : " +
		String::IntToString(
			static_cast<Int32>(
				_rigidIndexSize
				)
		)
	);

	GePrint(
		"PMX HEADER OFFSET : " +
		GetOffsetString()
	);

	GePrint(
		"PMX HEADER : OK"
	);

	return true;
}


// ============================================================
// Model Info
// ============================================================

Bool PMXReader::ReadModelInfo()
{
	SetStage(
		String("MODEL INFO")
	);

	String modelName;
	String modelNameUniversal;
	String comment;
	String commentUniversal;

	if (!ReadPMXString(
		&modelName
	))
		return ReaderFailed(
			String("MODEL NAME")
		);

	if (!ReadPMXString(
		&modelNameUniversal
	))
		return ReaderFailed(
			String("MODEL NAME UNIVERSAL")
		);

	if (!ReadPMXString(
		&comment
	))
		return ReaderFailed(
			String("COMMENT")
		);

	if (!ReadPMXString(
		&commentUniversal
	))
		return ReaderFailed(
			String("COMMENT UNIVERSAL")
		);

	GePrint(
		"PMX MODEL INFO OFFSET : " +
		GetOffsetString()
	);

	GePrint(
		"PMX MODEL INFO : OK"
	);

	return true;
}


// ============================================================
// Textures
// ============================================================

Bool PMXReader::ReadTextures(
	std::vector<PMXTexture>& textures
)
{
	SetStage(
		String("TEXTURE")
	);

	Int32 textureCount;

	if (!ReadInt32(
		textureCount
	))
		return ReaderFailed(
			String("TEXTURE COUNT READ")
		);

	if (textureCount < 0)
		return ReaderFailed(
			String("NEGATIVE TEXTURE COUNT")
		);

	try
	{
		textures.resize(
			static_cast<size_t>(
				textureCount
				)
		);
	}
	catch (...)
	{
		return ReaderFailed(
			String("TEXTURE VECTOR ALLOC")
		);
	}

	GePrint(
		"PMX TEXTURE COUNT : " +
		String::IntToString(
			textureCount
		)
	);

	for (
		Int32 i = 0;
		i < textureCount;
		++i
		)
	{
		if (!ReadPMXString(
			&textures[
				static_cast<size_t>(
					i
					)
			].path
		))
		{
			GePrint(
				"PMX TEXTURE READ FAILED"
			);

			GePrint(
				"  TEXTURE INDEX = " +
				String::IntToString(
					i
				)
			);

			GePrint(
				"  OFFSET = " +
				GetOffsetString()
			);

			return false;
		}

				GePrint(
					"PMX TEXTURE[" +
					String::IntToString(
						i
					) +
					"] = " +
					textures[
						static_cast<size_t>(
							i
							)
					].path
				);
	}

	GePrint(
		"PMX TEXTURE END OFFSET : " +
		GetOffsetString()
	);

	GePrint(
		"PMX TEXTURE : OK"
	);

	return true;
}


// ============================================================
// Materials
// ============================================================

Bool PMXReader::ReadMaterials(
	std::vector<PMXMaterial>& materials
)
{
	SetStage(
		String("MATERIAL")
	);

	Int32 materialCount;

	if (!ReadInt32(
		materialCount
	))
		return ReaderFailed(
			String("MATERIAL COUNT READ")
		);

	if (materialCount < 0)
		return ReaderFailed(
			String("NEGATIVE MATERIAL COUNT")
		);

	try
	{
		materials.resize(
			static_cast<size_t>(
				materialCount
				)
		);
	}
	catch (...)
	{
		return ReaderFailed(
			String("MATERIAL VECTOR ALLOC")
		);
	}

	GePrint(
		"PMX MATERIAL COUNT : " +
		String::IntToString(
			materialCount
		)
	);

	Int32 polygonStart = 0;

	for (
		Int32 i = 0;
		i < materialCount;
		++i
		)
	{
		PMXMaterial& material =
			materials[
				static_cast<size_t>(
					i
					)
			];

		material.polygonStart =
			polygonStart;

		if (!ReadPMXString(
			&material.name
		))
		{
			GePrint(
				"PMX MATERIAL READ FAILED"
			);

			GePrint(
				"  MATERIAL = " +
				String::IntToString(
					i
				)
			);

			GePrint(
				"  FIELD = NAME"
			);

			GePrint(
				"  OFFSET = " +
				GetOffsetString()
			);

			return false;
		}

		if (!ReadPMXString(
			&material.nameUniversal
		))
		{
			GePrint(
				"PMX MATERIAL READ FAILED"
			);

			GePrint(
				"  MATERIAL = " +
				String::IntToString(
					i
				)
			);

			GePrint(
				"  FIELD = UNIVERSAL NAME"
			);

			GePrint(
				"  OFFSET = " +
				GetOffsetString()
			);

			return false;
		}

		if (!ReadVector3(
			material.diffuse
		))
			return false;

		if (!ReadFloat32(
			material.diffuseAlpha
		))
			return false;

		if (!ReadVector3(
			material.specular
		))
			return false;

		if (!ReadFloat32(
			material.specularPower
		))
			return false;

		if (!ReadVector3(
			material.ambient
		))
			return false;

		if (!ReadUChar(
			material.drawFlags
		))
			return false;

		if (!ReadVector3(
			material.edgeColor
		))
			return false;

		if (!ReadFloat32(
			material.edgeAlpha
		))
			return false;

		if (!ReadFloat32(
			material.edgeSize
		))
			return false;

		if (!ReadIndex(
			_textureIndexSize,
			material.textureIndex
		))
		{
			GePrint(
				"PMX MATERIAL READ FAILED : TEXTURE INDEX"
			);

			GePrint(
				"  MATERIAL = " +
				String::IntToString(
					i
				)
			);

			return false;
		}

		if (!ReadIndex(
			_textureIndexSize,
			material.sphereTextureIndex
		))
		{
			GePrint(
				"PMX MATERIAL READ FAILED : SPHERE TEXTURE INDEX"
			);

			GePrint(
				"  MATERIAL = " +
				String::IntToString(
					i
				)
			);

			return false;
		}

		if (!ReadUChar(
			material.sphereMode
		))
			return false;

		if (!ReadUChar(
			material.toonFlag
		))
			return false;

		if (material.toonFlag == 0)
		{
			if (!ReadIndex(
				_textureIndexSize,
				material.toonTextureIndex
			))
				return false;
		}
		else
		{
			UChar toonIndex;

			if (!ReadUChar(
				toonIndex
			))
				return false;

			material.toonTextureIndex =
				static_cast<Int32>(
					toonIndex
					);
		}

		if (!ReadPMXString(
			&material.memo
		))
			return false;

		if (!ReadInt32(
			material.faceVertexCount
		))
			return false;

		if (material.faceVertexCount < 0)
			return false;

		if ((material.faceVertexCount % 3) != 0)
		{
			GePrint(
				"PMX MATERIAL WARNING : FACE VERTEX COUNT NOT DIVISIBLE BY 3"
			);
		}

		material.polygonCount =
			material.faceVertexCount / 3;

		polygonStart +=
			material.polygonCount;

		GePrint(
			"PMX MATERIAL[" +
			String::IntToString(
				i
			) +
			"]"
		);

		GePrint(
			"  NAME = " +
			material.name
		);

		GePrint(
			"  UNIVERSAL NAME = " +
			material.nameUniversal
		);

		GePrint(
			"  TEXTURE INDEX = " +
			String::IntToString(
				material.textureIndex
			)
		);

		GePrint(
			"  SPHERE TEXTURE INDEX = " +
			String::IntToString(
				material.sphereTextureIndex
			)
		);

		GePrint(
			"  TOON FLAG = " +
			String::IntToString(
				static_cast<Int32>(
					material.toonFlag
					)
			)
		);

		GePrint(
			"  TOON TEXTURE INDEX = " +
			String::IntToString(
				material.toonTextureIndex
			)
		);

		GePrint(
			"  DIFFUSE ALPHA = " +
			String::FloatToString(
				static_cast<Float>(
					material.diffuseAlpha
					)
			)
		);

		GePrint(
			"  FACE VERTEX COUNT = " +
			String::IntToString(
				material.faceVertexCount
			)
		);

		GePrint(
			"  POLYGON COUNT = " +
			String::IntToString(
				material.polygonCount
			)
		);

		GePrint(
			"  POLYGON START = " +
			String::IntToString(
				material.polygonStart
			)
		);

		GePrint(
			"  END OFFSET = " +
			GetOffsetString()
		);
	}

	Int32 totalFaceVertexCount = 0;

	for (
		size_t i = 0;
		i < materials.size();
		++i
		)
	{
		totalFaceVertexCount +=
			materials[i].faceVertexCount;
	}

	GePrint(
		"------------------------------------------------------------"
	);

	GePrint(
		"PMX MATERIAL TOTAL FACE VERTEX COUNT : " +
		String::IntToString(
			totalFaceVertexCount
		)
	);

	GePrint(
		"PMX MATERIAL TOTAL POLYGON COUNT : " +
		String::IntToString(
			totalFaceVertexCount / 3
		)
	);

	GePrint(
		"PMX MATERIAL END OFFSET : " +
		GetOffsetString()
	);

	GePrint(
		"PMX MATERIAL : OK"
	);

	return true;
}


// ============================================================
// Bones
// ============================================================

Bool PMXReader::ReadBones(
	std::vector<PMXBone>& bones
)
{
	SetStage(
		String("BONE")
	);

	Int32 boneCount;

	if (!ReadInt32(
		boneCount
	))
		return ReaderFailed(
			String("BONE COUNT READ")
		);

	if (boneCount < 0)
		return ReaderFailed(
			String("NEGATIVE BONE COUNT")
		);

	try
	{
		bones.resize(
			static_cast<size_t>(
				boneCount
				)
		);
	}
	catch (...)
	{
		return ReaderFailed(
			String("BONE VECTOR ALLOC")
		);
	}

	GePrint(
		"PMX BONE COUNT : " +
		String::IntToString(
			boneCount
		)
	);

	for (
		Int32 i = 0;
		i < boneCount;
		++i
		)
	{
		PMXBone& bone =
			bones[
				static_cast<size_t>(
					i
					)
			];

		if (!ReadPMXString(
			&bone.name
		))
			return ReaderFailed(
				String("BONE NAME")
			);

		if (!ReadPMXString(
			&bone.nameUniversal
		))
			return ReaderFailed(
				String("BONE UNIVERSAL NAME")
			);

		if (!ReadVector3(
			bone.position
		))
			return ReaderFailed(
				String("BONE POSITION")
			);

		if (!ReadIndex(
			_boneIndexSize,
			bone.parentIndex
		))
			return ReaderFailed(
				String("BONE PARENT INDEX")
			);

		if (!ReadInt32(
			bone.deformLayer
		))
			return ReaderFailed(
				String("BONE DEFORM LAYER")
			);

		if (!ReadUInt16(
			bone.flags
		))
			return ReaderFailed(
				String("BONE FLAGS")
			);

		if ((bone.flags & 0x0001) != 0)
		{
			if (!ReadIndex(
				_boneIndexSize,
				bone.tailBoneIndex
			))
				return ReaderFailed(
					String("BONE TAIL INDEX")
				);
		}
		else
		{
			if (!ReadVector3(
				bone.tailPositionOffset
			))
				return ReaderFailed(
					String("BONE TAIL POSITION")
				);
		}

		if (
			(bone.flags & 0x0100) != 0 ||
			(bone.flags & 0x0200) != 0
			)
		{
			if (!ReadIndex(
				_boneIndexSize,
				bone.appendBoneIndex
			))
				return ReaderFailed(
					String("BONE APPEND INDEX")
				);

			if (!ReadFloat32(
				bone.appendWeight
			))
				return ReaderFailed(
					String("BONE APPEND WEIGHT")
				);
		}

		if ((bone.flags & 0x0400) != 0)
		{
			if (!ReadVector3(
				bone.fixedAxis
			))
				return ReaderFailed(
					String("BONE FIXED AXIS")
				);
		}

		if ((bone.flags & 0x0800) != 0)
		{
			if (!ReadVector3(
				bone.localXAxis
			))
				return ReaderFailed(
					String("BONE LOCAL X AXIS")
				);

			if (!ReadVector3(
				bone.localZAxis
			))
				return ReaderFailed(
					String("BONE LOCAL Z AXIS")
				);
		}

		if ((bone.flags & 0x2000) != 0)
		{
			if (!ReadInt32(
				bone.externalParentKey
			))
				return ReaderFailed(
					String("BONE EXTERNAL PARENT KEY")
				);
		}

		if ((bone.flags & 0x0020) != 0)
		{
			if (!ReadIndex(
				_boneIndexSize,
				bone.ikTargetBoneIndex
			))
				return ReaderFailed(
					String("BONE IK TARGET")
				);

			if (!ReadInt32(
				bone.ikLoopCount
			))
				return ReaderFailed(
					String("BONE IK LOOP COUNT")
				);

			if (!ReadFloat32(
				bone.ikLoopAngleLimit
			))
				return ReaderFailed(
					String("BONE IK LOOP ANGLE LIMIT")
				);

			Int32 linkCount;

			if (!ReadInt32(
				linkCount
			))
				return ReaderFailed(
					String("BONE IK LINK COUNT")
				);

			if (linkCount < 0)
				return ReaderFailed(
					String("NEGATIVE BONE IK LINK COUNT")
				);

			try
			{
				bone.ikLinks.resize(
					static_cast<size_t>(
						linkCount
						)
				);
			}
			catch (...)
			{
				return ReaderFailed(
					String("BONE IK LINK VECTOR ALLOC")
				);
			}

			for (
				Int32 j = 0;
				j < linkCount;
				++j
				)
			{
				PMXIKLink& link =
					bone.ikLinks[
						static_cast<size_t>(
							j
							)
					];

				if (!ReadIndex(
					_boneIndexSize,
					link.boneIndex
				))
					return ReaderFailed(
						String("BONE IK LINK INDEX")
					);

				UChar hasLimit;

				if (!ReadUChar(
					hasLimit
				))
					return ReaderFailed(
						String("BONE IK LINK LIMIT FLAG")
					);

				link.hasLimit =
					hasLimit != 0;

				if (link.hasLimit)
				{
					if (!ReadVector3(
						link.minimum
					))
						return ReaderFailed(
							String("BONE IK LINK MINIMUM")
						);

					if (!ReadVector3(
						link.maximum
					))
						return ReaderFailed(
							String("BONE IK LINK MAXIMUM")
						);
				}
			}
		}
	}

	GePrint(
		"PMX BONE END OFFSET : " +
		GetOffsetString()
	);

	GePrint(
		"PMX BONE : OK"
	);

	return true;
}


// ============================================================
// Morphs
//
// PMX Morph Type
//
// 0 = Group
// 1 = Vertex
// 2 = Bone
// 3 = UV
// 4 = Additional UV1
// 5 = Additional UV2
// 6 = Additional UV3
// 7 = Additional UV4
// 8 = Material
// 9 = Flip
// 10 = Impulse
//
// STEP 10では Type 1 Vertex Morph のみ
// PMXMorph::vertexOffsets に保存する。
// その他のTypeはPMXバイナリを正しく消費する。
// ============================================================

Bool PMXReader::ReadMorphs(
	std::vector<PMXMorph>& morphs
)
{
	SetStage(
		String("MORPH")
	);

	Int32 morphCount;

	if (!ReadInt32(
		morphCount
	))
	{
		return ReaderFailed(
			String("MORPH COUNT READ")
		);
	}

	if (morphCount < 0)
	{
		return ReaderFailed(
			String("NEGATIVE MORPH COUNT")
		);
	}

	try
	{
		morphs.clear();

		morphs.resize(
			static_cast<size_t>(
				morphCount
				)
		);
	}
	catch (...)
	{
		return ReaderFailed(
			String("MORPH VECTOR ALLOC")
		);
	}

	GePrint(
		"PMX MORPH COUNT : " +
		String::IntToString(
			morphCount
		)
	);

	Int32 vertexMorphCount = 0;
	Int32 vertexMorphOffsetCount = 0;

	for (
		Int32 i = 0;
		i < morphCount;
		++i
		)
	{
		PMXMorph& morph =
			morphs[
				static_cast<size_t>(
					i
					)
			];

		if (!ReadPMXString(
			&morph.name
		))
		{
			GePrint(
				"PMX MORPH READ FAILED"
			);

			GePrint(
				"  MORPH = " +
				String::IntToString(
					i
				)
			);

			GePrint(
				"  FIELD = NAME"
			);

			return false;
		}

		if (!ReadPMXString(
			&morph.nameUniversal
		))
		{
			GePrint(
				"PMX MORPH READ FAILED"
			);

			GePrint(
				"  MORPH = " +
				String::IntToString(
					i
				)
			);

			GePrint(
				"  FIELD = UNIVERSAL NAME"
			);

			return false;
		}

		if (!ReadUChar(
			morph.panel
		))
		{
			GePrint(
				"PMX MORPH READ FAILED : PANEL"
			);

			return false;
		}

		if (!ReadUChar(
			morph.type
		))
		{
			GePrint(
				"PMX MORPH READ FAILED : TYPE"
			);

			return false;
		}

		Int32 offsetCount;

		if (!ReadInt32(
			offsetCount
		))
		{
			GePrint(
				"PMX MORPH READ FAILED : OFFSET COUNT"
			);

			return false;
		}

		if (offsetCount < 0)
		{
			GePrint(
				"PMX MORPH READ FAILED : NEGATIVE OFFSET COUNT"
			);

			GePrint(
				"  MORPH = " +
				String::IntToString(
					i
				)
			);

			return false;
		}

		// ----------------------------------------------------
		// Vertex Morph
		// ----------------------------------------------------

		if (morph.type == 1)
		{
			++vertexMorphCount;

			try
			{
				morph.vertexOffsets.clear();

				morph.vertexOffsets.resize(
					static_cast<size_t>(
						offsetCount
						)
				);
			}
			catch (...)
			{
				return ReaderFailed(
					String("VERTEX MORPH OFFSET VECTOR ALLOC")
				);
			}

			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				PMXVertexMorphOffset& offset =
					morph.vertexOffsets[
						static_cast<size_t>(
							j
							)
					];

				if (!ReadIndex(
					_vertexIndexSize,
					offset.vertexIndex
				))
				{
					GePrint(
						"PMX VERTEX MORPH READ FAILED : VERTEX INDEX"
					);

					GePrint(
						"  MORPH = " +
						String::IntToString(
							i
						)
					);

					GePrint(
						"  OFFSET = " +
						String::IntToString(
							j
						)
					);

					return false;
				}

				if (!ReadVector3(
					offset.offset
				))
				{
					GePrint(
						"PMX VERTEX MORPH READ FAILED : OFFSET"
					);

					GePrint(
						"  MORPH = " +
						String::IntToString(
							i
						)
					);

					GePrint(
						"  OFFSET = " +
						String::IntToString(
							j
						)
					);

					return false;
				}
			}

			vertexMorphOffsetCount +=
				offsetCount;
		}

		// ----------------------------------------------------
		// Group Morph
		// ----------------------------------------------------

		else if (morph.type == 0)
		{
			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				Int32 morphIndex;
				Float32 weight;

				if (!ReadIndex(
					_morphIndexSize,
					morphIndex
				))
				{
					GePrint(
						"PMX GROUP MORPH READ FAILED : MORPH INDEX"
					);

					return false;
				}

				if (!ReadFloat32(
					weight
				))
				{
					GePrint(
						"PMX GROUP MORPH READ FAILED : WEIGHT"
					);

					return false;
				}
			}
		}

		// ----------------------------------------------------
		// Bone Morph
		// ----------------------------------------------------

		else if (morph.type == 2)
		{
			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				Int32 boneIndex;
				Vector translation;

				if (!ReadIndex(
					_boneIndexSize,
					boneIndex
				))
				{
					GePrint(
						"PMX BONE MORPH READ FAILED : BONE INDEX"
					);

					return false;
				}

				if (!ReadVector3(
					translation
				))
				{
					GePrint(
						"PMX BONE MORPH READ FAILED : TRANSLATION"
					);

					return false;
				}

				Float32 rotationX;
				Float32 rotationY;
				Float32 rotationZ;
				Float32 rotationW;

				if (!ReadFloat32(
					rotationX
				))
					return false;

				if (!ReadFloat32(
					rotationY
				))
					return false;

				if (!ReadFloat32(
					rotationZ
				))
					return false;

				if (!ReadFloat32(
					rotationW
				))
					return false;
			}
		}

		// ----------------------------------------------------
		// UV / Additional UV Morph
		// ----------------------------------------------------

		else if (
			morph.type == 3 ||
			morph.type == 4 ||
			morph.type == 5 ||
			morph.type == 6 ||
			morph.type == 7
			)
		{
			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				Int32 vertexIndex;

				if (!ReadIndex(
					_vertexIndexSize,
					vertexIndex
				))
				{
					GePrint(
						"PMX UV MORPH READ FAILED : VERTEX INDEX"
					);

					return false;
				}

				if (!ReadAdditionalUV())
				{
					GePrint(
						"PMX UV MORPH READ FAILED : OFFSET"
					);

					return false;
				}
			}
		}

		// ----------------------------------------------------
		// Material Morph
		// ----------------------------------------------------

		else if (morph.type == 8)
		{
			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				Int32 materialIndex;
				UChar operation;

				if (!ReadIndex(
					_materialIndexSize,
					materialIndex
				))
				{
					GePrint(
						"PMX MATERIAL MORPH READ FAILED : MATERIAL INDEX"
					);

					return false;
				}

				if (!ReadUChar(
					operation
				))
				{
					GePrint(
						"PMX MATERIAL MORPH READ FAILED : OPERATION"
					);

					return false;
				}

				Vector diffuse;
				Vector specular;
				Vector ambient;
				Vector edgeColor;

				Float32 diffuseAlpha;
				Float32 specularPower;
				Float32 edgeAlpha;
				Float32 edgeSize;

				if (!ReadVector3(
					diffuse
				))
					return false;

				if (!ReadFloat32(
					diffuseAlpha
				))
					return false;

				if (!ReadVector3(
					specular
				))
					return false;

				if (!ReadFloat32(
					specularPower
				))
					return false;

				if (!ReadVector3(
					ambient
				))
					return false;

				if (!ReadVector3(
					edgeColor
				))
					return false;

				if (!ReadFloat32(
					edgeAlpha
				))
					return false;

				if (!ReadFloat32(
					edgeSize
				))
					return false;

				if (!ReadAdditionalUV())
					return false;

				if (!ReadAdditionalUV())
					return false;

				if (!ReadAdditionalUV())
					return false;
			}
		}

		// ----------------------------------------------------
		// Flip Morph
		// ----------------------------------------------------

		else if (morph.type == 9)
		{
			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				Int32 morphIndex;
				Float32 weight;

				if (!ReadIndex(
					_morphIndexSize,
					morphIndex
				))
				{
					GePrint(
						"PMX FLIP MORPH READ FAILED : MORPH INDEX"
					);

					return false;
				}

				if (!ReadFloat32(
					weight
				))
				{
					GePrint(
						"PMX FLIP MORPH READ FAILED : WEIGHT"
					);

					return false;
				}
			}
		}

		// ----------------------------------------------------
		// Impulse Morph
		// ----------------------------------------------------

		else if (morph.type == 10)
		{
			for (
				Int32 j = 0;
				j < offsetCount;
				++j
				)
			{
				Int32 rigidBodyIndex;
				UChar localFlag;

				Vector velocity;
				Vector torque;

				if (!ReadIndex(
					_rigidIndexSize,
					rigidBodyIndex
				))
				{
					GePrint(
						"PMX IMPULSE MORPH READ FAILED : RIGID INDEX"
					);

					return false;
				}

				if (!ReadUChar(
					localFlag
				))
				{
					GePrint(
						"PMX IMPULSE MORPH READ FAILED : LOCAL FLAG"
					);

					return false;
				}

				if (!ReadVector3(
					velocity
				))
					return false;

				if (!ReadVector3(
					torque
				))
					return false;
			}
		}

		// ----------------------------------------------------
		// Unknown Morph Type
		// ----------------------------------------------------

		else
		{
			GePrint(
				"PMX MORPH READ FAILED : UNKNOWN MORPH TYPE"
			);

			GePrint(
				"  MORPH = " +
				String::IntToString(
					i
				)
			);

			GePrint(
				"  TYPE = " +
				String::IntToString(
					static_cast<Int32>(
						morph.type
						)
				)
			);

			return false;
		}

		// ----------------------------------------------------
		// Diagnostic
		// ----------------------------------------------------

		GePrint(
			"PMX MORPH[" +
			String::IntToString(
				i
			) +
			"]"
		);

		GePrint(
			"  NAME = " +
			morph.name
		);

		GePrint(
			"  UNIVERSAL NAME = " +
			morph.nameUniversal
		);

		GePrint(
			"  PANEL = " +
			String::IntToString(
				static_cast<Int32>(
					morph.panel
					)
			)
		);

		GePrint(
			"  TYPE = " +
			String::IntToString(
				static_cast<Int32>(
					morph.type
					)
			)
		);

		GePrint(
			"  OFFSET COUNT = " +
			String::IntToString(
				offsetCount
			)
		);

		if (morph.type == 1)
		{
			GePrint(
				"  VERTEX OFFSETS STORED = " +
				String::IntToString(
					static_cast<Int32>(
						morph.vertexOffsets.size()
						)
				)
			);
		}
	}

	GePrint(
		"------------------------------------------------------------"
	);

	GePrint(
		"PMX MORPH TOTAL : " +
		String::IntToString(
			morphCount
		)
	);

	GePrint(
		"PMX VERTEX MORPH : " +
		String::IntToString(
			vertexMorphCount
		)
	);

	GePrint(
		"PMX VERTEX MORPH OFFSET TOTAL : " +
		String::IntToString(
			vertexMorphOffsetCount
		)
	);

	GePrint(
		"PMX MORPH END OFFSET : " +
		GetOffsetString()
	);

	GePrint(
		"PMX MORPH : OK"
	);

	return true;
}


// ============================================================
// Validate Materials
// ============================================================

Bool PMXReader::ValidateMaterials(
	const std::vector<PMXMaterial>& materials,
	Int32 geometryPolygonCount
)
{
	SetStage(
		String("MATERIAL VALIDATION")
	);

	Int32 materialPolygonCount = 0;

	for (
		size_t i = 0;
		i < materials.size();
		++i
		)
	{
		if (materials[i].polygonCount < 0)
		{
			GePrint(
				"PMX MATERIAL VALIDATION : NEGATIVE POLYGON COUNT"
			);

			return false;
		}

		materialPolygonCount +=
			materials[i].polygonCount;
	}

	GePrint(
		"PMX GEOMETRY POLYGON COUNT : " +
		String::IntToString(
			geometryPolygonCount
		)
	);

	GePrint(
		"PMX MATERIAL POLYGON COUNT : " +
		String::IntToString(
			materialPolygonCount
		)
	);

	if (
		geometryPolygonCount !=
		materialPolygonCount
		)
	{
		GePrint(
			"PMX MATERIAL VALIDATION : POLYGON COUNT MISMATCH"
		);

		return false;
	}

	Int32 expectedStart = 0;

	for (
		size_t i = 0;
		i < materials.size();
		++i
		)
	{
		if (
			materials[i].polygonStart !=
			expectedStart
			)
		{
			GePrint(
				"PMX MATERIAL VALIDATION : POLYGON START MISMATCH"
			);

			return false;
		}

		expectedStart +=
			materials[i].polygonCount;
	}

	GePrint(
		"PMX MATERIAL VALIDATION : OK"
	);

	return true;
}


// ============================================================
// Build Combined Object
// ============================================================

PolygonObject* PMXReader::BuildCombinedObject(
	const std::vector<PMXVertex>& vertices,
	const std::vector<Int32>& indices,
	Float scale,
	BaseDocument* doc,
	const std::vector<BaseObject*>& boneObjects
)
{
	const Int32 pointCount =
		static_cast<Int32>(
			vertices.size()
			);

	const Int32 polygonCount =
		static_cast<Int32>(
			indices.size() / 3
			);

	PolygonObject* object =
		PolygonObject::Alloc(
			pointCount,
			polygonCount
		);

	if (!object)
		return nullptr;

	Vector* points =
		object->GetPointW();

	if (!points)
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	for (
		Int32 i = 0;
		i < pointCount;
		++i
		)
	{
		points[i] =
			vertices[
				static_cast<size_t>(
					i
					)
			].position *
			scale;
	}

	CPolygon* polygons =
		object->GetPolygonW();

	if (!polygons)
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	for (
		Int32 i = 0;
		i < polygonCount;
		++i
		)
	{
		const Int32 base =
			i * 3;

		const Int32 a =
			indices[
				static_cast<size_t>(
					base
					)
			];

		const Int32 b =
			indices[
				static_cast<size_t>(
					base + 1
					)
			];

		const Int32 c =
			indices[
				static_cast<size_t>(
					base + 2
					)
			];

		polygons[i] =
			CPolygon(
				a,
				b,
				c,
				c
			);
	}

	if (!CreateNormalTag(
		object,
		vertices,
		indices
	))
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	if (!CreateUVW(
		object,
		vertices,
		indices
	))
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	if (!CreateSmoothTag(
		object
	))
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	std::vector<Int32> localToGlobal;

	try
	{
		localToGlobal.resize(
			vertices.size()
		);
	}
	catch (...)
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	for (
		size_t i = 0;
		i < vertices.size();
		++i
		)
	{
		localToGlobal[i] =
			static_cast<Int32>(
				i
				);
	}

	if (!boneObjects.empty())
	{
		if (!PMXBoneBuilder::CreateWeightTag(
			object,
			doc,
			vertices,
			localToGlobal,
			boneObjects
		))
		{
			PolygonObject::Free(
				object
			);

			return nullptr;
		}
	}

	if (!PMXMorphBuilder::CreateVertexMorphTag(
		object,
		doc,
		_morphs,
		localToGlobal,
		vertices,
		scale
	))
	{
		GePrint(
			"PMX VERTEX MORPH : BUILD FAILED"
		);

		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	return object;
}


// ============================================================
// Build Material Separated Object
// ============================================================

PolygonObject* PMXReader::BuildMaterialObject(
	const std::vector<PMXVertex>& vertices,
	const std::vector<Int32>& indices,
	const PMXMaterial& material,
	Float scale,
	BaseDocument* doc,
	const std::vector<BaseObject*>& boneObjects
)
{
	const Int32 polygonCount =
		material.polygonCount;

	if (polygonCount <= 0)
		return nullptr;

	const Int32 globalPolygonStart =
		material.polygonStart;

	const Int32 globalIndexStart =
		globalPolygonStart * 3;

	std::vector<Int32> localIndices;

	try
	{
		localIndices.resize(
			static_cast<size_t>(
				polygonCount * 3
				)
		);
	}
	catch (...)
	{
		return nullptr;
	}

	std::vector<Int32> globalToLocal;

	try
	{
		globalToLocal.resize(
			vertices.size()
		);
	}
	catch (...)
	{
		return nullptr;
	}

	for (
		size_t i = 0;
		i < globalToLocal.size();
		++i
		)
	{
		globalToLocal[i] = -1;
	}

	std::vector<Int32> localToGlobal;

	for (
		Int32 i = 0;
		i < polygonCount * 3;
		++i
		)
	{
		const Int32 globalIndex =
			indices[
				static_cast<size_t>(
					globalIndexStart + i
					)
			];

		if (
			globalIndex < 0 ||
			globalIndex >=
			static_cast<Int32>(
				vertices.size()
				)
			)
		{
			return nullptr;
		}

		Int32 localIndex =
			globalToLocal[
				static_cast<size_t>(
					globalIndex
					)
			];

		if (localIndex < 0)
		{
			localIndex =
				static_cast<Int32>(
					localToGlobal.size()
					);

			globalToLocal[
				static_cast<size_t>(
					globalIndex
					)
			] =
				localIndex;

				localToGlobal.push_back(
					globalIndex
				);
		}

		localIndices[
			static_cast<size_t>(
				i
				)
		] =
			localIndex;
	}

	const Int32 pointCount =
		static_cast<Int32>(
			localToGlobal.size()
			);

	PolygonObject* object =
		PolygonObject::Alloc(
			pointCount,
			polygonCount
		);

	if (!object)
		return nullptr;

	Vector* points =
		object->GetPointW();

	if (!points)
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	for (
		Int32 i = 0;
		i < pointCount;
		++i
		)
	{
		const Int32 globalIndex =
			localToGlobal[
				static_cast<size_t>(
					i
					)
			];

		points[i] =
			vertices[
				static_cast<size_t>(
					globalIndex
					)
			].position *
			scale;
	}

	CPolygon* polygons =
		object->GetPolygonW();

	if (!polygons)
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	for (
		Int32 i = 0;
		i < polygonCount;
		++i
		)
	{
		const Int32 base =
			i * 3;

		const Int32 a =
			localIndices[
				static_cast<size_t>(
					base
					)
			];

		const Int32 b =
			localIndices[
				static_cast<size_t>(
					base + 1
					)
			];

		const Int32 c =
			localIndices[
				static_cast<size_t>(
					base + 2
					)
			];

		polygons[i] =
			CPolygon(
				a,
				b,
				c,
				c
			);
	}

	std::vector<Int32> normalUVIndices;

	try
	{
		normalUVIndices.resize(
			localIndices.size()
		);
	}
	catch (...)
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	for (
		size_t i = 0;
		i < localIndices.size();
		++i
		)
	{
		const Int32 localIndex =
			localIndices[i];

		normalUVIndices[i] =
			localToGlobal[
				static_cast<size_t>(
					localIndex
					)
			];
	}

	if (!CreateNormalTag(
		object,
		vertices,
		normalUVIndices
	))
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	if (!CreateUVW(
		object,
		vertices,
		normalUVIndices
	))
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	if (!CreateSmoothTag(
		object
	))
	{
		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	if (!boneObjects.empty())
	{
		if (!PMXBoneBuilder::CreateWeightTag(
			object,
			doc,
			vertices,
			localToGlobal,
			boneObjects
		))
		{
			PolygonObject::Free(
				object
			);

			return nullptr;
		}
	}

	if (!PMXMorphBuilder::CreateVertexMorphTag(
		object,
		doc,
		_morphs,
		localToGlobal,
		vertices,
		scale
	))
	{
		GePrint(
			"PMX VERTEX MORPH : BUILD FAILED"
		);

		PolygonObject::Free(
			object
		);

		return nullptr;
	}

	return object;
}


// ============================================================
// Load
// ============================================================

Bool PMXReader::Load(
	const Filename& filename,
	BaseDocument* doc,
	const PMXImportSettings& settings
)
{
	if (!doc)
		return false;

	if (!Open(
		filename
	))
		return false;

	if (!ReadHeader())
		return false;

	if (!ReadModelInfo())
		return false;

	std::vector<PMXVertex> vertices;

	if (!ReadVertices(
		vertices
	))
		return false;

	std::vector<Int32> indices;

	if (!ReadFaces(
		vertices,
		indices
	))
	{
		return false;
	}

	if (!ValidateFaces(
		vertices,
		indices
	))
	{
		return false;
	}

	std::vector<PMXTexture> textures;

	if (!ReadTextures(
		textures
	))
		return false;

	std::vector<PMXMaterial> materials;

	if (!ReadMaterials(
		materials
	))
		return false;

	const Int32 geometryPolygonCount =
		static_cast<Int32>(
			indices.size() / 3
			);

	if (!ValidateMaterials(
		materials,
		geometryPolygonCount
	))
	{
		return false;
	}

	std::vector<PMXBone> bones;

	if (!ReadBones(
		bones
	))
		return false;


	// ========================================================
	// STEP 10
	// Morph Section
	// ========================================================

	_morphs.clear();

	if (!ReadMorphs(
		_morphs
	))
	{
		return ReaderFailed(
			String("MORPHS")
		);
	}


	// ========================================================
	// Import Scale
	// ========================================================

	PMXImportSettings calculatedSettings =
		settings;

	calculatedSettings.actualScale =
		settings.importScale;

	GePrint(
		"PMX IMPORT MODE : " +
		String(
			settings.mode ==
			PMX_IMPORT_COMBINED
			?
			"COMBINED"
			:
			"MATERIAL SEPARATED"
		)
	);

	GePrint(
		"PMX IMPORT SCALE : " +
		String::FloatToString(
			calculatedSettings.actualScale
		)
	);

	std::vector<BaseObject*> boneObjects;

	if (!PMXBoneBuilder::BuildBones(
		doc,
		bones,
		calculatedSettings.actualScale,
		boneObjects
	))
	{
		return false;
	}


	// ========================================================
	// Combined
	// ========================================================

	if (
		settings.mode ==
		PMX_IMPORT_COMBINED
		)
	{
		PolygonObject* object =
			BuildCombinedObject(
				vertices,
				indices,
				calculatedSettings.actualScale,
				doc,
				boneObjects
			);

		if (!object)
			return false;

		for (
			Int32 materialIndex = 0;
			materialIndex <
			static_cast<Int32>(
				materials.size()
				);
			++materialIndex
			)
		{
			const PMXMaterial& pmxMaterial =
				materials[
					static_cast<size_t>(
						materialIndex
						)
				];

			if (!SetupMaterial(
				object,
				doc,
				filename,
				textures,
				pmxMaterial,
				materialIndex,
				pmxMaterial.polygonStart
			))
			{
				PolygonObject::Free(
					object
				);

				return false;
			}
		}

		object->SetName(
			String("PMX Model")
		);

		doc->InsertObject(
			object,
			nullptr,
			nullptr
		);

		doc->SetActiveObject(
			object
		);

		object->Message(
			MSG_UPDATE
		);

		GePrint(
			"PMX OBJECT MODE : COMBINED"
		);

		GePrint(
			"PMX POINT COUNT : " +
			String::IntToString(
				static_cast<Int32>(
					vertices.size()
					)
			)
		);

		GePrint(
			"PMX POLYGON COUNT : " +
			String::IntToString(
				geometryPolygonCount
			)
		);
	}


	// ========================================================
	// Material Separated
	// ========================================================

	else
	{
		GePrint(
			"PMX OBJECT MODE : MATERIAL SEPARATED"
		);

		Bool firstObject =
			true;

		for (
			Int32 materialIndex = 0;
			materialIndex <
			static_cast<Int32>(
				materials.size()
				);
			++materialIndex
			)
		{
			const PMXMaterial& pmxMaterial =
				materials[
					static_cast<size_t>(
						materialIndex
						)
				];

			if (pmxMaterial.polygonCount <= 0)
				continue;

			PolygonObject* object =
				BuildMaterialObject(
					vertices,
					indices,
					pmxMaterial,
					calculatedSettings.actualScale,
					doc,
					boneObjects
				);

			if (!object)
			{
				GePrint(
					"PMX MATERIAL OBJECT : BUILD FAILED"
				);

				return false;
			}

			String objectName =
				pmxMaterial.name;

			if (objectName == String())
			{
				objectName =
					String("PMX Material ") +
					String::IntToString(
						materialIndex
					);
			}

			object->SetName(
				objectName
			);

			if (!SetupMaterial(
				object,
				doc,
				filename,
				textures,
				pmxMaterial,
				materialIndex,
				0
			))
			{
				PolygonObject::Free(
					object
				);

				return false;
			}

			doc->InsertObject(
				object,
				nullptr,
				nullptr
			);

			if (firstObject)
			{
				doc->SetActiveObject(
					object
				);

				firstObject =
					false;
			}

			object->Message(
				MSG_UPDATE
			);

			GePrint(
				"PMX MATERIAL OBJECT : CREATED"
			);

			GePrint(
				"  MATERIAL INDEX = " +
				String::IntToString(
					materialIndex
				)
			);

			GePrint(
				"  NAME = " +
				objectName
			);

			GePrint(
				"  POLYGON COUNT = " +
				String::IntToString(
					pmxMaterial.polygonCount
				)
			);
		}
	}


	GePrint(
		"============================================================"
	);

	GePrint(
		"PMX LOAD : SUCCESS"
	);

	GePrint(
		"STEP 10 : "
		"STEP 09 BASE + "
		"PMX BONE HIERARCHY + "
		"CAWEIGHTTAG SKINNING + "
		"PMX VERTEX MORPH"
	);

	GePrint(
		"============================================================"
	);

	return true;
}
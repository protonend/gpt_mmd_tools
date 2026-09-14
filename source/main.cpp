/*
main.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader - STEP 07

処理内容：
PMXファイルをCinema 4D R19の
Filename / BaseFile経由で直接読み込み、

1. PMXファイルオープン
2. PMXヘッダー解析
3. モデル情報解析
4. 頂点情報解析
5. 三角形インデックス解析
6. PMX Texture解析
7. PMX Material解析
8. C4D PolygonObject生成
9. PMX Vertex Normal → C4D NormalTag
10. PMX Vertex UV → C4D UVWTag
11. PMX Material → C4D BaseMaterial
12. PMX Material Name → C4D Material Name
13. PMX Texture Path → 絶対パスで実ファイル確認
14. PMX Texture Path → 相対FilenameとしてBitmap Shaderへ保存
15. Bitmap Shader生成
16. Bitmap Shader → Material Color Shader
17. Bitmap Shader → Material Alpha Shader
18. Material Alpha Image Alpha → ON
19. PMX Diffuse Alpha → Material Alpha Color
20. TextureTag生成
21. MaterialごとのPolygon範囲へ割り当て
22. Material名と同じ名前のSelectionTag生成
23. TphongによるPhong / Smooth Tag生成
24. ドキュメントへ挿入

STEP 07変更内容：

・PMX Index読み取りをlibMMD準拠へ修正。
・1 byte Indexは0xFFだけを-1として扱う。
・2 byte Indexは0xFFFFだけを-1として扱う。
・通常の128～254 / 32768～65534等を負数化しない。
・Reader内部で現在の読み取りバイト位置を追跡する。
・Vertex / Face / Texture / Material各段階の失敗位置を診断する。
・Face Indexの読み取り失敗位置を診断する。
・Material各フィールドの読み取り失敗位置を診断する。
・PMX ReaderとC4D生成側の検証を分離する。
・既存STEP 06 FIX6のC4D生成処理を維持する。
・BDEF1 / BDEF2 / BDEF4 / SDEF / QDEFを維持する。
・Cinema 4D R19 / Visual Studio 2013互換。

参考基準：
MMD Tools / Blender
libMMD / PMXFile.cpp

重要：
PMXの可変Indexは「符号付き整数」ではない。

1 byte:
0x00～0xFE = 0～254
0xFF         = -1

2 byte:
0x0000～0xFFFE = 0～65534
0xFFFF         = -1

4 byte:
32bit値を読み込む。

テクスチャファイルの存在確認にはPMXファイル位置から
解決した絶対パスを使用する。

C4D Bitmap Shaderに保存するFilenameは
PMXに記録されている相対テクスチャパスを使用する。

Cinema 4D R19
Visual Studio 2013
C++
*/


#include "c4d.h"
#include "c4d_filterdata.h"
#include "main.h"

#include <vector>


// ============================================================
// PMX Vertex
// ============================================================

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

	/*
	Reader自身が消費したバイト数を追跡する。

	これはBaseFileの実装依存APIに頼らず、
	現在のPMXストリーム位置を診断するための
	内部カウンタである。
	*/
	UInt64 _readOffset;


	/*
	現在のReader処理段階。

	例：

	HEADER
	MODEL INFO
	VERTEX
	FACE
	TEXTURE
	MATERIAL
	*/
	String _stage;


public:

	PMXReader()
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


	~PMXReader()
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


private:

	// ========================================================
	// Stage
	// ========================================================

	void SetStage(
		const String& stage
	)
	{
		_stage = stage;
	}


	// ========================================================
	// Offset String
	// ========================================================

	String GetOffsetString() const
	{
		return String::IntToString(
			static_cast<Int32>(
				_readOffset
				)
		);
	}


	// ========================================================
	// Diagnostic Failure
	// ========================================================

	Bool ReaderFailed(
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


	// ========================================================
	// Open
	// ========================================================

	Bool Open(
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


	// ========================================================
	// Read Bytes
	// ========================================================

	Bool ReadBytes(
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


	// ========================================================
	// Read UChar
	// ========================================================

	Bool ReadUChar(
		UChar& value
	)
	{
		return ReadBytes(
			&value,
			sizeof(UChar)
		);
	}


	// ========================================================
	// Read Int16
	// ========================================================

	Bool ReadInt16(
		Int16& value
	)
	{
		return ReadBytes(
			&value,
			sizeof(Int16)
		);
	}


	// ========================================================
	// Read Int32
	// ========================================================

	Bool ReadInt32(
		Int32& value
	)
	{
		return ReadBytes(
			&value,
			sizeof(Int32)
		);
	}


	// ========================================================
	// Read UInt16
	// ========================================================

	Bool ReadUInt16(
		UInt16& value
	)
	{
		return ReadBytes(
			&value,
			sizeof(UInt16)
		);
	}


	// ========================================================
	// Read UInt32
	// ========================================================

	Bool ReadUInt32(
		UInt32& value
	)
	{
		return ReadBytes(
			&value,
			sizeof(UInt32)
		);
	}


	// ========================================================
	// Read Float32
	// ========================================================

	Bool ReadFloat32(
		Float32& value
	)
	{
		return ReadBytes(
			&value,
			sizeof(Float32)
		);
	}


	// ========================================================
	// Read Vector3
	// ========================================================

	Bool ReadVector3(
		Vector& value
	)
	{
		Float32 x;
		Float32 y;
		Float32 z;


		if (!ReadFloat32(x))
			return false;

		if (!ReadFloat32(y))
			return false;

		if (!ReadFloat32(z))
			return false;


		value =
			Vector(
				x,
				y,
				z
			);


		return true;
	}


	// ========================================================
	// Read Additional UV
	// ========================================================

	Bool ReadAdditionalUV()
	{
		Float32 x;
		Float32 y;
		Float32 z;
		Float32 w;


		if (!ReadFloat32(x))
			return false;

		if (!ReadFloat32(y))
			return false;

		if (!ReadFloat32(z))
			return false;

		if (!ReadFloat32(w))
			return false;


		return true;
	}


	// ========================================================
	// Valid Index Size
	// ========================================================

	Bool IsValidIndexSize(
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


	// ========================================================
	// Read PMX Index
	// ========================================================

	Bool ReadIndex(
		UChar indexSize,
		Int32& value
	)
	{
		/*
		重要：

		PMXの1byte / 2byte Indexは
		通常のsigned integerとして読むものではない。

		PMX仕様：

		1 byte:
		0x00～0xFE = 正のIndex
		0xFF       = -1

		2 byte:
		0x0000～0xFFFE = 正のIndex
		0xFFFF         = -1
		*/


		if (indexSize == 1)
		{
			UChar v;


			if (!ReadUChar(v))
				return false;


			if (v == 0xFF)
			{
				value = -1;
			}
			else
			{
				value =
					static_cast<Int32>(v);
			}


			return true;
		}


		if (indexSize == 2)
		{
			UInt16 v;


			if (!ReadUInt16(v))
				return false;


			if (v == 0xFFFF)
			{
				value = -1;
			}
			else
			{
				value =
					static_cast<Int32>(v);
			}


			return true;
		}


		if (indexSize == 4)
		{
			UInt32 v;


			if (!ReadUInt32(v))
				return false;


			/*
			PMX 4byte Indexは32bit値。

			PMX仕様上の未指定値として
			0xFFFFFFFFを-1相当として扱えるようにする。
			*/

			if (v == 0xFFFFFFFFu)
			{
				value = -1;
			}
			else
			{
				value =
					static_cast<Int32>(v);
			}


			return true;
		}


		return false;
	}


	// ========================================================
	// Vertex Failure Diagnostic
	// ========================================================

	Bool VertexReadFailed(
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


	// ========================================================
	// PMX String
	// ========================================================

	Bool ReadPMXString(
		String* result = nullptr
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


		if ((byteLength & 1) != 0 &&
			_encoding == 0)
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
				static_cast<size_t>(i)
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


		// ----------------------------------------------------
		// UTF-16LE
		// ----------------------------------------------------

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
					static_cast<size_t>(i)
				] =
					static_cast<Utf16Char>(
						static_cast<UInt32>(lo) |
						(
							static_cast<UInt32>(hi)
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


		// ----------------------------------------------------
		// UTF-8
		// ----------------------------------------------------

		*result =
			String(
				reinterpret_cast<const Char*>(
					&buffer[0]
					),
				STRINGENCODING_UTF8
			);


		return true;
	}


	// ========================================================
	// Header
	// ========================================================

	Bool ReadHeader()
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


		if (signature[0] != 'P' ||
			signature[1] != 'M' ||
			signature[2] != 'X' ||
			signature[3] != ' ')
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


	// ========================================================
	// Model Information
	// ========================================================

	Bool ReadModelInfo()
	{
		SetStage(
			String("MODEL INFO")
		);


		if (!ReadPMXString())
			return ReaderFailed(
				String("MODEL NAME")
			);


		if (!ReadPMXString())
			return ReaderFailed(
				String("MODEL NAME UNIVERSAL")
			);


		if (!ReadPMXString())
			return ReaderFailed(
				String("COMMENT")
			);


		if (!ReadPMXString())
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


	// ========================================================
	// Vertices
	// ========================================================

	Bool ReadVertices(
		std::vector<PMXVertex>& vertices
	)
	{
		SetStage(
			String("VERTEX")
		);


		Int32 vertexCount;


		if (!ReadInt32(
			vertexCount
		))
		{
			return ReaderFailed(
				String("VERTEX COUNT READ")
			);
		}


		if (vertexCount <= 0)
		{
			return ReaderFailed(
				String("INVALID VERTEX COUNT")
			);
		}


		try
		{
			vertices.resize(
				static_cast<size_t>(
					vertexCount
					)
			);
		}
		catch (...)
		{
			return ReaderFailed(
				String("VERTEX VECTOR ALLOC")
			);
		}


		GePrint(
			"PMX VERTEX COUNT : " +
			String::IntToString(
				vertexCount
			)
		);


		for (
			Int32 i = 0;
			i < vertexCount;
			++i
			)
		{
			PMXVertex& vertex =
				vertices[
					static_cast<size_t>(i)
				];


			if (!ReadVector3(
				vertex.position
			))
			{
				return VertexReadFailed(
					i,
					String("POSITION"),
					0,
					false
				);
			}


			if (!ReadVector3(
				vertex.normal
			))
			{
				return VertexReadFailed(
					i,
					String("NORMAL"),
					0,
					false
				);
			}


			Float32 u;
			Float32 v;


			if (!ReadFloat32(u))
			{
				return VertexReadFailed(
					i,
					String("UV U"),
					0,
					false
				);
			}


			if (!ReadFloat32(v))
			{
				return VertexReadFailed(
					i,
					String("UV V"),
					0,
					false
				);
			}


			vertex.uv =
				Vector(
					u,
					v,
					0.0
				);


			for (
				Int32 uvIndex = 0;
				uvIndex <
				static_cast<Int32>(
					_additionalUV
					);
				++uvIndex
				)
			{
				if (!ReadAdditionalUV())
				{
					return VertexReadFailed(
						i,
						String("ADDITIONAL UV ") +
						String::IntToString(
							uvIndex
						),
						0,
						false
					);
				}
			}


			UChar weightType;


			if (!ReadUChar(
				weightType
			))
			{
				return VertexReadFailed(
					i,
					String("WEIGHT TYPE"),
					0,
					false
				);
			}


			// ------------------------------------------------
			// BDEF1
			// ------------------------------------------------

			if (weightType == 0)
			{
				Int32 boneIndex;


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF1 BONE"),
						weightType,
						true
					);
				}
			}


			// ------------------------------------------------
			// BDEF2
			// ------------------------------------------------

			else if (weightType == 1)
			{
				Int32 boneIndex1;
				Int32 boneIndex2;

				Float32 weight;


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex1
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF2 BONE1"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex2
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF2 BONE2"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(
					weight
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF2 WEIGHT"),
						weightType,
						true
					);
				}
			}


			// ------------------------------------------------
			// BDEF4
			// ------------------------------------------------

			else if (weightType == 2)
			{
				Int32 boneIndex1;
				Int32 boneIndex2;
				Int32 boneIndex3;
				Int32 boneIndex4;

				Float32 weight1;
				Float32 weight2;
				Float32 weight3;
				Float32 weight4;


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex1
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 BONE1"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex2
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 BONE2"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex3
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 BONE3"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex4
				))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 BONE4"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight1))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 WEIGHT1"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight2))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 WEIGHT2"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight3))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 WEIGHT3"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight4))
				{
					return VertexReadFailed(
						i,
						String("BDEF4 WEIGHT4"),
						weightType,
						true
					);
				}
			}


			// ------------------------------------------------
			// SDEF
			// ------------------------------------------------

			else if (weightType == 3)
			{
				Int32 boneIndex1;
				Int32 boneIndex2;

				Float32 weight;

				Vector c;
				Vector r0;
				Vector r1;


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex1
				))
				{
					return VertexReadFailed(
						i,
						String("SDEF BONE1"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex2
				))
				{
					return VertexReadFailed(
						i,
						String("SDEF BONE2"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(
					weight
				))
				{
					return VertexReadFailed(
						i,
						String("SDEF WEIGHT"),
						weightType,
						true
					);
				}


				if (!ReadVector3(c))
				{
					return VertexReadFailed(
						i,
						String("SDEF C"),
						weightType,
						true
					);
				}


				if (!ReadVector3(r0))
				{
					return VertexReadFailed(
						i,
						String("SDEF R0"),
						weightType,
						true
					);
				}


				if (!ReadVector3(r1))
				{
					return VertexReadFailed(
						i,
						String("SDEF R1"),
						weightType,
						true
					);
				}
			}


			// ------------------------------------------------
			// QDEF
			// ------------------------------------------------

			else if (weightType == 4)
			{
				Int32 boneIndex1;
				Int32 boneIndex2;
				Int32 boneIndex3;
				Int32 boneIndex4;

				Float32 weight1;
				Float32 weight2;
				Float32 weight3;
				Float32 weight4;


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex1
				))
				{
					return VertexReadFailed(
						i,
						String("QDEF BONE1"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex2
				))
				{
					return VertexReadFailed(
						i,
						String("QDEF BONE2"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex3
				))
				{
					return VertexReadFailed(
						i,
						String("QDEF BONE3"),
						weightType,
						true
					);
				}


				if (!ReadIndex(
					_boneIndexSize,
					boneIndex4
				))
				{
					return VertexReadFailed(
						i,
						String("QDEF BONE4"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight1))
				{
					return VertexReadFailed(
						i,
						String("QDEF WEIGHT1"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight2))
				{
					return VertexReadFailed(
						i,
						String("QDEF WEIGHT2"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight3))
				{
					return VertexReadFailed(
						i,
						String("QDEF WEIGHT3"),
						weightType,
						true
					);
				}


				if (!ReadFloat32(weight4))
				{
					return VertexReadFailed(
						i,
						String("QDEF WEIGHT4"),
						weightType,
						true
					);
				}
			}


			else
			{
				return VertexReadFailed(
					i,
					String("UNKNOWN WEIGHT TYPE"),
					weightType,
					true
				);
			}


			// ------------------------------------------------
			// Edge Scale
			// ------------------------------------------------

			Float32 edgeScale;


			if (!ReadFloat32(
				edgeScale
			))
			{
				return VertexReadFailed(
					i,
					String("EDGE SCALE"),
					weightType,
					true
				);
			}
		}


		GePrint(
			"PMX VERTEX END OFFSET : " +
			GetOffsetString()
		);


		GePrint(
			"PMX VERTEX : OK"
		);


		return true;
	}


	// ========================================================
	// Faces
	// ========================================================

	Bool ReadFaces(
		const std::vector<PMXVertex>& vertices,
		std::vector<Int32>& indices
	)
	{
		SetStage(
			String("FACE")
		);


		Int32 indexCount;


		if (!ReadInt32(
			indexCount
		))
		{
			return ReaderFailed(
				String("FACE COUNT READ")
			);
		}


		if (indexCount <= 0)
		{
			return ReaderFailed(
				String("INVALID FACE COUNT")
			);
		}


		GePrint(
			"PMX FACE INDEX COUNT : " +
			String::IntToString(
				indexCount
			)
		);


		GePrint(
			"PMX FACE VERTEX INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(
					_vertexIndexSize
					)
			)
		);


		/*
		libMMDではfaceCount / 3を
		triangle countとして扱う。

		正常なPMXでは必ず3の倍数になる。

		ここでは即座にReaderを停止させず、
		診断情報を出してfloor(indexCount / 3)個の
		三角形を読む。
		*/

		if ((indexCount % 3) != 0)
		{
			GePrint(
				"PMX FACE WARNING : INDEX COUNT IS NOT DIVISIBLE BY 3"
			);
		}


		const Int32 triangleCount =
			indexCount / 3;


		if (triangleCount <= 0)
		{
			return ReaderFailed(
				String("FACE TRIANGLE COUNT = 0")
			);
		}


		const Int32 readableIndexCount =
			triangleCount * 3;


		try
		{
			indices.resize(
				static_cast<size_t>(
					readableIndexCount
					)
			);
		}
		catch (...)
		{
			return ReaderFailed(
				String("FACE VECTOR ALLOC")
			);
		}


		for (
			Int32 i = 0;
			i < readableIndexCount;
			++i
			)
		{
			Int32 index;


			const UInt64 indexOffset =
				_readOffset;


			if (!ReadIndex(
				_vertexIndexSize,
				index
			))
			{
				GePrint(
					"PMX FACE INDEX READ FAILED"
				);

				GePrint(
					"  INDEX = " +
					String::IntToString(
						i
					)
				);

				GePrint(
					"  TRIANGLE = " +
					String::IntToString(
						i / 3
					)
				);

				GePrint(
					"  OFFSET = " +
					String::IntToString(
						static_cast<Int32>(
							indexOffset
							)
					)
				);

				return false;
			}


			/*
			Readerでは値を取得する。

			ここではまだ範囲エラーで
			Readerそのものを停止しない。

			Validatorで後段にまとめて確認する。
			*/

			indices[
				static_cast<size_t>(i)
			] = index;
		}


		GePrint(
			"PMX FACE END OFFSET : " +
			GetOffsetString()
		);


		GePrint(
			"PMX TRIANGLE COUNT : " +
			String::IntToString(
				triangleCount
			)
		);


		/*
		最初と最後のIndexを診断する。

		これにより1byte Indexの128～254が
		負数化されていた問題を確認しやすくする。
		*/

		if (!indices.empty())
		{
			GePrint(
				"PMX FACE FIRST INDEX = " +
				String::IntToString(
					indices[0]
				)
			);


			GePrint(
				"PMX FACE LAST INDEX = " +
				String::IntToString(
					indices[
						indices.size() - 1
					]
				)
			);
		}


		GePrint(
			"PMX FACE : OK"
		);


		return true;
	}


	// ========================================================
	// Face Validation
	// ========================================================

	Bool ValidateFaces(
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices
	)
	{
		SetStage(
			String("FACE VALIDATION")
		);


		const Int32 vertexCount =
			static_cast<Int32>(
				vertices.size()
				);


		for (
			size_t i = 0;
			i < indices.size();
			++i
			)
		{
			const Int32 index =
				indices[i];


			if (index < 0)
			{
				GePrint(
					"PMX FACE VALIDATION : INVALID NEGATIVE INDEX"
				);

				GePrint(
					"  INDEX POSITION = " +
					String::IntToString(
						static_cast<Int32>(
							i
							)
					)
				);

				GePrint(
					"  VALUE = " +
					String::IntToString(
						index
					)
				);

				return false;
			}


			if (index >= vertexCount)
			{
				GePrint(
					"PMX FACE VALIDATION : INDEX OUT OF RANGE"
				);

				GePrint(
					"  INDEX POSITION = " +
					String::IntToString(
						static_cast<Int32>(
							i
							)
					)
				);

				GePrint(
					"  VALUE = " +
					String::IntToString(
						index
					)
				);

				GePrint(
					"  VERTEX COUNT = " +
					String::IntToString(
						vertexCount
					)
				);

				return false;
			}
		}


		GePrint(
			"PMX FACE VALIDATION : OK"
		);


		return true;
	}


	// ========================================================
	// Textures
	// ========================================================

	Bool ReadTextures(
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
		{
			return ReaderFailed(
				String("TEXTURE COUNT READ")
			);
		}


		if (textureCount < 0)
		{
			return ReaderFailed(
				String("NEGATIVE TEXTURE COUNT")
			);
		}


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
					static_cast<size_t>(i)
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
						String::IntToString(i) +
						"] = " +
						textures[
							static_cast<size_t>(i)
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


	// ========================================================
	// Materials
	// ========================================================

	Bool ReadMaterials(
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
		{
			return ReaderFailed(
				String("MATERIAL COUNT READ")
			);
		}


		if (materialCount < 0)
		{
			return ReaderFailed(
				String("NEGATIVE MATERIAL COUNT")
			);
		}


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
					static_cast<size_t>(i)
				];


			material.polygonStart =
				polygonStart;


			// ------------------------------------------------
			// Name
			// ------------------------------------------------

			if (!ReadPMXString(
				&material.name
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
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


			// ------------------------------------------------
			// Universal Name
			// ------------------------------------------------

			if (!ReadPMXString(
				&material.nameUniversal
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
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


			// ------------------------------------------------
			// Diffuse
			// ------------------------------------------------

			if (!ReadVector3(
				material.diffuse
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : DIFFUSE"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				GePrint(
					"  OFFSET = " +
					GetOffsetString()
				);

				return false;
			}


			if (!ReadFloat32(
				material.diffuseAlpha
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : DIFFUSE ALPHA"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Specular
			// ------------------------------------------------

			if (!ReadVector3(
				material.specular
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : SPECULAR"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			if (!ReadFloat32(
				material.specularPower
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : SPECULAR POWER"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Ambient
			// ------------------------------------------------

			if (!ReadVector3(
				material.ambient
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : AMBIENT"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Draw Flags
			// ------------------------------------------------

			if (!ReadUChar(
				material.drawFlags
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : DRAW FLAGS"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Edge Color
			// ------------------------------------------------

			if (!ReadVector3(
				material.edgeColor
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : EDGE COLOR"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			if (!ReadFloat32(
				material.edgeAlpha
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : EDGE ALPHA"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			if (!ReadFloat32(
				material.edgeSize
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : EDGE SIZE"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Texture Index
			// ------------------------------------------------

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
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Sphere Texture Index
			// ------------------------------------------------

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
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Sphere Mode
			// ------------------------------------------------

			if (!ReadUChar(
				material.sphereMode
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : SPHERE MODE"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Toon Flag
			// ------------------------------------------------

			if (!ReadUChar(
				material.toonFlag
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : TOON FLAG"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Toon Texture
			// ------------------------------------------------

			if (material.toonFlag == 0)
			{
				if (!ReadIndex(
					_textureIndexSize,
					material.toonTextureIndex
				))
				{
					GePrint(
						"PMX MATERIAL READ FAILED : TOON TEXTURE INDEX"
					);

					GePrint(
						"  MATERIAL = " +
						String::IntToString(i)
					);

					return false;
				}
			}
			else
			{
				UChar toonIndex;


				if (!ReadUChar(
					toonIndex
				))
				{
					GePrint(
						"PMX MATERIAL READ FAILED : TOON SHARED INDEX"
					);

					GePrint(
						"  MATERIAL = " +
						String::IntToString(i)
					);

					return false;
				}


				material.toonTextureIndex =
					static_cast<Int32>(
						toonIndex
						);
			}


			// ------------------------------------------------
			// Memo
			// ------------------------------------------------

			if (!ReadPMXString(
				&material.memo
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : MEMO"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			// ------------------------------------------------
			// Face Vertex Count
			// ------------------------------------------------

			if (!ReadInt32(
				material.faceVertexCount
			))
			{
				GePrint(
					"PMX MATERIAL READ FAILED : FACE VERTEX COUNT"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			if (material.faceVertexCount < 0)
			{
				GePrint(
					"PMX MATERIAL READ FAILED : NEGATIVE FACE VERTEX COUNT"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);

				return false;
			}


			if ((material.faceVertexCount % 3) != 0)
			{
				GePrint(
					"PMX MATERIAL WARNING : FACE VERTEX COUNT NOT DIVISIBLE BY 3"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(i)
				);
			}


			material.polygonCount =
				material.faceVertexCount / 3;


			polygonStart +=
				material.polygonCount;


			// ------------------------------------------------
			// Material Diagnostic
			// ------------------------------------------------

			GePrint(
				"PMX MATERIAL[" +
				String::IntToString(i) +
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


	// ========================================================
	// Material Validation
	// ========================================================

	Bool ValidateMaterials(
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


		if (geometryPolygonCount !=
			materialPolygonCount)
		{
			GePrint(
				"PMX MATERIAL VALIDATION : POLYGON COUNT MISMATCH"
			);

			return false;
		}


		/*
		Material polygonStartの範囲も検証する。
		*/

		Int32 expectedStart = 0;


		for (
			size_t i = 0;
			i < materials.size();
			++i
			)
		{
			if (materials[i].polygonStart !=
				expectedStart)
			{
				GePrint(
					"PMX MATERIAL VALIDATION : POLYGON START MISMATCH"
				);

				GePrint(
					"  MATERIAL = " +
					String::IntToString(
						static_cast<Int32>(i)
					)
				);

				GePrint(
					"  STORED START = " +
					String::IntToString(
						materials[i].polygonStart
					)
				);

				GePrint(
					"  EXPECTED START = " +
					String::IntToString(
						expectedStart
					)
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


	// ========================================================
	// Normal Tag
	// ========================================================

	Bool CreateNormalTag(
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
			NormalTag::Free(
				normalTag
			);

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
							static_cast<size_t>(
								base
								)
						]
						)
				].normal;


			const Vector& normalB =
				vertices[
					static_cast<size_t>(
						indices[
							static_cast<size_t>(
								base + 1
								)
						]
						)
				].normal;


			const Vector& normalC =
				vertices[
					static_cast<size_t>(
						indices[
							static_cast<size_t>(
								base + 2
								)
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


		GePrint(
			"PMX NORMAL : APPLIED"
		);


		GePrint(
			"PMX NORMAL POLYGON COUNT : " +
			String::IntToString(
				polygonCount
			)
		);


		GePrint(
			"PMX NORMAL VERTEX COUNT : " +
			String::IntToString(
				static_cast<Int32>(
					vertices.size()
					)
			)
		);


		return true;
	}


	// ========================================================
	// UVW Tag
	// ========================================================

	Bool CreateUVW(
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
							static_cast<size_t>(
								base
								)
						]
						)
				].uv;


			const Vector& uvB =
				vertices[
					static_cast<size_t>(
						indices[
							static_cast<size_t>(
								base + 1
								)
						]
						)
				].uv;


			const Vector& uvC =
				vertices[
					static_cast<size_t>(
						indices[
							static_cast<size_t>(
								base + 2
								)
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


		GePrint(
			"PMX UV : APPLIED"
		);


		GePrint(
			"PMX UVW POLYGON COUNT : " +
			String::IntToString(
				polygonCount
			)
		);


		return true;
	}


	// ========================================================
	// Phong / Smooth Tag
	// ========================================================

	Bool CreateSmoothTag(
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


		GePrint(
			"PMX SMOOTH : ENABLED"
		);


		return true;
	}


	// ========================================================
	// Create C4D Material
	// ========================================================

	BaseMaterial* CreateC4DMaterial(
		const PMXMaterial& pmxMaterial,
		Int32 materialIndex
	)
	{
		BaseMaterial* material =
			BaseMaterial::Alloc(
				Mmaterial
			);


		if (!material)
		{
			GePrint(
				"PMX C4D MATERIAL : ALLOC FAILED"
			);

			return nullptr;
		}


		String materialName =
			pmxMaterial.name;


		if (materialName == String())
		{
			materialName =
				String(
					"PMX Material "
				) +
				String::IntToString(
					materialIndex
				);
		}


		material->SetName(
			materialName
		);


		material->SetParameter(
			DescID(
				MATERIAL_COLOR_COLOR
			),
			GeData(
				pmxMaterial.diffuse
			),
			DESCFLAGS_SET_0
		);


		GePrint(
			"PMX C4D MATERIAL : CREATED"
		);


		GePrint(
			"  MATERIAL INDEX = " +
			String::IntToString(
				materialIndex
			)
		);


		GePrint(
			"  MATERIAL NAME = " +
			materialName
		);


		GePrint(
			"  DIFFUSE COLOR APPLIED"
		);


		if (pmxMaterial.diffuseAlpha < 0.999f)
		{
			GePrint(
				"  PMX DIFFUSE ALPHA = " +
				String::FloatToString(
					static_cast<Float>(
						pmxMaterial.diffuseAlpha
						)
				)
			);
		}
		else
		{
			GePrint(
				"  PMX DIFFUSE ALPHA = 1.0"
			);
		}


		return material;
	}


	// ========================================================
	// Build Absolute Texture Filename
	// ========================================================

	Filename BuildTextureFilename(
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


	// ========================================================
	// Build Relative Texture Filename
	// ========================================================

	Filename BuildRelativeTextureFilename(
		const String& texturePath
	)
	{
		Filename relativeTextureFile;


		relativeTextureFile.SetString(
			texturePath
		);


		return relativeTextureFile;
	}


	// ========================================================
	// Create Bitmap Shader
	// ========================================================

	BaseShader* CreateBitmapShader(
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


		BaseShader* bitmapShader =
			BaseShader::Alloc(
				Xbitmap
			);


		if (!bitmapShader)
		{
			GePrint(
				"PMX BITMAP : SHADER ALLOC FAILED"
			);

			return nullptr;
		}


		if (!bitmapShader->SetParameter(
			DescID(
				BITMAPSHADER_FILENAME
			),
			GeData(
				relativeTextureFile
			),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(
				bitmapShader
			);


			GePrint(
				"PMX BITMAP : RELATIVE FILENAME SET FAILED"
			);


			return nullptr;
		}


		if (!material->SetParameter(
			DescID(
				MATERIAL_COLOR_SHADER
			),
			GeData(
				bitmapShader
			),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(
				bitmapShader
			);


			GePrint(
				"PMX BITMAP : COLOR SHADER LINK FAILED"
			);


			return nullptr;
		}


		if (!material->SetParameter(
			DescID(
				MATERIAL_USE_ALPHA
			),
			GeData(
				true
			),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(
				bitmapShader
			);


			GePrint(
				"PMX BITMAP : ALPHA CHANNEL ENABLE FAILED"
			);


			return nullptr;
		}


		if (!material->SetParameter(
			DescID(
				MATERIAL_ALPHA_SHADER
			),
			GeData(
				bitmapShader
			),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(
				bitmapShader
			);


			GePrint(
				"PMX BITMAP : ALPHA SHADER LINK FAILED"
			);


			return nullptr;
		}


		if (!material->SetParameter(
			DescID(
				MATERIAL_ALPHA_IMAGEALPHA
			),
			GeData(
				true
			),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(
				bitmapShader
			);


			GePrint(
				"PMX BITMAP : IMAGE ALPHA ENABLE FAILED"
			);


			return nullptr;
		}


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
			DescID(
				MATERIAL_ALPHA_COLOR
			),
			GeData(
				alphaColor
			),
			DESCFLAGS_SET_0
		);


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


		GePrint(
			"  COLOR SHADER LINK = OK"
		);


		GePrint(
			"  ALPHA SHADER LINK = OK"
		);


		GePrint(
			"  IMAGE ALPHA = ENABLED"
		);


		GePrint(
			"  PMX DIFFUSE ALPHA = " +
			String::FloatToString(
				alpha
			)
		);


		return bitmapShader;
	}


	// ========================================================
	// Create Material Selection Tag
	// ========================================================

	Bool CreateMaterialSelection(
		PolygonObject* object,
		const PMXMaterial& material,
		Int32 materialIndex,
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
				String(
					"PMX Material "
				) +
				String::IntToString(
					materialIndex
				);
		}


		selectionTag->SetName(
			selectionName
		);


		BaseSelect* selection =
			selectionTag->GetBaseSelect();


		if (!selection)
		{
			SelectionTag::Free(
				selectionTag
			);

			return false;
		}


		for (
			Int32 i = 0;
			i < material.polygonCount;
			++i
			)
		{
			selection->Select(
				material.polygonStart + i
			);
		}


		object->InsertTag(
			selectionTag
		);


		GePrint(
			"PMX MATERIAL SELECTION : CREATED"
		);


		GePrint(
			"  MATERIAL INDEX = " +
			String::IntToString(
				materialIndex
			)
		);


		GePrint(
			"  SELECTION NAME = " +
			selectionName
		);


		GePrint(
			"  POLYGON START = " +
			String::IntToString(
				material.polygonStart
			)
		);


		GePrint(
			"  POLYGON COUNT = " +
			String::IntToString(
				material.polygonCount
			)
		);


		return true;
	}


	// ========================================================
	// Create Material Texture Tag
	// ========================================================

	Bool CreateMaterialTag(
		PolygonObject* object,
		BaseMaterial* material,
		const PMXMaterial& pmxMaterial,
		Int32 materialIndex
	)
	{
		if (!object)
			return false;


		if (!material)
			return false;


		TextureTag* textureTag =
			TextureTag::Alloc();


		if (!textureTag)
		{
			GePrint(
				"PMX MATERIAL TAG : ALLOC FAILED"
			);

			return false;
		}


		textureTag->SetMaterial(
			material
		);


		textureTag->SetParameter(
			DescID(
				TEXTURETAG_PROJECTION
			),
			GeData(
				TEXTURETAG_PROJECTION_UVW
			),
			DESCFLAGS_SET_0
		);


		String selectionName;


		if (!CreateMaterialSelection(
			object,
			pmxMaterial,
			materialIndex,
			selectionName
		))
		{
			TextureTag::Free(
				textureTag
			);

			return false;
		}


		if (!textureTag->SetParameter(
			DescID(
				TEXTURETAG_RESTRICTION
			),
			GeData(
				selectionName
			),
			DESCFLAGS_SET_0
		))
		{
			TextureTag::Free(
				textureTag
			);

			GePrint(
				"PMX MATERIAL TAG : RESTRICTION SET FAILED"
			);

			return false;
		}


		object->InsertTag(
			textureTag
		);


		GePrint(
			"PMX MATERIAL TAG : CREATED"
		);


		GePrint(
			"  MATERIAL INDEX = " +
			String::IntToString(
				materialIndex
			)
		);


		GePrint(
			"  MATERIAL NAME = " +
			selectionName
		);


		GePrint(
			"  POLYGON START = " +
			String::IntToString(
				pmxMaterial.polygonStart
			)
		);


		GePrint(
			"  POLYGON COUNT = " +
			String::IntToString(
				pmxMaterial.polygonCount
			)
		);


		GePrint(
			"  RESTRICTION = " +
			selectionName
		);


		return true;
	}


public:

	// ========================================================
	// Load
	// ========================================================

	Bool Load(
		const Filename& filename,
		BaseDocument* doc
	)
	{
		if (!doc)
			return false;


		// ----------------------------------------------------
		// Open
		// ----------------------------------------------------

		if (!Open(
			filename
		))
		{
			return false;
		}


		// ----------------------------------------------------
		// Header
		// ----------------------------------------------------

		if (!ReadHeader())
			return false;


		// ----------------------------------------------------
		// Model Info
		// ----------------------------------------------------

		if (!ReadModelInfo())
			return false;


		// ----------------------------------------------------
		// Vertices
		// ----------------------------------------------------

		std::vector<PMXVertex> vertices;


		if (!ReadVertices(
			vertices
		))
		{
			return false;
		}


		// ----------------------------------------------------
		// Faces
		// ----------------------------------------------------

		std::vector<Int32> indices;


		if (!ReadFaces(
			vertices,
			indices
		))
		{
			return false;
		}


		// ----------------------------------------------------
		// Face Validation
		// ----------------------------------------------------

		if (!ValidateFaces(
			vertices,
			indices
		))
		{
			return false;
		}


		// ----------------------------------------------------
		// Textures
		// ----------------------------------------------------

		std::vector<PMXTexture> textures;


		if (!ReadTextures(
			textures
		))
		{
			return false;
		}


		// ----------------------------------------------------
		// Materials
		// ----------------------------------------------------

		std::vector<PMXMaterial> materials;


		if (!ReadMaterials(
			materials
		))
		{
			return false;
		}


		// ----------------------------------------------------
		// Polygon Count
		// ----------------------------------------------------

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


		GePrint(
			"PMX MATERIAL POLYGON RANGE : OK"
		);


		// ----------------------------------------------------
		// Polygon Object
		// ----------------------------------------------------

		const Int32 pointCount =
			static_cast<Int32>(
				vertices.size()
				);


		const Int32 polygonCount =
			geometryPolygonCount;


		PolygonObject* object =
			PolygonObject::Alloc(
				pointCount,
				polygonCount
			);


		if (!object)
			return false;


		// ----------------------------------------------------
		// Points
		// ----------------------------------------------------

		Vector* points =
			object->GetPointW();


		if (!points)
		{
			PolygonObject::Free(
				object
			);

			return false;
		}


		for (
			Int32 i = 0;
			i < pointCount;
			++i
			)
		{
			points[i] =
				vertices[
					static_cast<size_t>(i)
				].position;
		}


		// ----------------------------------------------------
		// Polygons
		// ----------------------------------------------------

		CPolygon* polygons =
			object->GetPolygonW();


		if (!polygons)
		{
			PolygonObject::Free(
				object
			);

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


			const Int32 a =
				indices[
					static_cast<size_t>(base)
				];


			const Int32 b =
				indices[
					static_cast<size_t>(base + 1)
				];


			const Int32 c =
				indices[
					static_cast<size_t>(base + 2)
				];


			polygons[i] =
				CPolygon(
					a,
					b,
					c,
					c
				);
		}


		// ----------------------------------------------------
		// Normal
		// ----------------------------------------------------

		if (!CreateNormalTag(
			object,
			vertices,
			indices
		))
		{
			PolygonObject::Free(
				object
			);

			return false;
		}


		// ----------------------------------------------------
		// UVW
		// ----------------------------------------------------

		if (!CreateUVW(
			object,
			vertices,
			indices
		))
		{
			PolygonObject::Free(
				object
			);

			return false;
		}


		// ----------------------------------------------------
		// Phong
		// ----------------------------------------------------

		if (!CreateSmoothTag(
			object
		))
		{
			PolygonObject::Free(
				object
			);

			return false;
		}


		// ----------------------------------------------------
		// C4D Materials
		// ----------------------------------------------------

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


			// ------------------------------------------------
			// Material
			// ------------------------------------------------

			BaseMaterial* material =
				CreateC4DMaterial(
					pmxMaterial,
					materialIndex
				);


			if (!material)
			{
				PolygonObject::Free(
					object
				);

				return false;
			}


			// ------------------------------------------------
			// Texture
			// ------------------------------------------------

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


					GePrint(
						"PMX MATERIAL TEXTURE : RESOLVED"
					);


					GePrint(
						"  TEXTURE INDEX = " +
						String::IntToString(
							pmxMaterial.textureIndex
						)
					);


					GePrint(
						"  PMX TEXTURE PATH = " +
						texturePath
					);


					GePrint(
						"  CHECK FILE = " +
						absoluteTextureFile.GetString()
					);


					GePrint(
						"  STORED PATH = " +
						relativeTextureFile.GetString()
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
				else
				{
					GePrint(
						"PMX MATERIAL TEXTURE : EMPTY PATH"
					);
				}
			}
			else
			{
				GePrint(
					"PMX MATERIAL TEXTURE : NONE"
				);
			}


			// ------------------------------------------------
			// Insert Material
			// ------------------------------------------------

			doc->InsertMaterial(
				material
			);


			// ------------------------------------------------
			// Material Tag
			// ------------------------------------------------

			if (!CreateMaterialTag(
				object,
				material,
				pmxMaterial,
				materialIndex
			))
			{
				return false;
			}
		}


		// ----------------------------------------------------
		// Object Name
		// ----------------------------------------------------

		object->SetName(
			String(
				"PMX Model"
			)
		);


		// ----------------------------------------------------
		// Insert Object
		// ----------------------------------------------------

		doc->InsertObject(
			object,
			nullptr,
			nullptr
		);


		// ----------------------------------------------------
		// Active Object
		// ----------------------------------------------------

		doc->SetActiveObject(
			object
		);


		object->Message(
			MSG_UPDATE
		);


		// ----------------------------------------------------
		// Result
		// ----------------------------------------------------

		GePrint(
			"PMX OBJECT : CREATED"
		);


		GePrint(
			"PMX POINT COUNT : " +
			String::IntToString(
				pointCount
			)
		);


		GePrint(
			"PMX POLYGON COUNT : " +
			String::IntToString(
				polygonCount
			)
		);


		GePrint(
			"PMX TEXTURE COUNT : " +
			String::IntToString(
				static_cast<Int32>(
					textures.size()
					)
			)
		);


		GePrint(
			"PMX MATERIAL COUNT : " +
			String::IntToString(
				static_cast<Int32>(
					materials.size()
					)
			)
		);


		GePrint(
			"PMX C4D MATERIALS : CREATED"
		);


		GePrint(
			"PMX BITMAP SHADERS : PROCESSED"
		);


		GePrint(
			"PMX MATERIAL SELECTIONS : CREATED"
		);


		GePrint(
			"PMX TEXTURE TAG RESTRICTIONS : APPLIED"
		);


		GePrint(
			"PMX RELATIVE TEXTURE PATHS : APPLIED"
		);


		GePrint(
			"PMX IMAGE ALPHA : LINKED"
		);


		GePrint(
			"PMX PHONG / SMOOTH : CREATED"
		);


		GePrint(
			"============================================================"
		);


		GePrint(
			"PMX LOAD : SUCCESS"
		);


		GePrint(
			"STEP 07 : "
			"PMX READER INDEX FIX + "
			"FACE/TEXTURE/MATERIAL OFFSET DIAGNOSTICS + "
			"GEOMETRY + UV + NORMAL + MATERIAL + "
			"RELATIVE BITMAP + ALPHA + MATERIAL NAME SELECTION + "
			"R19 PHONG COMPLETE"
		);


		GePrint(
			"============================================================"
		);


		return true;
	}
};


// ============================================================
// Identify
// ============================================================

Bool GPTMMDPMXLoader::Identify(
	BaseSceneLoader* node,
	const Filename& name,
	UChar* probe,
	Int32 size
)
{
	if (!probe)
		return false;


	if (size < 4)
		return false;


	if (probe[0] == 'P' &&
		probe[1] == 'M' &&
		probe[2] == 'X' &&
		probe[3] == ' ')
	{
		GePrint(
			"GPT MMD TOOLS : PMX IDENTIFIED"
		);

		return true;
	}


	return false;
}


// ============================================================
// Load
// ============================================================

FILEERROR GPTMMDPMXLoader::Load(
	BaseSceneLoader* node,
	const Filename& name,
	BaseDocument* doc,
	SCENEFILTER filterflags,
	String* error,
	BaseThread* bt
)
{
	GePrint(
		"============================================================"
	);


	GePrint(
		"GPT MMD TOOLS"
	);


	GePrint(
		"PMX SCENE LOADER - STEP 07"
	);


	GePrint(
		"Cinema 4D : R19"
	);


	GePrint(
		"============================================================"
	);


	GePrint(
		"File : " +
		name.GetString()
	);


	PMXReader reader;


	if (!reader.Load(
		name,
		doc
	))
	{
		GePrint(
			"PMX LOAD : FAILED"
		);


		if (error)
		{
			*error =
				String(
					"GPT MMD TOOLS : PMX LOAD FAILED"
				);
		}


		return FILEERROR_INVALID;
	}


	return FILEERROR_NONE;
}


// ============================================================
// Alloc
// ============================================================

NodeData* GPTMMDPMXLoader::Alloc()
{
	return NewObjClear(
		GPTMMDPMXLoader
	);
}


// ============================================================
// Plugin Start
// ============================================================

Bool PluginStart()
{
	if (!RegisterSceneLoaderPlugin(
		GPT_MMD_TOOLS_PMX_ID,
		String(
			"GPT MMD TOOLS - PMX"
		),
		0,
		GPTMMDPMXLoader::Alloc,
		String()
	))
	{
		return false;
	}


	GePrint(
		"GPT MMD TOOLS : PMX SCENE LOADER REGISTERED"
	);


	return true;
}


// ============================================================
// Plugin End
// ============================================================

void PluginEnd()
{
}


// ============================================================
// Plugin Message
// ============================================================

Bool PluginMessage(
	Int32 type,
	void* data
)
{
	switch (type)
	{
	case C4DPL_INIT_SYS:
	{
		return true;
	}
	}


	return false;
}
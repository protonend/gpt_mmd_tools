/*
main.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader - STEP 08

Cinema 4D R19
Visual Studio 2015
C++

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

STEP 08変更内容：

・STEP 07のPMX Readerを維持。
・PMX Index読み取りをlibMMD準拠で維持。
・Vertex / Face / Texture / Material診断を維持。
・インポート時設定ダイアログを追加。
・1オブジェクト統合モードを追加。
・マテリアルごとにPolygonObjectを分離するモードを追加。
・インポートサイズ入力を追加。
・インポートサイズ初期値は10.0。
・任意の数値入力に対応。
・サイズはObject Scaleではなく頂点座標へ直接適用。
・将来のBone / Morphでも同一スケール値を使用できる構造にする。
・Cinema 4D R19 / Visual Studio 2015互換。

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
*/


#include "c4d.h"
#include "c4d_filterdata.h"
#include "main.h"

#include <vector>


// ============================================================
// Import Mode
// ============================================================

enum PMXImportMode
{
	PMX_IMPORT_COMBINED = 0,
	PMX_IMPORT_MATERIAL_SEPARATED = 1
};


// ============================================================
// PMX Import Settings
// ============================================================

struct PMXImportSettings
{
	PMXImportMode mode;
	Float scale;

	PMXImportSettings()
	{
		mode = PMX_IMPORT_COMBINED;
		scale = 10.0;
	}
};


// ============================================================
// PMX Import Dialog
// ============================================================

class PMXImportDialog : public GeDialog
{
private:

	PMXImportSettings _settings;
	Bool _accepted;


public:

	PMXImportDialog()
	{
		_accepted = false;
	}


	Bool CreateLayout()
	{
		SetTitle(
			String("GPT MMD TOOLS - PMX IMPORT")
		);


		GroupBegin(
			1000,
			BFH_SCALEFIT,
			0,
			1,
			String(),
			0
		);


		AddStaticText(
			1001,
			BFH_LEFT,
			0,
			0,
			String("オブジェクト構造"),
			0
		);


		AddComboBox(
			1002,
			BFH_LEFT,
			180,
			0
		);


		AddChild(
			1002,
			PMX_IMPORT_COMBINED,
			String("1オブジェクトに統合")
		);


		AddChild(
			1002,
			PMX_IMPORT_MATERIAL_SEPARATED,
			String("マテリアルごとに分離")
		);


		AddStaticText(
			1003,
			BFH_LEFT,
			0,
			0,
			String("インポートサイズ"),
			0
		);


		AddEditNumber(
			1004,
			BFH_LEFT,
			180,
			0
		);


		GroupEnd();


		GroupBegin(
			1100,
			BFH_RIGHT,
			2,
			0,
			String(),
			0
		);


		AddButton(
			1101,
			BFH_RIGHT,
			100,
			0,
			String("キャンセル")
		);


		AddButton(
			1102,
			BFH_RIGHT,
			100,
			0,
			String("インポート")
		);


		GroupEnd();


		return true;
	}


	Bool InitValues()
	{
		SetInt32(
			1002,
			static_cast<Int32>(
				_settings.mode
				)
		);


		SetFloat(
			1004,
			_settings.scale,
			0.000001,
			1000000.0,
			0.1
		);


		return true;
	}


	Bool Command(
		Int32 id,
		const BaseContainer& msg
	)
	{
		if (id == 1101)
		{
			_accepted = false;

			Close();

			return true;
		}


		if (id == 1102)
		{
			Int32 mode;


			if (!GetInt32(
				1002,
				mode
			))
			{
				mode =
					PMX_IMPORT_COMBINED;
			}


			Float scale;


			if (!GetFloat(
				1004,
				scale
			))
			{
				scale = 10.0;
			}


			if (scale <= 0.0)
			{
				MessageDialog(
					String(
						"インポートサイズには0より大きい数値を入力してください。"
					)
				);

				return true;
			}


			if (mode ==
				PMX_IMPORT_MATERIAL_SEPARATED)
			{
				_settings.mode =
					PMX_IMPORT_MATERIAL_SEPARATED;
			}
			else
			{
				_settings.mode =
					PMX_IMPORT_COMBINED;
			}


			_settings.scale =
				scale;


			_accepted = true;

			Close();

			return true;
		}


		return GeDialog::Command(
			id,
			msg
		);
	}


	const PMXImportSettings& GetSettings() const
	{
		return _settings;
	}


	Bool WasAccepted() const
	{
		return _accepted;
	}
};


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

	UInt64 _readOffset;

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
		if (indexSize == 1)
		{
			UChar v;


			if (!ReadUChar(v))
				return false;


			if (v == 0xFF)
				value = -1;
			else
				value = static_cast<Int32>(v);


			return true;
		}


		if (indexSize == 2)
		{
			UInt16 v;


			if (!ReadUInt16(v))
				return false;


			if (v == 0xFFFF)
				value = -1;
			else
				value = static_cast<Int32>(v);


			return true;
		}


		if (indexSize == 4)
		{
			UInt32 v;


			if (!ReadUInt32(v))
				return false;


			if (v == 0xFFFFFFFFu)
				value = -1;
			else
				value = static_cast<Int32>(v);


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


		_encoding = headerData[0];
		_additionalUV = headerData[1];
		_vertexIndexSize = headerData[2];
		_textureIndexSize = headerData[3];
		_materialIndexSize = headerData[4];
		_boneIndexSize = headerData[5];
		_morphIndexSize = headerData[6];
		_rigidIndexSize = headerData[7];


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


		if (!IsValidIndexSize(_vertexIndexSize))
			return ReaderFailed(String("INVALID VERTEX INDEX SIZE"));

		if (!IsValidIndexSize(_textureIndexSize))
			return ReaderFailed(String("INVALID TEXTURE INDEX SIZE"));

		if (!IsValidIndexSize(_materialIndexSize))
			return ReaderFailed(String("INVALID MATERIAL INDEX SIZE"));

		if (!IsValidIndexSize(_boneIndexSize))
			return ReaderFailed(String("INVALID BONE INDEX SIZE"));

		if (!IsValidIndexSize(_morphIndexSize))
			return ReaderFailed(String("INVALID MORPH INDEX SIZE"));

		if (!IsValidIndexSize(_rigidIndexSize))
			return ReaderFailed(String("INVALID RIGID INDEX SIZE"));


		GePrint(
			"PMX VERSION : " +
			String::FloatToString(
				static_cast<Float>(_version)
			)
		);


		GePrint(
			"PMX HEADER SIZE : " +
			String::IntToString(
				static_cast<Int32>(headerSize)
			)
		);


		GePrint(
			"PMX ENCODING : " +
			String::IntToString(
				static_cast<Int32>(_encoding)
			)
		);


		GePrint(
			"PMX ADDITIONAL UV : " +
			String::IntToString(
				static_cast<Int32>(_additionalUV)
			)
		);


		GePrint(
			"PMX VERTEX INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_vertexIndexSize)
			)
		);


		GePrint(
			"PMX TEXTURE INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_textureIndexSize)
			)
		);


		GePrint(
			"PMX MATERIAL INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_materialIndexSize)
			)
		);


		GePrint(
			"PMX BONE INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_boneIndexSize)
			)
		);


		GePrint(
			"PMX MORPH INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_morphIndexSize)
			)
		);


		GePrint(
			"PMX RIGID INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_rigidIndexSize)
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
			return ReaderFailed(String("MODEL NAME"));

		if (!ReadPMXString())
			return ReaderFailed(String("MODEL NAME UNIVERSAL"));

		if (!ReadPMXString())
			return ReaderFailed(String("COMMENT"));

		if (!ReadPMXString())
			return ReaderFailed(String("COMMENT UNIVERSAL"));


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


		if (!ReadInt32(vertexCount))
			return ReaderFailed(String("VERTEX COUNT READ"));


		if (vertexCount <= 0)
			return ReaderFailed(String("INVALID VERTEX COUNT"));


		try
		{
			vertices.resize(
				static_cast<size_t>(vertexCount)
			);
		}
		catch (...)
		{
			return ReaderFailed(String("VERTEX VECTOR ALLOC"));
		}


		GePrint(
			"PMX VERTEX COUNT : " +
			String::IntToString(vertexCount)
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


			if (!ReadVector3(vertex.position))
				return VertexReadFailed(i, String("POSITION"), 0, false);

			if (!ReadVector3(vertex.normal))
				return VertexReadFailed(i, String("NORMAL"), 0, false);


			Float32 u;
			Float32 v;


			if (!ReadFloat32(u))
				return VertexReadFailed(i, String("UV U"), 0, false);

			if (!ReadFloat32(v))
				return VertexReadFailed(i, String("UV V"), 0, false);


			vertex.uv =
				Vector(
					u,
					v,
					0.0
				);


			for (
				Int32 uvIndex = 0;
				uvIndex < static_cast<Int32>(_additionalUV);
				++uvIndex
				)
			{
				if (!ReadAdditionalUV())
				{
					return VertexReadFailed(
						i,
						String("ADDITIONAL UV ") +
						String::IntToString(uvIndex),
						0,
						false
					);
				}
			}


			UChar weightType;


			if (!ReadUChar(weightType))
				return VertexReadFailed(i, String("WEIGHT TYPE"), 0, false);


			if (weightType == 0)
			{
				Int32 boneIndex;

				if (!ReadIndex(_boneIndexSize, boneIndex))
					return VertexReadFailed(i, String("BDEF1 BONE"), weightType, true);
			}
			else if (weightType == 1)
			{
				Int32 boneIndex1;
				Int32 boneIndex2;
				Float32 weight;

				if (!ReadIndex(_boneIndexSize, boneIndex1))
					return VertexReadFailed(i, String("BDEF2 BONE1"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex2))
					return VertexReadFailed(i, String("BDEF2 BONE2"), weightType, true);

				if (!ReadFloat32(weight))
					return VertexReadFailed(i, String("BDEF2 WEIGHT"), weightType, true);
			}
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

				if (!ReadIndex(_boneIndexSize, boneIndex1))
					return VertexReadFailed(i, String("BDEF4 BONE1"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex2))
					return VertexReadFailed(i, String("BDEF4 BONE2"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex3))
					return VertexReadFailed(i, String("BDEF4 BONE3"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex4))
					return VertexReadFailed(i, String("BDEF4 BONE4"), weightType, true);

				if (!ReadFloat32(weight1))
					return VertexReadFailed(i, String("BDEF4 WEIGHT1"), weightType, true);

				if (!ReadFloat32(weight2))
					return VertexReadFailed(i, String("BDEF4 WEIGHT2"), weightType, true);

				if (!ReadFloat32(weight3))
					return VertexReadFailed(i, String("BDEF4 WEIGHT3"), weightType, true);

				if (!ReadFloat32(weight4))
					return VertexReadFailed(i, String("BDEF4 WEIGHT4"), weightType, true);
			}
			else if (weightType == 3)
			{
				Int32 boneIndex1;
				Int32 boneIndex2;

				Float32 weight;

				Vector c;
				Vector r0;
				Vector r1;

				if (!ReadIndex(_boneIndexSize, boneIndex1))
					return VertexReadFailed(i, String("SDEF BONE1"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex2))
					return VertexReadFailed(i, String("SDEF BONE2"), weightType, true);

				if (!ReadFloat32(weight))
					return VertexReadFailed(i, String("SDEF WEIGHT"), weightType, true);

				if (!ReadVector3(c))
					return VertexReadFailed(i, String("SDEF C"), weightType, true);

				if (!ReadVector3(r0))
					return VertexReadFailed(i, String("SDEF R0"), weightType, true);

				if (!ReadVector3(r1))
					return VertexReadFailed(i, String("SDEF R1"), weightType, true);
			}
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

				if (!ReadIndex(_boneIndexSize, boneIndex1))
					return VertexReadFailed(i, String("QDEF BONE1"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex2))
					return VertexReadFailed(i, String("QDEF BONE2"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex3))
					return VertexReadFailed(i, String("QDEF BONE3"), weightType, true);

				if (!ReadIndex(_boneIndexSize, boneIndex4))
					return VertexReadFailed(i, String("QDEF BONE4"), weightType, true);

				if (!ReadFloat32(weight1))
					return VertexReadFailed(i, String("QDEF WEIGHT1"), weightType, true);

				if (!ReadFloat32(weight2))
					return VertexReadFailed(i, String("QDEF WEIGHT2"), weightType, true);

				if (!ReadFloat32(weight3))
					return VertexReadFailed(i, String("QDEF WEIGHT3"), weightType, true);

				if (!ReadFloat32(weight4))
					return VertexReadFailed(i, String("QDEF WEIGHT4"), weightType, true);
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


			Float32 edgeScale;


			if (!ReadFloat32(edgeScale))
				return VertexReadFailed(i, String("EDGE SCALE"), weightType, true);
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


		if (!ReadInt32(indexCount))
			return ReaderFailed(String("FACE COUNT READ"));


		if (indexCount <= 0)
			return ReaderFailed(String("INVALID FACE COUNT"));


		GePrint(
			"PMX FACE INDEX COUNT : " +
			String::IntToString(indexCount)
		);


		GePrint(
			"PMX FACE VERTEX INDEX SIZE : " +
			String::IntToString(
				static_cast<Int32>(_vertexIndexSize)
			)
		);


		if ((indexCount % 3) != 0)
		{
			GePrint(
				"PMX FACE WARNING : INDEX COUNT IS NOT DIVISIBLE BY 3"
			);
		}


		const Int32 triangleCount =
			indexCount / 3;


		if (triangleCount <= 0)
			return ReaderFailed(String("FACE TRIANGLE COUNT = 0"));


		const Int32 readableIndexCount =
			triangleCount * 3;


		try
		{
			indices.resize(
				static_cast<size_t>(readableIndexCount)
			);
		}
		catch (...)
		{
			return ReaderFailed(String("FACE VECTOR ALLOC"));
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
					String::IntToString(i)
				);

				GePrint(
					"  TRIANGLE = " +
					String::IntToString(i / 3)
				);

				GePrint(
					"  OFFSET = " +
					String::IntToString(
						static_cast<Int32>(indexOffset)
					)
				);

				return false;
			}


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
			String::IntToString(triangleCount)
		);


		if (!indices.empty())
		{
			GePrint(
				"PMX FACE FIRST INDEX = " +
				String::IntToString(indices[0])
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
						static_cast<Int32>(i)
					)
				);

				GePrint(
					"  VALUE = " +
					String::IntToString(index)
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
						static_cast<Int32>(i)
					)
				);

				GePrint(
					"  VALUE = " +
					String::IntToString(index)
				);

				GePrint(
					"  VERTEX COUNT = " +
					String::IntToString(vertexCount)
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


		if (!ReadInt32(textureCount))
			return ReaderFailed(String("TEXTURE COUNT READ"));


		if (textureCount < 0)
			return ReaderFailed(String("NEGATIVE TEXTURE COUNT"));


		try
		{
			textures.resize(
				static_cast<size_t>(textureCount)
			);
		}
		catch (...)
		{
			return ReaderFailed(String("TEXTURE VECTOR ALLOC"));
		}


		GePrint(
			"PMX TEXTURE COUNT : " +
			String::IntToString(textureCount)
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
					String::IntToString(i)
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


		if (!ReadInt32(materialCount))
			return ReaderFailed(String("MATERIAL COUNT READ"));


		if (materialCount < 0)
			return ReaderFailed(String("NEGATIVE MATERIAL COUNT"));


		try
		{
			materials.resize(
				static_cast<size_t>(materialCount)
			);
		}
		catch (...)
		{
			return ReaderFailed(String("MATERIAL VECTOR ALLOC"));
		}


		GePrint(
			"PMX MATERIAL COUNT : " +
			String::IntToString(materialCount)
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


			if (!ReadPMXString(&material.name))
			{
				GePrint("PMX MATERIAL READ FAILED");
				GePrint("  MATERIAL = " + String::IntToString(i));
				GePrint("  FIELD = NAME");
				GePrint("  OFFSET = " + GetOffsetString());
				return false;
			}


			if (!ReadPMXString(&material.nameUniversal))
			{
				GePrint("PMX MATERIAL READ FAILED");
				GePrint("  MATERIAL = " + String::IntToString(i));
				GePrint("  FIELD = UNIVERSAL NAME");
				GePrint("  OFFSET = " + GetOffsetString());
				return false;
			}


			if (!ReadVector3(material.diffuse))
				return false;

			if (!ReadFloat32(material.diffuseAlpha))
				return false;

			if (!ReadVector3(material.specular))
				return false;

			if (!ReadFloat32(material.specularPower))
				return false;

			if (!ReadVector3(material.ambient))
				return false;

			if (!ReadUChar(material.drawFlags))
				return false;

			if (!ReadVector3(material.edgeColor))
				return false;

			if (!ReadFloat32(material.edgeAlpha))
				return false;

			if (!ReadFloat32(material.edgeSize))
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
					String::IntToString(i)
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
					String::IntToString(i)
				);

				return false;
			}


			if (!ReadUChar(material.sphereMode))
				return false;

			if (!ReadUChar(material.toonFlag))
				return false;


			if (material.toonFlag == 0)
			{
				if (!ReadIndex(
					_textureIndexSize,
					material.toonTextureIndex
				))
				{
					return false;
				}
			}
			else
			{
				UChar toonIndex;


				if (!ReadUChar(toonIndex))
					return false;


				material.toonTextureIndex =
					static_cast<Int32>(toonIndex);
			}


			if (!ReadPMXString(&material.memo))
				return false;


			if (!ReadInt32(
				material.faceVertexCount
			))
			{
				return false;
			}


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


		if (!material->SetParameter(
			DescID(MATERIAL_COLOR_SHADER),
			GeData(bitmapShader),
			DESCFLAGS_SET_0
		))
		{
			BaseShader::Free(bitmapShader);

			return nullptr;
		}


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


	// ========================================================
	// Create Material Selection Tag
	// ========================================================

	Bool CreateMaterialSelection(
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


	// ========================================================
	// Create Material Texture Tag
	// ========================================================

	Bool CreateMaterialTag(
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


	// ========================================================
	// Create Combined Object
	// ========================================================

	PolygonObject* BuildCombinedObject(
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices,
		Float scale
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
			PolygonObject::Free(object);

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
					static_cast<size_t>(i)
				].position *
				scale;
		}


		CPolygon* polygons =
			object->GetPolygonW();


		if (!polygons)
		{
			PolygonObject::Free(object);

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


			polygons[i] =
				CPolygon(
					indices[
						static_cast<size_t>(base)
					],
					indices[
						static_cast<size_t>(base + 1)
					],
							indices[
								static_cast<size_t>(base + 2)
							],
							indices[
								static_cast<size_t>(base + 2)
							]
									);
		}


		if (!CreateNormalTag(
			object,
			vertices,
			indices
		))
		{
			PolygonObject::Free(object);

			return nullptr;
		}


		if (!CreateUVW(
			object,
			vertices,
			indices
		))
		{
			PolygonObject::Free(object);

			return nullptr;
		}


		if (!CreateSmoothTag(object))
		{
			PolygonObject::Free(object);

			return nullptr;
		}


		return object;
	}


	// ========================================================
	// Create Material Object
	// ========================================================

	PolygonObject* BuildMaterialObject(
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& indices,
		const PMXMaterial& material,
		Float scale
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


		/*
		分離Objectでは、
		元PMXのPolygon順をそのまま維持する。

		この段階ではPointも共有したままではなく、
		使用されたPointだけをローカルPointへ再構成する。
		*/


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


			if (globalIndex < 0 ||
				globalIndex >=
				static_cast<Int32>(
					vertices.size()
					))
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
				static_cast<size_t>(i)
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
			PolygonObject::Free(object);

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
					static_cast<size_t>(i)
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
			PolygonObject::Free(object);

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


			polygons[i] =
				CPolygon(
					localIndices[
						static_cast<size_t>(base)
					],
					localIndices[
						static_cast<size_t>(base + 1)
					],
							localIndices[
								static_cast<size_t>(base + 2)
							],
							localIndices[
								static_cast<size_t>(base + 2)
							]
									);
		}


		/*
		Normal / UVは元PMXのglobal pointから
		取得する。

		PMX Normal / UVはVertex単位なので、
		PolygonObject側のローカルPoint順に合わせて
		各Polygonの値を再構成する。
		*/

		std::vector<Int32> normalUVIndices;


		try
		{
			normalUVIndices.resize(
				localIndices.size()
			);
		}
		catch (...)
		{
			PolygonObject::Free(object);

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
			PolygonObject::Free(object);

			return nullptr;
		}


		if (!CreateUVW(
			object,
			vertices,
			normalUVIndices
		))
		{
			PolygonObject::Free(object);

			return nullptr;
		}


		if (!CreateSmoothTag(object))
		{
			PolygonObject::Free(object);

			return nullptr;
		}


		return object;
	}


	// ========================================================
	// Setup Material
	// ========================================================

	Bool SetupMaterial(
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


	// ========================================================
	// Load
	// ========================================================

public:

	Bool Load(
		const Filename& filename,
		BaseDocument* doc,
		const PMXImportSettings& settings
	)
	{
		if (!doc)
			return false;


		GePrint(
			"PMX IMPORT MODE : " +
			String::IntToString(
				static_cast<Int32>(
					settings.mode
					)
			)
		);


		GePrint(
			"PMX IMPORT SCALE : " +
			String::FloatToString(
				settings.scale
			)
		);


		if (!Open(filename))
			return false;

		if (!ReadHeader())
			return false;

		if (!ReadModelInfo())
			return false;


		std::vector<PMXVertex> vertices;


		if (!ReadVertices(vertices))
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


		if (!ReadTextures(textures))
			return false;


		std::vector<PMXMaterial> materials;


		if (!ReadMaterials(materials))
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


		// ====================================================
		// Combined
		// ====================================================

		if (settings.mode ==
			PMX_IMPORT_COMBINED)
		{
			PolygonObject* object =
				BuildCombinedObject(
					vertices,
					indices,
					settings.scale
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
					PolygonObject::Free(object);

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


		// ====================================================
		// Material Separated
		// ====================================================

		else
		{
			GePrint(
				"PMX OBJECT MODE : MATERIAL SEPARATED"
			);


			Bool firstObject = true;


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
						settings.scale
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
					PolygonObject::Free(object);

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

					firstObject = false;
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
			"STEP 08 : "
			"STEP 07 BASE + "
			"IMPORT MODE + "
			"MATERIAL SEPARATION + "
			"IMPORT SCALE"
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
		"PMX SCENE LOADER - STEP 08"
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


	// ========================================================
	// Import Settings
	// ========================================================

	PMXImportDialog dialog;


	if (!dialog.Open(
		DLG_TYPE_MODAL,
		0,
		-1,
		-1
	))
	{
		GePrint(
			"PMX IMPORT DIALOG : OPEN FAILED"
		);

		if (error)
		{
			*error =
				String(
					"GPT MMD TOOLS : IMPORT DIALOG FAILED"
				);
		}

		return FILEERROR_INVALID;
	}


	if (!dialog.WasAccepted())
	{
		GePrint(
			"PMX IMPORT : CANCELLED"
		);

		return FILEERROR_USERBREAK;
	}


	const PMXImportSettings& settings =
		dialog.GetSettings();


	GePrint(
		"PMX IMPORT SCALE : " +
		String::FloatToString(
			settings.scale
		)
	);


	GePrint(
		"PMX IMPORT MODE : " +
		String::IntToString(
			static_cast<Int32>(
				settings.mode
				)
		)
	);


	// ========================================================
	// Reader
	// ========================================================

	PMXReader reader;


	if (!reader.Load(
		name,
		doc,
		settings
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

/*
pmx_vertex.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMX Vertex読み込み処理をPMXReader本体から分離する。

STEP 10：
PMXReader::ReadVertices()を担当する。

Cinema 4D R19 / Visual Studio 2015
*/

#include "c4d.h"
#include "pmx_reader.h"

#include <vector>


// ============================================================
// Read Vertices
// ============================================================

Bool PMXReader::ReadVertices(
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
				static_cast<size_t>(
					i
					)
			];


		vertex.weightType = 0;
		vertex.weightCount = 0;


		// ====================================================
		// Position
		// ====================================================

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


		// ====================================================
		// Normal
		// ====================================================

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


		// ====================================================
		// UV
		// ====================================================

		Float32 u;
		Float32 v;


		if (!ReadFloat32(
			u
		))
		{
			return VertexReadFailed(
				i,
				String("UV U"),
				0,
				false
			);
		}


		if (!ReadFloat32(
			v
		))
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


		// ====================================================
		// Additional UV
		// ====================================================

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


		// ====================================================
		// Weight Type
		//
		// 0 = BDEF1
		// 1 = BDEF2
		// 2 = BDEF4
		// 3 = SDEF
		// 4 = QDEF
		// ====================================================

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


		vertex.weightType =
			weightType;

		vertex.weightCount =
			0;


		// ====================================================
		// BDEF1
		// ====================================================

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


			vertex.boneIndices[0] =
				boneIndex;


			vertex.boneWeights[0] =
				1.0f;


			vertex.weightCount =
				1;
		}


		// ====================================================
		// BDEF2
		// ====================================================

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


			vertex.boneIndices[0] =
				boneIndex1;


			vertex.boneIndices[1] =
				boneIndex2;


			vertex.boneWeights[0] =
				weight;


			vertex.boneWeights[1] =
				1.0f - weight;


			vertex.weightCount =
				2;
		}


		// ====================================================
		// BDEF4
		// ====================================================

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


			if (!ReadFloat32(
				weight1
			))
			{
				return VertexReadFailed(
					i,
					String("BDEF4 WEIGHT1"),
					weightType,
					true
				);
			}


			if (!ReadFloat32(
				weight2
			))
			{
				return VertexReadFailed(
					i,
					String("BDEF4 WEIGHT2"),
					weightType,
					true
				);
			}


			if (!ReadFloat32(
				weight3
			))
			{
				return VertexReadFailed(
					i,
					String("BDEF4 WEIGHT3"),
					weightType,
					true
				);
			}


			if (!ReadFloat32(
				weight4
			))
			{
				return VertexReadFailed(
					i,
					String("BDEF4 WEIGHT4"),
					weightType,
					true
				);
			}


			vertex.boneIndices[0] =
				boneIndex1;


			vertex.boneIndices[1] =
				boneIndex2;


			vertex.boneIndices[2] =
				boneIndex3;


			vertex.boneIndices[3] =
				boneIndex4;


			vertex.boneWeights[0] =
				weight1;


			vertex.boneWeights[1] =
				weight2;


			vertex.boneWeights[2] =
				weight3;


			vertex.boneWeights[3] =
				weight4;


			vertex.weightCount =
				4;
		}


		// ====================================================
		// SDEF
		//
		// 現在のSTEP 09仕様：
		// SDEF固有値を読み込み保持する。
		// 実際のウェイト処理ではBDEF2相当として扱う。
		// ====================================================

		else if (weightType == 3)
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


			if (!ReadVector3(
				vertex.sdefC
			))
			{
				return VertexReadFailed(
					i,
					String("SDEF C"),
					weightType,
					true
				);
			}


			if (!ReadVector3(
				vertex.sdefR0
			))
			{
				return VertexReadFailed(
					i,
					String("SDEF R0"),
					weightType,
					true
				);
			}


			if (!ReadVector3(
				vertex.sdefR1
			))
			{
				return VertexReadFailed(
					i,
					String("SDEF R1"),
					weightType,
					true
				);
			}


			vertex.boneIndices[0] =
				boneIndex1;


			vertex.boneIndices[1] =
				boneIndex2;


			vertex.boneWeights[0] =
				weight;


			vertex.boneWeights[1] =
				1.0f - weight;


			vertex.weightCount =
				2;
		}


		// ====================================================
		// QDEF
		//
		// 現在はBDEF4と同じ4ウェイト形式として
		// データを読み込む。
		// ====================================================

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


			if (!ReadFloat32(
				weight1
			))
			{
				return VertexReadFailed(
					i,
					String("QDEF WEIGHT1"),
					weightType,
					true
				);
			}


			if (!ReadFloat32(
				weight2
			))
			{
				return VertexReadFailed(
					i,
					String("QDEF WEIGHT2"),
					weightType,
					true
				);
			}


			if (!ReadFloat32(
				weight3
			))
			{
				return VertexReadFailed(
					i,
					String("QDEF WEIGHT3"),
					weightType,
					true
				);
			}


			if (!ReadFloat32(
				weight4
			))
			{
				return VertexReadFailed(
					i,
					String("QDEF WEIGHT4"),
					weightType,
					true
				);
			}


			vertex.boneIndices[0] =
				boneIndex1;


			vertex.boneIndices[1] =
				boneIndex2;


			vertex.boneIndices[2] =
				boneIndex3;


			vertex.boneIndices[3] =
				boneIndex4;


			vertex.boneWeights[0] =
				weight1;


			vertex.boneWeights[1] =
				weight2;


			vertex.boneWeights[2] =
				weight3;


			vertex.boneWeights[3] =
				weight4;


			vertex.weightCount =
				4;
		}


		// ====================================================
		// Unknown
		// ====================================================

		else
		{
			return VertexReadFailed(
				i,
				String("UNKNOWN WEIGHT TYPE"),
				weightType,
				true
			);
		}


		// ====================================================
		// Edge Scale
		// ====================================================

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
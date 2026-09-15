/*
pmx_face.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMX Face読み込み・Face検証処理を
PMXReader本体から分離する。

STEP 10-B：
PMXReader::ReadFaces()
PMXReader::ValidateFaces()

STEP 09の処理内容を変更せず、
そのまま別cppへ移動する。

Cinema 4D R19 / Visual Studio 2015
*/

#include "c4d.h"
#include "pmx_reader.h"
#include "pmx_face.h"

#include <vector>


// ============================================================
// Read Faces
// ============================================================

Bool PMXReader::ReadFaces(
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


// ============================================================
// Validate Faces
// ============================================================

Bool PMXReader::ValidateFaces(
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
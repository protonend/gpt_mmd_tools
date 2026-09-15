#include "c4d.h"
#include "lib_ca.h"
#include "tcaposemorph.h"

#include "pmx_morph.h"
#include "pmx_types.h"

#include <vector>


// ============================================================
// GPT MMD TOOLS
// PMX Vertex Morph Builder
//
// Cinema 4D R19
// Visual Studio 2015
//
// STEP10
//
// PMX Morph Type:
//
//   0 = Group
//   1 = Vertex
//   2 = Bone
//   3 = UV
//   4 = Additional UV 1
//   5 = Additional UV 2
//   6 = Additional UV 3
//   7 = Additional UV 4
//   8 = Material
//   9 = Flip
//  10 = Impulse
//
// STEP10Ç≈ÇÕ Vertex Morph ÇÃÇ›ÇC4D Pose MorphÇ÷ìäì¸Ç∑ÇÈÅB
//
// PMX Vertex Morph:
//
//     target position = base position + offset
//
// C4D:
//
//     CAMORPH_MODE_REL
//
// Geometry:
//
//     C4D position = PMX position * scale
//
// Morph offset:
//
//     C4D offset = PMX offset * scale
//
// Combined Object:
//
//     local point index == PMX global vertex index
//
// Material-separated Object:
//
//     localToGlobal[local] == PMX global vertex
// ============================================================



// ============================================================
// FindPointMorphNode
// ============================================================

CAMorphNode* PMXMorphBuilder::FindPointMorphNode(
	CAMorph* morph
)
{
	if (!morph)
		return nullptr;


	CAMorphNode* node =
		morph->GetFirst();


	while (node)
	{
		const CAMORPH_DATA_FLAGS info =
			node->GetInfo();


		// R19Ç≈ÇÕCAMORPH_DATA_FLAGSÇÇªÇÃÇ‹Ç‹
		// BooléÆÇ∆ÇµÇƒàµÇ§Ç∆IllegalFlagAccessÇ…Ç»ÇÈÇΩÇﬂ
		// Int32Ç÷ñæé¶ïœä∑Ç∑ÇÈÅB
		const Int32 infoValue =
			static_cast<Int32>(info);


		if (
			(infoValue &
				CAMORPH_DATA_FLAGS_POINTS) != 0
			)
		{
			return node;
		}


		node =
			node->GetNext();
	}


	return nullptr;
}


// ============================================================
// BuildGlobalToLocalMap
// ============================================================

Bool PMXMorphBuilder::BuildGlobalToLocalMap(
	const std::vector<Int32>& localToGlobal,
	Int32 globalVertexCount,
	std::vector<Int32>& globalToLocal
)
{
	if (globalVertexCount <= 0)
		return false;


	try
	{
		globalToLocal.resize(
			static_cast<size_t>(
				globalVertexCount
				)
		);
	}
	catch (...)
	{
		return false;
	}


	for (
		Int32 i = 0;
		i < globalVertexCount;
		++i
		)
	{
		globalToLocal[
			static_cast<size_t>(i)
		] = -1;
	}


	for (
		Int32 localIndex = 0;
		localIndex <
		static_cast<Int32>(
			localToGlobal.size()
			);
		++localIndex
		)
	{
		const Int32 globalIndex =
			localToGlobal[
				static_cast<size_t>(
					localIndex
					)
			];


		if (
			globalIndex < 0 ||
			globalIndex >= globalVertexCount
			)
		{
			continue;
		}


		globalToLocal[
			static_cast<size_t>(
				globalIndex
				)
		] =
			localIndex;
	}


	return true;
}


// ============================================================
// CreateMorphTag
// ============================================================

Bool PMXMorphBuilder::CreateMorphTag(
	PolygonObject* object,
	BaseDocument* doc,
	CAPoseMorphTag*& morphTag
)
{
	morphTag = nullptr;


	if (!object)
		return false;


	if (!doc)
		return false;


	// --------------------------------------------------------
	// Allocate Pose Morph Tag
	// --------------------------------------------------------

	morphTag =
		CAPoseMorphTag::Alloc();


	if (!morphTag)
	{
		GePrint(
			"PMX MORPH : CAPoseMorphTag ALLOC FAILED"
		);

		return false;
	}


	// --------------------------------------------------------
	// Insert
	// --------------------------------------------------------

	object->InsertTag(
		morphTag
	);


	// --------------------------------------------------------
	// Initialize
	// --------------------------------------------------------

	morphTag->InitMorphs();


	// --------------------------------------------------------
	// Exit Edit
	// --------------------------------------------------------

	if (!morphTag->ExitEdit(
		doc,
		true
	))
	{
		GePrint(
			"PMX MORPH : ExitEdit FAILED"
		);

		morphTag->Remove();

		CAPoseMorphTag::Free(
			morphTag
		);

		morphTag = nullptr;

		return false;
	}


	// --------------------------------------------------------
	// Base Morph
	// --------------------------------------------------------

	CAMorph* baseMorph =
		morphTag->AddMorph();


	if (!baseMorph)
	{
		GePrint(
			"PMX MORPH : BASE MORPH ADD FAILED"
		);

		morphTag->Remove();

		CAPoseMorphTag::Free(
			morphTag
		);

		morphTag = nullptr;

		return false;
	}


	// --------------------------------------------------------
	// Store Base Morph
	// --------------------------------------------------------

	if (!baseMorph->Store(
		doc,
		morphTag,
		CAMORPH_DATA_FLAGS_ASTAG
	))
	{
		GePrint(
			"PMX MORPH : BASE MORPH STORE FAILED"
		);

		morphTag->Remove();

		CAPoseMorphTag::Free(
			morphTag
		);

		morphTag = nullptr;

		return false;
	}


	// --------------------------------------------------------
	// R19
	//
	// UpdateMorphs()
	// --------------------------------------------------------

	morphTag->UpdateMorphs();


	morphTag->Message(
		MSG_UPDATE
	);


	return true;
}


// ============================================================
// CreateVertexMorphTag
// ============================================================

Bool PMXMorphBuilder::CreateVertexMorphTag(
	PolygonObject* object,
	BaseDocument* doc,
	const std::vector<PMXMorph>& morphs,
	const std::vector<Int32>& localToGlobal,
	const std::vector<PMXVertex>& vertices,
	Float scale
)
{
	if (!object)
	{
		GePrint(
			"PMX MORPH : OBJECT IS NULL"
		);

		return false;
	}


	if (!doc)
	{
		GePrint(
			"PMX MORPH : DOCUMENT IS NULL"
		);

		return false;
	}


	const Int32 globalVertexCount =
		static_cast<Int32>(
			vertices.size()
			);


	if (globalVertexCount <= 0)
	{
		GePrint(
			"PMX MORPH : NO VERTICES"
		);

		return false;
	}


	// ========================================================
	// Build global -> local map
	// ========================================================

	std::vector<Int32> globalToLocal;


	if (!BuildGlobalToLocalMap(
		localToGlobal,
		globalVertexCount,
		globalToLocal
	))
	{
		GePrint(
			"PMX MORPH : GLOBAL TO LOCAL MAP FAILED"
		);

		return false;
	}


	// ========================================================
	// Create Pose Morph Tag
	// ========================================================

	CAPoseMorphTag* morphTag =
		nullptr;


	if (!CreateMorphTag(
		object,
		doc,
		morphTag
	))
	{
		return false;
	}


	// ========================================================
	// Count PMX Vertex Morphs
	// ========================================================

	Int32 vertexMorphCount =
		0;


	for (
		size_t morphIndex = 0;
		morphIndex < morphs.size();
		++morphIndex
		)
	{
		const PMXMorph& pmxMorph =
			morphs[morphIndex];


		// PMX Morph Type 1 = Vertex Morph
		if (pmxMorph.type != 1)
			continue;


		if (pmxMorph.vertexOffsets.empty())
			continue;


		++vertexMorphCount;
	}


	if (vertexMorphCount == 0)
	{
		GePrint(
			"PMX MORPH : NO VERTEX MORPHS"
		);

		morphTag->Message(
			MSG_UPDATE
		);

		return true;
	}


	// ========================================================
	// Enable Point Morph
	//
	// tcaposemorph.h
	// supplies ID_CA_POSE_POINTS.
	//
	// Cinema 4D R19:
	//
	//     DescID(ID_CA_POSE_POINTS)
	//     DESCFLAGS_SET_0
	//
	// ========================================================

	if (!morphTag->SetParameter(
		DescID(
			ID_CA_POSE_POINTS
		),
		GeData(true),
		DESCFLAGS_SET_0
	))
	{
		GePrint(
			"PMX MORPH : ENABLE POINT MORPH FAILED"
		);

		return false;
	}


	// ========================================================
	// Statistics
	// ========================================================

	Int32 createdMorphCount =
		0;

	Int32 skippedMorphCount =
		0;

	Int32 appliedOffsetCount =
		0;


	// ========================================================
	// Process PMX Morphs
	// ========================================================

	for (
		size_t morphIndex = 0;
		morphIndex < morphs.size();
		++morphIndex
		)
	{
		const PMXMorph& pmxMorph =
			morphs[morphIndex];


		// ----------------------------------------------------
		// STEP10:
		// Only PMX Vertex Morph
		// ----------------------------------------------------

		if (pmxMorph.type != 1)
			continue;


		if (pmxMorph.vertexOffsets.empty())
			continue;


		// ====================================================
		// IMPORTANT
		//
		// Do NOT create a Morph first and then try to remove
		// it when the material-separated object has no
		// corresponding vertices.
		//
		// C4D R19's public C++ wrapper does not expose
		// CAPoseMorphTag::Remove(Int32) as morphTag->Remove().
		//
		// Therefore determine the valid offsets BEFORE
		// AddMorph().
		// ====================================================

		Int32 validOffsetCount =
			0;


		for (
			size_t offsetIndex = 0;
			offsetIndex <
			pmxMorph.vertexOffsets.size();
			++offsetIndex
			)
		{
			const PMXVertexMorphOffset& offset =
				pmxMorph.vertexOffsets[
					offsetIndex
				];


			const Int32 globalVertexIndex =
				offset.vertexIndex;


			if (
				globalVertexIndex < 0 ||
				globalVertexIndex >= globalVertexCount
				)
			{
				continue;
			}


			const Int32 localPointIndex =
				globalToLocal[
					static_cast<size_t>(
						globalVertexIndex
						)
				];


			if (localPointIndex < 0)
			{
				continue;
			}


			++validOffsetCount;
		}


		// ----------------------------------------------------
		// No vertex belonging to this object.
		//
		// Do not create an empty C4D Morph.
		// ----------------------------------------------------

		if (validOffsetCount == 0)
		{
			++skippedMorphCount;

			continue;
		}


		// ====================================================
		// Add C4D Morph
		// ====================================================

		CAMorph* morph =
			morphTag->AddMorph();


		if (!morph)
		{
			GePrint(
				"PMX MORPH : ADD MORPH FAILED"
			);

			return false;
		}


		++createdMorphCount;


		// ====================================================
		// Set Morph Name
		// ====================================================

		String morphName =
			pmxMorph.name;


		if (morphName == String())
		{
			morphName =
				String("PMX Morph ") +
				String::IntToString(
					static_cast<Int32>(
						morphIndex
						)
				);
		}


		morph->SetName(
			morphName
		);


		// ====================================================
		// Store Morph
		// ====================================================

		if (!morph->Store(
			doc,
			morphTag,
			CAMORPH_DATA_FLAGS_ASTAG
		))
		{
			GePrint(
				"PMX MORPH : STORE FAILED"
			);

			return false;
		}


		// ====================================================
		// Set Relative Mode
		//
		// PMX Vertex Morph is relative.
		// ====================================================

		if (!morph->SetMode(
			doc,
			morphTag,
			static_cast<CAMORPH_MODE_FLAGS>(
				CAMORPH_MODE_FLAGS_ALL |
				CAMORPH_MODE_FLAGS_EXPAND
				),
			CAMORPH_MODE_REL
		))
		{
			GePrint(
				"PMX MORPH : SET REL MODE FAILED"
			);

			return false;
		}


		// ====================================================
		// Find POINTS Node
		// ====================================================

		CAMorphNode* morphNode =
			FindPointMorphNode(
				morph
			);


		if (!morphNode)
		{
			GePrint(
				"PMX MORPH : POINT NODE NOT FOUND"
			);

			return false;
		}


		// ====================================================
		// Apply PMX Vertex Morph Offsets
		// ====================================================

		for (
			size_t offsetIndex = 0;
			offsetIndex <
			pmxMorph.vertexOffsets.size();
			++offsetIndex
			)
		{
			const PMXVertexMorphOffset& offset =
				pmxMorph.vertexOffsets[
					offsetIndex
				];


			const Int32 globalVertexIndex =
				offset.vertexIndex;


			// ------------------------------------------------
			// Validate PMX vertex
			// ------------------------------------------------

			if (
				globalVertexIndex < 0 ||
				globalVertexIndex >= globalVertexCount
				)
			{
				++skippedMorphCount;

				continue;
			}


			// ------------------------------------------------
			// PMX global -> C4D local
			// ------------------------------------------------

			const Int32 localPointIndex =
				globalToLocal[
					static_cast<size_t>(
						globalVertexIndex
						)
				];


			// ------------------------------------------------
			// Vertex does not exist in this material object.
			// ------------------------------------------------

			if (localPointIndex < 0)
			{
				++skippedMorphCount;

				continue;
			}


			// ------------------------------------------------
			// Apply importer scale
			// ------------------------------------------------

			const Vector c4dOffset =
				offset.offset *
				scale;


			// ------------------------------------------------
			// Store relative offset
			// ------------------------------------------------

			morphNode->SetPoint(
				localPointIndex,
				c4dOffset
			);


			++appliedOffsetCount;
		}


		// ====================================================
		// Return to AUTO mode
		// ====================================================

		if (!morph->SetMode(
			doc,
			morphTag,
			static_cast<CAMORPH_MODE_FLAGS>(
				CAMORPH_MODE_FLAGS_ALL |
				CAMORPH_MODE_FLAGS_COLLAPSE
				),
			CAMORPH_MODE_AUTO
		))
		{
			GePrint(
				"PMX MORPH : SET AUTO MODE FAILED"
			);

			return false;
		}


		// ----------------------------------------------------
		// Imported Morph starts at zero strength.
		// ----------------------------------------------------

		morph->SetStrength(
			0.0f
		);
	}


	// ========================================================
	// Final Update
	// ========================================================

	morphTag->UpdateMorphs();


	morphTag->Message(
		MSG_UPDATE
	);


	// ========================================================
	// Diagnostic
	// ========================================================

	GePrint(
		"============================================================"
	);

	GePrint(
		"PMX MORPH : VERTEX MORPH IMPORT"
	);

	GePrint(
		"  PMX MORPH TOTAL = " +
		String::IntToString(
			static_cast<Int32>(
				morphs.size()
				)
		)
	);

	GePrint(
		"  PMX VERTEX MORPH = " +
		String::IntToString(
			vertexMorphCount
		)
	);

	GePrint(
		"  C4D MORPH CREATED = " +
		String::IntToString(
			createdMorphCount
		)
	);

	GePrint(
		"  OFFSETS APPLIED = " +
		String::IntToString(
			appliedOffsetCount
		)
	);

	GePrint(
		"  MORPHS SKIPPED = " +
		String::IntToString(
			skippedMorphCount
		)
	);

	GePrint(
		"  SCALE = " +
		String::FloatToString(
			scale
		)
	);

	GePrint(
		"PMX MORPH : CREATED"
	);

	GePrint(
		"============================================================"
	);


	return true;
}
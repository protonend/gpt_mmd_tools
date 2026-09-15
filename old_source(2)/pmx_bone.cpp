/*
pmx_bone.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMXボーン階層をC4D Joint階層へ変換し、
PMXのBDEF1/BDEF2/BDEF4/SDEF/QDEFウェイトを
C4D R19のCAWeightTag + Oskinへ設定する。
*/

#include "pmx_bone.h"

#include <lib_ca.h>


namespace
{
	static Vector ScaledPosition(
		const Vector& position,
		Float scale
	)
	{
		return position * scale;
	}


	static Float ClampWeight(
		Float weight
	)
	{
		if (weight < 0.0)
			return 0.0;

		if (weight > 1.0)
			return 1.0;

		return weight;
	}
}


// ============================================================
// Build Bones
// ============================================================

Bool PMXBoneBuilder::BuildBones(
	BaseDocument* doc,
	const std::vector<PMXBone>& bones,
	Float scale,
	std::vector<BaseObject*>& boneObjects
)
{
	if (!doc)
		return false;


	boneObjects.clear();


	if (bones.empty())
		return true;


	BaseObject* root =
		BaseObject::Alloc(
			Onull
		);


	if (!root)
		return false;


	root->SetName(
		String("MMD Bones")
	);


	doc->InsertObject(
		root,
		nullptr,
		nullptr
	);


	try
	{
		boneObjects.resize(
			bones.size(),
			nullptr
		);
	}
	catch (...)
	{
		return false;
	}


	// ========================================================
	// Joint objects
	// ========================================================

	for (
		size_t i = 0;
		i < bones.size();
		++i
		)
	{
		BaseObject* joint =
			BaseObject::Alloc(
				Ojoint
			);


		if (!joint)
			return false;


		String name =
			bones[i].name;


		if (name == String())
		{
			name =
				String("Bone ") +
				String::IntToString(
					static_cast<Int32>(i)
				);
		}


		joint->SetName(
			name
		);


		boneObjects[i] =
			joint;
	}


	// ========================================================
	// Joint hierarchy
	// ========================================================

	for (
		size_t i = 0;
		i < bones.size();
		++i
		)
	{
		BaseObject* joint =
			boneObjects[i];


		if (!joint)
			return false;


		const PMXBone& bone =
			bones[i];


		const Vector worldPosition =
			ScaledPosition(
				bone.position,
				scale
			);


		if (
			bone.parentIndex >= 0 &&
			bone.parentIndex <
			static_cast<Int32>(
				boneObjects.size()
				) &&
			boneObjects[
				static_cast<size_t>(
					bone.parentIndex
					)
			]
			)
		{
			const Vector parentPosition =
				ScaledPosition(
					bones[
						static_cast<size_t>(
							bone.parentIndex
							)
					].position,
					scale
							);


			const Vector localPosition =
				worldPosition -
				parentPosition;


			joint->SetFrozenPos(
				localPosition
			);


			joint->InsertUnder(
				boneObjects[
					static_cast<size_t>(
						bone.parentIndex
						)
				]
			);
		}
		else
		{
			joint->SetFrozenPos(
				worldPosition
			);


			joint->InsertUnder(
				root
			);
		}
	}


	GePrint(
		"PMX BONE OBJECTS : CREATED"
	);


	GePrint(
		"  BONE COUNT = " +
		String::IntToString(
			static_cast<Int32>(
				bones.size()
				)
		)
	);


	return true;
}


// ============================================================
// Set Vertex Weights
// ============================================================

Bool PMXBoneBuilder::SetVertexWeights(
	CAWeightTag* weightTag,
	Int32 pointIndex,
	const PMXVertex& vertex,
	const std::vector<Int32>& boneTagIndices
)
{
	if (!weightTag)
		return false;


	if (pointIndex < 0)
		return false;


	const Int32 influenceCount =
		static_cast<Int32>(
			vertex.weightCount
			);


	for (
		Int32 i = 0;
		i < influenceCount;
		++i
		)
	{
		const Int32 boneIndex =
			vertex.boneIndices[i];


		if (
			boneIndex < 0 ||
			boneIndex >=
			static_cast<Int32>(
				boneTagIndices.size()
				)
			)
		{
			continue;
		}


		const Int32 jointIndex =
			boneTagIndices[
				static_cast<size_t>(
					boneIndex
					)
			];


		if (jointIndex < 0)
			continue;


		const Float weight =
			ClampWeight(
				static_cast<Float>(
					vertex.boneWeights[i]
					)
			);


		if (weight <= 0.0)
			continue;


		if (!weightTag->SetWeight(
			jointIndex,
			pointIndex,
			weight
		))
		{
			return false;
		}
	}


	return true;
}


// ============================================================
// Create Weight Tag
// ============================================================

Bool PMXBoneBuilder::CreateWeightTag(
	PolygonObject* object,
	BaseDocument* doc,
	const std::vector<PMXVertex>& vertices,
	const std::vector<Int32>& localToGlobal,
	const std::vector<BaseObject*>& boneObjects
)
{
	if (!object)
		return false;


	if (!doc)
		return false;


	if (vertices.empty())
		return true;


	if (boneObjects.empty())
		return true;


	const Int32 pointCount =
		object->GetPointCount();


	if (pointCount <= 0)
		return false;


	if (
		localToGlobal.size() !=
		static_cast<size_t>(
			pointCount
			)
		)
	{
		GePrint(
			"PMX WEIGHT TAG : LOCAL/GLOBAL MAP SIZE ERROR"
		);

		return false;
	}


	// ========================================================
	// CAWeightTag
	// ========================================================

	CAWeightTag* weightTag =
		CAWeightTag::Alloc();


	if (!weightTag)
		return false;


	object->InsertTag(
		weightTag
	);


	// ========================================================
	// PMX bone index
	// -> CAWeightTag joint index
	// ========================================================

	std::vector<Int32> boneTagIndices;


	try
	{
		boneTagIndices.resize(
			boneObjects.size(),
			-1
		);
	}
	catch (...)
	{
		return false;
	}


	for (
		size_t i = 0;
		i < boneObjects.size();
		++i
		)
	{
		BaseObject* joint =
			boneObjects[i];


		if (!joint)
			continue;


		const Int32 jointIndex =
			weightTag->AddJoint(
				joint
			);


		if (jointIndex < 0)
		{
			GePrint(
				"PMX WEIGHT TAG : ADD JOINT FAILED"
			);

			GePrint(
				"  BONE INDEX = " +
				String::IntToString(
					static_cast<Int32>(i)
				)
			);

			return false;
		}


		boneTagIndices[i] =
			jointIndex;
	}


	// ========================================================
	// Geometry matrix
	// ========================================================

	weightTag->SetGeomMg(
		object->GetMg()
	);


	// ========================================================
	// Create Oskin
	//
	// C4D_MMD_Tool reference:
	// CAWeightTag + Oskin
	// ========================================================

	BaseObject* skinObject =
		BaseObject::Alloc(
			Oskin
		);


	if (!skinObject)
	{
		GePrint(
			"PMX SKIN OBJECT : ALLOC FAILED"
		);

		return false;
	}


	skinObject->SetName(
		String("MMD Skin")
	);


	skinObject->InsertUnderLast(
		object
	);


	// ========================================================
	// Joint Weight Maps
	//
	// [joint][point]
	// ========================================================

	std::vector< std::vector<Float32> > weightMaps;


	try
	{
		weightMaps.resize(
			boneObjects.size()
		);


		for (
			size_t jointIndex = 0;
			jointIndex < boneObjects.size();
			++jointIndex
			)
		{
			weightMaps[
				jointIndex
			].resize(
				static_cast<size_t>(
					pointCount
					),
				0.0f
			);
		}
	}
	catch (...)
	{
		return false;
	}


	// ========================================================
	// Convert PMX vertex weights
	// into joint weight maps.
	// ========================================================

	Int32 weightedPointCount = 0;
	Int32 zeroWeightPointCount = 0;


	for (
		Int32 pointIndex = 0;
		pointIndex < pointCount;
		++pointIndex
		)
	{
		const Int32 globalIndex =
			localToGlobal[
				static_cast<size_t>(
					pointIndex
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
			GePrint(
				"PMX WEIGHT : INVALID GLOBAL VERTEX INDEX"
			);

			GePrint(
				"  POINT = " +
				String::IntToString(
					pointIndex
				)
			);

			GePrint(
				"  GLOBAL = " +
				String::IntToString(
					globalIndex
				)
			);

			return false;
		}


		const PMXVertex& vertex =
			vertices[
				static_cast<size_t>(
					globalIndex
					)
			];


		Bool hasWeight = false;


		const Int32 influenceCount =
			static_cast<Int32>(
				vertex.weightCount
				);


		for (
			Int32 influence = 0;
			influence < influenceCount;
			++influence
			)
		{
			const Int32 boneIndex =
				vertex.boneIndices[
					influence
				];


			if (
				boneIndex < 0 ||
				boneIndex >=
				static_cast<Int32>(
					boneTagIndices.size()
					)
				)
			{
				continue;
			}


			const Int32 jointIndex =
				boneTagIndices[
					static_cast<size_t>(
						boneIndex
						)
				];


			if (jointIndex < 0)
				continue;


			const Float32 weight =
				static_cast<Float32>(
					ClampWeight(
						static_cast<Float>(
							vertex.boneWeights[
								influence
							]
							)
					)
					);


			if (weight <= 0.0f)
				continue;


			weightMaps[
				static_cast<size_t>(
					jointIndex
					)
			][
				static_cast<size_t>(
					pointIndex
					)
			] += weight;


				hasWeight = true;
		}


		if (hasWeight)
		{
			++weightedPointCount;
		}
		else
		{
			++zeroWeightPointCount;
		}
	}


	// ========================================================
	// Apply Weight Maps
	// ========================================================

	for (
		size_t jointIndex = 0;
		jointIndex < weightMaps.size();
		++jointIndex
		)
	{
		const Int32 c4dJointIndex =
			boneTagIndices[
				jointIndex
			];


		if (c4dJointIndex < 0)
			continue;


		if (
			weightMaps[
				jointIndex
			].empty()
					)
		{
			continue;
		}


				if (!weightTag->SetWeightMap(
					c4dJointIndex,
					&weightMaps[
						jointIndex
					][0],
							pointCount
							))
				{
					GePrint(
						"PMX WEIGHT MAP : SET FAILED"
					);

					GePrint(
						"  JOINT = " +
						String::IntToString(
							c4dJointIndex
						)
					);

					return false;
				}
	}


	// ========================================================
	// Calculate Bone States
	// ========================================================

	for (
		Int32 jointIndex = 0;
		jointIndex <
		weightTag->GetJointCount();
		++jointIndex
		)
	{
		weightTag->CalculateBoneStates(
			jointIndex
		);
	}


	weightTag->Message(
		MSG_UPDATE
	);


	skinObject->Message(
		MSG_UPDATE
	);


	object->Message(
		MSG_UPDATE
	);


	// ========================================================
	// Diagnostics
	// ========================================================

	GePrint(
		"PMX WEIGHT TAG : CREATED"
	);


	GePrint(
		"  POINT COUNT = " +
		String::IntToString(
			pointCount
		)
	);


	GePrint(
		"  JOINT COUNT = " +
		String::IntToString(
			weightTag->GetJointCount()
		)
	);


	GePrint(
		"PMX SKIN OBJECT : CREATED"
	);


	GePrint(
		"PMX WEIGHT DATA : CREATED"
	);


	GePrint(
		"  WEIGHTED POINT COUNT = " +
		String::IntToString(
			weightedPointCount
		)
	);


	GePrint(
		"  ZERO WEIGHT POINT COUNT = " +
		String::IntToString(
			zeroWeightPointCount
		)
	);


	return true;
}
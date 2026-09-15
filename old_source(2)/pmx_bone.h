/*
pmx_bone.h

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMXボーン情報からC4D Joint階層を生成し、
PMX頂点ウェイトをR19 CAWeightTagへ変換する。
*/

#ifndef GPT_MMD_TOOLS_PMX_BONE_H__
#define GPT_MMD_TOOLS_PMX_BONE_H__

#include "c4d.h"
#include <lib_ca.h>
#include "pmx_types.h"

#include <vector>

class PMXBoneBuilder
{
public:

	static Bool BuildBones(
		BaseDocument* doc,
		const std::vector<PMXBone>& bones,
		Float scale,
		std::vector<BaseObject*>& boneObjects
	);

	static Bool CreateWeightTag(
		PolygonObject* object,
		BaseDocument* doc,
		const std::vector<PMXVertex>& vertices,
		const std::vector<Int32>& localToGlobal,
		const std::vector<BaseObject*>& boneObjects
	);

private:

	static Bool SetVertexWeights(
		CAWeightTag* weightTag,
		Int32 pointIndex,
		const PMXVertex& vertex,
		const std::vector<Int32>& boneTagIndices
	);
};

#endif

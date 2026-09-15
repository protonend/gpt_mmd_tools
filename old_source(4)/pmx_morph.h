#ifndef PMX_MORPH_H__
#define PMX_MORPH_H__

#include "c4d.h"
#include "lib_ca.h"
#include "tcaposemorph.h"

#include "pmx_types.h"

#include <vector>


class PMXMorphBuilder
{
public:

	static Bool CreateVertexMorphTag(
		PolygonObject* object,
		BaseDocument* doc,
		const std::vector<PMXMorph>& morphs,
		const std::vector<Int32>& localToGlobal,
		const std::vector<PMXVertex>& vertices,
		Float scale
	);

private:

	static CAMorphNode* FindPointMorphNode(
		CAMorph* morph
	);

	static Bool BuildGlobalToLocalMap(
		const std::vector<Int32>& localToGlobal,
		Int32 globalVertexCount,
		std::vector<Int32>& globalToLocal
	);

	static Bool CreateMorphTag(
		PolygonObject* object,
		BaseDocument* doc,
		CAPoseMorphTag*& morphTag
	);
};


#endif
/*
main.h

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMX Scene Loader本体のクラス宣言と
プラグインIDを定義する。
*/

#ifndef GPT_MMD_TOOLS_MAIN_H__
#define GPT_MMD_TOOLS_MAIN_H__

#include "c4d.h"
#include "c4d_filterdata.h"


#define GPT_MMD_TOOLS_PMX_ID 1059999


class GPTMMDPMXLoader : public SceneLoaderData
{
public:

	virtual Bool Identify(
		BaseSceneLoader* node,
		const Filename& name,
		UChar* probe,
		Int32 size
	);

	virtual FILEERROR Load(
		BaseSceneLoader* node,
		const Filename& name,
		BaseDocument* doc,
		SCENEFILTER filterflags,
		String* error,
		BaseThread* bt
	);

	static NodeData* Alloc();
};


#endif
/*
main.cpp

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader - STEP 08.2 / SPLIT 02

Cinema 4D R19
Visual Studio 2015
C++

処理内容：
PMXファイルをCinema 4D R19の
Filename / BaseFile経由で直接読み込み。

STEP 08.2：
・インポート倍率は直接倍率
・デフォルト 10.0
・Combined / Material Separated対応
・PMX Reader維持
・日本語ダイアログ文字列はUTF-8バイト列から生成
Visual Studio 2015のソース文字コードに依存しない。
*/


#include "c4d.h"
#include "c4d_filterdata.h"
#include "main.h"
#include "pmx_dialog.h"
#include "pmx_reader.h"

#include <vector>


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
		"PMX SCENE LOADER - STEP 08.2"
	);


	GePrint(
		"Cinema 4D : R19"
	);


	GePrint(
		"Visual Studio : 2015"
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
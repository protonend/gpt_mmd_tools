/*
pmx_dialog.h

GPT MMD TOOLS
Cinema 4D R19 PMX Scene Loader

処理内容：
PMXインポート方式・インポート倍率を設定する
日本語インポートダイアログを定義する。
*/

#ifndef GPT_MMD_TOOLS_PMX_DIALOG_H__
#define GPT_MMD_TOOLS_PMX_DIALOG_H__

#include "c4d.h"

// ============================================================
// Dialog UTF-8 String Helper
// ============================================================
//
// 日本語文字列をmain.cppの文字コードに依存させない。
// UTF-8のバイト列をASCIIのエスケープ表記で記述し、
// Cinema 4D StringへUTF-8として変換する。
//
// これによりVisual Studio 2015でmain.cppが
// Shift-JIS / UTF-8 BOM / UTF-8などどの状態でも、
// ダイアログ文字列そのものはUTF-8として確実に解釈される。
// ============================================================

static String PMXDialogString(
	const Char* utf8
)
{
	return String(
		utf8,
		STRINGENCODING_UTF8
	);
}


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

	// ユーザーが入力する「インポート倍率」
	//
	// 1.0  = 元サイズ
	// 2.0  = 2倍
	// 5.0  = 5倍
	// 10.0 = 10倍
	Float importScale;

	// PMX座標へ実際に掛ける倍率。
	// STEP 08.2ではimportScaleをそのまま使用する。
	Float actualScale;

	PMXImportSettings()
	{
		mode = PMX_IMPORT_COMBINED;

		importScale = 10.0;

		actualScale = 10.0;
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
			PMXDialogString(
				"\107\120\124\040\115\115\104\040\124\117\117\114\123\040\055\040\120\115\130\040\343\202\244\343\203\263\343\203\235\343\203\274\343\203\210"
			)
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
			PMXDialogString(
				"\343\202\244\343\203\263\343\203\235\343\203\274\343\203\210\346\226\271\345\274\217"
			),
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
			PMXDialogString(
				"\061\343\201\244\343\201\256\343\202\252\343\203\226\343\202\270\343\202\247\343\202\257\343\203\210"
			)
		);


		AddChild(
			1002,
			PMX_IMPORT_MATERIAL_SEPARATED,
			PMXDialogString(
				"\346\235\220\350\263\252\343\201\224\343\201\250\343\201\253\345\210\206\351\233\242"
			)
		);


		AddStaticText(
			1003,
			BFH_LEFT,
			0,
			0,
			PMXDialogString(
				"\343\202\244\343\203\263\343\203\235\343\203\274\343\203\210\345\200\215\347\216\207"
			),
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
			PMXDialogString(
				"\343\202\255\343\203\243\343\203\263\343\202\273\343\203\253"
			)
		);


		AddButton(
			1102,
			BFH_RIGHT,
			100,
			0,
			PMXDialogString(
				"\343\202\244\343\203\263\343\203\235\343\203\274\343\203\210"
			)
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
			_settings.importScale,
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


			Float importScale;


			if (!GetFloat(
				1004,
				importScale
			))
			{
				importScale = 10.0;
			}


			if (importScale <= 0.0)
			{
				MessageDialog(
					PMXDialogString(
						"\343\202\244\343\203\263\343\203\235\343\203\274\343\203\210\345\200\215\347\216\207\343\201\257\060\343\202\210\343\202\212\345\244\247\343\201\215\343\201\204\345\200\244\343\202\222\345\205\245\345\212\233\343\201\227\343\201\246\343\201\217\343\201\240\343\201\225\343\201\204\343\200\202"
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


			_settings.importScale =
				importScale;


			// STEP 08.2：
			// 入力された倍率をそのまま実スケールとして使用する。
			_settings.actualScale =
				importScale;


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

#endif

/**************************************************************************/
/*  psd_ui_editor_plugin.h                                                */
/**************************************************************************/
#pragma once

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/dialogs.h"

class LineEdit;
class CheckBox;
class Label;
class TextEdit;
class EditorFileDialog;

// Modal dialog that lets the user pick a .psd file and an output folder, then converts the PSD into an editable
// GUI scene (.tscn) with external PNG textures and Label nodes for text layers.
class PsdUiConvertDialog : public ConfirmationDialog {
	GDCLASS(PsdUiConvertDialog, ConfirmationDialog);

	LineEdit *psd_path_edit = nullptr;
	LineEdit *out_dir_edit = nullptr;
	CheckBox *skip_hidden_check = nullptr;
	CheckBox *root_size_check = nullptr;
	CheckBox *text_as_label_check = nullptr;
	TextEdit *asset_dirs_edit = nullptr;
	TextEdit *scene_dirs_edit = nullptr;
	Label *status_label = nullptr;

	EditorFileDialog *psd_file_dialog = nullptr;
	EditorFileDialog *out_dir_dialog = nullptr;
	EditorFileDialog *asset_dir_dialog = nullptr;
	EditorFileDialog *scene_dir_dialog = nullptr;

	void _browse_psd();
	void _browse_out_dir();
	void _browse_asset_dir();
	void _browse_scene_dir();
	void _on_psd_selected(const String &p_path);
	void _on_out_dir_selected(const String &p_dir);
	void _on_asset_dir_selected(const String &p_dir);
	void _on_scene_dir_selected(const String &p_dir);
	void _append_dir_line(TextEdit *p_edit, const String &p_dir);
	void _convert();
	void _set_status(const String &p_message, bool p_is_error);

protected:
	static void _bind_methods() {}

public:
	void popup_convert_dialog();

	PsdUiConvertDialog();
};

class PsdUiEditorPlugin : public EditorPlugin {
	GDCLASS(PsdUiEditorPlugin, EditorPlugin);

	PsdUiConvertDialog *dialog = nullptr;

	void _open_dialog();

protected:
	static void _bind_methods() {}

public:
	PsdUiEditorPlugin();
};

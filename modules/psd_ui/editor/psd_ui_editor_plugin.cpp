/**************************************************************************/
/*  psd_ui_editor_plugin.cpp                                              */
/**************************************************************************/

#include "psd_ui_editor_plugin.h"

#include "modules/psd_ui/psd_ui_converter.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/templates/hash_set.h"
#include "editor/editor_interface.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/control.h"
#include "scene/gui/text_edit.h"
#include "scene/resources/packed_scene.h"

namespace {
const char *PSD_UI_ASSET_DIRS_SETTING = "psd_ui/common_asset_dirs";
const char *PSD_UI_SCENE_DIRS_SETTING = "psd_ui/common_scene_dirs";

// Normalize a name for matching: drop extension, apply node-name sanitization, lowercase.
String psd_normalize(const String &p_name) {
	return PsdUiConverter::sanitize_name(p_name.get_basename()).to_lower();
}

// Recursively scan a res:// directory, mapping normalized base name -> resource path for files whose extension
// is in p_exts. The first match for a given normalized key wins.
void psd_scan_dir(const String &p_dir, const HashSet<String> &p_exts, HashMap<String, String> &r_map) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String name = da->get_next();
	while (!name.is_empty()) {
		if (name != "." && name != "..") {
			const String full = p_dir.path_join(name);
			if (da->current_is_dir()) {
				psd_scan_dir(full, p_exts, r_map);
			} else if (p_exts.has(name.get_extension().to_lower())) {
				const String key = psd_normalize(name);
				if (!r_map.has(key)) {
					r_map[key] = full;
				} else {
					WARN_PRINT("psd_ui: duplicate common resource name '" + key + "', keeping " + r_map[key] + ", ignoring " + full);
				}
			}
		}
		name = da->get_next();
	}
	da->list_dir_end();
}

// Parse a multi-line string (one res:// dir per line) into a list of trimmed, non-empty dir paths.
Vector<String> psd_dir_lines(const String &p_text) {
	Vector<String> out;
	const Vector<String> lines = p_text.split("\n", false);
	for (int i = 0; i < lines.size(); ++i) {
		const String d = lines[i].strip_edges();
		if (!d.is_empty()) {
			out.push_back(d);
		}
	}
	return out;
}
} // namespace


void PsdUiConvertDialog::_set_status(const String &p_message, bool p_is_error) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_message);
	status_label->add_theme_color_override(SceneStringName(font_color), p_is_error ? Color(1, 0.4, 0.4) : Color(0.4, 1, 0.4));
}

void PsdUiConvertDialog::_browse_psd() {
	if (psd_file_dialog) {
		psd_file_dialog->popup_file_dialog();
	}
}

void PsdUiConvertDialog::_browse_out_dir() {
	if (out_dir_dialog) {
		out_dir_dialog->popup_file_dialog();
	}
}

void PsdUiConvertDialog::_on_psd_selected(const String &p_path) {
	psd_path_edit->set_text(p_path);
}

void PsdUiConvertDialog::_on_out_dir_selected(const String &p_dir) {
	out_dir_edit->set_text(p_dir);
}

void PsdUiConvertDialog::_browse_asset_dir() {
	if (asset_dir_dialog) {
		asset_dir_dialog->popup_file_dialog();
	}
}

void PsdUiConvertDialog::_browse_scene_dir() {
	if (scene_dir_dialog) {
		scene_dir_dialog->popup_file_dialog();
	}
}

void PsdUiConvertDialog::_append_dir_line(TextEdit *p_edit, const String &p_dir) {
	if (!p_edit) {
		return;
	}
	String t = p_edit->get_text();
	if (!t.is_empty() && !t.ends_with("\n")) {
		t += "\n";
	}
	t += p_dir;
	p_edit->set_text(t);
}

void PsdUiConvertDialog::_on_asset_dir_selected(const String &p_dir) {
	_append_dir_line(asset_dirs_edit, p_dir);
}

void PsdUiConvertDialog::_on_scene_dir_selected(const String &p_dir) {
	_append_dir_line(scene_dirs_edit, p_dir);
}

void PsdUiConvertDialog::_convert() {
	const String psd_path = psd_path_edit->get_text().strip_edges();
	String out_dir = out_dir_edit->get_text().strip_edges();

	if (psd_path.is_empty() || !FileAccess::exists(psd_path)) {
		_set_status(TTR("Please choose a valid .psd file."), true);
		return;
	}
	if (out_dir.is_empty()) {
		out_dir = "res://";
	}
	if (!out_dir.begins_with("res://")) {
		_set_status(TTR("The output folder must be inside the project (a res:// path)."), true);
		return;
	}

	PsdUiConverter::Options options;
	options.skip_hidden = skip_hidden_check->is_pressed();
	options.root_size = root_size_check->is_pressed();
	options.text_as_label = text_as_label_check->is_pressed();

	// Persist the common directories to ProjectSettings so they are remembered next time.
	const Vector<String> asset_dirs = psd_dir_lines(asset_dirs_edit->get_text());
	const Vector<String> scene_dirs = psd_dir_lines(scene_dirs_edit->get_text());
	if (ProjectSettings *ps = ProjectSettings::get_singleton()) {
		PackedStringArray a;
		for (const String &d : asset_dirs) {
			a.push_back(d);
		}
		PackedStringArray s;
		for (const String &d : scene_dirs) {
			s.push_back(d);
		}
		ps->set_setting(PSD_UI_ASSET_DIRS_SETTING, a);
		ps->set_setting(PSD_UI_SCENE_DIRS_SETTING, s);
		ps->save();
	}

	Vector<PsdLayerInfo> layers;
	Size2i doc_size;
	Error err = PsdUiConverter::parse(psd_path, options, layers, doc_size);
	if (err != OK) {
		_set_status(TTR("Failed to parse the PSD file. See the Output panel for details."), true);
		return;
	}

	// Scan the common directories: normalized name -> res:// path.
	static const char *kImageExts[] = { "png", "svg", "jpg", "jpeg", "webp", "bmp", "tga" };
	HashSet<String> image_exts;
	for (const char *e : kImageExts) {
		image_exts.insert(String(e));
	}
	HashSet<String> scene_exts;
	scene_exts.insert("tscn");
	scene_exts.insert("scn");

	HashMap<String, String> asset_map;
	for (const String &d : asset_dirs) {
		psd_scan_dir(d, image_exts, asset_map);
	}
	HashMap<String, String> scene_map;
	for (const String &d : scene_dirs) {
		psd_scan_dir(d, scene_exts, scene_map);
	}

	const String safe_base = PsdUiConverter::sanitize_name(psd_path.get_file().get_basename());
	const String tex_dir = out_dir.path_join(safe_base + "_textures");

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (da.is_valid()) {
		da->make_dir_recursive(tex_dir);
	}

	// Decide each layer's source. Priority: common scene > common asset > exported PNG.
	// PNG export is done in three phases because importing and loading a texture in the same step is unreliable:
	// the imported resource is not yet available to ResourceLoader::load() right after ResourceLoader::import().
	EditorFileSystem *efs = EditorFileSystem::get_singleton();

	HashMap<int, Ref<PackedScene>> external_scenes;
	HashMap<int, Ref<Texture2D>> textures;
	HashMap<int, String> layer_png;
	Vector<String> png_paths;

	for (int i = 0; i < layers.size(); ++i) {
		const PsdLayerInfo &info = layers[i];
		const String key = psd_normalize(info.name);

		// 1) Common scene substitution (any layer type). PSD child layers are preserved under the instance.
		if (scene_map.has(key)) {
			Ref<PackedScene> ps_scene = ResourceLoader::load(scene_map[key], "PackedScene");
			if (ps_scene.is_valid()) {
				external_scenes[i] = ps_scene;
				continue;
			}
			WARN_PRINT("psd_ui: failed to load common scene, falling back: " + scene_map[key]);
		}

		// Only image layers are eligible for texture substitution / export below.
		if (info.is_group) {
			continue;
		}
		if (info.is_text && options.text_as_label) {
			continue; // becomes a Label, no texture needed
		}
		if (info.image.is_null()) {
			continue;
		}

		// 2) Common image asset substitution: reference the shared asset directly (no local PNG export).
		if (asset_map.has(key)) {
			Ref<Texture2D> atex = ResourceLoader::load(asset_map[key], "Texture2D");
			if (atex.is_valid()) {
				textures[i] = atex;
				continue;
			}
			WARN_PRINT("psd_ui: failed to load common asset, falling back: " + asset_map[key]);
		}

		// 3) Export the layer as an external PNG (Phase 1: write + register for the batch import below).
		const String png_path = tex_dir.path_join(PsdUiConverter::sanitize_name(info.name) + "_" + itos(i) + ".png");
		const Error png_err = info.image->save_png(png_path);
		if (png_err != OK) {
			WARN_PRINT("Failed to save PSD layer texture: " + png_path);
			continue;
		}
		layer_png[i] = png_path;
		png_paths.push_back(png_path);
		if (efs) {
			efs->update_file(png_path);
		}
	}

	// Phase 2: import all exported PNGs in a single batch so the imported textures are ready before loading.
	if (efs && !png_paths.is_empty()) {
		efs->reimport_files(png_paths);
	}

	// Phase 3: load each imported texture. Referencing a res:// texture makes ResourceSaver write an [ext_resource].
	int embedded_fallback = 0;
	for (const KeyValue<int, String> &E : layer_png) {
		Ref<Texture2D> tex = ResourceLoader::load(E.value, "Texture2D");
		if (tex.is_valid()) {
			textures[E.key] = tex;
		} else {
			++embedded_fallback;
			WARN_PRINT("Could not load imported texture, embedding instead: " + E.value);
		}
	}
	if (embedded_fallback > 0) {
		WARN_PRINT(vformat("PSD->GUI: %d layer texture(s) fell back to embedding.", embedded_fallback));
	}

	Control *root = PsdUiConverter::build_scene(layers, doc_size, options, PsdUiConverter::TEXTURE_EXTERNAL, &textures, &external_scenes);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	err = packed_scene->pack(root);
	if (err != OK) {
		memdelete(root);
		_set_status(TTR("Failed to pack the generated scene."), true);
		return;
	}

	const String tscn_path = out_dir.path_join(safe_base + ".tscn");
	err = ResourceSaver::save(packed_scene, tscn_path);
	memdelete(root);
	if (err != OK) {
		_set_status(TTR("Failed to save the scene: ") + tscn_path, true);
		return;
	}

	// Refresh the FileSystem dock and open the new scene for editing.
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->scan();
	}
	_set_status(vformat(TTR("Created %s"), tscn_path), false);
	hide();

	if (EditorInterface::get_singleton()) {
		EditorInterface::get_singleton()->open_scene_from_path(tscn_path);
	}
}

void PsdUiConvertDialog::popup_convert_dialog() {
	_set_status(String(), false);

	// Load remembered common directories from ProjectSettings.
	if (ProjectSettings *ps = ProjectSettings::get_singleton()) {
		if (asset_dirs_edit) {
			const PackedStringArray a = ps->get_setting(PSD_UI_ASSET_DIRS_SETTING, PackedStringArray());
			asset_dirs_edit->set_text(String("\n").join(a));
		}
		if (scene_dirs_edit) {
			const PackedStringArray s = ps->get_setting(PSD_UI_SCENE_DIRS_SETTING, PackedStringArray());
			scene_dirs_edit->set_text(String("\n").join(s));
		}
	}

	// The dialog is already parented to the editor base control, so just center and show it.
	popup_centered(Size2i(560, 470));
}

PsdUiConvertDialog::PsdUiConvertDialog() {
	set_title(TTR("Convert PSD to GUI Scene"));
	set_ok_button_text(TTR("Convert"));
	set_hide_on_ok(false); // keep the dialog open on validation errors; we hide() manually on success.

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	// PSD file row.
	{
		HBoxContainer *hb = memnew(HBoxContainer);
		vb->add_child(hb);

		Label *l = memnew(Label);
		l->set_text(TTR("PSD File:"));
		l->set_custom_minimum_size(Size2(120, 0));
		hb->add_child(l);

		psd_path_edit = memnew(LineEdit);
		psd_path_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		psd_path_edit->set_placeholder(TTR("Path to a .psd file"));
		hb->add_child(psd_path_edit);

		Button *b = memnew(Button);
		b->set_text(TTR("Browse..."));
		b->connect(SceneStringName(pressed), callable_mp(this, &PsdUiConvertDialog::_browse_psd));
		hb->add_child(b);
	}

	// Output folder row.
	{
		HBoxContainer *hb = memnew(HBoxContainer);
		vb->add_child(hb);

		Label *l = memnew(Label);
		l->set_text(TTR("Output Folder:"));
		l->set_custom_minimum_size(Size2(120, 0));
		hb->add_child(l);

		out_dir_edit = memnew(LineEdit);
		out_dir_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		out_dir_edit->set_text("res://");
		hb->add_child(out_dir_edit);

		Button *b = memnew(Button);
		b->set_text(TTR("Browse..."));
		b->connect(SceneStringName(pressed), callable_mp(this, &PsdUiConvertDialog::_browse_out_dir));
		hb->add_child(b);
	}

	skip_hidden_check = memnew(CheckBox);
	skip_hidden_check->set_text(TTR("Skip hidden layers"));
	skip_hidden_check->set_pressed(true);
	vb->add_child(skip_hidden_check);

	root_size_check = memnew(CheckBox);
	root_size_check->set_text(TTR("Set root size to document size"));
	root_size_check->set_pressed(true);
	vb->add_child(root_size_check);

	text_as_label_check = memnew(CheckBox);
	text_as_label_check->set_text(TTR("Convert text layers to Label nodes"));
	text_as_label_check->set_pressed(true);
	vb->add_child(text_as_label_check);

	// Common asset directories (images). One res:// path per line; a layer whose name matches an asset here
	// references that shared image instead of exporting its own PNG.
	{
		Label *l = memnew(Label);
		l->set_text(TTR("Common asset dirs (images) — one res:// path per line:"));
		vb->add_child(l);

		HBoxContainer *hb = memnew(HBoxContainer);
		vb->add_child(hb);

		asset_dirs_edit = memnew(TextEdit);
		asset_dirs_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		asset_dirs_edit->set_custom_minimum_size(Size2(0, 48));
		hb->add_child(asset_dirs_edit);

		Button *b = memnew(Button);
		b->set_text(TTR("Add Dir..."));
		b->connect(SceneStringName(pressed), callable_mp(this, &PsdUiConvertDialog::_browse_asset_dir));
		hb->add_child(b);
	}

	// Common UI scene directories (.tscn). A layer whose name matches a scene here becomes an instance of it,
	// with the layer's PSD children kept underneath.
	{
		Label *l = memnew(Label);
		l->set_text(TTR("Common UI dirs (.tscn) — one res:// path per line:"));
		vb->add_child(l);

		HBoxContainer *hb = memnew(HBoxContainer);
		vb->add_child(hb);

		scene_dirs_edit = memnew(TextEdit);
		scene_dirs_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		scene_dirs_edit->set_custom_minimum_size(Size2(0, 48));
		hb->add_child(scene_dirs_edit);

		Button *b = memnew(Button);
		b->set_text(TTR("Add Dir..."));
		b->connect(SceneStringName(pressed), callable_mp(this, &PsdUiConvertDialog::_browse_scene_dir));
		hb->add_child(b);
	}

	status_label = memnew(Label);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->set_custom_minimum_size(Size2(0, 24));
	vb->add_child(status_label);

	// File pickers. EditorFileDialog is a Window, so AcceptDialog does not lay it out as content.
	psd_file_dialog = memnew(EditorFileDialog);
	psd_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	psd_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	psd_file_dialog->add_filter("*.psd", TTR("Photoshop Document"));
	psd_file_dialog->connect("file_selected", callable_mp(this, &PsdUiConvertDialog::_on_psd_selected));
	add_child(psd_file_dialog);

	out_dir_dialog = memnew(EditorFileDialog);
	out_dir_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	out_dir_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	out_dir_dialog->connect("dir_selected", callable_mp(this, &PsdUiConvertDialog::_on_out_dir_selected));
	add_child(out_dir_dialog);

	asset_dir_dialog = memnew(EditorFileDialog);
	asset_dir_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	asset_dir_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	asset_dir_dialog->connect("dir_selected", callable_mp(this, &PsdUiConvertDialog::_on_asset_dir_selected));
	add_child(asset_dir_dialog);

	scene_dir_dialog = memnew(EditorFileDialog);
	scene_dir_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	scene_dir_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	scene_dir_dialog->connect("dir_selected", callable_mp(this, &PsdUiConvertDialog::_on_scene_dir_selected));
	add_child(scene_dir_dialog);

	connect(SceneStringName(confirmed), callable_mp(this, &PsdUiConvertDialog::_convert));
}

void PsdUiEditorPlugin::_open_dialog() {
	if (!dialog) {
		dialog = memnew(PsdUiConvertDialog);
		EditorInterface::get_singleton()->get_base_control()->add_child(dialog);
	}
	dialog->popup_convert_dialog();
}

PsdUiEditorPlugin::PsdUiEditorPlugin() {
	add_tool_menu_item("Convert PSD to GUI Scene...", callable_mp(this, &PsdUiEditorPlugin::_open_dialog));
}

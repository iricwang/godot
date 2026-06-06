/**************************************************************************/
/*  psd_ui_importer.cpp                                                   */
/**************************************************************************/

#include "psd_ui_importer.h"
#include "psd_ui_converter.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "editor/file_system/editor_file_system.h"

String PsdUiImporter::get_importer_name() const {
	return "psd_ui";
}

String PsdUiImporter::get_visible_name() const {
	return "PSD to UI";
}

void PsdUiImporter::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("psd");
}

String PsdUiImporter::get_save_extension() const {
	return "tscn";
}

String PsdUiImporter::get_resource_type() const {
	return "PackedScene";
}

int PsdUiImporter::get_preset_count() const {
	return 0;
}

String PsdUiImporter::get_preset_name(int p_idx) const {
	return String();
}

void PsdUiImporter::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "ui/skip_hidden"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "ui/root_size"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "ui/text_as_label"), true));
}

bool PsdUiImporter::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error PsdUiImporter::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	PsdUiConverter::Options options;
	options.skip_hidden = p_options.has("ui/skip_hidden") ? (bool)p_options["ui/skip_hidden"] : true;
	options.root_size = p_options.has("ui/root_size") ? (bool)p_options["ui/root_size"] : true;
	options.text_as_label = p_options.has("ui/text_as_label") ? (bool)p_options["ui/text_as_label"] : true;

	// 1) Parse the PSD file into layers.
	Vector<PsdLayerInfo> layers;
	Size2i doc_size;
	Error err = PsdUiConverter::parse(p_source_file, options, layers, doc_size);
	if (err != OK) {
		return err;
	}

	// 2) Export layer images as PNG files so the generated scene references
	//    external textures rather than embedding ImageTexture data.
	//    Place textures alongside the generated .tscn:  <save_path>.tscn + <save_path>_textures/
	const String tscn_path = p_save_path + "." + get_save_extension();
	const String tex_dir = p_save_path + "_textures";

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (da.is_valid()) {
		da->make_dir_recursive(tex_dir);
	}

	HashMap<int, Ref<Texture2D>> textures;
	Vector<String> png_paths;
	EditorFileSystem *efs = EditorFileSystem::get_singleton();

	for (int i = 0; i < layers.size(); ++i) {
		const PsdLayerInfo &info = layers[i];

		// Only image layers produce textures; groups and text-as-label layers are skipped.
		if (info.is_group) {
			continue;
		}
		if (info.is_text && options.text_as_label) {
			continue;
		}
		if (info.image.is_null()) {
			continue;
		}

		const String png_path = tex_dir.path_join(PsdUiConverter::sanitize_name(info.name) + "_" + itos(i) + ".png");
		err = info.image->save_png(png_path);
		if (err != OK) {
			WARN_PRINT("psd_ui: failed to export layer texture: " + png_path);
			continue;
		}
		png_paths.push_back(png_path);
		if (efs) {
			efs->update_file(png_path);
		}
	}

	// Batch-import all exported PNGs so imported textures are ready before loading.
	if (efs && !png_paths.is_empty()) {
		efs->reimport_files(png_paths);
	}

	// Load each imported texture.  Referencing a res:// texture makes ResourceSaver
	// write an [ext_resource] entry instead of embedding an [sub_resource] in the .tscn.
	int embedded_fallback = 0;
	for (int i = 0; i < layers.size(); ++i) {
		const PsdLayerInfo &info = layers[i];
		if (info.is_group || info.image.is_null()) {
			continue;
		}
		if (info.is_text && options.text_as_label) {
			continue;
		}

		const String png_path = tex_dir.path_join(PsdUiConverter::sanitize_name(info.name) + "_" + itos(i) + ".png");
		Ref<Texture2D> tex = ResourceLoader::load(png_path, "Texture2D");
		if (tex.is_valid()) {
			textures[i] = tex;
		} else {
			++embedded_fallback;
			WARN_PRINT("psd_ui: could not load imported texture, embedding instead: " + png_path);
		}
	}
	if (embedded_fallback > 0) {
		WARN_PRINT(vformat("psd_ui: %d layer texture(s) fell back to embedding.", embedded_fallback));
	}

	// Register all generated PNGs so the engine tracks them as sub-resources.
	if (r_gen_files) {
		for (const String &png_path : png_paths) {
			r_gen_files->push_back(png_path);
		}
	}

	// 3) Build the scene tree.  Pass external textures for every layer whose PNG was
	//    successfully loaded; layers without an entry in the map fall back to embedding
	//    (TEXTURE_EXTERNAL + missing → ImageTexture::create_from_image in build_scene).
	Control *root = PsdUiConverter::build_scene(layers, doc_size, options,
			PsdUiConverter::TEXTURE_EXTERNAL, &textures, nullptr);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	err = packed_scene->pack(root);
	if (err != OK) {
		ERR_PRINT("psd_ui: failed to pack generated scene.");
		memdelete(root);
		return err;
	}

	err = ResourceSaver::save(packed_scene, tscn_path);
	memdelete(root);
	if (err != OK) {
		ERR_PRINT("psd_ui: failed to save generated scene: " + tscn_path);
		return err;
	}

	return OK;
}

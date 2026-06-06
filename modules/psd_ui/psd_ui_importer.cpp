/**************************************************************************/
/*  psd_ui_importer.cpp                                                   */
/**************************************************************************/

#include "psd_ui_importer.h"
#include "psd_ui_converter.h"

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

	return PsdUiConverter::convert(p_source_file, p_save_path + "." + get_save_extension(), options);
}

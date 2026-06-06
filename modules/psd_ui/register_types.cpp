/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "core/io/resource_importer.h"
#include "psd_ui_importer.h"

#ifdef TOOLS_ENABLED
#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/psd_ui_editor_plugin.h"
#endif

static Ref<PsdUiImporter> psd_ui_importer;

void initialize_psd_ui_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		psd_ui_importer.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(psd_ui_importer);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Common resource directories used by the PSD->GUI converter (see psd_ui_editor_plugin.cpp).
		GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, "psd_ui/common_asset_dirs"), PackedStringArray());
		GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, "psd_ui/common_scene_dirs"), PackedStringArray());

		GDREGISTER_INTERNAL_CLASS(PsdUiConvertDialog);
		GDREGISTER_INTERNAL_CLASS(PsdUiEditorPlugin);
		EditorPlugins::add_by_type<PsdUiEditorPlugin>();
	}
#endif
}

void uninitialize_psd_ui_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (psd_ui_importer.is_valid()) {
			ResourceFormatImporter::get_singleton()->remove_importer(psd_ui_importer);
			psd_ui_importer.unref();
		}
	}
}

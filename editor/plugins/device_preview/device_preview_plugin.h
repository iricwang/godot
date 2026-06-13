/**************************************************************************/
/*  device_preview_plugin.h                                               */
/**************************************************************************/
#pragma once

#include "editor/plugins/editor_plugin.h"

class DeviceProfile;
class MenuButton;

class DevicePreviewPlugin : public EditorPlugin {
	GDCLASS(DevicePreviewPlugin, EditorPlugin);

	enum {
		MENU_FREE = 0,
		MENU_CUSTOM = 1,
		MENU_SEPARATOR = 2,
		MENU_DEVICE = 3,
	};

	MenuButton *device_menu = nullptr;

	String current_device_name;
	Ref<DeviceProfile> current_profile;

	Vector2i saved_viewport_size;
	bool preview_active = false;

	void _build_menu();
	void _on_device_selected(int p_id);
	void _apply_preview(const Ref<DeviceProfile> &p_profile);
	void _remove_preview();
	void _update_toolbar_label();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual String get_plugin_name() const override { return "DevicePreview"; }
	virtual bool has_main_screen() const override { return false; }

	DevicePreviewPlugin();
};

/**************************************************************************/
/*  device_preview_plugin.cpp                                             */
/**************************************************************************/

#include "device_preview_plugin.h"
#include "device_database.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_interface.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/window.h"

void DevicePreviewPlugin::_build_menu() {
	PopupMenu *popup = device_menu->get_popup();
	popup->clear();

	popup->add_item(TTR("Free"), MENU_FREE);
	popup->add_separator(TTR("Devices"));
	popup->add_item(TTR("Custom Resolution..."), MENU_CUSTOM);
	popup->add_separator();

	const Vector<Ref<DeviceProfile>> &presets = DeviceDatabase::get_presets();
	for (int i = 0; i < presets.size(); ++i) {
		const Ref<DeviceProfile> &dp = presets[i];
		String label = vformat("%s  (%d\xc3\x97%d)", dp->get_device_name(), dp->get_resolution().x, dp->get_resolution().y);
		popup->add_radio_check_item(label, MENU_DEVICE + i);
		if (dp == current_profile) {
			popup->set_item_checked(popup->get_item_count() - 1, true);
		}
	}

	if (current_profile.is_null()) {
		popup->set_item_checked(popup->get_item_index(MENU_FREE), true);
	}
}

void DevicePreviewPlugin::_on_device_selected(int p_id) {
	if (p_id == MENU_FREE) {
		current_profile.unref();
		current_device_name = "";
		_remove_preview();
	} else if (p_id == MENU_CUSTOM) {
		current_profile.unref();
		current_device_name = "";
		_remove_preview();
	} else if (p_id >= MENU_DEVICE) {
		int idx = p_id - MENU_DEVICE;
		const Vector<Ref<DeviceProfile>> &presets = DeviceDatabase::get_presets();
		if (idx >= 0 && idx < presets.size()) {
			current_profile = presets[idx];
			current_device_name = current_profile->get_device_name();
			_apply_preview(current_profile);
		}
	}
	_update_toolbar_label();
	_build_menu();
}

void DevicePreviewPlugin::_apply_preview(const Ref<DeviceProfile> &p_profile) {
	_remove_preview();

	EditorInterface *ei = EditorInterface::get_singleton();
	ERR_FAIL_NULL(ei);

	SubViewport *scene_vp = ei->get_editor_viewport_2d();
	ERR_FAIL_NULL(scene_vp);

	Vector2i res = _apply_preview_orientation(p_profile->get_resolution());

	// Save the current viewport size so we can restore it later.
	saved_viewport_size = scene_vp->get_size();

	// Use size_2d_override to drive the Control "layout parent rect" to the
	// device resolution. The editor's SubViewportContainer has stretch=true,
	// so set_size() on this SubViewport is ignored — only size_2d_override
	// and the stretch transform actually take effect.
	scene_vp->set_size_2d_override(res);
	scene_vp->set_size_2d_override_stretch(true);

	preview_active = true;
}

Vector2i DevicePreviewPlugin::_apply_preview_orientation(const Vector2i &p_size) const {
	if (p_size == Size2i() || p_size.x == p_size.y) {
		return p_size;
	}
	const bool is_landscape = p_size.x > p_size.y;
	if (preview_resolution_landscape != is_landscape) {
		Size2i swapped = p_size;
		SWAP(swapped.x, swapped.y);
		return swapped;
	}
	return p_size;
}

void DevicePreviewPlugin::_remove_preview() {
	if (!preview_active) {
		return;
	}

	EditorInterface *ei = EditorInterface::get_singleton();
	if (!ei) {
		return;
	}

	SubViewport *scene_vp = ei->get_editor_viewport_2d();
	if (scene_vp) {
		// Reset the 2D layout parent rect and the stretch transform. The
		// SubViewport's own size is left alone (the parent SubViewportContainer
		// has stretch=true, so set_size is ignored on it anyway).
		scene_vp->set_size_2d_override(Size2i());
		scene_vp->set_size_2d_override_stretch(false);
	}

	saved_viewport_size = Vector2i();
	preview_active = false;
}

void DevicePreviewPlugin::_update_toolbar_label() {
	if (current_device_name.is_empty()) {
		device_menu->set_text(TTR("Free"));
	} else {
		device_menu->set_text(current_device_name);
	}
}

void DevicePreviewPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			device_menu = memnew(MenuButton);
			device_menu->set_text(TTR("Free"));
			device_menu->set_tooltip_text(TTR("Device Preview Resolution"));
			device_menu->set_flat(false);
			device_menu->get_popup()->connect("id_pressed",
					callable_mp(this, &DevicePreviewPlugin::_on_device_selected));

			add_control_to_container(CONTAINER_TOOLBAR, device_menu);
			_build_menu();
			_update_toolbar_label();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_remove_preview();
			if (device_menu) {
				remove_control_from_container(CONTAINER_TOOLBAR, device_menu);
				device_menu->queue_free();
				device_menu = nullptr;
			}
		} break;
	}
}

void DevicePreviewPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_preview_resolution_landscape", "landscape"), &DevicePreviewPlugin::set_preview_resolution_landscape);
	ClassDB::bind_method(D_METHOD("is_preview_resolution_landscape"), &DevicePreviewPlugin::is_preview_resolution_landscape);
	ClassDB::bind_method(D_METHOD("get_preview_resolution"), &DevicePreviewPlugin::get_preview_resolution);
}

void DevicePreviewPlugin::set_preview_resolution_landscape(bool p_landscape) {
	if (preview_resolution_landscape == p_landscape) {
		return;
	}
	preview_resolution_landscape = p_landscape;
	if (current_profile.is_valid()) {
		_apply_preview(current_profile);
	}
	_update_toolbar_label();
	_build_menu();
}

Vector2i DevicePreviewPlugin::get_preview_resolution() const {
	if (current_profile.is_null() || !preview_active) {
		return Size2i();
	}
	return _apply_preview_orientation(current_profile->get_resolution());
}

DevicePreviewPlugin::DevicePreviewPlugin() {
}

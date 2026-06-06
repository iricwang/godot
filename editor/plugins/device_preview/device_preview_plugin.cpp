/**************************************************************************/
/*  device_preview_plugin.cpp                                             */
/**************************************************************************/

#include "device_preview_plugin.h"
#include "device_database.h"

#include "core/object/callable_mp.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_main_screen.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/resources/style_box_flat.h"

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
		if (dp->get_device_name() == current_device_name) {
			popup->set_item_checked(popup->get_item_count() - 1, true);
		}
	}

	if (current_device_name.is_empty()) {
		popup->set_item_checked(popup->get_item_index(MENU_FREE), true);
	}
}

void DevicePreviewPlugin::_on_device_selected(int p_id) {
	if (p_id == MENU_FREE) {
		current_device_name = "";
		_remove_preview();
	} else if (p_id == MENU_CUSTOM) {
		// TODO: Show a dialog for custom resolution input.
		current_device_name = "";
		_remove_preview();
	} else if (p_id >= MENU_DEVICE) {
		int idx = p_id - MENU_DEVICE;
		const Vector<Ref<DeviceProfile>> &presets = DeviceDatabase::get_presets();
		if (idx >= 0 && idx < presets.size()) {
			const Ref<DeviceProfile> &dp = presets[idx];
			current_device_name = dp->get_device_name();
			_apply_preview(dp);
		}
	}
	_update_toolbar_label();
	_build_menu();
}

void DevicePreviewPlugin::_apply_preview(const Ref<DeviceProfile> &p_profile) {
	_remove_preview();

	if (p_profile.is_null()) {
		return;
	}

	EditorInterface *ei = EditorInterface::get_singleton();
	ERR_FAIL_NULL(ei);

	// Get the editor main screen area where the 2D/3D viewport lives.
	VBoxContainer *main_screen = ei->get_editor_main_screen();
	ERR_FAIL_NULL(main_screen);

	Vector2i res = p_profile->get_resolution();

	matte = memnew(MarginContainer);
	matte->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	matte->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	// Dark matte background.
	Ref<StyleBoxFlat> matte_style;
	matte_style.instantiate();
	matte_style->set_bg_color(Color(0.05, 0.05, 0.05, 1.0));
	matte->add_theme_style_override("panel", matte_style);

	ratio_container = memnew(AspectRatioContainer);
	ratio_container->set_ratio((float)res.x / (float)res.y);
	ratio_container->set_stretch_mode(AspectRatioContainer::STRETCH_FIT);

	// The preview frame: a semi-transparent rectangle at the device resolution.
	Panel *frame = memnew(Panel);
	frame->set_custom_minimum_size(Vector2(res));

	Ref<StyleBoxFlat> frame_style;
	frame_style.instantiate();
	frame_style->set_bg_color(Color(0, 0, 0, 0)); // transparent interior
	frame_style->set_border_width_all(2);
	frame_style->set_border_color(Color(1, 1, 1, 0.4)); // white semi-transparent border
	frame->add_theme_style_override("panel", frame_style);

	// Device name + resolution label overlay.
	Label *info_label = memnew(Label);
	info_label->set_text(vformat("%s\n%d\xc3\x97%d @ %.0f dpi",
			p_profile->get_device_name(), res.x, res.y, p_profile->get_dpi()));
	info_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	info_label->add_theme_color_override("font_color", Color(1, 1, 1, 0.6));
	info_label->add_theme_font_size_override("font_size", 12 * EDSCALE);

	VBoxContainer *frame_vb = memnew(VBoxContainer);
	frame_vb->add_child(info_label);
	frame_vb->add_child(frame);

	ratio_container->add_child(frame_vb);
	matte->add_child(ratio_container);
	main_screen->add_child(matte);

	preview_overlay = matte;
}

void DevicePreviewPlugin::_remove_preview() {
	if (preview_overlay) {
		preview_overlay->queue_free();
		preview_overlay = nullptr;
		matte = nullptr;
		ratio_container = nullptr;
		svp_container = nullptr;
		preview_viewport = nullptr;
	}
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
			// Create the toolbar button.
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
}

DevicePreviewPlugin::DevicePreviewPlugin() {
}

DevicePreviewPlugin::~DevicePreviewPlugin() {
}

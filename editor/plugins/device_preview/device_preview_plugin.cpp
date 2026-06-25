/**************************************************************************/
/*  device_preview_plugin.cpp                                             */
/**************************************************************************/

#include "device_preview_plugin.h"
#include "device_database.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_interface.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/subviewport_container.h"

DevicePreviewPlugin *DevicePreviewPlugin::singleton = nullptr;

static String _label_for_class(DeviceProfile::DeviceClass p_class) {
	switch (p_class) {
		case DeviceProfile::DEVICE_CLASS_PHONE:
			return TTR("Phones");
		case DeviceProfile::DEVICE_CLASS_TABLET:
			return TTR("Tablets");
		case DeviceProfile::DEVICE_CLASS_DESKTOP:
			return TTR("Desktop");
		case DeviceProfile::DEVICE_CLASS_CONSOLE:
			return TTR("Consoles");
		case DeviceProfile::DEVICE_CLASS_GENERIC:
			break;
	}
	return TTR("Generic");
}

void DevicePreviewPlugin::_build_menu() {
	device_picker->clear();

	// The "Free" entry is the only OptionButton item that lives at the top
	// (no separator group label above it) -- matches the Renderer dropdown
	// pattern of having a plain default at index 0.
	device_picker->add_item(TTR("Free"), ITEM_FREE);

	// Group presets by DeviceClass so the user can tell at a glance which
	// branch a preset will activate. Order is intentional: PHONE, TABLET,
	// DESKTOP, CONSOLE, GENERIC -- mobile presets first because that is
	// the most common reason to open this picker.
	const DeviceProfile::DeviceClass class_order[] = {
		DeviceProfile::DEVICE_CLASS_PHONE,
		DeviceProfile::DEVICE_CLASS_TABLET,
		DeviceProfile::DEVICE_CLASS_DESKTOP,
		DeviceProfile::DEVICE_CLASS_CONSOLE,
		DeviceProfile::DEVICE_CLASS_GENERIC,
	};

	int selected_index = 0; // "Free" by default.
	const Vector<Ref<DeviceProfile>> &presets = DeviceDatabase::get_presets();
	for (DeviceProfile::DeviceClass cls : class_order) {
		bool printed_header = false;
		for (int i = 0; i < presets.size(); ++i) {
			const Ref<DeviceProfile> &dp = presets[i];
			if (dp->get_device_class() != cls) {
				continue;
			}
			if (!printed_header) {
				device_picker->add_separator(_label_for_class(cls));
				printed_header = true;
			}
			// Popup item: full "Name  (W×H)" so the user can size-check at a
			// glance. The button face itself is kept shorter; see
			// _update_picker_face.
			String label = vformat("%s  (%d\xc3\x97%d)", dp->get_device_name(), dp->get_resolution().x, dp->get_resolution().y);
			device_picker->add_item(label, ITEM_DEVICE_BASE + i);
			if (dp == current_profile) {
				selected_index = device_picker->get_item_count() - 1;
			}
		}
	}

	device_picker->select(selected_index);
}

void DevicePreviewPlugin::_on_item_selected(int p_index) {
	const int id = device_picker->get_item_id(p_index);

	if (id == ITEM_FREE) {
		current_profile.unref();
		current_device_name = "";
		_remove_preview();
	} else if (id >= ITEM_DEVICE_BASE) {
		const int preset_idx = id - ITEM_DEVICE_BASE;
		const Vector<Ref<DeviceProfile>> &presets = DeviceDatabase::get_presets();
		if (preset_idx >= 0 && preset_idx < presets.size()) {
			current_profile = presets[preset_idx];
			current_device_name = current_profile->get_device_name();
			_apply_preview(current_profile);
		}
	}

	// Persist the selection so the next editor session re-applies it.
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set_project_metadata("device_preview", "current_device", current_device_name);
	}

	_update_picker_face();
}

void DevicePreviewPlugin::_apply_preview(const Ref<DeviceProfile> &p_profile) {
	_remove_preview();

	EditorInterface *ei = EditorInterface::get_singleton();
	ERR_FAIL_NULL(ei);

	SubViewport *scene_vp = ei->get_editor_viewport_2d();
	ERR_FAIL_NULL(scene_vp);

	Vector2i res = _apply_preview_orientation(p_profile->get_resolution());

	// Set the viewport to render at the device resolution.
	// The editor's SubViewportContainer (stretch=true) intercepts set_size
	// (SubViewport::_internal_set_size is a no-op there, and current Godot
	// even emits an INTERNAL WARNING for it), so size_2d_override + stretch
	// is what actually drives the layout parent rect via the SubViewport's
	// own stretch_transform. The ProjectSettings update below is the
	// load-bearing piece for Control re-layout INSIDE the previewed scene
	// -- it does NOT scale the editor chrome (which uses its own window
	// size, not display/window/size).
	scene_vp->set_size_2d_override(res);
	scene_vp->set_size_2d_override_stretch(true);

	// Drive the 2D editor's "layout parent rect" through the same channel
	// the running game uses: in-memory ProjectSettings.
	// Control::get_parent_anchorable_rect() has a TOOLS-mode fast path that
	// reads `display/window/size/viewport_width/height` directly, so
	// changing those is what actually re-anchors Container children in the
	// scene being edited.
	//
	// NOTE: We intentionally do NOT touch the editor's main Window
	// content scale here. Setting it to (393, 852) + CANVAS_ITEMS would
	// stretch the entire editor UI when the host window is larger than the
	// device resolution. The scene preview is already correctly framed by
	// the SubViewportContainer above; the Game workspace has its own
	// embedded-window sizing path (see `_update_embed_window_size`).
	{
		ProjectSettings *ps = ProjectSettings::get_singleton();
		// Snapshot only on the first device selection after Free — otherwise
		// a device → device switch would clobber the user's real project
		// settings with the previous device's resolution.
		if (!_has_saved_state) {
			saved_viewport_width = (int)ps->get_setting("display/window/size/viewport_width");
			saved_viewport_height = (int)ps->get_setting("display/window/size/viewport_height");
			saved_stretch_mode = ps->get_setting("display/window/stretch/mode");
			saved_stretch_aspect = ps->get_setting("display/window/stretch/aspect");
		}

		ps->set_setting("display/window/size/viewport_width", res.x);
		ps->set_setting("display/window/size/viewport_height", res.y);
		ps->set_setting("display/window/stretch/mode", "canvas_items");
		ps->set_setting("display/window/stretch/aspect", "keep");
	}

	_has_saved_state = true;

	preview_active = true;

	// Inject the profile's feature tags (e.g. "mobile" for a PHONE profile)
	// into ProjectSettings::custom_features. This is what makes the editor
	// itself (e.g. @tool scripts) and any F5-launched child process see
	// `OS.has_feature("mobile") == true` for the duration of the preview.
	_apply_feature_tags(p_profile->get_effective_feature_tags());
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
	// Drop injected tags first so any layout/observer code reacting to
	// the ProjectSettings restore below sees the post-preview feature set.
	_clear_feature_tags();

	// Restore ProjectSettings before tearing down the viewport so a Control
	// re-layout triggered by the viewport reset sees the correct (free)
	// parent rect.
	if (_has_saved_state) {
		ProjectSettings *ps = ProjectSettings::get_singleton();
		ps->set_setting("display/window/size/viewport_width", saved_viewport_width);
		ps->set_setting("display/window/size/viewport_height", saved_viewport_height);
		ps->set_setting("display/window/stretch/mode", saved_stretch_mode);
		ps->set_setting("display/window/stretch/aspect", saved_stretch_aspect);
		_has_saved_state = false;
	}

	if (!preview_active) {
		return;
	}

	EditorInterface *ei = EditorInterface::get_singleton();
	if (!ei) {
		return;
	}

	SubViewport *scene_vp = ei->get_editor_viewport_2d();
	if (scene_vp) {
		scene_vp->set_size_2d_override(Size2i());
		scene_vp->set_size_2d_override_stretch(false);
	}

	preview_active = false;
}

void DevicePreviewPlugin::_apply_feature_tags(const PackedStringArray &p_tags) {
	const PackedStringArray previous = injected_feature_tags;

	ProjectSettings *ps = ProjectSettings::get_singleton();
	for (const String &tag : injected_feature_tags) {
		ps->remove_custom_feature(tag);
	}
	injected_feature_tags.clear();

	for (const String &tag : p_tags) {
		const String trimmed = tag.strip_edges();
		if (trimmed.is_empty()) {
			continue;
		}
		ps->add_custom_feature(trimmed);
		injected_feature_tags.push_back(trimmed);
	}

	// Single coalesced notification: callers should only see the final
	// state, not the transient "cleared, then re-applied" intermediate.
	if (injected_feature_tags != previous) {
		_update_picker_face();
		emit_signal(SNAME("active_feature_tags_changed"));
	}
}

void DevicePreviewPlugin::_clear_feature_tags() {
	if (injected_feature_tags.is_empty()) {
		return;
	}
	ProjectSettings *ps = ProjectSettings::get_singleton();
	for (const String &tag : injected_feature_tags) {
		ps->remove_custom_feature(tag);
	}
	injected_feature_tags.clear();
	_update_picker_face();
	emit_signal(SNAME("active_feature_tags_changed"));
}

void DevicePreviewPlugin::_update_picker_face() {
	if (!device_picker) {
		return;
	}

	// The OptionButton automatically shows the selected item's full label
	// (which includes the resolution). We only override the *tooltip* so it
	// surfaces the active feature branch -- the button face stays clean.
	String tooltip;
	if (current_device_name.is_empty()) {
		tooltip = TTR("Device preview: Free.\nThe scene is rendered at its project-default resolution and uses the host platform's feature tags.");
	} else if (current_profile.is_valid()) {
		const Vector2i oriented = _apply_preview_orientation(current_profile->get_resolution());
		tooltip = vformat(TTR("Device preview: %s (%d\xc3\x97%d)"), current_device_name, oriented.x, oriented.y);
	} else {
		tooltip = current_device_name;
	}
	if (!injected_feature_tags.is_empty()) {
		tooltip += "\n" + vformat(TTR("Active branch: %s"), String(", ").join(injected_feature_tags));
	} else {
		tooltip += "\n" + TTR("Active branch: (none injected; using host platform tags)");
	}
	device_picker->set_tooltip_text(tooltip);
}

void DevicePreviewPlugin::_restore_persisted_selection() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	const String persisted = EditorSettings::get_singleton()->get_project_metadata("device_preview", "current_device", String());
	if (persisted.is_empty()) {
		return;
	}
	Ref<DeviceProfile> dp = DeviceDatabase::find_by_name(persisted);
	if (dp.is_null()) {
		// The persisted device is gone (e.g. database changed). Drop the
		// stale metadata so we don't keep retrying every editor launch.
		EditorSettings::get_singleton()->set_project_metadata("device_preview", "current_device", String());
		return;
	}
	current_profile = dp;
	current_device_name = dp->get_device_name();
	_apply_preview(dp);
}

void DevicePreviewPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			device_picker = memnew(OptionButton);
			// Match the Renderer dropdown that sits in the same title bar:
			// `TopBarOptionButton` is a project-wide theme variation that
			// renders bold text and tighter padding, and `set_flat(true)`
			// drops the raised-button background.
			device_picker->set_theme_type_variation("TopBarOptionButton");
			device_picker->set_flat(true);
			// Don't expand to the longest item's width -- the title bar is a
			// scarce horizontal resource and the popup carries the full text.
			device_picker->set_fit_to_longest_item(false);
			device_picker->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
			device_picker->set_accessibility_name(TTRC("Device Preview"));
			device_picker->connect(SceneStringName(item_selected),
					callable_mp(this, &DevicePreviewPlugin::_on_item_selected));

			add_control_to_container(CONTAINER_TOOLBAR, device_picker);
			_restore_persisted_selection();
			_build_menu();
			_update_picker_face();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_remove_preview();
			if (device_picker) {
				remove_control_from_container(CONTAINER_TOOLBAR, device_picker);
				device_picker->queue_free();
				device_picker = nullptr;
			}
		} break;
	}
}

void DevicePreviewPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_preview_resolution_landscape", "landscape"), &DevicePreviewPlugin::set_preview_resolution_landscape);
	ClassDB::bind_method(D_METHOD("is_preview_resolution_landscape"), &DevicePreviewPlugin::is_preview_resolution_landscape);
	ClassDB::bind_method(D_METHOD("get_preview_resolution"), &DevicePreviewPlugin::get_preview_resolution);
	ClassDB::bind_method(D_METHOD("get_active_feature_tags"), &DevicePreviewPlugin::get_active_feature_tags);

	// Emitted whenever the injected feature-tag set changes (a device is
	// picked, switched, or cleared). Other UIs (e.g. the Game workspace's
	// own resolution menu) can connect to refresh their "[mobile]/[pc]"
	// hint label in real time.
	ADD_SIGNAL(MethodInfo("active_feature_tags_changed"));
}

void DevicePreviewPlugin::set_preview_resolution_landscape(bool p_landscape) {
	if (preview_resolution_landscape == p_landscape) {
		return;
	}
	preview_resolution_landscape = p_landscape;
	if (current_profile.is_valid()) {
		_apply_preview(current_profile);
	}
	_update_picker_face();
}

Vector2i DevicePreviewPlugin::get_preview_resolution() const {
	if (current_profile.is_null() || !preview_active) {
		return Size2i();
	}
	return _apply_preview_orientation(current_profile->get_resolution());
}

DevicePreviewPlugin::DevicePreviewPlugin() {
	singleton = this;
}

DevicePreviewPlugin::~DevicePreviewPlugin() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

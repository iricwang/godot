/**************************************************************************/
/*  device_preview_plugin.h                                               */
/**************************************************************************/
#pragma once

#include "core/math/vector2i.h"
#include "core/variant/typed_array.h"
#include "device_profile.h"
#include "editor/plugins/editor_plugin.h"

class OptionButton;

class DevicePreviewPlugin : public EditorPlugin {
	GDCLASS(DevicePreviewPlugin, EditorPlugin);

	// Process-wide singleton so other editor subsystems (notably the
	// Run Instances dialog, which prepares GODOT_EDITOR_CUSTOM_FEATURES
	// right before each F5 child launches) can ask the active plugin for
	// its current feature-tag set without going through EditorPlugin lookup.
	static DevicePreviewPlugin *singleton;

	// "Free" lives at this metadata id; everything >= ITEM_DEVICE_BASE is a
	// real DeviceProfile, with the index into DeviceDatabase::get_presets()
	// being `id - ITEM_DEVICE_BASE`. We don't use OptionButton's own
	// "selected index" as the source of truth because the item ordering
	// changes when the database is rebuilt (e.g. presets are added).
	enum {
		ITEM_FREE = 0,
		ITEM_DEVICE_BASE = 1,
	};

	OptionButton *device_picker = nullptr;

	String current_device_name;
	Ref<DeviceProfile> current_profile;

	bool preview_active = false;

	// Orientation hint for the preview. When false (default) the device's
	// native resolution is used. When true the resolution is rotated 90
	// degrees (landscape). The flag is sticky across device switches — it
	// reflects the user's preferred orientation, not the device's.
	bool preview_resolution_landscape = false;

	// Snapshot of in-memory ProjectSettings (viewport size + stretch settings)
	// captured on first preview activation so we can restore them when the
	// user picks Free again. Only valid when _has_saved_state is true.
	// NOTE: The editor's main Window content scale is deliberately NOT
	// captured / mutated here -- see _apply_preview for why.
	int saved_viewport_width = 0;
	int saved_viewport_height = 0;
	String saved_stretch_mode;
	String saved_stretch_aspect;
	bool _has_saved_state = false;

	// Feature tags currently injected into ProjectSettings::custom_features
	// and intended to be forwarded to child processes via
	// GODOT_EDITOR_CUSTOM_FEATURES. Tracked separately so _clear_feature_tags
	// can undo exactly what was applied, even if the active profile changes
	// in between.
	PackedStringArray injected_feature_tags;

	void _build_menu();
	void _on_item_selected(int p_index);
	void _apply_preview(const Ref<DeviceProfile> &p_profile);
	void _remove_preview();
	void _update_picker_face();
	Vector2i _apply_preview_orientation(const Vector2i &p_size) const;

	void _apply_feature_tags(const PackedStringArray &p_tags);
	void _clear_feature_tags();

	void _restore_persisted_selection();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static DevicePreviewPlugin *get_singleton() { return singleton; }

	virtual String get_plugin_name() const override { return "DevicePreview"; }
	virtual bool has_main_screen() const override { return false; }

	// Set the orientation for the device preview. When true the device's
	// resolution is rotated 90 degrees (landscape); when false the native
	// resolution is used (portrait for a portrait device). Has no effect
	// when no device is selected.
	void set_preview_resolution_landscape(bool p_landscape);
	bool is_preview_resolution_landscape() const { return preview_resolution_landscape; }

	// Currently applied device resolution (already oriented). Returns
	// Size2i() when no device preview is active.
	Vector2i get_preview_resolution() const;

	// Tags that are currently injected (mobile / pc / ...). Empty when no
	// device is active, or when the active device's class is GENERIC and
	// it has no explicit feature_tags override.
	PackedStringArray get_active_feature_tags() const { return injected_feature_tags; }

	// Public injection points used by other editor UIs (notably the Game
	// workspace's preview_resolution_menu, which picks a device in a
	// separate menu and wants to drive the same branch). Unlike
	// `_apply_preview`, these do NOT touch the editor viewport, content
	// scale, or project settings -- they only mutate the feature-tag set
	// (and emit `active_feature_tags_changed`).
	void set_active_feature_tags(const PackedStringArray &p_tags) { _apply_feature_tags(p_tags); }
	void clear_active_feature_tags() { _clear_feature_tags(); }

	DevicePreviewPlugin();
	~DevicePreviewPlugin();
};

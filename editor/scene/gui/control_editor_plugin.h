/**************************************************************************/
/*  control_editor_plugin.h                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/margin_container.h"

class CheckBox;
class CheckButton;
class EditorSelection;
class EditorSpinSlider;
class GridContainer;
class Label;
class OptionButton;
class PanelContainer;
class PopupPanel;
class Separator;
class TextureRect;

// Inspector controls.
class ControlPositioningWarning : public MarginContainer {
	GDCLASS(ControlPositioningWarning, MarginContainer);

	Control *control_node = nullptr;

	PanelContainer *bg_panel = nullptr;
	GridContainer *grid = nullptr;
	TextureRect *title_icon = nullptr;
	TextureRect *hint_icon = nullptr;
	Label *title_label = nullptr;
	Label *hint_label = nullptr;
	Control *hint_filler_left = nullptr;
	Control *hint_filler_right = nullptr;

	void _update_warning();
	void _update_toggler();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

protected:
	void _notification(int p_notification);

public:
	void set_control(Control *p_node);

	ControlPositioningWarning();
};

class EditorPropertyAnchorsPreset : public EditorProperty {
	GDCLASS(EditorPropertyAnchorsPreset, EditorProperty);
	OptionButton *options = nullptr;

	void _option_selected(int p_which);

protected:
	virtual void _set_read_only(bool p_read_only) override;
	void _notification(int p_what);

public:
	void setup(const Vector<String> &p_options);
	virtual void update_property() override;
	EditorPropertyAnchorsPreset();
};

class EditorPropertySizeFlags : public EditorProperty {
	GDCLASS(EditorPropertySizeFlags, EditorProperty);

	enum FlagPreset {
		SIZE_FLAGS_PRESET_FILL,
		SIZE_FLAGS_PRESET_SHRINK_BEGIN,
		SIZE_FLAGS_PRESET_SHRINK_CENTER,
		SIZE_FLAGS_PRESET_SHRINK_END,
		SIZE_FLAGS_PRESET_CUSTOM,
	};

	OptionButton *flag_presets = nullptr;
	CheckBox *flag_expand = nullptr;
	VBoxContainer *flag_options = nullptr;
	Vector<CheckBox *> flag_checks;

	bool vertical = false;

	bool keep_selected_preset = false;

	void _preset_selected(int p_which);
	void _expand_toggled();
	void _flag_toggled();

protected:
	virtual void _set_read_only(bool p_read_only) override;

public:
	void setup(const Vector<String> &p_options, bool p_vertical);
	virtual void update_property() override;
	EditorPropertySizeFlags();
};

// ── Offset Transform visual inspector ─────────────────────────────────────────

// Small 72×72 interactive pivot/rotation diagram shown on the left side of the
// Offset Transform property panel.  Click a 3×3 zone to emit a pivot preset.
class ControlOffsetTransformDiagram : public Control {
	GDCLASS(ControlOffsetTransformDiagram, Control);

	Vector2 pivot_ratio = Vector2(0.5f, 0.5f);
	float rotation = 0.0f;
	bool visual_only = false;

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	virtual Size2 get_minimum_size() const override;
	void update_values(Vector2 p_pivot_ratio, float p_rotation, bool p_visual_only);

	ControlOffsetTransformDiagram();
};

// Single-property editor for the standard Control pivot_offset_ratio:
// reuses the 3×3 diagram for visual preset picking + two 0-1 spin sliders.
class EditorPropertyPivotOffsetRatio : public EditorProperty {
	GDCLASS(EditorPropertyPivotOffsetRatio, EditorProperty);

	ControlOffsetTransformDiagram *diagram = nullptr;
	EditorSpinSlider *spin_x = nullptr;
	EditorSpinSlider *spin_y = nullptr;
	bool updating = false;

	void _spin_changed(double p_val);
	void _on_pivot_preset(Vector2 p_pivot);

protected:
	virtual void _set_read_only(bool p_read_only) override;
	static void _bind_methods();

public:
	virtual void update_property() override;
	EditorPropertyPivotOffsetRatio();
};

// Unity RectTransform–style multi-property editor for all offset_transform_*
// sub-properties.  Rendered as a single full-width panel inside the inspector.
class ControlOffsetTransformEditor : public EditorProperty {
	GDCLASS(ControlOffsetTransformEditor, EditorProperty);

	ControlOffsetTransformDiagram *diagram = nullptr;

	EditorSpinSlider *spin_pos_x = nullptr;
	EditorSpinSlider *spin_pos_y = nullptr;
	EditorSpinSlider *spin_scale_x = nullptr;
	EditorSpinSlider *spin_scale_y = nullptr;
	EditorSpinSlider *spin_rotation = nullptr;
	EditorSpinSlider *spin_pivot_x = nullptr;
	EditorSpinSlider *spin_pivot_y = nullptr;
	CheckBox *cb_visual_only = nullptr;

	bool updating = false;

	void _spin_changed(double p_val, const StringName &p_prop);
	void _visual_only_toggled();
	void _on_pivot_preset(Vector2 p_pivot);

protected:
	virtual void _set_read_only(bool p_read_only) override;
	static void _bind_methods();

public:
	virtual void update_property() override;
	ControlOffsetTransformEditor();
};

class EditorInspectorPluginControl : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginControl, EditorInspectorPlugin);

	bool inside_control_category = false;
	bool ot_editor_added = false;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
	virtual void parse_category(Object *p_object, const String &p_category) override;
	virtual void parse_group(Object *p_object, const String &p_group) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

// Toolbar controls.
class ControlEditorPopupButton : public Button {
	GDCLASS(ControlEditorPopupButton, Button);

	Ref<Texture2D> arrow_icon;

	PopupPanel *popup_panel = nullptr;
	VBoxContainer *popup_vbox = nullptr;

	void _popup_visibility_changed(bool p_visible);

protected:
	void _notification(int p_what);

public:
	virtual Size2 get_minimum_size() const override;
	virtual void toggled(bool p_pressed) override;

	VBoxContainer *get_popup_hbox() const { return popup_vbox; }

	ControlEditorPopupButton();
};

class ControlEditorPresetPicker : public MarginContainer {
	GDCLASS(ControlEditorPresetPicker, MarginContainer);

	virtual void _preset_button_pressed(const int p_preset) {}

protected:
	static constexpr int grid_separation = 0;
	HashMap<int, Button *> preset_buttons;

	void _add_row_button(HBoxContainer *p_row, const int p_preset, const String &p_name);
	void _add_separator(BoxContainer *p_box, Separator *p_separator);
	void _update_preset_button_state(int p_preset);
};

class AnchorPresetPicker : public ControlEditorPresetPicker {
	GDCLASS(AnchorPresetPicker, ControlEditorPresetPicker);

	virtual void _preset_button_pressed(const int p_preset) override;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void set_selected_preset(int p_preset);

	AnchorPresetPicker();
};

class SizeFlagPresetPicker : public ControlEditorPresetPicker {
	GDCLASS(SizeFlagPresetPicker, ControlEditorPresetPicker);

	CheckButton *expand_button = nullptr;

	bool vertical = false;

	virtual void _preset_button_pressed(const int p_preset) override;
	void _expand_button_pressed();

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void set_allowed_flags(Vector<SizeFlags> &p_flags);
	void set_selected_preset(int p_preset);
	void set_expand_flag(bool p_expand);

	SizeFlagPresetPicker(bool p_vertical);
};

class ControlEditorToolbar : public HBoxContainer {
	GDCLASS(ControlEditorToolbar, HBoxContainer);

	EditorSelection *editor_selection = nullptr;

	ControlEditorPopupButton *anchors_button = nullptr;
	ControlEditorPopupButton *containers_button = nullptr;
	Button *anchor_mode_button = nullptr;

	AnchorPresetPicker *anchors_picker = nullptr;

	SizeFlagPresetPicker *container_h_picker = nullptr;
	SizeFlagPresetPicker *container_v_picker = nullptr;

	bool anchors_mode = false;

	void _anchors_preset_selected(int p_preset);
	void _anchors_to_current_ratio();
	void _anchor_mode_toggled(bool p_status);
	void _container_flags_selected(int p_flags, bool p_vertical);
	void _expand_flag_toggled(bool p_expand, bool p_vertical);
	void _update_anchor_selection_ui(bool p_pressed);
	void _update_container_sizing_selection_ui(bool p_pressed);

	Vector2 _position_to_anchor(const Control *p_control, Vector2 position);
	bool _is_node_locked(const Node *p_node);
	List<Control *> _get_edited_controls();
	void _selection_changed();

protected:
	void _notification(int p_notification);

	static ControlEditorToolbar *singleton;

public:
	bool is_anchors_mode_enabled() { return anchors_mode; }

	static ControlEditorToolbar *get_singleton() { return singleton; }

	ControlEditorToolbar();
};

class ControlOffsetTransformPreview : public Control {
	GDCLASS(ControlOffsetTransformPreview, Control);

	EditorPlugin *plugin = nullptr;
	Control *selected_control = nullptr;

	friend class ControlEditorPlugin;

public:
	void edit(Control *p_control);

	void forward_canvas_draw_over_viewport(Control *p_overlay) const;

	ControlOffsetTransformPreview(EditorPlugin *p_plugin);
};

// Editor plugin.
class ControlEditorPlugin : public EditorPlugin {
	GDCLASS(ControlEditorPlugin, EditorPlugin);

	ControlEditorToolbar *toolbar = nullptr;
	ControlOffsetTransformPreview *offset_transform_preview = nullptr;

public:
	virtual String get_plugin_name() const override { return "Control"; }

	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;

	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override;

	ControlEditorPlugin();
};

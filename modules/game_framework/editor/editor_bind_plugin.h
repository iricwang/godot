/**************************************************************************/
/*  editor_bind_plugin.h                                                  */
/**************************************************************************/
#pragma once

#ifdef TOOLS_ENABLED

#include "editor/inspector/editor_inspector.h"

class Button;
class MenuButton;
class OptionButton;
class SceneTreeDialog;

// Shows the MVVM binding editor for a @BindProperty variable.
// Layout: [VM property label] [target NodePath picker ▼] [target property ▼] [mode toggle]
class EditorPropertyBindTarget : public EditorProperty {
	GDCLASS(EditorPropertyBindTarget, EditorProperty);

	Button *assign = nullptr;
	MenuButton *menu = nullptr;
	OptionButton *mode_selector = nullptr;

	SceneTreeDialog *scene_tree = nullptr;
	bool dropping = false;
	bool editing_node = true;

	void _node_selected(const NodePath &p_path, bool p_absolute = true);
	void _node_assign();
	void _assign_draw();
	void _update_menu();
	void _menu_option(int p_idx);
	void _update_target_properties();
	void _mode_changed(int p_mode);

	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	bool is_drop_valid(const Dictionary &p_drag_data) const;

	String get_target_node_path() const;
	String get_target_property_name() const;
	int get_bind_mode() const;

protected:
	virtual void _set_read_only(bool p_read_only) override;
	void _notification(int p_what);

public:
	virtual void update_property() override;
	EditorPropertyBindTarget();
};

// Inspector plugin that recognizes ViewModel objects and adds binding editors
// for @BindProperty / @BindSignal annotations.
class EditorInspectorPluginBind : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginBind, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
	virtual void parse_end(Object *p_object) override;
};

#endif // TOOLS_ENABLED

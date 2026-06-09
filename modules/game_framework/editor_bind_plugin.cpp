/**************************************************************************/
/*  editor_bind_plugin.cpp                                                */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "editor_bind_plugin.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/scene/scene_tree_editor.h"
#include "scene/gui/button.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/texture.h"

// ---- EditorPropertyBindTarget ----

void EditorPropertyBindTarget::_node_selected(const NodePath &p_path, bool p_absolute) {
	ERR_FAIL_COND(!get_tree());
	Node *edited_scene_root = get_tree()->get_edited_scene_root();
	ERR_FAIL_NULL(edited_scene_root);

	NodePath path;
	if (p_absolute && edited_scene_root) {
		path = edited_scene_root->get_path_to(get_node(p_path));
	} else {
		path = p_path;
	}

	// Store binding config as a Dictionary: { target_node: NodePath, target_prop: String, mode: int }
	Dictionary config;
	config["target_node"] = path;
	config["target_prop"] = get_target_property_name();
	config["mode"] = get_bind_mode();

	emit_changed(get_edited_property(), config);
	update_property();
}

void EditorPropertyBindTarget::_node_assign() {
	if (!scene_tree) {
		scene_tree = memnew(SceneTreeDialog);
		scene_tree->get_scene_tree()->set_show_enabled_subscene(true);
		add_child(scene_tree);
		scene_tree->connect("selected", callable_mp(this, &EditorPropertyBindTarget::_node_selected).bind(true));
	}
	scene_tree->popup_scenetree_dialog();
}

void EditorPropertyBindTarget::_assign_draw() {
	if (dropping) {
		Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		assign->draw_rect(Rect2(Point2(), assign->get_size()), color, false);
	}
}

void EditorPropertyBindTarget::_update_menu() {
	String node_path = get_target_node_path();

	menu->get_popup()->set_item_disabled(0, node_path.is_empty());   // Clear
	menu->get_popup()->set_item_disabled(1, node_path.is_empty());   // Select
}

void EditorPropertyBindTarget::_menu_option(int p_idx) {
	switch (p_idx) {
		case 0: { // Clear
			emit_changed(get_edited_property(), Dictionary());
		} break;
		case 1: { // Select Node
			_node_assign();
		} break;
	}
	update_property();
}

void EditorPropertyBindTarget::_update_target_properties() {
	// Scan the target node for settable properties and update mode_selector dropdown.
	// This is a placeholder — in production, we'd list the node's properties.
	// For now, the user types property name in the editor or it's set from annotation.
}

void EditorPropertyBindTarget::_mode_changed(int p_mode) {
	Variant val = get_edited_property_value();
	Dictionary config;
	if (val.get_type() == Variant::DICTIONARY) {
		config = val;
	}
	config["mode"] = p_mode;
	emit_changed(get_edited_property(), config);
}

bool EditorPropertyBindTarget::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	return !is_read_only() && is_drop_valid(p_data);
}

void EditorPropertyBindTarget::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	ERR_FAIL_COND(!is_drop_valid(p_data));
	Dictionary data_dict = p_data;
	Array nodes = data_dict["nodes"];

	Node *edited_scene_root = get_tree()->get_edited_scene_root();
	ERR_FAIL_NULL(edited_scene_root);

	Node *node = edited_scene_root->get_node(nodes[0]);
	if (node) {
		_node_selected(node->get_path());
	}
}

bool EditorPropertyBindTarget::is_drop_valid(const Dictionary &p_drag_data) const {
	if (!p_drag_data.has("type") || p_drag_data["type"] != "nodes") {
		return false;
	}
	Array nodes = p_drag_data["nodes"];
	if (nodes.size() != 1) {
		return false;
	}

	Object *data_root = p_drag_data.get("scene_root", (Object *)nullptr);
	if (data_root && get_tree()->get_edited_scene_root() != data_root) {
		return false;
	}

	return true;
}

String EditorPropertyBindTarget::get_target_node_path() const {
	Variant val = get_edited_property_value();
	if (val.get_type() != Variant::DICTIONARY) {
		return "";
	}
	Dictionary config = val;
	return config.get("target_node", "");
}

String EditorPropertyBindTarget::get_target_property_name() const {
	Variant val = get_edited_property_value();
	if (val.get_type() != Variant::DICTIONARY) {
		return "";
	}
	Dictionary config = val;
	return config.get("target_prop", "");
}

int EditorPropertyBindTarget::get_bind_mode() const {
	Variant val = get_edited_property_value();
	if (val.get_type() != Variant::DICTIONARY) {
		return 0;
	}
	Dictionary config = val;
	return config.get("mode", 0);
}

void EditorPropertyBindTarget::_set_read_only(bool p_read_only) {
	if (assign) {
		assign->set_disabled(p_read_only);
	}
	if (menu) {
		menu->set_disabled(p_read_only);
	}
	if (mode_selector) {
		mode_selector->set_disabled(p_read_only);
	}
}

void EditorPropertyBindTarget::update_property() {
	Dictionary config;
	Variant val = get_edited_property_value();
	if (val.get_type() == Variant::DICTIONARY) {
		config = val;
	}

	NodePath target_path = config.get("target_node", NodePath());
	String target_prop = config.get("target_prop", "");
	int mode = config.get("mode", 0);

	String default_label = vformat("[%s] %s", get_edited_property(), target_prop.is_empty() ? "(No target binding)" : "");

	if (target_path.is_empty()) {
		assign->set_button_icon(Ref<Texture2D>());
		assign->set_text(vformat(TTR("Assign...") + "  [%s]", String(target_prop)));
		assign->set_flat(false);
	} else {
		assign->set_flat(true);
		Node *edited_scene_root = get_tree()->get_edited_scene_root();
		if (edited_scene_root && edited_scene_root->has_node(target_path)) {
			Node *target_node = edited_scene_root->get_node(target_path);
			String new_text = vformat("%s.%s", target_node->get_name(), target_prop);
			assign->set_text(new_text);
			assign->set_button_icon(EditorNode::get_singleton()->get_object_icon(target_node));
		} else {
			assign->set_button_icon(Ref<Texture2D>());
			assign->set_text(String(target_path) + "." + target_prop);
		}
	}

	assign->set_tooltip_text(String(target_path) + "." + target_prop);
	if (mode_selector) {
		mode_selector->select(mode);
	}
}

void EditorPropertyBindTarget::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			if (menu) {
				menu->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			}
		} break;
		case NOTIFICATION_DRAG_BEGIN: {
			if (!is_read_only() && is_drop_valid(get_viewport()->gui_get_drag_data())) {
				dropping = true;
				assign->queue_redraw();
			}
		} break;
		case NOTIFICATION_DRAG_END: {
			dropping = false;
			assign->queue_redraw();
		} break;
	}
}

EditorPropertyBindTarget::EditorPropertyBindTarget() {
	// Layout: HBox with [property label (from EditorProperty)] [assign button] [menu button] [mode selector]
	HBoxContainer *hbc = memnew(HBoxContainer);
	hbc->set_h_size_flags(SIZE_EXPAND_FILL);

	assign = memnew(Button);
	assign->set_h_size_flags(SIZE_EXPAND_FILL);
	assign->set_clip_text(true);
	assign->set_text(TTR("Assign..."));
	assign->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyBindTarget::_node_assign));
	assign->connect(SceneStringName(draw), callable_mp(this, &EditorPropertyBindTarget::_assign_draw));
	assign->set_drag_forwarding(Callable(), callable_mp(this, &EditorPropertyBindTarget::can_drop_data_fw), callable_mp(this, &EditorPropertyBindTarget::drop_data_fw));
	hbc->add_child(assign);

	mode_selector = memnew(OptionButton);
	mode_selector->add_item(TTR("→")); // one-way: VM → View
	mode_selector->add_item(TTR("↔")); // two-way
	mode_selector->set_tooltip_text("Binding mode:");
	mode_selector->get_popup()->set_item_tooltip(0, TTR("One-way: ViewModel → target property"));
	mode_selector->get_popup()->set_item_tooltip(1, TTR("Two-way: bidirectional binding"));
	mode_selector->connect("item_selected", callable_mp(this, &EditorPropertyBindTarget::_mode_changed));
	hbc->add_child(mode_selector);

	menu = memnew(MenuButton);
	menu->set_flat(true);
	menu->set_tooltip_text(TTR("Binding options"));
	menu->get_popup()->add_item(TTR("Clear Binding"), 0);
	menu->get_popup()->add_item(TTR("Select Node..."), 1);
	menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyBindTarget::_menu_option));
	hbc->add_child(menu);

	add_child(hbc);
}

// ---- EditorInspectorPluginBind ----

bool EditorInspectorPluginBind::can_handle(Object *p_object) {
	// Only activate for ViewModel-derived objects.
	return p_object && p_object->is_class("ViewModel");
}

void EditorInspectorPluginBind::parse_begin(Object *p_object) {
	// Called before parsing properties. We can add a section header here if desired.
}

bool EditorInspectorPluginBind::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	switch (p_hint) {
		case PROPERTY_HINT_BIND_PROPERTY: {
			EditorPropertyBindTarget *editor = memnew(EditorPropertyBindTarget);
			add_property_editor(p_path, editor);
			return true;
		}
		case PROPERTY_HINT_BIND_SIGNAL: {
			// For signal bindings, we create a simple NodePath editor that stores the binding config.
			// Future enhancement: show a dedicated signal binding editor.
			return false; // Let default editor handle it for now.
		}
		default:
			return false; // Not our property, let other plugins handle.
	}
}

void EditorInspectorPluginBind::parse_end(Object *p_object) {
	// Called after all properties parsed.
}

#endif // TOOLS_ENABLED

/**************************************************************************/
/*  node_palette_dock.cpp                                                 */
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

#include "node_palette_dock.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_feature_profile.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"

NodePaletteDock *NodePaletteDock::singleton = nullptr;

void NodePaletteDock::_fill_type_list() {
	type_list.clear();
	custom_type_parents.clear();
	custom_type_indices.clear();

	LocalVector<StringName> complete_type_list;
	ClassDB::get_class_list(complete_type_list);
	ScriptServer::get_global_class_list(complete_type_list);

	EditorData &ed = EditorNode::get_editor_data();

	for (const StringName &type : complete_type_list) {
		if (!_should_hide_type(type)) {
			type_list.push_back(type);

			if (!ed.get_custom_types().has(type)) {
				continue;
			}

			const Vector<EditorData::CustomType> &ct = ed.get_custom_types()[type];
			for (int i = 0; i < ct.size(); i++) {
				custom_type_parents[ct[i].name] = type;
				custom_type_indices[ct[i].name] = i;
				type_list.push_back(ct[i].name);
			}
		}
	}

	type_list.sort_custom<NaturalNoCaseComparator>();
}

bool NodePaletteDock::_should_hide_type(const StringName &p_type) const {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_valid() && profile->is_class_disabled(p_type)) {
		return true;
	}

	if (p_type.operator String().begins_with("Editor")) {
		return true;
	}

	if (ClassDB::class_exists(p_type)) {
		if (!ClassDB::can_instantiate(p_type) || ClassDB::is_virtual(p_type)) {
			return true;
		}

		if (!ClassDB::is_parent_class(p_type, "Node")) {
			return true;
		}

		if (!ClassDB::is_class_exposed(p_type)) {
			return true;
		}
	} else {
		if (!ScriptServer::is_global_class(p_type)) {
			return true;
		}
		if (!EditorNode::get_editor_data().script_class_is_parent(p_type, "Node")) {
			return true;
		}

		StringName native_type = ScriptServer::get_global_class_native_base(p_type);
		if (ClassDB::class_exists(native_type)) {
			if (!ClassDB::can_instantiate(native_type)) {
				return true;
			}
		}

		String script_path = ScriptServer::get_global_class_path(p_type);
		if (script_path.begins_with("res://addons/")) {
			int i = script_path.find_char('/', 13);
			while (i > -1) {
				const String plugin_path = script_path.substr(0, i).path_join("plugin.cfg");
				if (FileAccess::exists(plugin_path)) {
					return !EditorNode::get_singleton()->is_addon_plugin_enabled(plugin_path);
				}
				i = script_path.find_char('/', i + 1);
			}
		}

		String path = ScriptServer::get_global_class_path(p_type);
		Ref<Script> scr = ResourceLoader::load(path, "Script");
		if (scr.is_null() || scr->is_abstract()) {
			return true;
		}
	}

	return false;
}

bool NodePaletteDock::_passes_filter(const StringName &p_type) const {
	switch (current_filter) {
		case FILTER_ALL:
			return true;
		case FILTER_CONTROL:
			return ClassDB::is_parent_class(p_type, "Control") ||
					EditorNode::get_editor_data().script_class_is_parent(p_type, "Control");
		case FILTER_2D:
			return ClassDB::is_parent_class(p_type, "Node2D") ||
					EditorNode::get_editor_data().script_class_is_parent(p_type, "Node2D");
		case FILTER_3D:
			return ClassDB::is_parent_class(p_type, "Node3D") ||
					EditorNode::get_editor_data().script_class_is_parent(p_type, "Node3D");
		case FILTER_OTHER:
			bool is_control = ClassDB::is_parent_class(p_type, "Control") ||
							EditorNode::get_editor_data().script_class_is_parent(p_type, "Control");
			bool is_2d = ClassDB::is_parent_class(p_type, "Node2D") ||
						 EditorNode::get_editor_data().script_class_is_parent(p_type, "Node2D");
			bool is_3d = ClassDB::is_parent_class(p_type, "Node3D") ||
						 EditorNode::get_editor_data().script_class_is_parent(p_type, "Node3D");
			return !is_control && !is_2d && !is_3d;
	}
	return true;
}

void NodePaletteDock::_add_type(const StringName &p_type) {
	if (tree_items.has(p_type)) {
		return;
	}

	StringName inherits;
	bool is_custom_type = false;

	if (ClassDB::class_exists(p_type)) {
		inherits = ClassDB::get_parent_class(p_type);
	} else if (ScriptServer::is_global_class(p_type)) {
		inherits = ScriptServer::get_global_class_base(p_type);
	} else {
		inherits = custom_type_parents[p_type];
		is_custom_type = true;
	}

	if (inherits == StringName()) {
		return;
	}

	_add_type(inherits);

	TreeItem *parent_item = tree_items[inherits];
	TreeItem *item = tree->create_item(parent_item);
	tree_items[p_type] = item;

	item->set_text(0, p_type);
	item->set_icon(0, EditorNode::get_singleton()->get_class_icon(p_type));
	item->set_metadata(0, String(p_type));

	if (is_custom_type) {
		Ref<Texture2D> icon = EditorNode::get_editor_data().get_custom_types()[custom_type_parents[p_type]][custom_type_indices[p_type]].icon;
		if (icon.is_valid()) {
			item->set_icon(0, icon);
		}
	}
}

void NodePaletteDock::_update_tree() {
	tree->clear();
	tree_items.clear();

	TreeItem *root = tree->create_item();
	root->set_text(0, "Node");
	root->set_icon(0, tree->get_editor_theme_icon(SNAME("Node")));
	root->set_metadata(0, String("Node"));
	tree_items["Node"] = root;

	const String search_text = search_box->get_text().to_lower();

	for (const String &type : type_list) {
		if (!_passes_filter(type)) {
			continue;
		}

		if (!search_text.is_empty()) {
			if (!type.to_lower().contains(search_text)) {
				continue;
			}
		}

		_add_type(type);
	}

	// Collapse all but the root.
	TreeItem *child = root->get_first_child();
	while (child) {
		child->set_collapsed(true);
		child = child->get_next();
	}
}

void NodePaletteDock::_filter_all_toggled() {
	current_filter = FILTER_ALL;
	filter_all->set_pressed(true);
	filter_control->set_pressed(false);
	filter_2d->set_pressed(false);
	filter_3d->set_pressed(false);
	_update_tree();
}

void NodePaletteDock::_filter_control_toggled() {
	current_filter = FILTER_CONTROL;
	filter_all->set_pressed(false);
	filter_control->set_pressed(true);
	filter_2d->set_pressed(false);
	filter_3d->set_pressed(false);
	_update_tree();
}

void NodePaletteDock::_filter_2d_toggled() {
	current_filter = FILTER_2D;
	filter_all->set_pressed(false);
	filter_control->set_pressed(false);
	filter_2d->set_pressed(true);
	filter_3d->set_pressed(false);
	_update_tree();
}

void NodePaletteDock::_filter_3d_toggled() {
	current_filter = FILTER_3D;
	filter_all->set_pressed(false);
	filter_control->set_pressed(false);
	filter_2d->set_pressed(false);
	filter_3d->set_pressed(true);
	_update_tree();
}

void NodePaletteDock::_search_changed(const String &p_text) {
	_update_tree();
}

void NodePaletteDock::_item_activated() {
	TreeItem *selected = tree->get_selected();
	if (!selected) {
		return;
	}

	String class_name = selected->get_metadata(0);
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (edited_scene) {
		SceneTreeDock::get_singleton()->add_node_by_class(class_name, edited_scene);
	} else {
		SceneTreeDock::get_singleton()->add_node_by_class(class_name, nullptr);
	}
}

Variant NodePaletteDock::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	TreeItem *selected = tree->get_selected();
	if (!selected) {
		return Variant();
	}

	String class_name = selected->get_metadata(0);

	Dictionary drag_data;
	drag_data["type"] = "node_class";
	drag_data["class_name"] = class_name;

	Label *preview = memnew(Label);
	preview->set_text(class_name);
	preview->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	set_drag_preview(preview);

	return drag_data;
}

bool NodePaletteDock::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	return false; // We don't accept drops.
}

void NodePaletteDock::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	// No-op.
}

void NodePaletteDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_fill_type_list();
			_update_tree();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			filter_all->set_button_icon(get_editor_theme_icon(SNAME("Node")));
			filter_control->set_button_icon(get_editor_theme_icon(SNAME("Control")));
			filter_2d->set_button_icon(get_editor_theme_icon(SNAME("Node2D")));
			filter_3d->set_button_icon(get_editor_theme_icon(SNAME("Node3D")));
			_update_tree();
		} break;
	}
}

void NodePaletteDock::_bind_methods() {
}

NodePaletteDock::NodePaletteDock() {
	singleton = this;

	set_title(TTR("Node Palette"));
	set_icon_name(SNAME("Node"));
	set_default_slot(DOCK_SLOT_RIGHT_UL);

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(vbc);

	HBoxContainer *filter_hb = memnew(HBoxContainer);
	vbc->add_child(filter_hb);

	filter_all = memnew(Button);
	filter_all->set_tooltip_text(TTR("All Nodes"));
	filter_all->set_toggle_mode(true);
	filter_all->set_pressed(true);
	filter_all->set_flat(true);
	filter_all->set_focus_mode(FOCUS_NONE);
	filter_all->connect(SceneStringName(pressed), callable_mp(this, &NodePaletteDock::_filter_all_toggled));
	filter_hb->add_child(filter_all);

	filter_control = memnew(Button);
	filter_control->set_tooltip_text(TTR("Control Nodes"));
	filter_control->set_toggle_mode(true);
	filter_control->set_flat(true);
	filter_control->set_focus_mode(FOCUS_NONE);
	filter_control->connect(SceneStringName(pressed), callable_mp(this, &NodePaletteDock::_filter_control_toggled));
	filter_hb->add_child(filter_control);

	filter_2d = memnew(Button);
	filter_2d->set_tooltip_text(TTR("Node2D Nodes"));
	filter_2d->set_toggle_mode(true);
	filter_2d->set_flat(true);
	filter_2d->set_focus_mode(FOCUS_NONE);
	filter_2d->connect(SceneStringName(pressed), callable_mp(this, &NodePaletteDock::_filter_2d_toggled));
	filter_hb->add_child(filter_2d);

	filter_3d = memnew(Button);
	filter_3d->set_tooltip_text(TTR("Node3D Nodes"));
	filter_3d->set_toggle_mode(true);
	filter_3d->set_flat(true);
	filter_3d->set_focus_mode(FOCUS_NONE);
	filter_3d->connect(SceneStringName(pressed), callable_mp(this, &NodePaletteDock::_filter_3d_toggled));
	filter_hb->add_child(filter_3d);

	search_box = memnew(LineEdit);
	search_box->set_placeholder(TTR("Filter Nodes"));
	search_box->set_clear_button_enabled(true);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &NodePaletteDock::_search_changed));
	vbc->add_child(search_box);

	tree = memnew(Tree);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_hide_root(false);
	tree->set_select_mode(Tree::SELECT_SINGLE);
	tree->connect("item_activated", callable_mp(this, &NodePaletteDock::_item_activated));
	vbc->add_child(tree);

	SET_DRAG_FORWARDING_GCD(tree, NodePaletteDock);
}

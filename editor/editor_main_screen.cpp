/**************************************************************************/
/*  editor_main_screen.cpp                                                */
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

#include "editor_main_screen.h"

#include "core/io/config_file.h"
#include "core/object/callable_mp.h"
#include "editor/docks/dock_tab_container.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/docks/main_screen_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"

void EditorMainScreen::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			set_accessibility_region(true);
			if (EDITOR_3D < buttons.size() && buttons[EDITOR_3D]->is_visible()) {
				// If the 3D editor is enabled, use this as the default.
				select(EDITOR_3D);
				return;
			}

			// Switch to the first main screen plugin that is enabled. Usually this is
			// 2D, but may be subsequent ones if 2D is disabled in the feature profile.
			for (int i = 0; i < buttons.size(); i++) {
				Button *editor_button = buttons[i];
				if (editor_button->is_visible()) {
					select(i);
					return;
				}
			}

			select(-1);
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			for (int i = 0; i < buttons.size(); i++) {
				Button *tb = buttons[i];
				EditorPlugin *p_editor = editor_table[i];
				Ref<Texture2D> icon = p_editor->get_plugin_icon();

				if (icon.is_valid()) {
					tb->set_button_icon(icon);
				} else if (has_theme_icon(p_editor->get_plugin_name(), EditorStringName(EditorIcons))) {
					tb->set_button_icon(get_theme_icon(p_editor->get_plugin_name(), EditorStringName(EditorIcons)));
				}
			}
		} break;
	}
}

void EditorMainScreen::set_button_container(HBoxContainer *p_button_hb) {
	button_hb = p_button_hb;
}

void EditorMainScreen::save_layout_to_config(Ref<ConfigFile> p_config_file, const String &p_section) const {
	int selected_main_editor_idx = get_selected_index();
	if (selected_main_editor_idx != -1) {
		p_config_file->set_value(p_section, "selected_main_editor_idx", selected_main_editor_idx);
	} else {
		p_config_file->set_value(p_section, "selected_main_editor_idx", Variant());
	}
}

void EditorMainScreen::load_layout_from_config(Ref<ConfigFile> p_config_file, const String &p_section) {
	int selected_main_editor_idx = p_config_file->get_value(p_section, "selected_main_editor_idx", -1);
	if (selected_main_editor_idx >= 0 && selected_main_editor_idx < buttons.size()) {
		callable_mp(this, &EditorMainScreen::select).call_deferred(selected_main_editor_idx);
	}
}

void EditorMainScreen::set_button_enabled(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, buttons.size());
	buttons[p_index]->set_visible(p_enabled);
	if (p_index < dock_table.size()) {
		EditorDockManager::get_singleton()->set_dock_enabled(dock_table[p_index], p_enabled);
	}
	if (!p_enabled && buttons[p_index]->is_pressed()) {
		for (int i = 0; i < buttons.size(); i++) {
			if (i != p_index && buttons[i]->is_visible()) {
				select(i);
				return;
			}
		}
	}
}

bool EditorMainScreen::is_button_enabled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, buttons.size(), false);
	return buttons[p_index]->is_visible();
}

int EditorMainScreen::_get_current_main_editor() const {
	for (int i = 0; i < editor_table.size(); i++) {
		if (editor_table[i] == selected_plugin) {
			return i;
		}
	}

	return 0;
}

void EditorMainScreen::select_next() {
	int editor = _get_current_main_editor();

	do {
		if (editor == editor_table.size() - 1) {
			editor = 0;
		} else {
			editor++;
		}
	} while (!buttons[editor]->is_visible());

	select(editor);
}

void EditorMainScreen::select_prev() {
	int editor = _get_current_main_editor();

	do {
		if (editor == 0) {
			editor = editor_table.size() - 1;
		} else {
			editor--;
		}
	} while (!buttons[editor]->is_visible());

	select(editor);
}

void EditorMainScreen::select_by_name(const String &p_name) {
	ERR_FAIL_COND(p_name.is_empty());

	for (int i = 0; i < buttons.size(); i++) {
		if (buttons[i]->get_text() == p_name) {
			select(i);
			return;
		}
	}

	ERR_FAIL_MSG("The editor name '" + p_name + "' was not found.");
}

void EditorMainScreen::_set_selected_plugin(EditorPlugin *p_plugin) {
	ERR_FAIL_NULL(p_plugin);

	bool selection_changed = selected_plugin != p_plugin;
	selected_plugin = p_plugin;
	selected_plugin->make_visible(true);
	selected_plugin->selected_notify();
	set_accessibility_name(selected_plugin->get_plugin_name());

	for (int i = 0; i < buttons.size(); i++) {
		buttons[i]->set_pressed_no_signal(editor_table[i] == selected_plugin);
	}

	if (selection_changed) {
		EditorData &editor_data = EditorNode::get_editor_data();
		int plugin_count = editor_data.get_editor_plugin_count();
		for (int i = 0; i < plugin_count; i++) {
			editor_data.get_editor_plugin(i)->notify_main_screen_changed(selected_plugin->get_plugin_name());
		}

		EditorNode::get_singleton()->update_distraction_free_mode();
	}
}

void EditorMainScreen::_dock_visibility_changed(MainScreenDock *p_dock) {
	ERR_FAIL_NULL(p_dock);
	EditorPlugin *plugin = p_dock->get_plugin();
	ERR_FAIL_NULL(plugin);

	if (!p_dock->is_visible_in_tree()) {
		plugin->make_visible(false);
		if (selected_plugin == plugin) {
			selected_plugin = nullptr;
		}
		return;
	}

	_set_selected_plugin(plugin);
}

void EditorMainScreen::select(int p_index) {
	if (EditorNode::get_singleton()->is_changing_scene()) {
		return;
	}

	ERR_FAIL_INDEX(p_index, editor_table.size());

	if (!buttons[p_index]->is_visible()) { // Button hidden, no editor.
		return;
	}

	EditorPlugin *new_editor = editor_table[p_index];
	ERR_FAIL_NULL(new_editor);
	MainScreenDock *dock = dock_table[p_index];
	ERR_FAIL_NULL(dock);

	EditorDockManager::get_singleton()->focus_dock(dock);
	_set_selected_plugin(new_editor);
}

int EditorMainScreen::get_selected_index() const {
	for (int i = 0; i < editor_table.size(); i++) {
		if (selected_plugin == editor_table[i]) {
			return i;
		}
	}
	return -1;
}

int EditorMainScreen::get_plugin_index(EditorPlugin *p_editor) const {
	int screen = -1;
	for (int i = 0; i < editor_table.size(); i++) {
		if (p_editor == editor_table[i]) {
			screen = i;
			break;
		}
	}
	return screen;
}

EditorPlugin *EditorMainScreen::get_selected_plugin() const {
	return selected_plugin;
}

EditorPlugin *EditorMainScreen::get_plugin_by_name(const String &p_plugin_name) const {
	ERR_FAIL_COND_V(!main_editor_plugins.has(p_plugin_name), nullptr);
	return main_editor_plugins[p_plugin_name];
}

bool EditorMainScreen::can_auto_switch_screens() const {
	if (selected_plugin == nullptr) {
		return true;
	}
	// Only allow auto-switching if the selected button is to the left of the Script button.
	for (int i = 0; i < button_hb->get_child_count(); i++) {
		Button *button = Object::cast_to<Button>(button_hb->get_child(i));
		if (button->get_text() == "Script") {
			// Selected button is at or after the Script button.
			return false;
		}
		if (button->get_text() == selected_plugin->get_plugin_name()) {
			// Selected button is before the Script button.
			return true;
		}
	}
	return false;
}

VBoxContainer *EditorMainScreen::get_control() const {
	return main_screen_vbox;
}

Control *EditorMainScreen::get_visible_workspace_control() const {
	if (selected_plugin) {
		int selected_index = get_selected_index();
		if (selected_index >= 0 && selected_index < dock_table.size()) {
			MainScreenDock *dock = dock_table[selected_index];
			if (dock && dock->is_visible_in_tree()) {
				return dock;
			}
			Control *plugin_root = dock ? dock->get_plugin_root() : nullptr;
			if (plugin_root && plugin_root->is_visible_in_tree()) {
				return plugin_root;
			}
		}
	}
	for (MainScreenDock *dock : dock_table) {
		if (dock && dock->is_visible_in_tree()) {
			return dock;
		}
	}
	return main_screen_vbox;
}

void EditorMainScreen::add_main_plugin(EditorPlugin *p_editor) {
	Button *tb = memnew(Button);
	tb->set_toggle_mode(true);
	tb->set_theme_type_variation("MainScreenButton");
	tb->set_name(p_editor->get_plugin_name());
	tb->set_text(p_editor->get_plugin_name());

	Ref<Shortcut> shortcut = EditorSettings::get_singleton()->get_shortcut("editor/editor_" + p_editor->get_plugin_name().to_lower());
	if (shortcut.is_valid()) {
		tb->set_shortcut(shortcut);
	}

	Ref<Texture2D> icon = p_editor->get_plugin_icon();
	if (icon.is_null() && has_theme_icon(p_editor->get_plugin_name(), EditorStringName(EditorIcons))) {
		icon = get_editor_theme_icon(p_editor->get_plugin_name());
	}
	if (icon.is_valid()) {
		tb->set_button_icon(icon);
		// Make sure the control is updated if the icon is reimported.
		icon->connect_changed(callable_mp((Control *)tb, &Control::update_minimum_size));
	}

	const int plugin_index = buttons.size();
	tb->connect(SceneStringName(pressed), callable_mp(this, &EditorMainScreen::select).bind(plugin_index));

	Control *plugin_root = nullptr;
	if (main_screen_vbox->get_child_count() > 0) {
		plugin_root = Object::cast_to<Control>(main_screen_vbox->get_child(main_screen_vbox->get_child_count() - 1));
	}
	ERR_FAIL_NULL(plugin_root);

	MainScreenDock *dock = memnew(MainScreenDock);
	dock->set_name(p_editor->get_plugin_name() + "WorkspaceDock");
	dock->set_title(p_editor->get_plugin_name());
	dock->set_layout_key("workspace_" + p_editor->get_plugin_name().to_lower());
	dock->set_dock_shortcut(shortcut);
	if (icon.is_valid()) {
		dock->set_dock_icon(icon);
	}
	dock->bind_plugin(p_editor, plugin_root);
	dock->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorMainScreen::_dock_visibility_changed).bind(dock));

	buttons.push_back(tb);
	button_hb->add_child(tb);
	editor_table.push_back(p_editor);
	dock_table.push_back(dock);
	main_editor_plugins.insert(p_editor->get_plugin_name(), p_editor);
	EditorDockManager::get_singleton()->add_dock(dock);
}

void EditorMainScreen::remove_main_plugin(EditorPlugin *p_editor) {
	int remove_index = get_plugin_index(p_editor);
	ERR_FAIL_COND(remove_index == -1);

	if (buttons[remove_index]->is_pressed() && editor_table.size() > 1) {
		select(remove_index == EDITOR_SCRIPT ? EDITOR_2D : EDITOR_SCRIPT);
	}

	MainScreenDock *dock = dock_table[remove_index];
	if (dock) {
		dock->disconnect(SceneStringName(visibility_changed), callable_mp(this, &EditorMainScreen::_dock_visibility_changed).bind(dock));
		Control *plugin_root = dock->get_plugin_root();
		if (plugin_root && plugin_root->get_parent() == dock) {
			dock->remove_child(plugin_root);
			main_screen_vbox->add_child(plugin_root);
			plugin_root->hide();
		}
		EditorDockManager::get_singleton()->remove_dock(dock);
		memdelete(dock);
	}

	memdelete(buttons[remove_index]);
	buttons.remove_at(remove_index);
	editor_table.remove_at(remove_index);
	dock_table.remove_at(remove_index);
	main_editor_plugins.erase(p_editor->get_plugin_name());

	for (int i = remove_index; i < buttons.size(); i++) {
		buttons[i]->disconnect(SceneStringName(pressed), callable_mp(this, &EditorMainScreen::select));
		buttons[i]->connect(SceneStringName(pressed), callable_mp(this, &EditorMainScreen::select).bind(i));
	}

	if (selected_plugin == p_editor) {
		selected_plugin = nullptr;
	}
}

EditorMainScreen::EditorMainScreen() {
	main_screen_vbox = memnew(VBoxContainer);
	main_screen_vbox->set_name("MainScreen");
	main_screen_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_screen_vbox->add_theme_constant_override("separation", 0);
	add_child(main_screen_vbox);
}

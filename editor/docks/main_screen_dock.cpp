/**************************************************************************/
/*  main_screen_dock.cpp                                                  */
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

#include "main_screen_dock.h"

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/control.h"

void MainScreenDock::_bind_methods() {
}

void MainScreenDock::bind_plugin(EditorPlugin *p_plugin, Control *p_root) {
	plugin = p_plugin;
	plugin_root = p_root;

	if (plugin_root) {
		if (plugin_root->get_parent()) {
			plugin_root->get_parent()->remove_child(plugin_root);
		}
		add_child(plugin_root);
		plugin_root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		plugin_root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	}
}

void MainScreenDock::update_layout(DockLayout p_layout) {
	// Phase 1 stub: forward to base for GDScript virtual dispatch only.
	EditorDock::update_layout(p_layout);
}

void MainScreenDock::save_layout_to_config(Ref<ConfigFile> &p_layout, const String &p_section) const {
	EditorDock::save_layout_to_config(p_layout, p_section);
}

void MainScreenDock::load_layout_from_config(const Ref<ConfigFile> &p_layout, const String &p_section) {
	EditorDock::load_layout_from_config(p_layout, p_section);
}

MainScreenDock::MainScreenDock() {
	// Main-screen docks live in the center slot by default, can also be moved to
	// side/bottom slots or floated, and are not closable (the user always needs
	// access to the workspaces).
	set_default_slot(DOCK_SLOT_CENTER);
	set_available_layouts(DOCK_LAYOUT_ALL | DOCK_LAYOUT_CENTER);
	set_closable(false);
	set_global(false);
}

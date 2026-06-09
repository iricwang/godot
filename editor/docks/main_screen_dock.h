/**************************************************************************/
/*  main_screen_dock.h                                                    */
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

#include "editor/docks/editor_dock.h"

class Control;
class EditorPlugin;

// Bridge that exposes a main-screen EditorPlugin (2D / 3D / Script / Game / AssetLib)
// as an EditorDock so it participates in EditorDockManager's drag/snap/float/layout system.
//
// Phase 1: skeleton only — not registered with the dock manager yet, and EditorMainScreen
// continues to manage the five plugins via its old exclusive-button mechanism. Phase 2
// will wire add_main_plugin to create one MainScreenDock per plugin and reparent the
// plugin's root Control into it.
class MainScreenDock : public EditorDock {
	GDCLASS(MainScreenDock, EditorDock);

	EditorPlugin *plugin = nullptr;
	Control *plugin_root = nullptr;

protected:
	static void _bind_methods();

	// EditorDock overrides — Phase 1 stubs (no behavior change). Phase 2/3 will implement.
	virtual void update_layout(DockLayout p_layout) override;
	virtual void save_layout_to_config(Ref<ConfigFile> &p_layout, const String &p_section) const override;
	virtual void load_layout_from_config(const Ref<ConfigFile> &p_layout, const String &p_section) override;

public:
	void bind_plugin(EditorPlugin *p_plugin, Control *p_root);
	EditorPlugin *get_plugin() const { return plugin; }
	Control *get_plugin_root() const { return plugin_root; }

	MainScreenDock();
};

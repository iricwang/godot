/**************************************************************************/
/*  node_palette_dock.h                                                   */
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
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"

class NodePaletteDock : public EditorDock {
	GDCLASS(NodePaletteDock, EditorDock);

	enum Filter {
		FILTER_ALL,
		FILTER_CONTROL,
		FILTER_2D,
		FILTER_3D,
		FILTER_OTHER
	};

	Tree *tree = nullptr;
	LineEdit *search_box = nullptr;
	Button *filter_all = nullptr;
	Button *filter_control = nullptr;
	Button *filter_2d = nullptr;
	Button *filter_3d = nullptr;

	Filter current_filter = FILTER_ALL;

	HashMap<String, TreeItem *> tree_items;
	HashMap<String, String> custom_type_parents;
	HashMap<String, int> custom_type_indices;
	List<String> type_list;

	void _fill_type_list();
	bool _should_hide_type(const StringName &p_type) const;
	bool _passes_filter(const StringName &p_type) const;
	void _add_type(const StringName &p_type);
	void _update_tree();
	void _filter_all_toggled();
	void _filter_control_toggled();
	void _filter_2d_toggled();
	void _filter_3d_toggled();
	void _search_changed(const String &p_text);
	void _item_activated();

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	static NodePaletteDock *singleton;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static NodePaletteDock *get_singleton() { return singleton; }

	NodePaletteDock();
};

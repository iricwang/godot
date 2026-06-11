/**************************************************************************/
/*  ei_editor_plugin.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                             https://godotengine.org                   */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
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
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/**************************************************************************/

#pragma once

// P7 (spec §4.9): custom inspector surface for Enhanced Input resources.
//
// Following the pattern of `editor/inspector/input_event_editor_plugin.h`:
// * `EIInspectorPlugin` (subclass of `EditorInspectorPlugin`) decides which
//   resource types it can render and, for those, injects a custom control
//   at the top of the inspector.
// * `EIEditorPlugin` (subclass of `EditorPlugin`) is the entry-point that
//   the engine registers via `EditorPlugins::add_by_type<>()` and which, on
//   construction, instantiates the inspector plugin and adds it to the
//   editor's inspector plugin list.
//
// Scope of v1 (per spec §4.9):
// * `EIMappingContext` gets a custom table-like UI for its mappings
//   (event -> action + remove button + add button). Replaces the default
//   Array<Dictionary> rendering which is opaque and hard to author.
// * `EIAction`'s default_modifiers / default_triggers are already editable
//   via the default inspector (`TypedArray<EIModifier>` / `TypedArray<EITrigger>`
//   render as resource arrays with pickers filtered to the right base class
//   because we registered the concrete subclasses). So no custom inspector
//   is needed for EIAction in v1.

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

namespace ei {
class EIMappingContext;
} // namespace ei

class Button;
class ConfirmationDialog;
class EditorResourcePicker;
class Label;
class VBoxContainer;

class EIMappingContextMappingsControl : public VBoxContainer {
	GDCLASS(EIMappingContextMappingsControl, VBoxContainer);

	Ref<ei::EIMappingContext> _ctx;
	Label *_summary_label = nullptr;
	VBoxContainer *_rows = nullptr;

	// Add-Mapping popup state.
	ConfirmationDialog *_add_dialog = nullptr;
	EditorResourcePicker *_add_event_picker = nullptr;
	EditorResourcePicker *_add_action_picker = nullptr;
	Button *_add_consumes_button = nullptr;
	bool _add_consumes = true;

	void _rebuild_rows();
	void _on_remove_pressed(int p_index);
	void _on_clear_all_pressed();
	void _on_add_button_pressed();
	void _on_add_dialog_confirmed();

public:
	void set_context(const Ref<ei::EIMappingContext> &p_ctx);

	EIMappingContextMappingsControl();
};

class EIInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(EIInspectorPlugin, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EIEditorPlugin : public EditorPlugin {
	GDCLASS(EIEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "EnhancedInput"; }

	EIEditorPlugin();
};
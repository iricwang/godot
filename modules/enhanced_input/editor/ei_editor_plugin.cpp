/**************************************************************************/
/*  ei_editor_plugin.cpp                                                  */
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

#include "ei_editor_plugin.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/typed_array.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector/editor_resource_picker.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"

#include "../core/ei_action.h"
#include "../core/ei_mapping_context.h"
#include "../core/ei_modifier.h"
#include "../core/ei_trigger.h"

namespace {

// Format an InputEvent for display in a single short line. Used for the
// mappings list rows. Mirrors the kind of text the existing
// InputEventEditorPlugin would produce via `as_text()`, but stripped down
// to keep table rows compact.
String _format_event_short(const Ref<InputEvent> &p_event) {
	if (p_event.is_null()) {
		return "<no event>";
	}
	return p_event->as_text();
}

// Format an EIAction for display. Description is the primary label; the
// value_type is appended in brackets so the user can sanity-check at a
// glance whether a mapping's typed correctly.
String _format_action_short(const Ref<ei::EIAction> &p_action) {
	if (p_action.is_null()) {
		return "<no action>";
	}
	String desc = p_action->get_description();
	if (desc.is_empty()) {
		desc = "<no description>";
	}
	String type_name;
	switch (p_action->get_value_type()) {
		case ei::EIAction::VALUE_TYPE_BOOL:
			type_name = "Bool";
			break;
		case ei::EIAction::VALUE_TYPE_AXIS1D:
			type_name = "1D";
			break;
		case ei::EIAction::VALUE_TYPE_AXIS2D:
			type_name = "2D";
			break;
		case ei::EIAction::VALUE_TYPE_AXIS3D:
			type_name = "3D";
			break;
	}
	return vformat("%s  [%s]", desc, type_name);
}

} // namespace

// ---------------------------------------------------------------------------
// EIMappingContextMappingsControl
// ---------------------------------------------------------------------------

void EIMappingContextMappingsControl::_rebuild_rows() {
	// Clear previous row widgets (keep the summary label and the bottom
	// action buttons — those are added once in the ctor).
	while (_rows->get_child_count() > 0) {
		Node *child = _rows->get_child(0);
		_rows->remove_child(child);
		child->queue_free();
	}

	if (_ctx.is_null()) {
		_summary_label->set_text("Mappings (0) — no context");
		return;
	}

	TypedArray<Dictionary> mappings = _ctx->get_mappings();
	_summary_label->set_text(vformat("Mappings (%d)", mappings.size()));

	for (int i = 0; i < mappings.size(); i++) {
		Dictionary d = mappings[i];
		Ref<InputEvent> event = d["event"];
		Ref<ei::EIAction> action = d["action"];
		TypedArray<ei::EIModifier> mods = d["modifiers"];
		TypedArray<ei::EITrigger> trigs = d["triggers"];
		bool consumes = d["consumes"];

		HBoxContainer *row = memnew(HBoxContainer);
		row->add_theme_constant_override("separation", 8);

		// Index badge — small numeric label so the row is identifiable
		// in remove-button binding without resorting to object metadata.
		Label *idx_label = memnew(Label);
		idx_label->set_text(itos(i));
		idx_label->set_custom_minimum_size(Size2(24, 0));
		idx_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
		row->add_child(idx_label);

		Label *event_label = memnew(Label);
		event_label->set_text(_format_event_short(event));
		event_label->set_size_flags(Control::SIZE_EXPAND_FILL);
		event_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		row->add_child(event_label);

		Label *arrow = memnew(Label);
		arrow->set_text("→");
		row->add_child(arrow);

		Label *action_label = memnew(Label);
		action_label->set_text(_format_action_short(action));
		action_label->set_size_flags(Control::SIZE_EXPAND_FILL);
		action_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		row->add_child(action_label);

		Label *meta_label = memnew(Label);
		String meta = vformat("mods:%d  trigs:%d  %s",
				mods.size(), trigs.size(), consumes ? "consumes" : "pass-through");
		meta_label->set_text(meta);
		meta_label->set_custom_minimum_size(Size2(220, 0));
		row->add_child(meta_label);

		Button *remove_btn = memnew(Button);
		remove_btn->set_text("Remove");
		// Capture `i` by value in the lambda-equivalent Callable binding.
		// We use bind() rather than capturing `this` + reading the index
		// at click time so the row removal is deterministic even after
		// subsequent add/remove cycles (the rebuild clears widgets; we
		// don't rely on stale widget state).
		remove_btn->connect(SceneStringName(pressed),
				callable_mp(this, &EIMappingContextMappingsControl::_on_remove_pressed).bind(i));
		row->add_child(remove_btn);

		_rows->add_child(row);
	}
}

void EIMappingContextMappingsControl::_on_remove_pressed(int p_index) {
	if (_ctx.is_null()) {
		return;
	}
	TypedArray<Dictionary> mappings = _ctx->get_mappings();
	if (p_index < 0 || p_index >= mappings.size()) {
		return; // Stale row from a prior rebuild; safe to ignore.
	}
	Dictionary d = mappings[p_index];
	Ref<InputEvent> event = d["event"];
	Ref<ei::EIAction> action = d["action"];
	TypedArray<ei::EIModifier> mods = d["modifiers"];
	TypedArray<ei::EITrigger> trigs = d["triggers"];
	bool consumes = d["consumes"];

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Mapping"));
	undo_redo->add_do_method(_ctx.ptr(), "remove_mapping", event, action);
	undo_redo->add_undo_method(_ctx.ptr(), "add_mapping",
			event, action, mods, trigs, consumes);
	undo_redo->commit_action();
	// The Resource's changed signal will fire; the inspector usually
	// rebuilds via _notification(NOTIFICATION_RESET), but we explicitly
	// rebuild so the row list stays in sync even if the inspector
	// doesn't refresh (e.g. when this control is the only thing open).
	_rebuild_rows();
}

void EIMappingContextMappingsControl::_on_clear_all_pressed() {
	if (_ctx.is_null()) {
		return;
	}
	TypedArray<Dictionary> mappings = _ctx->get_mappings();
	if (mappings.is_empty()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Clear Mappings"));
	undo_redo->add_do_method(_ctx.ptr(), "clear_mappings");
	for (int i = 0; i < mappings.size(); i++) {
		Dictionary d = mappings[i];
		undo_redo->add_undo_method(_ctx.ptr(), "add_mapping",
				d["event"], d["action"], d["modifiers"], d["triggers"], d["consumes"]);
	}
	undo_redo->commit_action();
	_rebuild_rows();
}

void EIMappingContextMappingsControl::_on_add_button_pressed() {
	// Reset state for a fresh add.
	_add_event_picker->set_edited_resource(Ref<Resource>());
	_add_action_picker->set_edited_resource(Ref<Resource>());
	_add_consumes = true;
	_add_consumes_button->set_pressed(true);
	_add_dialog->popup_centered(Size2(420, 240) * EDSCALE);
}

void EIMappingContextMappingsControl::_on_add_dialog_confirmed() {
	if (_ctx.is_null()) {
		return;
	}
	Ref<InputEvent> event = _add_event_picker->get_edited_resource();
	Ref<ei::EIAction> action = _add_action_picker->get_edited_resource();
	if (event.is_null() || action.is_null()) {
		return; // Both pickers must resolve; silently no-op to avoid
				// partial mappings landing on undo stack.
	}
	bool consumes = _add_consumes_button->is_pressed();
	TypedArray<ei::EIModifier> empty_mods;
	TypedArray<ei::EITrigger> empty_trigs;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Mapping"));
	undo_redo->add_do_method(_ctx.ptr(), "add_mapping",
			event, action, empty_mods, empty_trigs, consumes);
	undo_redo->add_undo_method(_ctx.ptr(), "remove_mapping", event, action);
	undo_redo->commit_action();
	_rebuild_rows();
}

void EIMappingContextMappingsControl::set_context(const Ref<ei::EIMappingContext> &p_ctx) {
	_ctx = p_ctx;
	_rebuild_rows();
}

EIMappingContextMappingsControl::EIMappingContextMappingsControl() {
	_summary_label = memnew(Label);
	_summary_label->set_text("Mappings (0)");
	add_child(_summary_label);

	_rows = memnew(VBoxContainer);
	_rows->add_theme_constant_override("separation", 2);
	add_child(_rows);

	add_child(memnew(HSeparator));

	// Bottom action bar: Clear All + Add.
	HBoxContainer *actions = memnew(HBoxContainer);
	actions->add_theme_constant_override("separation", 8);

	Button *add_btn = memnew(Button);
	add_btn->set_text("Add Mapping…");
	add_btn->connect(SceneStringName(pressed),
			callable_mp(this, &EIMappingContextMappingsControl::_on_add_button_pressed));
	actions->add_child(add_btn);

	Button *clear_btn = memnew(Button);
	clear_btn->set_text("Clear All");
	clear_btn->connect(SceneStringName(pressed),
			callable_mp(this, &EIMappingContextMappingsControl::_on_clear_all_pressed));
	actions->add_child(clear_btn);

	add_child(actions);

	// Add-Mapping popup.
	_add_dialog = memnew(ConfirmationDialog);
	_add_dialog->set_title("Add Mapping");
	_add_dialog->set_ok_button_text(TTR("Add"));
	_add_dialog->connect(SceneStringName(confirmed),
			callable_mp(this, &EIMappingContextMappingsControl::_on_add_dialog_confirmed));

	VBoxContainer *dialog_body = memnew(VBoxContainer);
	_add_dialog->add_child(dialog_body);

	Label *event_label = memnew(Label);
	event_label->set_text("Event:");
	dialog_body->add_child(event_label);
	_add_event_picker = memnew(EditorResourcePicker);
	_add_event_picker->set_base_type("InputEvent");
	dialog_body->add_child(_add_event_picker);

	Label *action_label = memnew(Label);
	action_label->set_text("Action:");
	dialog_body->add_child(action_label);
	_add_action_picker = memnew(EditorResourcePicker);
	// "EIAction" base type — the picker lists .tres files of EIAction
	// (and its subclasses, of which there are none yet) plus the
	// "New EIAction" / "Load…" affordances.
	_add_action_picker->set_base_type("EIAction");
	dialog_body->add_child(_add_action_picker);

	_add_consumes_button = memnew(CheckButton);
	_add_consumes_button->set_text("Consumes input (stops lower-priority contexts)");
	_add_consumes_button->set_pressed(true);
	dialog_body->add_child(_add_consumes_button);

	// ConfirmationDialog needs an explicit parent before popup_centered
	// can resolve positioning correctly.
	add_child(_add_dialog);
}

// ---------------------------------------------------------------------------
// EIInspectorPlugin
// ---------------------------------------------------------------------------

bool EIInspectorPlugin::can_handle(Object *p_object) {
	return Object::cast_to<ei::EIMappingContext>(p_object) != nullptr;
}

void EIInspectorPlugin::parse_begin(Object *p_object) {
	Ref<ei::EIMappingContext> ctx = Object::cast_to<ei::EIMappingContext>(p_object);
	ERR_FAIL_NULL(ctx.ptr());

	EIMappingContextMappingsControl *control = memnew(EIMappingContextMappingsControl);
	control->set_context(ctx);
	add_custom_control(control);

	// Default property rendering of `mappings` (Array<Dictionary>) and
	// `context_name` (String) still happens below the custom control —
	// which is fine: context_name is a one-line field and mappings'
	// default view acts as a structured backup / debug view of the same
	// data the custom UI edits. Spec §4.9 asks for an "editable
	// EditorInspector table"; this layered rendering gives both an
	// author-friendly UI and a raw debug view at zero extra code.
}

// ---------------------------------------------------------------------------
// EIEditorPlugin
// ---------------------------------------------------------------------------

EIEditorPlugin::EIEditorPlugin() {
	Ref<EIInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
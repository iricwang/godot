/**************************************************************************/
/*  ei_bridge.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
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

// Sibling include (same `core/` directory) — see ei_modifier.h /
// ei_action.h for the rationale (lets this header resolve from both
// in-module sources and tests/).
#include "ei_action.h"
#include "ei_mapping_context.h"
#include "core/io/resource.h"
#include "core/object/ref_counted.h"

class InputMap;

namespace ei {

// Read-only importer from Godot's `InputMap` to Enhanced Input.
//
// Per spec §4.10 (enhanced-input.md v0.2):
// * `EIBridge` SHALL NOT modify `InputMap` or `Input`. It is a
//   read-only importer.
// * Conversion SHALL be lossy in v1: triggers, modifiers, and
//   composite bindings are NOT imported — they require explicit
//   EI authoring. The imported `EIMappingContext` has bare 1:1
//   mappings (event → action, no modifiers, no triggers, consumes=true).
//
// Usage from GDScript:
//   var ia := EIBridge.import_action("ui_accept")
//   var imc := EIBridge.import_context("player_jump", "Gameplay", 0)
//   imc.add_mapping(...)  # fine, layer on top of the import
//
// The intent of v1 is to give consumers a fast on-ramp: bring their
// existing InputMap action names into EI without rewriting every
// binding. Once they're in EI, the EI-specific affordances
// (modifiers, triggers, contexts) are layered on via .tres editing
// or programmatic API.
class EIBridge : public RefCounted {
	GDCLASS(EIBridge, RefCounted);

public:
	// Build an `EIAction` resource named after the InputMap action.
	// The action has no default modifiers / triggers — those are
	// added by the consumer.
	//
	// `p_type` defaults to BOOL because most InputMap actions are
	// button-like. For axis-bound actions, pass the matching
	// `EIAction::ValueType` explicitly.
	//
	// Returns a null Ref if the InputMap singleton is unavailable
	// or the named action does not exist — symmetric with
	// `import_context`. This keeps typos loud (an `EIAction` with
	// a description that doesn't match any live action is useless
	// and silent failure would mask binding bugs).
	static Ref<EIAction> import_action(const StringName &p_input_map_action,
			EIAction::ValueType p_type = EIAction::VALUE_TYPE_BOOL);

	// Build an `EIMappingContext` whose mappings are 1:1 with the
	// InputMap action's events. Each mapping is a bare (event →
	// action) with no modifiers, no triggers, and consumes=true.
	// Composite bindings (multiple events combined into one
	// action) are NOT supported in v1.
	//
	// Returns a null Ref if the InputMap action does not exist.
	static Ref<EIMappingContext> import_context(const StringName &p_input_map_action,
			const String &p_context_name,
			int p_priority = 0);

protected:
	static void _bind_methods();
};

} // namespace ei

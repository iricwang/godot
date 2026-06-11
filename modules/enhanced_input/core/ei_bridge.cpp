/**************************************************************************/
/*  ei_bridge.cpp                                                          */
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

#include "ei_bridge.h"

#include "core/input/input_map.h"
#include "core/object/class_db.h"
#include "ei_modifier.h"
#include "ei_trigger.h"

namespace ei {

Ref<EIAction> EIBridge::import_action(const StringName &p_input_map_action, EIAction::ValueType p_type) {
	// Same existence check as `import_context`: a typo'd InputMap
	// name should fail loudly, not silently return an EIAction
	// whose description doesn't correspond to any real action.
	InputMap *im = InputMap::get_singleton();
	if (im == nullptr || !im->has_action(p_input_map_action)) {
		return Ref<EIAction>();
	}
	Ref<EIAction> a;
	a.instantiate();
	a->set_value_type(p_type);
	a->set_description(String(p_input_map_action));
	// No default modifiers / triggers — v1 keeps the import
	// minimal so consumers can layer their own without fighting
	// bridge-injected state.
	return a;
}

Ref<EIMappingContext> EIBridge::import_context(const StringName &p_input_map_action,
		const String &p_context_name,
		int p_priority) {
	// InputMap::has_action + action_get_events is the read-only path
	// we use (no writes, per spec §4.10). If the action doesn't exist
	// we return a null Ref so callers can detect "unknown action"
	// and skip.
	InputMap *im = InputMap::get_singleton();
	if (im == nullptr) {
		return Ref<EIMappingContext>();
	}
	if (!im->has_action(p_input_map_action)) {
		return Ref<EIMappingContext>();
	}
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	ctx->set_context_name(p_context_name);

	// Build a single EIAction for the action; we share one action
	// across all mappings (1 InputMap action → 1 EI action → N
	// mappings in this IMC, one per InputEvent).
	Ref<EIAction> action = import_action(p_input_map_action);

	const List<Ref<InputEvent>> *events = im->action_get_events(p_input_map_action);
	if (events != nullptr) {
		for (const Ref<InputEvent> &event : *events) {
			if (event.is_null()) {
				continue;
			}
			// Bare 1:1 mapping: event → action, no modifiers, no
			// triggers, consumes=true. Consumers can layer more
			// on top after import.
			ctx->add_mapping(event, action, TypedArray<EIModifier>(), TypedArray<EITrigger>(), /*consumes=*/true);
		}
	}

	// p_priority is accepted but not stored on the IMC itself —
	// priority is a property of the *registration* in EISubsystem,
	// not of the IMC resource. We don't store it here because that
	// would duplicate the priority state at runtime vs authoring
	// time and risk divergence. The caller passes it to
	// `add_mapping_context(ctx, priority)` at registration.
	(void)p_priority;

	return ctx;
}

void EIBridge::_bind_methods() {
	ClassDB::bind_static_method("EIBridge", D_METHOD("import_action", "input_map_action", "value_type"), &EIBridge::import_action, DEFVAL(EIAction::VALUE_TYPE_BOOL));
	ClassDB::bind_static_method("EIBridge", D_METHOD("import_context", "input_map_action", "context_name", "priority"), &EIBridge::import_context, DEFVAL(0));
}

} // namespace ei

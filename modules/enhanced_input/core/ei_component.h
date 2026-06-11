/**************************************************************************/
/*  ei_component.h                                                        */
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

// Sibling includes (same `core/` directory) — see ei_modifier.h for the
// rationale. The engine headers (core/variant/..., scene/main/...)
// stay module-root because the engine's CPPATH is set via SCsub's
// `Dir("#")` and resolves them.
#include "core/templates/vector.h"
#include "core/variant/callable.h"
#include "core/variant/typed_array.h"
#include "ei_action.h"
#include "ei_enums.h"
#include "scene/main/node.h"

namespace ei {

// Per-instance subscription registry. Forward-declared; full type
// in ei_subsystem.h.
class EISubsystem;

// A component (Node) that owns a set of trigger-event subscriptions
// and forwards them to `EISubsystem::bind_action()` / `unbind_action()`.
//
// Per spec §4.7 (enhanced-input.md v0.2):
// * Auto-registers with `EISubsystem::get_singleton()` on `_ready`.
// * Auto-unregisters on `_exit_tree`, removing all its bindings.
// * If a bound `Callable` becomes invalid (target freed), the system
//   SHALL silently skip it without spamming errors.
// * `is_bound` SHALL return `true` only if a subscription matches both
//   the action and the trigger event exactly.
//
// Standard usage from GDScript:
//   var comp := EIComponent.new()
//   add_child(comp)
//   comp.bind(ia_jump, EI.ETriggerEvent.TRIGGERED, _on_jump)
//   ...
//   comp.unbind_all()  # optional, also done automatically on _exit_tree
class EIComponent : public Node {
	GDCLASS(EIComponent, Node);

public:
	// Bind a Callable. Same triple-binding is idempotent.
	void bind(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
	void unbind(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
	void unbind_all();

	// Editor / debug introspection.
	bool is_bound(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event) const;
	int get_bound_count() const;

protected:
	static void _bind_methods();

	// Godot lifecycle. Use _notification to override these — Node
	// forwards _ready / _exit_tree through _notification under the
	// hood, so we hook there.
	void _notification(int p_what);

private:
	struct Subscription {
		Ref<EIAction> action;
		ETriggerEvent event;
		Callable callable;
	};

	Vector<Subscription> _subscriptions;

	// Find the EISubsystem singleton. Returns nullptr if not yet alive
	// (e.g. tests that don't run the autoload).
	EISubsystem *_get_subsystem() const;
};

} // namespace ei

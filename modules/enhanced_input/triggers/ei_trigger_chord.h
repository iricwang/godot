/**************************************************************************/
/*  ei_trigger_chord.h                                                   */
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

#include "../core/ei_action.h"
#include "../core/ei_trigger.h"
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"

namespace ei {

// EITriggerChord per spec §4.4:
//   Fire when ALL listed `EIAction` (chord actions) are currently in
//   `Triggered` state.
//
// Spec scenario: `EITriggerChord([IA_Dash, IA_Attack])` only fires when
// both are currently Triggered.
//
// Implementation note: this trigger needs to know the live "is this
// action currently Triggered?" state of each chord action. The spec's
// `update_state` signature has no subsystem handle, so the trigger
// exposes `set_chord_action_active(...)` that the EISubsystem calls
// (P5) when each chord action's trigger fires / completes. The
// per-chord-action "active" map lives on the TRIGGER RESOURCE (shared
// across all actions using the same chord), not in TriggerRuntimeState
// (which is per-action).
//
// Per-action state (the FSM position) is in TriggerRuntimeState:
//   * STATE_NONE      : not all chord actions are active
//   * STATE_TRIGGERED : all chord actions are active; we just fired
//                        (or are still firing) TRIGGERED
//
// Lifecycle:
//   * update_state, all active, state was NONE        -> TRIGGERED
//   * update_state, all active, state was TRIGGERED   -> ONGOING
//   * update_state, not all active, state was TRIGGERED -> COMPLETED
//   * update_state, not all active, state was NONE    -> NONE
//   * empty chord list                                -> NONE always
//                                                        (unless was
//                                                        TRIGGERED, in
//                                                        which case
//                                                        COMPLETED)
//
// Resource circular refs (spec §4.7) are not validated in P4; P5 will
// add a load-time check that a chord action does not transitively
// reference its parent action.

class EITriggerChord : public EITrigger {
	GDCLASS(EITriggerChord, EITrigger);

public:
	// Chord configuration. The chord action list is part of the
	// trigger resource, not the runtime state, so it can be saved as
	// .tres and shared across actions.
	void add_chord_action(const Ref<EIAction> &p_action);
	void clear_chord_actions();
	int get_chord_action_count() const;
	Ref<EIAction> get_chord_action(int p_index) const;

	// Array-style setter/getter used by ADD_PROPERTY so the ClassDB
	// getter/setter type check passes. The inspector calls
	// set_chord_actions() to replace the whole list.
	void set_chord_actions(const TypedArray<EIAction> &p_actions);
	TypedArray<EIAction> get_chord_actions() const;

	// Live chord action state. The subsystem calls this to push the
	// current "is this chord action currently Triggered?" answer into
	// the trigger. Stored on the resource so the answer is shared
	// across all consumers of this chord definition.
	void set_chord_action_active(const Ref<EIAction> &p_action, bool p_active);
	bool is_chord_action_active(const Ref<EIAction> &p_action) const;

	virtual UpdateResult update_state(
			TriggerRuntimeState &p_runtime,
			const EIValue &p_value,
			double p_delta_t,
			bool p_event_valid,
			bool p_pressed) const override;

	virtual String get_trigger_name() const override;

protected:
	static void _bind_methods();

private:
	TypedArray<EIAction> _chord_actions;

	// ObjectID -> bool: per-chord-action "currently Triggered" state.
	// Keyed by ObjectID (not Ref<>) so we can look up cheaply on every
	// update without touching refcounts. P5 wires the subsystem to call
	// set_chord_action_active(); the test does the same.
	mutable HashMap<ObjectID, bool> _action_active;
};

} // namespace ei

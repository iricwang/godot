/**************************************************************************/
/*  ei_trigger_chord.cpp                                                 */
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

#include "ei_trigger_chord.h"

#include "core/object/class_db.h"

namespace ei {

// ---------------------------------------------------------------------------
// Chord configuration
// ---------------------------------------------------------------------------

void EITriggerChord::add_chord_action(const Ref<EIAction> &p_action) {
	if (p_action.is_null()) {
		return;
	}
	_chord_actions.push_back(p_action);
	// Default new chord action to inactive; subsystem flips it on
	// trigger events. Using ObjectID as the key matches the lookup in
	// is_chord_action_active / update_state.
	_action_active[p_action->get_instance_id()] = false;
}

void EITriggerChord::clear_chord_actions() {
	_chord_actions.clear();
	_action_active.clear();
}

int EITriggerChord::get_chord_action_count() const {
	return _chord_actions.size();
}

Ref<EIAction> EITriggerChord::get_chord_action(int p_index) const {
	if (p_index < 0 || p_index >= _chord_actions.size()) {
		return Ref<EIAction>();
	}
	return _chord_actions[p_index];
}

void EITriggerChord::set_chord_actions(const TypedArray<EIAction> &p_actions) {
	clear_chord_actions();
	for (int i = 0; i < p_actions.size(); i++) {
		Ref<EIAction> a = p_actions[i];
		add_chord_action(a);
	}
}

TypedArray<EIAction> EITriggerChord::get_chord_actions() const {
	return _chord_actions;
}

// ---------------------------------------------------------------------------
// Live chord action state (pushed by subsystem / test)
// ---------------------------------------------------------------------------

void EITriggerChord::set_chord_action_active(const Ref<EIAction> &p_action, bool p_active) {
	if (p_action.is_null()) {
		return;
	}
	_action_active[p_action->get_instance_id()] = p_active;
}

bool EITriggerChord::is_chord_action_active(const Ref<EIAction> &p_action) const {
	if (p_action.is_null()) {
		return false;
	}
	const HashMap<ObjectID, bool>::ConstIterator it = _action_active.find(p_action->get_instance_id());
	return it && it->value;
}

// ---------------------------------------------------------------------------
// update_state
// ---------------------------------------------------------------------------

EITrigger::UpdateResult EITriggerChord::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_value;
	(void)p_event_valid;
	(void)p_pressed;

	// Accumulate dt so the runtime clock is consistent across all
	// trigger subclasses; chord doesn't actually need it.
	p_runtime.elapsed += p_delta_t;

	// Empty chord: treat as "no condition to satisfy, never fire".
	// If we WERE triggered (somehow), transition to NONE + COMPLETED so
	// listeners see a clean shutdown.
	if (_chord_actions.is_empty()) {
		if (p_runtime.state == STATE_TRIGGERED) {
			p_runtime.state = STATE_NONE;
			return RESULT_COMPLETED;
		}
		return RESULT_NONE;
	}

	// Evaluate: are all chord actions currently active?
	bool all_active = true;
	for (int i = 0; i < _chord_actions.size(); i++) {
		Ref<EIAction> a = _chord_actions[i];
		if (a.is_null()) {
			// A null chord action never counts as active; the chord
			// won't fire until the user wires a real action in.
			all_active = false;
			break;
		}
		const HashMap<ObjectID, bool>::ConstIterator it = _action_active.find(a->get_instance_id());
		if (!it || !it->value) {
			all_active = false;
			break;
		}
	}

	if (all_active) {
		if (p_runtime.state == STATE_NONE) {
			// Rising edge: all chord actions just became active. Fire.
			p_runtime.state = STATE_TRIGGERED;
			return RESULT_TRIGGERED;
		}
		// Already triggered, still all active: sustain with ONGOING.
		return RESULT_ONGOING;
	}

	// Not all active.
	if (p_runtime.state == STATE_TRIGGERED) {
		// Falling edge: was all-active, now not. Complete.
		p_runtime.state = STATE_NONE;
		return RESULT_COMPLETED;
	}
	return RESULT_NONE;
}

String EITriggerChord::get_trigger_name() const {
	return "Chord";
}

void EITriggerChord::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_chord_action", "action"), &EITriggerChord::add_chord_action);
	ClassDB::bind_method(D_METHOD("clear_chord_actions"), &EITriggerChord::clear_chord_actions);
	ClassDB::bind_method(D_METHOD("get_chord_action_count"), &EITriggerChord::get_chord_action_count);
	ClassDB::bind_method(D_METHOD("get_chord_action", "index"), &EITriggerChord::get_chord_action);

	ClassDB::bind_method(D_METHOD("set_chord_actions", "actions"), &EITriggerChord::set_chord_actions);
	ClassDB::bind_method(D_METHOD("get_chord_actions"), &EITriggerChord::get_chord_actions);

	ClassDB::bind_method(D_METHOD("set_chord_action_active", "action", "active"), &EITriggerChord::set_chord_action_active);
	ClassDB::bind_method(D_METHOD("is_chord_action_active", "action"), &EITriggerChord::is_chord_action_active);

	// Use the array-style setter/getter pair so the ClassDB
	// getter/setter type check passes (add_chord_action is a mutator
	// with no matching getter, so we don't use it for ADD_PROPERTY).
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "chord_actions", PROPERTY_HINT_ARRAY_TYPE, "EIAction"),
			"set_chord_actions", "get_chord_actions");
}

} // namespace ei

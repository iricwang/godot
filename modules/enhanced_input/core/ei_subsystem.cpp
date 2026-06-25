/**************************************************************************/
/*  ei_subsystem.cpp                                                      */
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

#include "ei_subsystem.h"

#include "ei_input_event_sampler.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "triggers/ei_trigger_chord.h"

namespace ei {

EISubsystem *EISubsystem::singleton = nullptr;

EISubsystem *EISubsystem::get_singleton() {
	return singleton;
}

EISubsystem::EISubsystem() {
	if (singleton == nullptr) {
		singleton = this;
	}
}

EISubsystem::~EISubsystem() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

// ---------------------------------------------------------------------------
// IMC management
// ---------------------------------------------------------------------------

void EISubsystem::add_mapping_context(const Ref<EIMappingContext> &p_ctx, int p_priority) {
	if (p_ctx.is_null()) {
		return;
	}
	// Re-adding: update priority in place, keep insertion order
	// so the new priority doesn't pull the IMC ahead of older
	// IMCs at the same priority.
	for (int i = 0; i < _contexts.size(); i++) {
		if (_contexts[i].ctx == p_ctx) {
			_contexts.write[i].priority = p_priority;
			_sort_contexts();
			return;
		}
	}
	ContextEntry e;
	e.ctx = p_ctx;
	e.priority = p_priority;
	e.order = _context_seq++;
	_contexts.push_back(e);
	_sort_contexts();
}

void EISubsystem::remove_mapping_context(const Ref<EIMappingContext> &p_ctx) {
	if (p_ctx.is_null()) {
		return;
	}
	for (int i = 0; i < _contexts.size(); i++) {
		if (_contexts[i].ctx == p_ctx) {
			_contexts.remove_at(i);
			return;
		}
	}
}

void EISubsystem::clear_all_mapping_contexts() {
	_contexts.clear();
}

bool EISubsystem::has_mapping_context(const Ref<EIMappingContext> &p_ctx) const {
	if (p_ctx.is_null()) {
		return false;
	}
	for (int i = 0; i < _contexts.size(); i++) {
		if (_contexts[i].ctx == p_ctx) {
			return true;
		}
	}
	return false;
}

void EISubsystem::_sort_contexts() {
	// Insertion sort: stable, fast for small N (typical: a handful
	// of IMCs). Spec: priority DESC, order ASC.
	for (int i = 1; i < _contexts.size(); i++) {
		const ContextEntry cur = _contexts[i];
		int j = i - 1;
		while (j >= 0) {
			const ContextEntry &prev = _contexts[j];
			// Higher priority comes first; equal priority → lower
			// insertion order (older) comes first.
			if (prev.priority > cur.priority ||
					(prev.priority == cur.priority && prev.order <= cur.order)) {
				break;
			}
			_contexts.write[j + 1] = prev;
			j--;
		}
		_contexts.write[j + 1] = cur;
	}
}

// ---------------------------------------------------------------------------
// Bindings
// ---------------------------------------------------------------------------

void EISubsystem::bind_action(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable) {
	if (p_action.is_null() || !p_callable.is_valid()) {
		return;
	}
	// Skip EI_TRIGGER_EVENT_NONE — nothing fires on NONE, so binding
	// to it would be a silent no-op. We refuse explicitly so a caller
	// can debug.
	if (p_trigger_event == EI_TRIGGER_EVENT_NONE) {
		return;
	}
	// Idempotency: if the same (action, event, callable) triple is
	// already bound, do nothing. Spec §4.6: "calling it twice with
	// the same action/event/callable SHALL be idempotent."
	HashMap<Ref<EIAction>, HashMap<ETriggerEvent, Vector<ComponentBinding>>>::Iterator ait = _bindings.find(p_action);
	if (ait) {
		HashMap<ETriggerEvent, Vector<ComponentBinding>>::Iterator eit = ait->value.find(p_trigger_event);
		if (eit) {
			// Event already present: check for duplicate Callable.
			for (int i = 0; i < eit->value.size(); i++) {
				if (eit->value[i].callable == p_callable) {
					return; // already bound
				}
			}
			ComponentBinding b;
			b.callable = p_callable;
			eit->value.push_back(b);
			return;
		}
		// New event on an existing action: insert (preserves
		// existing entries — `ait->value = new_map` would clobber
		// them, which is a footgun).
		ComponentBinding b;
		b.callable = p_callable;
		ait->value.insert(p_trigger_event, Vector<ComponentBinding>{ b });
		return;
	}
	// New action: build the full nested structure.
	HashMap<ETriggerEvent, Vector<ComponentBinding>> new_event_map;
	ComponentBinding b;
	b.callable = p_callable;
	new_event_map[p_trigger_event] = { b };
	_bindings[p_action] = new_event_map;
}

void EISubsystem::unbind_action(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable) {
	if (p_action.is_null()) {
		return;
	}
	HashMap<Ref<EIAction>, HashMap<ETriggerEvent, Vector<ComponentBinding>>>::Iterator ait = _bindings.find(p_action);
	if (!ait) {
		return;
	}
	HashMap<ETriggerEvent, Vector<ComponentBinding>>::Iterator eit = ait->value.find(p_trigger_event);
	if (!eit) {
		return;
	}
	for (int i = 0; i < eit->value.size(); i++) {
		if (eit->value[i].callable == p_callable) {
			eit->value.remove_at(i);
			// Clean up empty buckets.
			if (eit->value.is_empty()) {
				ait->value.erase(p_trigger_event);
				if (ait->value.is_empty()) {
					_bindings.erase(p_action);
				}
			}
			return;
		}
	}
}

void EISubsystem::clear_bindings() {
	_bindings.clear();
}

// ---------------------------------------------------------------------------
// Programmatic input
// ---------------------------------------------------------------------------

void EISubsystem::inject_input(const Ref<InputEvent> &p_event) {
	if (p_event.is_null()) {
		return;
	}
	_frame_counter++;
	_dispatch_event(p_event);
}

void EISubsystem::tick(double p_delta) {
	// Per-frame tick for time-based triggers (Hold, Pulse). The
	// autoload wires `_process` to call this. Tests can also call
	// it directly with a controlled dt to exercise trigger timing
	// without spinning up a real SceneTree.
	_frame_counter++;
	_process_triggers(p_delta);
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

EIValue EISubsystem::get_action_value(const Ref<EIAction> &p_action) const {
	if (p_action.is_null()) {
		return EIValue::make_bool(false);
	}
	const HashMap<Ref<EIAction>, ActionRuntime>::ConstIterator it = _action_runtimes.find(p_action);
	if (!it) {
		return EIValue::make_bool(false);
	}
	return it->value.current_value;
}

bool EISubsystem::is_action_active(const Ref<EIAction> &p_action) const {
	if (p_action.is_null()) {
		return false;
	}
	const HashMap<Ref<EIAction>, ActionRuntime>::ConstIterator it = _action_runtimes.find(p_action);
	if (!it) {
		return false;
	}
	return it->value.is_held;
}

ETriggerEvent EISubsystem::get_action_trigger_event(const Ref<EIAction> &p_action) const {
	if (p_action.is_null()) {
		return EI_TRIGGER_EVENT_NONE;
	}
	const HashMap<Ref<EIAction>, ActionRuntime>::ConstIterator it = _action_runtimes.find(p_action);
	if (!it) {
		return EI_TRIGGER_EVENT_NONE;
	}
	return it->value.last_event;
}

Variant EISubsystem::get_action_value_variant(const Ref<EIAction> &p_action) const {
	return get_action_value(p_action).to_variant();
}

EISubsystem::ActionRuntime &EISubsystem::_get_or_create_runtime(const Ref<EIAction> &p_action) {
	HashMap<Ref<EIAction>, ActionRuntime>::Iterator it = _action_runtimes.find(p_action);
	if (it) {
		return it->value;
	}
	ActionRuntime rt;
	_ensure_action_triggers(rt, p_action);
	_action_runtimes.insert(p_action, rt);
	it = _action_runtimes.find(p_action);
	return it->value;
}

// ---------------------------------------------------------------------------
// Modifier chain
// ---------------------------------------------------------------------------

EIValue EISubsystem::_apply_modifier_chain(const Ref<EIAction> &p_action,
		const EIMappingContext::Mapping &p_mapping,
		const EIValue &p_raw_value) {
	EIValue v = p_raw_value;
	// Per spec §4.6 / §6.2: per-mapping modifiers first (in
	// declaration order), then action.default_modifiers.
	for (int i = 0; i < p_mapping.modifiers.size(); i++) {
		Ref<EIModifier> m = p_mapping.modifiers[i];
		if (m.is_valid()) {
			v = m->modify_value(v);
		}
	}
	const TypedArray<EIModifier> &def_mods = p_action->get_default_modifiers();
	for (int i = 0; i < def_mods.size(); i++) {
		Ref<EIModifier> m = def_mods[i];
		if (m.is_valid()) {
			v = m->modify_value(v);
		}
	}
	return v;
}

// ---------------------------------------------------------------------------
// Trigger chain assembly (lazy)
// ---------------------------------------------------------------------------

void EISubsystem::_ensure_action_triggers(ActionRuntime &p_runtime, const Ref<EIAction> &p_action) {
	// The trigger chain for an action is the union of all per-mapping
	// triggers from its currently-bound mappings, plus the action's
	// default_triggers. Built lazily on first dispatch. We track the
	// set of contexts the chain was assembled from in the runtime
	// (not stored in P5b — we re-derive on every dispatch for
	// simplicity; v2 can cache by context-set hash if profiling
	// demands it).
	p_runtime.triggers.clear();
	p_runtime.trigger_states.clear();

	const TypedArray<EITrigger> &def_trigs = p_action->get_default_triggers();
	for (int i = 0; i < def_trigs.size(); i++) {
		Ref<EITrigger> t = def_trigs[i];
		if (t.is_valid()) {
			p_runtime.triggers.push_back(t);
			EITrigger::TriggerRuntimeState s;
			p_runtime.trigger_states.insert(t, s);
		}
	}
}

// ---------------------------------------------------------------------------
// Event aggregation
// ---------------------------------------------------------------------------

ETriggerEvent EISubsystem::_aggregate_events(const Vector<ETriggerEvent> &p_events) {
	// Spec §4.6 priority:
	//   TRIGGERED > ONGOING > STARTED > COMPLETED > CANCELED > NONE
	bool seen_triggered = false;
	bool seen_ongoing = false;
	bool seen_started = false;
	bool seen_completed = false;
	bool seen_canceled = false;
	for (int i = 0; i < p_events.size(); i++) {
		switch (p_events[i]) {
			case EI_TRIGGER_EVENT_TRIGGERED:
				seen_triggered = true;
				break;
			case EI_TRIGGER_EVENT_ONGOING:
				seen_ongoing = true;
				break;
			case EI_TRIGGER_EVENT_STARTED:
				seen_started = true;
				break;
			case EI_TRIGGER_EVENT_COMPLETED:
				seen_completed = true;
				break;
			case EI_TRIGGER_EVENT_CANCELED:
				seen_canceled = true;
				break;
			case EI_TRIGGER_EVENT_NONE:
				break;
		}
	}
	if (seen_triggered) {
		return EI_TRIGGER_EVENT_TRIGGERED;
	}
	if (seen_ongoing) {
		return EI_TRIGGER_EVENT_ONGOING;
	}
	if (seen_started) {
		return EI_TRIGGER_EVENT_STARTED;
	}
	if (seen_completed) {
		return EI_TRIGGER_EVENT_COMPLETED;
	}
	if (seen_canceled) {
		return EI_TRIGGER_EVENT_CANCELED;
	}
	return EI_TRIGGER_EVENT_NONE;
}

// ---------------------------------------------------------------------------
// Fire event (callable invocation)
// ---------------------------------------------------------------------------

void EISubsystem::_fire_event(const Ref<EIAction> &p_action, ETriggerEvent p_event, const EIValue &p_value) {
	// Look up bindings for (action, event).
	const HashMap<Ref<EIAction>, HashMap<ETriggerEvent, Vector<ComponentBinding>>>::ConstIterator ait = _bindings.find(p_action);
	if (!ait) {
		return;
	}
	const HashMap<ETriggerEvent, Vector<ComponentBinding>> &event_map = ait->value;
	const HashMap<ETriggerEvent, Vector<ComponentBinding>>::ConstIterator eit = event_map.find(p_event);
	if (!eit) {
		return;
	}
	const Vector<ComponentBinding> &list = eit->value;
	for (int i = 0; i < list.size(); i++) {
		const Callable &cb = list[i].callable;
		// Skip Callables whose target Object was freed. Per spec
		// §4.7: "If a bound Callable becomes invalid (target freed),
		// the system SHALL silently skip it without spamming errors."
		if (!cb.is_valid()) {
			continue;
		}
		// Call with (action, event, value), degrading to fewer args so
		// 0/1/2-arg callbacks also work. Godot does NOT silently truncate
		// extra args for GDScript methods: passing too many is a hard
		// CALL_ERROR_TOO_MANY_ARGUMENTS and the body never runs. So we
		// retry with a smaller argc until the callee accepts the count
		// (or we hit a different error, e.g. a real exception inside it).
		const Variant v_action = p_action;
		const Variant v_event = static_cast<int64_t>(p_event);
		const Variant v_value = p_value.to_variant();
		const Variant *args[3] = { &v_action, &v_event, &v_value };
		Variant ret;
		Callable::CallError err;
		for (int argc = 3; argc >= 0; argc--) {
			cb.callp(args, argc, ret, err);
			if (err.error != Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
				break;
			}
		}
		// TOO_FEW / INVALID_ARGUMENT can still happen for genuinely
		// mismatched callbacks; those are tolerated. Other errors are
		// logged at verbose level only.
		if (err.error != Callable::CallError::CALL_OK &&
				err.error != Callable::CallError::CALL_ERROR_INVALID_METHOD &&
				err.error != Callable::CallError::CALL_ERROR_INVALID_ARGUMENT &&
				err.error != Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS &&
				err.error != Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
			if (_log_level >= 2) {
				print_line("[EI] _fire_event: callable call error code=", (int)err.error);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Dispatch (the main loop)
// ---------------------------------------------------------------------------

void EISubsystem::_dispatch_event(const Ref<InputEvent> &p_event) {
	if (_log_level >= 1) {
		print_line("[EI] dispatch: ", p_event);
	}

	// Per spec §4.6: walk contexts in priority order, and within
	// each context, walk mappings in declaration order. The first
	// matching mapping per context "wins"; lower-priority contexts
	// still get a chance unless a higher-priority mapping's
	// `consumes=true` blocks them.
	bool consumed = false;
	for (int ci = 0; ci < _contexts.size(); ci++) {
		if (consumed) {
			break; // a higher-priority consumes=true short-circuits
		}
		const ContextEntry &entry = _contexts[ci];
		Ref<EIMappingContext> ctx = entry.ctx;
		if (ctx.is_null()) {
			continue;
		}
		const Vector<EIMappingContext::Mapping> &mappings = ctx->get_mappings_internal();
		for (int mi = 0; mi < mappings.size(); mi++) {
			const EIMappingContext::Mapping &m = mappings[mi];
			if (m.event.is_null() || m.action.is_null()) {
				continue;
			}
			// is_match(): Godot 4.7 InputEvent has a virtual
			// `is_match(event, exact)` that subclasses override
			// (InputEventKey matches on keycode+modifiers, etc.).
			// exact=false tolerates extra modifiers on the incoming
			// event, matching the dispatcher semantics in spec
			// §4.6 step 1.
			if (!m.event->is_match(p_event, /*exact=*/false)) {
				continue;
			}

			// Found a matching mapping. Sample the raw value,
			// apply the modifier chain, then evaluate triggers.
			ActionRuntime &rt = _get_or_create_runtime(m.action);
			const EIAction::ValueType vt = m.action->get_value_type();
			EIValue raw = EIInputEventSampler::sample(p_event, vt);
			EIValue v = _apply_modifier_chain(m.action, m, raw);

			// Determine the `pressed` flag for trigger update. The
			// sampler naturally gives a non-zero value on press and
			// zero on release; we use that to derive "pressed".
			const bool pressed = !v.is_zero();

			// Update the state transition.
			rt.previous_value = rt.current_value;
			rt.raw_value = raw;
			rt.current_value = v;
			const bool was_held = rt.is_held;
			rt.is_held = pressed;
			if (pressed && !was_held) {
				rt.held_since_frame = _frame_counter;
			}

			// Ensure the trigger chain is built (lazy) from the action's
			// default_triggers.
			if (rt.triggers.is_empty()) {
				_ensure_action_triggers(rt, m.action);
			}

			// Merge this mapping's per-mapping triggers into the runtime
			// chain, deduped by trigger ref, so they ALSO advance on the
			// per-frame tick (see _process_triggers), not only on event
			// arrival. Without this a per-mapping Hold/Pulse/Tap would
			// never reach its time threshold. Position relative to the
			// defaults is irrelevant: _aggregate_events is priority-based.
			for (int ti = 0; ti < m.triggers.size(); ti++) {
				Ref<EITrigger> mt = m.triggers[ti];
				if (mt.is_null() || rt.trigger_states.has(mt)) {
					continue;
				}
				rt.triggers.push_back(mt);
				rt.trigger_states.insert(mt, EITrigger::TriggerRuntimeState());
			}

			// Refresh chord membership before evaluating so any
			// EITriggerChord in the chain sees this frame's active flags.
			_refresh_chord_states();

			// Evaluate the whole chain (defaults + per-mapping) and aggregate.
			Vector<ETriggerEvent> evs;
			for (int ti = 0; ti < rt.triggers.size(); ti++) {
				Ref<EITrigger> t = rt.triggers[ti];
				if (t.is_null()) {
					continue;
				}
				HashMap<Ref<EITrigger>, EITrigger::TriggerRuntimeState>::Iterator sit = rt.trigger_states.find(t);
				if (!sit) {
					EITrigger::TriggerRuntimeState s;
					rt.trigger_states.insert(t, s);
					sit = rt.trigger_states.find(t);
				}
				const EITrigger::UpdateResult ur = t->update_state(sit->value, v, 0.0, true, pressed);
				evs.push_back(static_cast<ETriggerEvent>(ur));
			}

			const ETriggerEvent agg = _aggregate_events(evs);
			if (agg != EI_TRIGGER_EVENT_NONE) {
				rt.previous_event = rt.last_event;
				rt.last_event = agg;
				rt.last_event_frame = _frame_counter;
				_fire_event(m.action, agg, v);
			}

			// Consume short-circuit: this mapping stops lower-priority
			// contexts from seeing the same event.
			if (m.consumes) {
				consumed = true;
			}
			// Spec §6.2: "first matching mapping wins; the rest are
			// skipped in that context for that event." Break out
			// of the inner mapping loop.
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Per-frame trigger tick
// ---------------------------------------------------------------------------

void EISubsystem::_process_triggers(double p_delta) {
	// For every action with state, run each trigger with
	// (state, current_value, dt, event_valid=false, pressed=is_held).
	// Aggregate and fire any non-NONE results.
	// Refresh chord membership first so EITriggerChord ticks see the
	// current is_held state of their member actions.
	_refresh_chord_states();
	for (KeyValue<Ref<EIAction>, ActionRuntime> &kv : _action_runtimes) {
		ActionRuntime &rt = kv.value;
		if (rt.triggers.is_empty()) {
			continue;
		}
		Vector<ETriggerEvent> evs;
		for (int ti = 0; ti < rt.triggers.size(); ti++) {
			Ref<EITrigger> t = rt.triggers[ti];
			if (t.is_null()) {
				continue;
			}
			HashMap<Ref<EITrigger>, EITrigger::TriggerRuntimeState>::Iterator sit = rt.trigger_states.find(t);
			if (!sit) {
				EITrigger::TriggerRuntimeState s;
				rt.trigger_states.insert(t, s);
				sit = rt.trigger_states.find(t);
			}
			const EITrigger::UpdateResult ur = t->update_state(sit->value, rt.current_value, p_delta, false, rt.is_held);
			evs.push_back(static_cast<ETriggerEvent>(ur));
		}
		const ETriggerEvent agg = _aggregate_events(evs);
		if (agg != EI_TRIGGER_EVENT_NONE) {
			rt.previous_event = rt.last_event;
			rt.last_event = agg;
			rt.last_event_frame = _frame_counter;
			_fire_event(kv.key, agg, rt.current_value);
		}
	}
}

// ---------------------------------------------------------------------------
// Application focus cancel (spec §6.0)
// ---------------------------------------------------------------------------

bool EISubsystem::is_application_focused() const {
	return _app_focused;
}

void EISubsystem::_on_application_focus_changed(bool p_focused) {
	_app_focused = p_focused;
	if (!p_focused) {
		_cancel_all_held();
	}
}

void EISubsystem::_cancel_all_held() {
	// Spec §6.0: on focus loss, all `is_held == true` actions fire
	// CANCELED and reset to zero. No phantom Completed events.
	for (KeyValue<Ref<EIAction>, ActionRuntime> &kv : _action_runtimes) {
		ActionRuntime &rt = kv.value;
		if (rt.is_held) {
			// Reset all trigger states to fresh starts so a
			// subsequent press doesn't think we mid-action.
			for (KeyValue<Ref<EITrigger>, EITrigger::TriggerRuntimeState> &ts : rt.trigger_states) {
				ts.value = EITrigger::TriggerRuntimeState();
			}
			rt.previous_value = rt.current_value;
			rt.current_value = EIValue::make_bool(false);
			rt.is_held = false;
			rt.previous_event = rt.last_event;
			rt.last_event = EI_TRIGGER_EVENT_CANCELED;
			rt.last_event_frame = _frame_counter;
			_fire_event(kv.key, EI_TRIGGER_EVENT_CANCELED, rt.current_value);
		}
	}
}

// ---------------------------------------------------------------------------
// Chord membership refresh
// ---------------------------------------------------------------------------

void EISubsystem::_refresh_chord_states() {
	// EITriggerChord can't reach the subsystem from update_state(), so we
	// feed it here: push each chord member action's current is_held state
	// into every EITriggerChord found in any action's trigger chain. A full
	// overwrite each call keeps shared chord resources from going stale.
	for (KeyValue<Ref<EIAction>, ActionRuntime> &kv : _action_runtimes) {
		const Vector<Ref<EITrigger>> &chain = kv.value.triggers;
		for (int ti = 0; ti < chain.size(); ti++) {
			EITriggerChord *chord = Object::cast_to<EITriggerChord>(chain[ti].ptr());
			if (chord == nullptr) {
				continue;
			}
			const int n = chord->get_chord_action_count();
			for (int ci = 0; ci < n; ci++) {
				Ref<EIAction> ca = chord->get_chord_action(ci);
				if (ca.is_null()) {
					continue;
				}
				const HashMap<Ref<EIAction>, ActionRuntime>::Iterator cit = _action_runtimes.find(ca);
				chord->set_chord_action_active(ca, cit && cit->value.is_held);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void EISubsystem::set_log_level(int p_level) {
	_log_level = p_level;
}

int EISubsystem::get_log_level() const {
	return _log_level;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void EISubsystem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// Enable the per-frame internal process tick. Without this,
			// NOTIFICATION_INTERNAL_PROCESS is never delivered and all
			// time-based triggers (Hold, Pulse, Tap/DoubleTap timeouts)
			// would be frozen at runtime.
			set_process_internal(true);
			if (_log_level >= 1) {
				print_line("[EI] Subsystem ready (log_level=", _log_level, ")");
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (_log_level >= 2) {
				print_line("[EI] _process tick");
			}
			// Per spec §4.6: tick all active triggers with
			// p_event_valid=false so EITriggerHold / EITriggerPulse advance
			// between key events. Use the real frame delta so thresholds
			// are wall-clock accurate regardless of frame rate. Tests call
			// tick() directly with a controlled dt instead.
			_frame_counter++;
			_process_triggers(get_process_delta_time());
		} break;
		case NOTIFICATION_APPLICATION_FOCUS_OUT: {
			// Spec §6.0: on focus loss, cancel all in-progress triggers
			// and reset held actions to zero. SceneTree forwards this
			// MainLoop notification to every node, so no explicit signal
			// connection is needed.
			_on_application_focus_changed(false);
		} break;
		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			_on_application_focus_changed(true);
		} break;
		case NOTIFICATION_WM_CLOSE_REQUEST: {
			if (_log_level >= 1) {
				print_line("[EI] WM close request");
			}
		} break;
		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// Method binding
// ---------------------------------------------------------------------------

void EISubsystem::_bind_methods() {
	// IMC management.
	ClassDB::bind_method(D_METHOD("add_mapping_context", "context", "priority"), &EISubsystem::add_mapping_context);
	ClassDB::bind_method(D_METHOD("remove_mapping_context", "context"), &EISubsystem::remove_mapping_context);
	ClassDB::bind_method(D_METHOD("clear_all_mapping_contexts"), &EISubsystem::clear_all_mapping_contexts);
	ClassDB::bind_method(D_METHOD("has_mapping_context", "context"), &EISubsystem::has_mapping_context);

	// Bindings (P5c — bound now so .tres / GDScript can call them
	// once P5c lands the body; for P5b they're no-ops).
	ClassDB::bind_method(D_METHOD("bind_action", "action", "event", "callable"), &EISubsystem::bind_action);
	ClassDB::bind_method(D_METHOD("unbind_action", "action", "event", "callable"), &EISubsystem::unbind_action);
	ClassDB::bind_method(D_METHOD("clear_bindings"), &EISubsystem::clear_bindings);

	// Input + tick.
	ClassDB::bind_method(D_METHOD("inject_input", "event"), &EISubsystem::inject_input);
	ClassDB::bind_method(D_METHOD("tick", "delta"), &EISubsystem::tick);

	// State queries. C++ callers use get_action_value() (typed
	// EIValue return); GDScript uses get_action_value_variant()
	// because EIValue isn't a Variant-compatible type.
	ClassDB::bind_method(D_METHOD("get_action_value_variant", "action"), &EISubsystem::get_action_value_variant);
	ClassDB::bind_method(D_METHOD("is_action_active", "action"), &EISubsystem::is_action_active);
	ClassDB::bind_method(D_METHOD("get_action_trigger_event", "action"), &EISubsystem::get_action_trigger_event);

	// Diagnostics.
	ClassDB::bind_method(D_METHOD("set_log_level", "level"), &EISubsystem::set_log_level);
	ClassDB::bind_method(D_METHOD("get_log_level"), &EISubsystem::get_log_level);
	ClassDB::bind_method(D_METHOD("is_application_focused"), &EISubsystem::is_application_focused);
	ClassDB::bind_method(D_METHOD("_on_application_focus_changed", "focused"), &EISubsystem::_on_application_focus_changed);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "log_level"), "set_log_level", "get_log_level");

	// Expose the dispatch trigger-event enum to GDScript. It lives in the
	// `ei` namespace (not as a member of any class), so without this the
	// constants are invisible to script and `bind_action(...)` callers have
	// no symbolic name to pass. Reachable as EISubsystem.EI_TRIGGER_EVENT_*.
	BIND_ENUM_CONSTANT(EI_TRIGGER_EVENT_NONE);
	BIND_ENUM_CONSTANT(EI_TRIGGER_EVENT_STARTED);
	BIND_ENUM_CONSTANT(EI_TRIGGER_EVENT_TRIGGERED);
	BIND_ENUM_CONSTANT(EI_TRIGGER_EVENT_ONGOING);
	BIND_ENUM_CONSTANT(EI_TRIGGER_EVENT_COMPLETED);
	BIND_ENUM_CONSTANT(EI_TRIGGER_EVENT_CANCELED);
}

} // namespace ei

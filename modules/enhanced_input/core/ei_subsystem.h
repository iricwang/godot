/**************************************************************************/
/*  ei_subsystem.h                                                        */
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

// Sibling includes (same `core/` directory) for the EI headers — see
// ei_modifier.h / ei_action.h for the rationale. Lets this header
// resolve from both in-module sources and tests/. The engine headers
// (core/object/..., scene/main/...) stay module-root because the
// engine's CPPATH is set via SCsub's `Dir("#")` and resolves them.
#include "core/input/input_event.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"
#include "core/variant/typed_array.h"
#include "ei_action.h"
#include "ei_enums.h"
#include "ei_input_event_sampler.h"
#include "ei_mapping_context.h"
#include "ei_modifier.h"
#include "ei_trigger.h"
#include "ei_value.h"
#include "scene/main/node.h"

namespace ei {

// The central dispatcher + state holder for the Enhanced Input system.
//
// Per spec §4.6 (enhanced-input.md v0.2):
// * Singleton (autoload-style) — one per SceneTree. The C++ instance
//   is created by the GDScript autoload (`ei_autoload.gd`); see the
//   file header in register_types.cpp for the lifecycle rationale.
// * Owns the active IMCs (priority-ordered), per-action runtime state,
//   trigger evaluation, and event dispatch.
// * `_input` and `_process` are the Godot lifecycle hooks; the
//   autoload wires them up. Tests use the public `tick()` and the
//   already-existing `inject_input()` entry points.
//
// Threading: the subsystem is single-threaded by design (Godot's main
// thread). All state mutations happen on the dispatch / tick paths.

class EISubsystem : public Node {
	GDCLASS(EISubsystem, Node);

public:
	static EISubsystem *get_singleton();

	EISubsystem();
	~EISubsystem();

private:
	// Module-owned singleton pointer. Set in the constructor, cleared
	// in the destructor. The GDScript autoload retrieves the live
	// instance via `Engine.get_singleton("EISubsystem")` which the
	// autoload shim populates after `ClassDB.instantiate()`.
	static EISubsystem *singleton;

public:

	// -------------------------------------------------------------------
	// IMC management
	// -------------------------------------------------------------------

	// Register an IMC. `p_priority` follows UE semantics: higher value
	// is matched first; ties broken by insertion order. Re-adding the
	// same IMC with a new priority updates the existing entry.
	void add_mapping_context(const Ref<EIMappingContext> &p_ctx, int p_priority);
	void remove_mapping_context(const Ref<EIMappingContext> &p_ctx);
	void clear_all_mapping_contexts();
	bool has_mapping_context(const Ref<EIMappingContext> &p_ctx) const;

	// -------------------------------------------------------------------
	// Bindings (P5c — declared here so _bind_methods can expose them)
	// -------------------------------------------------------------------

	// Bind a Callable to a (action, trigger_event) pair. Idempotent:
	// adding the same (action, event, callable) twice is a no-op.
	void bind_action(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
	void unbind_action(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
	void clear_bindings();

	// -------------------------------------------------------------------
	// Programmatic input + state queries
	// -------------------------------------------------------------------

	// Inject a single input event. The autoload forwards OS-level
	// InputEvents here. The dispatcher walks the IMCs in priority
	// order, finds the first matching mapping per context, applies
	// the modifier chain, evaluates triggers, updates action state,
	// and fires any bound Callables.
	void inject_input(const Ref<InputEvent> &p_event);

	// Per-frame tick. Drives time-based triggers (Hold, Pulse). The
	// autoload's `_process` calls this with the current frame delta;
	// tests call it directly with a controlled dt.
	void tick(double p_delta);

	// State queries. Per spec §4.6: these read the post-modifier
	// current_value / last_event from the action's ActionRuntime.
	EIValue get_action_value(const Ref<EIAction> &p_action) const;
	bool is_action_active(const Ref<EIAction> &p_action) const;
	ETriggerEvent get_action_trigger_event(const Ref<EIAction> &p_action) const;

	// Variant-returning wrapper for GDScript callers. EIValue is a
	// plain C++ struct that ClassDB doesn't know about, so we expose
	// a separate method that does the to_variant() conversion. The
	// C++ API (above) keeps the typed return for performance.
	Variant get_action_value_variant(const Ref<EIAction> &p_action) const;

	// -------------------------------------------------------------------
	// Diagnostics
	// -------------------------------------------------------------------

	void set_log_level(int p_level);
	int get_log_level() const;

	// True while the main window is focused. `false` cancels all
	// in-progress triggers per spec §6.0. Updated by the application
	// focus signal hook in `_ready`.
	bool is_application_focused() const;

	// P6+ public surface stub. P5c wires the actual signal connection.
	void _on_application_focus_changed(bool p_focused);

protected:
	static void _bind_methods();

	// Godot lifecycle hook. C++ Node subclasses can't override
	// `_input` / `_ready` / `_process` directly; the proper C++
	// override point is `Object::_notification(int p_what)`. We use
	// NOTIFICATION_INTERNAL_PROCESS for the per-frame tick and
	// NOTIFICATION_READY to log subsystem startup.
	void _notification(int p_what);

private:
	// -------------------------------------------------------------------
	// IMC ordering
	// -------------------------------------------------------------------

	struct ContextEntry {
		Ref<EIMappingContext> ctx;
		int priority = 0;
		int order = 0; // insertion order; tiebreaker when priority is equal
	};

	// All registered IMCs. `add_mapping_context` appends; reads sort
	// by (priority DESC, order ASC).
	Vector<ContextEntry> _contexts;
	int _context_seq = 0;

	// -------------------------------------------------------------------
	// Bindings
	// -------------------------------------------------------------------

	struct ComponentBinding {
		Callable callable;
	};

	// Lookup: action → (event → list of Callables). Multi-level hash
	// keeps the dispatch hot path tight (skip empty buckets, then walk
	// the per-event list). See spec §4.6 `_bindings`.
	HashMap<Ref<EIAction>, HashMap<ETriggerEvent, Vector<ComponentBinding>>> _bindings;

	// -------------------------------------------------------------------
	// Per-action runtime state
	// -------------------------------------------------------------------

	struct ActionRuntime {
		// Post-modifier value as of the last dispatch. This is what
		// get_action_value() returns.
		EIValue current_value;

		// Pre-modifier (raw sampler output) from the last dispatch.
		// Useful for the Release trigger's "non-zero -> zero" edge
		// detection (it uses current_value via is_zero, but having
		// the raw around is convenient for diagnostics).
		EIValue raw_value;

		// Snapshot of the prior frame's value. Used by Release
		// trigger; also lets `is_held` correctly handle the
		// transition edge.
		EIValue previous_value;

		// Convenience: true when current_value is non-zero.
		bool is_held = false;

		// Frame counter (Engine::get_process_frames()) at which the
		// action first became non-zero. Used for held-since queries
		// (not yet exposed in P5b but kept for forward-compat).
		uint64_t held_since_frame = 0;

		// The most recent ETriggerEvent this action fired. NONE
		// means "never fired". See spec §4.6: get_action_trigger_event
		// returns this.
		ETriggerEvent last_event = EI_TRIGGER_EVENT_NONE;
		ETriggerEvent previous_event = EI_TRIGGER_EVENT_NONE;
		uint64_t last_event_frame = 0;

		// Trigger chain for this action. Computed lazily on first
		// dispatch as the union of per-mapping triggers + the
		// action's default_triggers (in that order). The trigger
		// runtime state is per-(action, trigger) — see
		// spec §4.4 TriggerRuntimeState.
		Vector<Ref<EITrigger>> triggers;
		HashMap<Ref<EITrigger>, EITrigger::TriggerRuntimeState> trigger_states;
	};

	HashMap<Ref<EIAction>, ActionRuntime> _action_runtimes;

	// -------------------------------------------------------------------
	// Frame counter for ActionRuntime timing
	// -------------------------------------------------------------------

	uint64_t _frame_counter = 0;

	// True while the main window has focus. spec §6.0: on focus
	// loss, all held actions fire CANCELED and reset to zero.
	bool _app_focused = true;

	int _log_level = 0; // 0=silent, 1=event, 2=verbose

	// -------------------------------------------------------------------
	// Dispatch internals
	// -------------------------------------------------------------------

	// Dispatch a single event through the IMCs. Called by
	// inject_input and by `_notification(NOTIFICATION_INPUT)` (P6+).
	// P5b uses the public entry point only.
	void _dispatch_event(const Ref<InputEvent> &p_event);

	// Walk each action's trigger chain, ticking each trigger with
	// (state, current_value, dt, event_valid=false, pressed=is_held).
	// Fires any non-NONE results after priority aggregation.
	void _process_triggers(double p_delta);

	// Look up the (lazily-built) trigger chain for an action, cache
	// it in ActionRuntime, and return it. The chain is the union of
	// (mapping_0.triggers, mapping_1.triggers, ..., action.default_triggers)
	// for the active mappings on this action. Per spec §4.6, the
	// trigger ordering is per-mapping-then-default.
	void _ensure_action_triggers(ActionRuntime &p_runtime, const Ref<EIAction> &p_action);

	// Apply per-mapping modifiers then action.default_modifiers in
	// order (spec §4.6 / §6.2 modifier chain order).
	EIValue _apply_modifier_chain(const Ref<EIAction> &p_action,
			const EIMappingContext::Mapping &p_mapping,
			const EIValue &p_raw_value);

	// Aggregate multiple UpdateResults into a single ETriggerEvent
	// using the spec §4.6 priority order:
	//   TRIGGERED > ONGOING > STARTED > COMPLETED > CANCELED > NONE
	static ETriggerEvent _aggregate_events(const Vector<ETriggerEvent> &p_events);

	// Fire a (action, event) on all bound Callables for that pair,
	// then update ActionRuntime.{last_event, previous_event, ...}.
	// Silently skips Callables whose target Object was freed.
	void _fire_event(const Ref<EIAction> &p_action, ETriggerEvent p_event, const EIValue &p_value);

	// Cancel all in-progress triggers and reset held actions to zero.
	// Called on application focus loss (spec §6.0).
	void _cancel_all_held();

	// Get-or-create the ActionRuntime for an action. Used by the
	// dispatch path. Returns a reference so callers can mutate in
	// place.
	ActionRuntime &_get_or_create_runtime(const Ref<EIAction> &p_action);

	// Sort `_contexts` by (priority DESC, order ASC) before each
	// dispatch. Stable for contexts with equal priority. The list
	// is small (handful of IMCs) so an insertion sort is fine; we
	// re-sort only on add/remove/clear.
	void _sort_contexts();
};

} // namespace ei

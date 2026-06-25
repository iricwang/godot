/**************************************************************************/
/*  test_ei_subsystem.h                                                    */
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

// Module-relative includes so the test compiles from tests/ without
// the module's CPPATH.
#include "../core/ei_action.h"
#include "../core/ei_mapping_context.h"
#include "../core/ei_modifier.h"
#include "../core/ei_subsystem.h"
#include "../core/ei_trigger.h"
#include "../core/ei_value.h"
#include "../modifiers/ei_modifier_negate.h"
#include "../modifiers/ei_modifier_scale.h"
#include "../triggers/ei_trigger_pressed.h"
#include "../triggers/ei_trigger_hold.h"
#include "../triggers/ei_trigger_release.h"
#include "../triggers/ei_trigger_chord.h"

#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "tests/test_macros.h"

namespace TestEISubsystem {

using namespace ei;

static Ref<InputEventKey> make_key(Key p_keycode, bool p_pressed) {
	Ref<InputEventKey> k;
	k.instantiate();
	k->set_keycode(p_keycode);
	k->set_pressed(p_pressed);
	return k;
}

// ---------------------------------------------------------------------------
// Setup helpers — keep tests focused on the spec scenario, not boilerplate.
// ---------------------------------------------------------------------------

// A standalone EISubsystem owned by a raw pointer (Node is not
// RefCounted, so we manage lifetime manually). Created fresh in
// each test so the singleton/static state from prior tests can't
// leak in. The dtor of EISubsystem clears the static singleton
// pointer, so back-to-back tests just overwrite it.
struct Fixture {
	EISubsystem *sub = nullptr;

	Fixture() {
		sub = memnew(EISubsystem);
	}
	~Fixture() {
		memdelete(sub);
		sub = nullptr;
	}
};

// Create a Bool action and return the Ref.
static Ref<EIAction> make_bool_action(const String &p_desc = "") {
	Ref<EIAction> a;
	a.instantiate();
	a->set_value_type(EIAction::VALUE_TYPE_BOOL);
	if (!p_desc.is_empty()) {
		a->set_description(p_desc);
	}
	return a;
}

// Create a mapping context with one (key, action) mapping and the
// given consumes flag.
static Ref<EIMappingContext> make_imc(const Ref<InputEvent> &p_event, const Ref<EIAction> &p_action, bool p_consumes = true) {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	ctx->add_mapping(p_event, p_action, TypedArray<EIModifier>(), TypedArray<EITrigger>(), p_consumes);
	return ctx;
}

// ---------------------------------------------------------------------------
// IMC management
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] add_mapping_context / has / remove / clear") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	Ref<EIMappingContext> ctx = make_imc(make_key(Key::SPACE, true), a);

	CHECK_FALSE(f.sub->has_mapping_context(ctx));
	f.sub->add_mapping_context(ctx, 0);
	CHECK(f.sub->has_mapping_context(ctx));

	// Re-adding is idempotent (no duplicate entry).
	f.sub->add_mapping_context(ctx, 0);
	CHECK(f.sub->has_mapping_context(ctx));

	// Remove + verify gone.
	f.sub->remove_mapping_context(ctx);
	CHECK_FALSE(f.sub->has_mapping_context(ctx));

	// Re-adding with a different priority is allowed.
	f.sub->add_mapping_context(ctx, 10);
	CHECK(f.sub->has_mapping_context(ctx));

	// Null ctx is silently ignored.
	Ref<EIMappingContext> null_ctx;
	f.sub->add_mapping_context(null_ctx, 5);
	f.sub->remove_mapping_context(null_ctx);
	// No crash; nothing changed.

	// Clear all.
	f.sub->clear_all_mapping_contexts();
	CHECK_FALSE(f.sub->has_mapping_context(ctx));
}

// ---------------------------------------------------------------------------
// Dispatch: key press routes to action, value + event updated
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] key press dispatches to action and updates state") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	// Default Pressed trigger so events fire (with no trigger, the
	// dispatcher correctly emits no events per spec §4.6 — events
	// only come from triggers returning non-NONE).
	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	// Pre-state: no value, not active, NONE event.
	CHECK(f.sub->is_action_active(a) == false);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_NONE);

	// Dispatch a press.
	f.sub->inject_input(space);

	// After press: action active (current_value != zero), Pressed
	// returned STARTED on the rising edge, aggregate picks STARTED.
	CHECK(f.sub->is_action_active(a) == true);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_STARTED);
	// Value should be Bool(true) for a Bool action.
	EIValue v = f.sub->get_action_value(a);
	CHECK(v.get_type() == EIValue::TYPE_BOOL);
	CHECK(v.get_bool() == true);

	// Dispatch a release — Pressed returns COMPLETED.
	Ref<InputEventKey> space_release = make_key(Key::SPACE, false);
	f.sub->inject_input(space_release);
	CHECK(f.sub->is_action_active(a) == false);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_COMPLETED);
}

TEST_CASE("[EnhancedInput][Subsystem] null / unregistered event is a no-op") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	// No IMCs registered.
	f.sub->inject_input(make_key(Key::SPACE, true));
	// No mappings -> no state change.
	CHECK(f.sub->is_action_active(a) == false);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_NONE);
}

TEST_CASE("[EnhancedInput][Subsystem] event that doesn't match any mapping is a no-op") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	// Press A — no mapping for A.
	f.sub->inject_input(make_key(Key::A, true));
	CHECK(f.sub->is_action_active(a) == false);
}

// ---------------------------------------------------------------------------
// Context priority ordering
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] higher-priority context matches first") {
	Fixture f;
	Ref<EIAction> menu_a = make_bool_action("menu_a");
	Ref<EIAction> gameplay_a = make_bool_action("gameplay_a");

	// Two contexts both bound to SPACE, with different priorities.
	Ref<EIMappingContext> menu_ctx = make_imc(make_key(Key::SPACE, true), menu_a);
	Ref<EIMappingContext> game_ctx = make_imc(make_key(Key::SPACE, true), gameplay_a);

	f.sub->add_mapping_context(game_ctx, 0);
	f.sub->add_mapping_context(menu_ctx, 10);

	// Press SPACE — menu (priority 10) matches first.
	f.sub->inject_input(make_key(Key::SPACE, true));

	CHECK(f.sub->is_action_active(menu_a) == true);
	CHECK(f.sub->is_action_active(gameplay_a) == false);
}

TEST_CASE("[EnhancedInput][Subsystem] spec scenario: Menu>Gameplay, fall-through when Menu has no mapping") {
	Fixture f;
	Ref<EIAction> jump = make_bool_action("jump");

	// Menu context has a mapping for ESC, not for SPACE.
	Ref<EIAction> pause = make_bool_action("pause");
	Ref<EIMappingContext> menu_ctx = make_imc(make_key(Key::ESCAPE, true), pause);

	// Gameplay context has a mapping for SPACE.
	Ref<EIMappingContext> game_ctx = make_imc(make_key(Key::SPACE, true), jump);

	f.sub->add_mapping_context(game_ctx, 0);
	f.sub->add_mapping_context(menu_ctx, 10);

	// SPACE: menu has no match -> fall through to gameplay.
	f.sub->inject_input(make_key(Key::SPACE, true));
	CHECK(f.sub->is_action_active(jump) == true);
	CHECK(f.sub->is_action_active(pause) == false);

	// Reset for the next assertion.
	f.sub->inject_input(make_key(Key::SPACE, false));
	f.sub->clear_all_mapping_contexts();
	f.sub->add_mapping_context(game_ctx, 0);
	f.sub->add_mapping_context(menu_ctx, 10);

	// ESCAPE: menu matches -> gameplay doesn't see it.
	f.sub->inject_input(make_key(Key::ESCAPE, true));
	CHECK(f.sub->is_action_active(pause) == true);
	CHECK(f.sub->is_action_active(jump) == false);
}

TEST_CASE("[EnhancedInput][Subsystem] consumes=true short-circuits lower-priority contexts") {
	Fixture f;
	Ref<EIAction> menu_a = make_bool_action("menu_a");
	Ref<EIAction> game_a = make_bool_action("game_a");

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> menu_ctx = make_imc(space, menu_a, /*consumes=*/true);
	Ref<EIMappingContext> game_ctx = make_imc(space, game_a, /*consumes=*/true);

	f.sub->add_mapping_context(game_ctx, 0);
	f.sub->add_mapping_context(menu_ctx, 10);

	f.sub->inject_input(space);
	// menu wins; game does NOT see the event because menu consumed it.
	CHECK(f.sub->is_action_active(menu_a) == true);
	CHECK(f.sub->is_action_active(game_a) == false);
}

TEST_CASE("[EnhancedInput][Subsystem] consumes=false: lower-priority context also fires (layered UI)") {
	Fixture f;
	Ref<EIAction> menu_a = make_bool_action("menu_a");
	Ref<EIAction> game_a = make_bool_action("game_a");

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> menu_ctx = make_imc(space, menu_a, /*consumes=*/false);
	Ref<EIMappingContext> game_ctx = make_imc(space, game_a, /*consumes=*/true);

	f.sub->add_mapping_context(game_ctx, 0);
	f.sub->add_mapping_context(menu_ctx, 10);

	f.sub->inject_input(space);
	// menu fires AND game fires (menu doesn't consume).
	CHECK(f.sub->is_action_active(menu_a) == true);
	CHECK(f.sub->is_action_active(game_a) == true);
}

TEST_CASE("[EnhancedInput][Subsystem] first matching mapping wins per context") {
	Fixture f;
	Ref<EIAction> a1 = make_bool_action("a1");
	Ref<EIAction> a2 = make_bool_action("a2");

	// Same context, two mappings, both for SPACE.
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	ctx->add_mapping(space, a1, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	ctx->add_mapping(space, a2, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);

	f.sub->add_mapping_context(ctx, 0);
	f.sub->inject_input(space);

	// a1 wins (first match); a2 stays inactive.
	CHECK(f.sub->is_action_active(a1) == true);
	CHECK(f.sub->is_action_active(a2) == false);
}

// ---------------------------------------------------------------------------
// Modifier chain order
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] per-mapping modifier then action default modifier (Axis1D chain)") {
	Fixture f;
	Ref<EIAction> a;
	a.instantiate();
	a->set_value_type(EIAction::VALUE_TYPE_AXIS1D);

	// Per-mapping: Scale(2.0). Action default: Scale(0.5). Net: 1.0 -> 2.0 -> 1.0.
	Ref<EIModifierScale> per_map_scale;
	per_map_scale.instantiate();
	per_map_scale->set_scale_x(2.0f);

	Ref<EIModifierScale> default_scale;
	default_scale.instantiate();
	default_scale->set_scale_x(0.5f);

	TypedArray<EIModifier> per_map_mods;
	per_map_mods.push_back(per_map_scale);

	// Wrap the default scale in a TypedArray (set_default_modifiers
	// takes an array, not a single ref).
	TypedArray<EIModifier> default_mods;
	default_mods.push_back(default_scale);
	a->set_default_modifiers(default_mods);

	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	ctx->add_mapping(space, a, per_map_mods, TypedArray<EITrigger>(), true);

	f.sub->add_mapping_context(ctx, 0);
	f.sub->inject_input(space);

	// Expected chain: sampler (1.0) -> per-mapping Scale(2.0) -> 2.0
	//                 -> default Scale(0.5)    -> 1.0
	EIValue v = f.sub->get_action_value(a);
	CHECK(v.get_type() == EIValue::TYPE_AXIS1D);
	CHECK(Math::abs(v.get_axis1d() - 1.0f) < 1e-5f);
}

// ---------------------------------------------------------------------------
// Trigger chain & aggregation
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] default trigger on action: Pressed fires Started on press, Completed on release") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<EIMappingContext> ctx = make_imc(make_key(Key::SPACE, true), a);
	f.sub->add_mapping_context(ctx, 0);

	f.sub->inject_input(make_key(Key::SPACE, true));
	// On a single press event the Pressed trigger returns STARTED
	// (state NONE -> TRIGGERED); aggregate picks STARTED.
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_STARTED);

	f.sub->inject_input(make_key(Key::SPACE, false));
	// Release event: Pressed returns COMPLETED.
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_COMPLETED);
}

TEST_CASE("[EnhancedInput][Subsystem] time-based trigger (Hold) advances via tick()") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	// Hold(0.3s).
	Ref<EITriggerHold> hold;
	hold.instantiate();
	hold->set_hold_time_threshold(0.3);
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(hold);
	a->set_default_triggers(def_trigs);

	Ref<EIMappingContext> ctx = make_imc(make_key(Key::SPACE, true), a);
	f.sub->add_mapping_context(ctx, 0);

	// Press: STARTED.
	f.sub->inject_input(make_key(Key::SPACE, true));
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_STARTED);

	// Tick for 0.1s — below threshold, no event.
	f.sub->tick(0.1);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_STARTED);

	// Tick for another 0.2s — at threshold, TRIGGERED.
	f.sub->tick(0.2);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_TRIGGERED);

	// Release: COMPLETED.
	f.sub->inject_input(make_key(Key::SPACE, false));
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_COMPLETED);
}

TEST_CASE("[EnhancedInput][Subsystem] Release trigger fires on value transition (bool true -> false)") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	Ref<EITriggerRelease> release;
	release.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(release);
	a->set_default_triggers(def_trigs);

	Ref<EIMappingContext> ctx = make_imc(make_key(Key::SPACE, true), a);
	f.sub->add_mapping_context(ctx, 0);

	// Initial: value is zero, no event.
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_NONE);

	// Press — value becomes non-zero, but Release fires on the
	// non-zero -> zero transition, so no event yet.
	f.sub->inject_input(make_key(Key::SPACE, true));
	// The default Pressed-like behavior of Pressed on press fires
	// STARTED via the default; but here we have only Release.
	// Release on the rising edge: nothing.
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_NONE);

	// Release — value drops to zero: Release fires TRIGGERED.
	f.sub->inject_input(make_key(Key::SPACE, false));
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_TRIGGERED);
}

// ---------------------------------------------------------------------------
// State queries on a never-seen action return defaults
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] state queries for unknown / null action return safe defaults") {
	Fixture f;

	Ref<EIAction> never_seen = make_bool_action();
	CHECK(f.sub->is_action_active(never_seen) == false);
	CHECK(f.sub->get_action_trigger_event(never_seen) == EI_TRIGGER_EVENT_NONE);
	EIValue v = f.sub->get_action_value(never_seen);
	CHECK(v.get_type() == EIValue::TYPE_BOOL);
	CHECK(v.get_bool() == false);

	Ref<EIAction> null_action;
	CHECK(f.sub->is_action_active(null_action) == false);
	CHECK(f.sub->get_action_trigger_event(null_action) == EI_TRIGGER_EVENT_NONE);
	CHECK(f.sub->get_action_value(null_action).get_type() == EIValue::TYPE_BOOL);
}

// ---------------------------------------------------------------------------
// Application focus cancel (spec §6.0)
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] focus loss cancels all held actions") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	Ref<EIMappingContext> ctx = make_imc(make_key(Key::SPACE, true), a);
	f.sub->add_mapping_context(ctx, 0);

	// Press: held.
	f.sub->inject_input(make_key(Key::SPACE, true));
	CHECK(f.sub->is_action_active(a) == true);

	// Simulate focus loss.
	CHECK(f.sub->is_application_focused() == true);
	f.sub->_on_application_focus_changed(false);
	CHECK(f.sub->is_application_focused() == false);

	// After focus loss, the action is no longer held and the
	// last event is CANCELED.
	CHECK(f.sub->is_action_active(a) == false);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_CANCELED);

	// Focus regained — nothing fires.
	f.sub->_on_application_focus_changed(true);
	CHECK(f.sub->is_application_focused() == true);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_CANCELED);
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] log_level round-trips through setter") {
	Fixture f;
	f.sub->set_log_level(0);
	CHECK(f.sub->get_log_level() == 0);
	f.sub->set_log_level(2);
	CHECK(f.sub->get_log_level() == 2);
}

// ---------------------------------------------------------------------------
// Bindings (P5c)
// ---------------------------------------------------------------------------

// A tiny counter object the test Callable can mutate. The Callable
// must point at a method on a stable object, so we use a heap-allocated
// instance tracked by the test's local scope.
class CallbackSpy : public Object {
	GDCLASS(CallbackSpy, Object);

public:
	int call_count = 0;
	int zero_call_count = 0; // bumped by the 0-arg callback below
	int last_event_seen = -1; // raw int form of the ETriggerEvent
	Vector3 last_value_axis3d; // for inspecting Axis3D passthroughs
	EIValue last_value_ei; // typed EIValue from the dispatcher

	void on_event(const Ref<EIAction> &p_action, int p_event, const Variant &p_value) {
		call_count++;
		last_event_seen = p_event;
		// Reconstruct the EIValue from the Variant so the test can
		// assert on the typed value.
		if (p_value.get_type() == Variant::BOOL) {
			last_value_ei = EIValue::make_bool((bool)p_value);
		} else if (p_value.get_type() == Variant::FLOAT || p_value.get_type() == Variant::INT) {
			last_value_ei = EIValue::make_axis1d((float)p_value);
		} else if (p_value.get_type() == Variant::VECTOR2) {
			last_value_ei = EIValue::make_axis2d((Vector2)p_value);
		} else if (p_value.get_type() == Variant::VECTOR3) {
			last_value_ei = EIValue::make_axis3d((Vector3)p_value);
			last_value_axis3d = (Vector3)p_value;
		}
		(void)p_action;
	}

	// Zero-argument callback — the shape every FortySix Activity uses.
	// The dispatcher fires with (action, event, value); a 0-arg method
	// rejects the extra args (TOO_MANY) unless the dispatcher degrades
	// the argc, so this guards the _fire_event arity fix.
	void on_event_zero() {
		zero_call_count++;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("on_event", "action", "event", "value"), &CallbackSpy::on_event);
		ClassDB::bind_method(D_METHOD("on_event_zero"), &CallbackSpy::on_event_zero);
	}
};

TEST_CASE("[EnhancedInput][Subsystem] bind_action fires the bound Callable on trigger events") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	// Default Pressed trigger.
	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	// Spy + bind.
	CallbackSpy *spy = memnew(CallbackSpy);
	Callable cb = Callable(spy, "on_event");
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, cb);
	f.sub->bind_action(a, EI_TRIGGER_EVENT_COMPLETED, cb);

	// Press: Pressed returns STARTED, aggregate fires STARTED.
	f.sub->inject_input(space);
	CHECK(spy->call_count == 1);
	CHECK(spy->last_event_seen == (int)EI_TRIGGER_EVENT_STARTED);
	// Value passed through is Bool(true).
	CHECK(spy->last_value_ei.get_type() == EIValue::TYPE_BOOL);
	CHECK(spy->last_value_ei.get_bool() == true);

	// Release: COMPLETED.
	f.sub->inject_input(make_key(Key::SPACE, false));
	CHECK(spy->call_count == 2);
	CHECK(spy->last_event_seen == (int)EI_TRIGGER_EVENT_COMPLETED);
	CHECK(spy->last_value_ei.get_type() == EIValue::TYPE_BOOL);
	CHECK(spy->last_value_ei.get_bool() == false);

	// Cleanup: spy is a heap Object; the test owns it.
	memdelete(spy);
}

TEST_CASE("[EnhancedInput][Subsystem] bind_action is idempotent on (action, event, callable) triple") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	CallbackSpy *spy = memnew(CallbackSpy);
	Callable cb = Callable(spy, "on_event");

	// Bind the same triple 3 times — only one effective binding.
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, cb);
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, cb);
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, cb);

	f.sub->inject_input(space);
	CHECK(spy->call_count == 1); // not 3

	memdelete(spy);
}

TEST_CASE("[EnhancedInput][Subsystem] bind_action on different events fires each independently") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	CallbackSpy *spy_started = memnew(CallbackSpy);
	CallbackSpy *spy_completed = memnew(CallbackSpy);
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, Callable(spy_started, "on_event"));
	f.sub->bind_action(a, EI_TRIGGER_EVENT_COMPLETED, Callable(spy_completed, "on_event"));

	f.sub->inject_input(space); // STARTED
	f.sub->inject_input(make_key(Key::SPACE, false)); // COMPLETED

	CHECK(spy_started->call_count == 1);
	CHECK(spy_started->last_event_seen == (int)EI_TRIGGER_EVENT_STARTED);
	CHECK(spy_completed->call_count == 1);
	CHECK(spy_completed->last_event_seen == (int)EI_TRIGGER_EVENT_COMPLETED);

	memdelete(spy_started);
	memdelete(spy_completed);
}

TEST_CASE("[EnhancedInput][Subsystem] unbind_action removes a binding") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();

	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	CallbackSpy *spy = memnew(CallbackSpy);
	Callable cb = Callable(spy, "on_event");
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, cb);

	// First press fires.
	f.sub->inject_input(space);
	CHECK(spy->call_count == 1);

	// Unbind, release to clear state.
	f.sub->unbind_action(a, EI_TRIGGER_EVENT_STARTED, cb);
	f.sub->inject_input(make_key(Key::SPACE, false));

	// Second press: no binding -> no fire.
	spy->call_count = 0;
	f.sub->inject_input(space);
	CHECK(spy->call_count == 0);

	memdelete(spy);
}

TEST_CASE("[EnhancedInput][Subsystem] clear_bindings drops everything") {
	Fixture f;
	Ref<EIAction> a1 = make_bool_action();
	Ref<EIAction> a2 = make_bool_action();

	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a1->set_default_triggers(def_trigs);
	a2->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx1 = make_imc(space, a1);
	Ref<EIMappingContext> ctx2 = make_imc(make_key(Key::A, true), a2);
	f.sub->add_mapping_context(ctx1, 0);
	f.sub->add_mapping_context(ctx2, 0);

	CallbackSpy *spy = memnew(CallbackSpy);
	f.sub->bind_action(a1, EI_TRIGGER_EVENT_STARTED, Callable(spy, "on_event"));
	f.sub->bind_action(a2, EI_TRIGGER_EVENT_STARTED, Callable(spy, "on_event"));

	f.sub->clear_bindings();

	f.sub->inject_input(space);
	f.sub->inject_input(make_key(Key::A, true));
	CHECK(spy->call_count == 0);

	memdelete(spy);
}

TEST_CASE("[EnhancedInput][Subsystem] bind_action rejects null / invalid inputs") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	CallbackSpy *spy = memnew(CallbackSpy);
	Callable cb = Callable(spy, "on_event");

	// Null action: silent no-op.
	f.sub->bind_action(Ref<EIAction>(), EI_TRIGGER_EVENT_STARTED, cb);

	// EI_TRIGGER_EVENT_NONE: explicit refuse (not a valid event to
	// bind to).
	f.sub->bind_action(a, EI_TRIGGER_EVENT_NONE, cb);

	// None of these should have bound anything; verify by checking
	// that the spy isn't called on a trigger event.
	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);
	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);
	f.sub->inject_input(space);
	CHECK(spy->call_count == 0);

	memdelete(spy);
}

// ---------------------------------------------------------------------------
// Regression: per-mapping triggers must advance on tick (fix #2)
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] per-mapping trigger advances via tick (regression)") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	// NOTE: no default_triggers — the Hold is attached as a PER-MAPPING
	// trigger. Before the fix, per-mapping triggers only got event-time
	// updates and never received tick(), so they never crossed threshold.

	Ref<EITriggerHold> hold;
	hold.instantiate();
	hold->set_hold_time_threshold(0.3);

	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	TypedArray<EITrigger> per_mapping_trigs;
	per_mapping_trigs.push_back(hold);
	ctx->add_mapping(make_key(Key::SPACE, true), a, TypedArray<EIModifier>(), per_mapping_trigs, true);
	f.sub->add_mapping_context(ctx, 0);

	// Press: STARTED.
	f.sub->inject_input(make_key(Key::SPACE, true));
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_STARTED);

	// Below threshold: still STARTED, no new event.
	f.sub->tick(0.1);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_STARTED);

	// Crossing the threshold via tick — the per-mapping Hold fires TRIGGERED.
	f.sub->tick(0.25);
	CHECK(f.sub->get_action_trigger_event(a) == EI_TRIGGER_EVENT_TRIGGERED);
}

// ---------------------------------------------------------------------------
// Regression: chord fires from subsystem-pushed member state (fix #3)
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] chord fires via subsystem-pushed member state (regression)") {
	Fixture f;
	Ref<EIAction> a = make_bool_action("A");
	Ref<EIAction> b = make_bool_action("B");
	Ref<EIAction> host = make_bool_action("Host");

	// `host` fires a chord requiring both A and B to be held. Nothing in
	// the test calls set_chord_action_active(); the subsystem must push
	// the member state itself (the bug that #3 fixed).
	Ref<EITriggerChord> chord;
	chord.instantiate();
	chord->add_chord_action(a);
	chord->add_chord_action(b);
	TypedArray<EITrigger> host_trigs;
	host_trigs.push_back(chord);
	host->set_default_triggers(host_trigs);

	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	ctx->add_mapping(make_key(Key::A, true), a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), false);
	ctx->add_mapping(make_key(Key::B, true), b, TypedArray<EIModifier>(), TypedArray<EITrigger>(), false);
	ctx->add_mapping(make_key(Key::C, true), host, TypedArray<EIModifier>(), TypedArray<EITrigger>(), false);
	f.sub->add_mapping_context(ctx, 0);

	// Establish the host runtime so its chord trigger gets ticked.
	f.sub->inject_input(make_key(Key::C, true));

	// Press A and B — both members become held.
	f.sub->inject_input(make_key(Key::A, true));
	f.sub->inject_input(make_key(Key::B, true));

	// Tick: subsystem refreshes chord membership (A, B held) and fires.
	f.sub->tick(0.016);
	CHECK(f.sub->get_action_trigger_event(host) == EI_TRIGGER_EVENT_TRIGGERED);

	// Release A — chord no longer satisfied; next tick completes.
	f.sub->inject_input(make_key(Key::A, false));
	f.sub->tick(0.016);
	CHECK(f.sub->get_action_trigger_event(host) == EI_TRIGGER_EVENT_COMPLETED);
}

// ---------------------------------------------------------------------------
// Regression: zero-arg callbacks fire (arity degraded in _fire_event)
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Subsystem] zero-arg callback fires despite 3-arg dispatch (regression)") {
	Fixture f;
	Ref<EIAction> a = make_bool_action();
	Ref<EITriggerPressed> pressed;
	pressed.instantiate();
	TypedArray<EITrigger> def_trigs;
	def_trigs.push_back(pressed);
	a->set_default_triggers(def_trigs);

	Ref<InputEventKey> space = make_key(Key::SPACE, true);
	Ref<EIMappingContext> ctx = make_imc(space, a);
	f.sub->add_mapping_context(ctx, 0);

	CallbackSpy *spy = memnew(CallbackSpy);
	// Bind a ZERO-arg method. Before the fix the dispatcher always called
	// with 3 args; an Object/GDScript method rejects extra args and never
	// runs, so this would stay 0.
	f.sub->bind_action(a, EI_TRIGGER_EVENT_STARTED, Callable(spy, "on_event_zero"));

	f.sub->inject_input(space);
	CHECK(spy->zero_call_count == 1);

	memdelete(spy);
}

} // namespace TestEISubsystem

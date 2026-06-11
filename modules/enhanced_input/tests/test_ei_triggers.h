/**************************************************************************/
/*  test_ei_triggers.h                                                    */
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

// Includes resolve relative to the module root when test_main.cpp compiles
// from tests/. Use module-relative paths so the same include works for
// both in-module sources and the generated tests bundle.
#include "../core/ei_action.h"
#include "../core/ei_trigger.h"
#include "../triggers/ei_trigger_chord.h"
#include "../triggers/ei_trigger_double_tap.h"
#include "../triggers/ei_trigger_hold.h"
#include "../triggers/ei_trigger_pressed.h"
#include "../triggers/ei_trigger_pulse.h"
#include "../triggers/ei_trigger_release.h"
#include "../triggers/ei_trigger_tap.h"

#include "core/object/class_db.h"
#include "tests/test_macros.h"

namespace TestEITriggers {

using namespace ei;

// Short aliases so test bodies read like English.
using State = EITrigger::TriggerState;
using Result = EITrigger::UpdateResult;
using RS = EITrigger::TriggerRuntimeState;

// Convenience: feed one update_state() call and return the result.
static Result call(
		const EITrigger &p_trig,
		RS &p_state,
		bool p_pressed,
		bool p_event_valid = true,
		double p_dt = 0.0,
		const EIValue &p_value = EIValue::make_bool(true)) {
	return p_trig.update_state(p_state, p_value, p_dt, p_event_valid, p_pressed);
}

// ---------------------------------------------------------------------------
// EITriggerPressed
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] Pressed full lifecycle (event -> tick -> release)") {
	ei::EITriggerPressed tr;
	RS s;

	// 1. Press event: state NONE -> TRIGGERED, return STARTED.
	Result r = call(tr, s, /*pressed=*/true, /*event_valid=*/true, /*dt=*/0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_TRIGGERED);

	// 2. Tick while held: emit ONGOING so listeners can distinguish
	//    "just pressed" from "still pressed".
	r = call(tr, s, /*pressed=*/true, /*event_valid=*/false, /*dt=*/0.016);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_TRIGGERED);

	// 3. Release event: state TRIGGERED -> NONE, return COMPLETED.
	r = call(tr, s, /*pressed=*/false, /*event_valid=*/true, /*dt=*/0.0);
	CHECK(r == Result::RESULT_COMPLETED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Pressed idle ticks return NONE") {
	ei::EITriggerPressed tr;
	RS s;

	// Idle ticks: no event, no state change, no result.
	Result r = call(tr, s, /*pressed=*/false, /*event_valid=*/false, /*dt=*/0.016);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_NONE);

	r = call(tr, s, /*pressed=*/false, /*event_valid=*/false, /*dt=*/0.5);
	CHECK(r == Result::RESULT_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Pressed re-press while still triggered reports TRIGGERED") {
	ei::EITriggerPressed tr;
	RS s;

	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);

	// A second press event while still in TRIGGERED returns TRIGGERED
	// (e.g. keyboard auto-repeat in raw event streams).
	r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_TRIGGERED);

	// Release to clean up.
	r = call(tr, s, false, true, 0.0);
	CHECK(r == Result::RESULT_COMPLETED);
}

// ---------------------------------------------------------------------------
// EITriggerHold (spec §4.4 scenario)
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] Hold(0.3) spec scenario: Started->Triggered->Ongoing->Ongoing->Completed") {
	ei::EITriggerHold tr;
	tr.set_hold_time_threshold(0.3);
	RS s;

	// t=0.0  press event   -> STARTED     (state NONE -> ONGOING)
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);

	// t=0.0..0.3: ticks below threshold return NONE.
	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_ONGOING);

	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_ONGOING);

	// t=0.3: hit threshold, fire TRIGGERED. State ONGOING -> TRIGGERED.
	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.state == State::STATE_TRIGGERED);

	// t=0.4, 0.5: each tick emits ONGOING.
	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_TRIGGERED);

	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_TRIGGERED);

	// release event: COMPLETED. State TRIGGERED -> NONE.
	r = call(tr, s, false, true, 0.0);
	CHECK(r == Result::RESULT_COMPLETED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Hold canceled when released before threshold") {
	ei::EITriggerHold tr;
	tr.set_hold_time_threshold(0.3);
	RS s;

	// Press at t=0.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);

	// Release at t=0.1 (well before 0.3 threshold). Should fire CANCELED.
	r = call(tr, s, false, true, 0.1);
	CHECK(r == Result::RESULT_CANCELED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Hold release while idle is NONE") {
	ei::EITriggerHold tr;
	tr.set_hold_time_threshold(0.3);
	RS s;

	// Release without ever pressing: nothing to cancel or complete.
	Result r = call(tr, s, false, true, 0.0);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_NONE);
}

// ---------------------------------------------------------------------------
// EITriggerTap
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] Tap(0.2) success: release within window fires TRIGGERED") {
	ei::EITriggerTap tr;
	tr.set_tap_release_time(0.2);
	RS s;

	// Press at t=0.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);

	// Tick at t=0.05: still in window, no event.
	r = call(tr, s, true, false, 0.05);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_ONGOING);

	// Release at t=0.1 (within 0.2s window): TRIGGERED.
	r = call(tr, s, false, true, 0.05);
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Tap(0.2) spec scenario: held 0.3s ends in CANCELED") {
	ei::EITriggerTap tr;
	tr.set_tap_release_time(0.2);
	RS s;

	// Press at t=0.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);

	// Tick at t=0.1: still in window.
	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_ONGOING);

	// Tick at t=0.2: window exceeded (>0.2), fire CANCELED.
	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_CANCELED);
	CHECK(s.state == State::STATE_NONE);

	// Release at t=0.3: nothing to do.
	r = call(tr, s, false, true, 0.1);
	CHECK(r == Result::RESULT_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Tap idle ticks return NONE") {
	ei::EITriggerTap tr;
	tr.set_tap_release_time(0.2);
	RS s;

	Result r = call(tr, s, false, false, 0.016);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_NONE);
}

// ---------------------------------------------------------------------------
// EITriggerDoubleTap
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] DoubleTap(0.3) success: two presses in window fire TRIGGERED") {
	ei::EITriggerDoubleTap tr;
	tr.set_double_tap_time(0.3);
	RS s;

	// 1st press at t=0.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);
	CHECK(s.consecutive_tap_count == 1);

	// Tick at t=0.1: still in window.
	r = call(tr, s, true, false, 0.1);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_ONGOING);

	// 2nd press at t=0.1 (within 0.3s window): TRIGGERED.
	r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.state == State::STATE_TRIGGERED);
	CHECK(s.consecutive_tap_count == 2);

	// Release: COMPLETED.
	r = call(tr, s, false, true, 0.0);
	CHECK(r == Result::RESULT_COMPLETED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] DoubleTap(0.2) cancel: 2nd press past window is treated as a new 1st press") {
	ei::EITriggerDoubleTap tr;
	tr.set_double_tap_time(0.2);
	RS s;

	// 1st press at t=0.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);

	// Tick at t=0.3: 0.3 > 0.2, no 2nd press came in time -> CANCELED.
	r = call(tr, s, true, false, 0.3);
	CHECK(r == Result::RESULT_CANCELED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] DoubleTap release while waiting for 2nd press keeps ONGOING") {
	ei::EITriggerDoubleTap tr;
	tr.set_double_tap_time(0.3);
	RS s;

	// 1st press at t=0.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);

	// Release at t=0.05: the 1st tap released, but we're still waiting
	// for the 2nd press. Stay ONGOING.
	r = call(tr, s, false, true, 0.05);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_ONGOING);
	CHECK(s.consecutive_tap_count == 1);
}

// ---------------------------------------------------------------------------
// EITriggerRelease
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] Release fires TRIGGERED on non-zero -> zero transition") {
	ei::EITriggerRelease tr;
	RS s;

	// Idle: value is zero, was_active is false -> NONE.
	Result r = call(tr, s, /*pressed=*/false, /*event_valid=*/false, /*dt=*/0.0,
			EIValue::make_bool(false));
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.was_active == false);

	// Value becomes non-zero (the "active" part): we just record it, no event.
	r = call(tr, s, false, false, 0.016, EIValue::make_bool(true));
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.was_active == true);

	// More ticks while still active: nothing.
	r = call(tr, s, false, false, 0.016, EIValue::make_bool(true));
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.was_active == true);

	// Value drops to zero: this is the FALLING edge. Fire TRIGGERED.
	r = call(tr, s, false, false, 0.016, EIValue::make_bool(false));
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.was_active == false);

	// Subsequent zero ticks: no event.
	r = call(tr, s, false, false, 0.016, EIValue::make_bool(false));
	CHECK(r == Result::RESULT_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Release works with Axis1D: float 0.5 -> 0.0 fires") {
	ei::EITriggerRelease tr;
	RS s;

	// Value goes non-zero.
	Result r = call(tr, s, false, false, 0.0, EIValue::make_axis1d(0.5f));
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.was_active == true);

	// Value drops to zero.
	r = call(tr, s, false, false, 0.0, EIValue::make_axis1d(0.0f));
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.was_active == false);
}

TEST_CASE("[EnhancedInput][Trigger] Release re-arms across multiple cycles") {
	ei::EITriggerRelease tr;
	RS s;

	// Cycle 1: up then down.
	call(tr, s, false, false, 0.0, EIValue::make_axis1d(1.0f));
	Result r = call(tr, s, false, false, 0.0, EIValue::make_axis1d(0.0f));
	CHECK(r == Result::RESULT_TRIGGERED);

	// Cycle 2: up then down -> TRIGGERED again.
	call(tr, s, false, false, 0.0, EIValue::make_axis1d(1.0f));
	r = call(tr, s, false, false, 0.0, EIValue::make_axis1d(0.0f));
	CHECK(r == Result::RESULT_TRIGGERED);

	// Cycle 3: stays at zero the whole time -> no events.
	r = call(tr, s, false, false, 0.0, EIValue::make_axis1d(0.0f));
	CHECK(r == Result::RESULT_NONE);
}

// ---------------------------------------------------------------------------
// EITriggerPulse
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] Pulse(0.5) full lifecycle: press -> pulse -> release") {
	ei::EITriggerPulse tr;
	tr.set_pulse_interval(0.5);
	RS s;

	// 1. Press event: state NONE -> ONGOING, return STARTED.
	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);
	CHECK(s.state == State::STATE_ONGOING);

	// 2. Tick at 0.3s: not yet due (0.3 < 0.5), return NONE.
	r = call(tr, s, true, false, 0.3);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_ONGOING);

	// 3. Tick at 0.5s total (dt=0.2): 0.5 >= 0.5, fire TRIGGERED.
	r = call(tr, s, true, false, 0.2);
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.state == State::STATE_TRIGGERED);

	// 4. Tick at 0.7s (dt=0.2): state is TRIGGERED, return ONGOING.
	r = call(tr, s, true, false, 0.2);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_TRIGGERED);

	// 5. Tick at 1.0s (dt=0.3): 1.0 - 0.5 = 0.5 >= 0.5, fire TRIGGERED again.
	r = call(tr, s, true, false, 0.3);
	CHECK(r == Result::RESULT_TRIGGERED);

	// 6. Release event: COMPLETED.
	r = call(tr, s, false, true, 0.0);
	CHECK(r == Result::RESULT_COMPLETED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Pulse release before first fire returns COMPLETED without TRIGGERED") {
	ei::EITriggerPulse tr;
	tr.set_pulse_interval(0.5);
	RS s;

	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);

	// Release before any tick passes the 0.5s interval.
	r = call(tr, s, false, true, 0.1);
	CHECK(r == Result::RESULT_COMPLETED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Pulse zero interval fires every tick") {
	ei::EITriggerPulse tr;
	tr.set_pulse_interval(0.0);
	RS s;

	Result r = call(tr, s, true, true, 0.0);
	CHECK(r == Result::RESULT_STARTED);

	// With 0 interval, any tick with dt>0 fires.
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_TRIGGERED);
}

// ---------------------------------------------------------------------------
// EITriggerChord
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] Chord fires only when ALL chord actions are active") {
	ei::EITriggerChord tr;
	RS s;

	Ref<EIAction> a = memnew(ei::EIAction);
	Ref<EIAction> b = memnew(ei::EIAction);
	tr.add_chord_action(a);
	tr.add_chord_action(b);

	// Initially neither is active: NONE.
	Result r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_NONE);

	// Only a is active: still NONE.
	tr.set_chord_action_active(a, true);
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_NONE);
	CHECK(s.state == State::STATE_NONE);

	// Both a and b active: TRIGGERED on rising edge.
	tr.set_chord_action_active(b, true);
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_TRIGGERED);
	CHECK(s.state == State::STATE_TRIGGERED);

	// Still both active: ONGOING.
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_ONGOING);
	CHECK(s.state == State::STATE_TRIGGERED);

	// a no longer active: COMPLETED.
	tr.set_chord_action_active(a, false);
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_COMPLETED);
	CHECK(s.state == State::STATE_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Chord re-arms across multiple cycles") {
	ei::EITriggerChord tr;
	RS s;

	Ref<EIAction> a = memnew(ei::EIAction);
	Ref<EIAction> b = memnew(ei::EIAction);
	tr.add_chord_action(a);
	tr.add_chord_action(b);

	// Cycle 1: a + b active -> TRIGGERED.
	tr.set_chord_action_active(a, true);
	tr.set_chord_action_active(b, true);
	Result r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_TRIGGERED);

	// Release both -> COMPLETED.
	tr.set_chord_action_active(a, false);
	tr.set_chord_action_active(b, false);
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_COMPLETED);

	// Cycle 2: a + b active again -> TRIGGERED.
	tr.set_chord_action_active(a, true);
	tr.set_chord_action_active(b, true);
	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_TRIGGERED);
}

TEST_CASE("[EnhancedInput][Trigger] Chord with empty action list never fires") {
	ei::EITriggerChord tr;
	RS s;

	// No chord actions registered. Should return NONE always.
	Result r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_NONE);

	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_NONE);

	// add then clear, still empty.
	Ref<EIAction> a = memnew(ei::EIAction);
	tr.add_chord_action(a);
	tr.clear_chord_actions();
	CHECK(tr.get_chord_action_count() == 0);

	r = call(tr, s, true, false, 0.016);
	CHECK(r == Result::RESULT_NONE);
}

TEST_CASE("[EnhancedInput][Trigger] Chord exposes get/set chord_action_active for the subsystem") {
	ei::EITriggerChord tr;
	Ref<EIAction> a = memnew(ei::EIAction);
	tr.add_chord_action(a);

	CHECK(tr.get_chord_action_count() == 1);
	CHECK(tr.get_chord_action(0) == a);
	// Default: inactive.
	CHECK(tr.is_chord_action_active(a) == false);

	tr.set_chord_action_active(a, true);
	CHECK(tr.is_chord_action_active(a) == true);

	// get_chord_action out of range returns null Ref.
	Ref<EIAction> oob = tr.get_chord_action(99);
	CHECK(oob.is_null());
}

// ---------------------------------------------------------------------------
// Configuration getters / setters and get_trigger_name
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Trigger] configurable thresholds round-trip through setters") {
	ei::EITriggerHold h;
	h.set_hold_time_threshold(0.5);
	CHECK(h.get_hold_time_threshold() == doctest::Approx(0.5));

	ei::EITriggerTap t;
	t.set_tap_release_time(0.15);
	CHECK(t.get_tap_release_time() == doctest::Approx(0.15));

	ei::EITriggerDoubleTap d;
	d.set_double_tap_time(0.25);
	CHECK(d.get_double_tap_time() == doctest::Approx(0.25));

	ei::EITriggerPulse p;
	p.set_pulse_interval(0.2);
	CHECK(p.get_pulse_interval() == doctest::Approx(0.2));

	// Negative values are clamped to 0.
	h.set_hold_time_threshold(-1.0);
	CHECK(h.get_hold_time_threshold() == 0.0);
}

TEST_CASE("[EnhancedInput][Trigger] subclass names") {
	ei::EITriggerPressed p;
	ei::EITriggerHold h;
	ei::EITriggerTap t;
	ei::EITriggerDoubleTap d;
	ei::EITriggerPulse pu;
	ei::EITriggerChord c;
	ei::EITriggerRelease r;
	CHECK(p.get_trigger_name() == "Pressed");
	CHECK(h.get_trigger_name() == "Hold");
	CHECK(t.get_trigger_name() == "Tap");
	CHECK(d.get_trigger_name() == "DoubleTap");
	CHECK(pu.get_trigger_name() == "Pulse");
	CHECK(c.get_trigger_name() == "Chord");
	CHECK(r.get_trigger_name() == "Release");
}

TEST_CASE("[EnhancedInput][Trigger] EITrigger is registered as abstract base") {
	// Spec §4.4 calls for register_abstract_class so the compiler
	// doesn't try to instantiate the base. Sanity-check the ClassDB
	// side: EITrigger exists in the registry as a base class, but you
	// can't instantiate it directly.
	CHECK(ClassDB::class_exists("EITrigger"));
	CHECK(!ClassDB::can_instantiate("EITrigger"));

	// All 7 concrete subclasses are instantiable.
	CHECK(ClassDB::class_exists("EITriggerPressed"));
	CHECK(ClassDB::can_instantiate("EITriggerPressed"));
	CHECK(ClassDB::class_exists("EITriggerHold"));
	CHECK(ClassDB::can_instantiate("EITriggerHold"));
	CHECK(ClassDB::class_exists("EITriggerTap"));
	CHECK(ClassDB::can_instantiate("EITriggerTap"));
	CHECK(ClassDB::class_exists("EITriggerDoubleTap"));
	CHECK(ClassDB::can_instantiate("EITriggerDoubleTap"));
	CHECK(ClassDB::class_exists("EITriggerPulse"));
	CHECK(ClassDB::can_instantiate("EITriggerPulse"));
	CHECK(ClassDB::class_exists("EITriggerChord"));
	CHECK(ClassDB::can_instantiate("EITriggerChord"));
	CHECK(ClassDB::class_exists("EITriggerRelease"));
	CHECK(ClassDB::can_instantiate("EITriggerRelease"));
}

} // namespace TestEITriggers

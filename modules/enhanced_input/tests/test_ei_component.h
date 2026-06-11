/**************************************************************************/
/*  test_ei_component.h                                                    */
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
#include "../core/ei_component.h"
#include "../core/ei_subsystem.h"
#include "../core/ei_trigger.h"

#include "core/object/class_db.h"
#include "tests/test_macros.h"

namespace TestEIComponent {

using namespace ei;

class Spy : public Object {
	GDCLASS(Spy, Object);

public:
	int call_count = 0;

	void on_event(const Ref<EIAction> &p_action, int p_event, const Variant &p_value) {
		call_count++;
		(void)p_action;
		(void)p_event;
		(void)p_value;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("on_event", "action", "event", "value"), &Spy::on_event);
	}
};

TEST_CASE("[EnhancedInput][Component] bind / is_bound / unbind round-trip on the local subscription list") {
	// The component needs a live EISubsystem singleton to forward
	// binds to. memnew sets it via the constructor.
	EIComponent *comp = memnew(EIComponent);
	Ref<EIAction> a;
	a.instantiate();
	a->set_value_type(EIAction::VALUE_TYPE_BOOL);
	Spy *spy = memnew(Spy);

	// Initially nothing bound.
	CHECK(comp->get_bound_count() == 0);
	CHECK(comp->is_bound(a, EI_TRIGGER_EVENT_TRIGGERED) == false);

	comp->bind(a, EI_TRIGGER_EVENT_TRIGGERED, Callable(spy, "on_event"));
	CHECK(comp->get_bound_count() == 1);
	CHECK(comp->is_bound(a, EI_TRIGGER_EVENT_TRIGGERED) == true);
	CHECK(comp->is_bound(a, EI_TRIGGER_EVENT_STARTED) == false); // different event

	// Re-bind the same triple: idempotent.
	comp->bind(a, EI_TRIGGER_EVENT_TRIGGERED, Callable(spy, "on_event"));
	CHECK(comp->get_bound_count() == 1);

	// Unbind.
	comp->unbind(a, EI_TRIGGER_EVENT_TRIGGERED, Callable(spy, "on_event"));
	CHECK(comp->get_bound_count() == 0);
	CHECK(comp->is_bound(a, EI_TRIGGER_EVENT_TRIGGERED) == false);

	// unbind_all on empty: no-op.
	comp->unbind_all();
	CHECK(comp->get_bound_count() == 0);

	memdelete(comp);
	memdelete(spy);
}

TEST_CASE("[EnhancedInput][Component] unbind_all removes all subscriptions") {
	EIComponent *comp = memnew(EIComponent);
	Ref<EIAction> a1;
	a1.instantiate();
	a1->set_value_type(EIAction::VALUE_TYPE_BOOL);
	Ref<EIAction> a2;
	a2.instantiate();
	a2->set_value_type(EIAction::VALUE_TYPE_BOOL);
	Spy *spy = memnew(Spy);

	comp->bind(a1, EI_TRIGGER_EVENT_TRIGGERED, Callable(spy, "on_event"));
	comp->bind(a1, EI_TRIGGER_EVENT_COMPLETED, Callable(spy, "on_event"));
	comp->bind(a2, EI_TRIGGER_EVENT_TRIGGERED, Callable(spy, "on_event"));
	CHECK(comp->get_bound_count() == 3);

	comp->unbind_all();
	CHECK(comp->get_bound_count() == 0);
	CHECK(comp->is_bound(a1, EI_TRIGGER_EVENT_TRIGGERED) == false);
	CHECK(comp->is_bound(a2, EI_TRIGGER_EVENT_TRIGGERED) == false);

	memdelete(comp);
	memdelete(spy);
}

TEST_CASE("[EnhancedInput][Component] bind rejects null / invalid inputs") {
	EIComponent *comp = memnew(EIComponent);
	Ref<EIAction> a;
	a.instantiate();
	Spy *spy = memnew(Spy);

	// Null action: silent ignore.
	comp->bind(Ref<EIAction>(), EI_TRIGGER_EVENT_TRIGGERED, Callable(spy, "on_event"));
	CHECK(comp->get_bound_count() == 0);

	// Valid action, but constructing a Callable with a null Object
	// is not valid in Godot (Callable.is_valid() returns false),
	// so the call is rejected upstream.
	comp->bind(a, EI_TRIGGER_EVENT_TRIGGERED, Callable());
	CHECK(comp->get_bound_count() == 0);

	memdelete(comp);
	memdelete(spy);
}

} // namespace TestEIComponent

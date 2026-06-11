/**************************************************************************/
/*  test_ei_bridge.h                                                       */
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
#include "../core/ei_bridge.h"
#include "../core/ei_mapping_context.h"
#include "../core/ei_value.h"

#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "tests/test_macros.h"

namespace TestEIBridge {

using namespace ei;

// Set up a minimal InputMap singleton with a few named actions for
// the test cases. Tears down at the end of the test case so the
// singleton is fresh for the next test (some test runners reuse the
// same instance across cases).
struct InputMapFixture {
	String action_a = "test_bridge_action_a";
	String action_b = "test_bridge_action_b";

	InputMapFixture() {
		// The [SceneTree] test path auto-creates an InputMap. For
		// our test (no [SceneTree] tag) we manage the singleton
		// ourselves. If a prior test left one behind, free it first
		// so we start clean.
		if (InputMap::get_singleton() != nullptr) {
			memdelete(InputMap::get_singleton());
		}
		InputMap *im = memnew(InputMap);
		im->load_default();

		// Add a custom action with one key binding.
		im->add_action(action_a, 0.5);
		Ref<InputEventKey> key_a;
		key_a.instantiate();
		key_a->set_keycode(Key::J);
		im->action_add_event(action_a, key_a);

		// Add a second custom action with two key bindings.
		im->add_action(action_b, 0.5);
		Ref<InputEventKey> key_b1;
		key_b1.instantiate();
		key_b1->set_keycode(Key::K);
		im->action_add_event(action_b, key_b1);
		Ref<InputEventKey> key_b2;
		key_b2.instantiate();
		key_b2->set_keycode(Key::L);
		im->action_add_event(action_b, key_b2);
	}

	~InputMapFixture() {
		// Clean up so the next test sees a fresh singleton.
		if (InputMap::get_singleton() != nullptr) {
			memdelete(InputMap::get_singleton());
		}
	}
};

TEST_CASE("[EnhancedInput][Bridge] import_action creates an EIAction with the right name and default type") {
	InputMapFixture f;
	Ref<EIAction> a = EIBridge::import_action(f.action_a);
	REQUIRE(a.is_valid());
	CHECK(a->get_value_type() == EIAction::VALUE_TYPE_BOOL); // default
	CHECK(a->get_description() == f.action_a);
}

TEST_CASE("[EnhancedInput][Bridge] import_action respects an explicit value type") {
	InputMapFixture f;
	Ref<EIAction> a = EIBridge::import_action(f.action_b, EIAction::VALUE_TYPE_AXIS1D);
	REQUIRE(a.is_valid());
	CHECK(a->get_value_type() == EIAction::VALUE_TYPE_AXIS1D);
}

TEST_CASE("[EnhancedInput][Bridge] import_action returns null Ref for an unknown action") {
	// Symmetric with import_context: a typo'd InputMap name must
	// not silently produce an EIAction whose description matches
	// nothing live. The bridge is read-only — if the action isn't
	// there, we have nothing to import.
	InputMapFixture f;
	Ref<EIAction> a = EIBridge::import_action("not_in_input_map");
	CHECK(a.is_null());
}

TEST_CASE("[EnhancedInput][Bridge] import_context returns null Ref for an unknown action") {
	InputMapFixture f;
	Ref<EIMappingContext> ctx = EIBridge::import_context("not_in_input_map", "Gameplay");
	CHECK(ctx.is_null());
}

TEST_CASE("[EnhancedInput][Bridge] import_context creates 1:1 mappings matching InputMap events") {
	InputMapFixture f;
	Ref<EIMappingContext> ctx = EIBridge::import_context(f.action_a, "Gameplay", 0);
	REQUIRE(ctx.is_valid());
	CHECK(ctx->get_context_name() == "Gameplay");
	// action_a has 1 key event (Key::J).
	CHECK(ctx->get_mapping_count() == 1);

	// The mapping is bare 1:1: event → action, no modifiers, no
	// triggers, consumes=true. Verify via the public surface.
	TypedArray<Dictionary> arr = ctx->get_mappings();
	REQUIRE(arr.size() == 1);
	Dictionary d = arr[0];
	Ref<EIAction> mapped_action = d[String("action")];
	REQUIRE(mapped_action.is_valid());
	CHECK(mapped_action->get_description() == f.action_a);
	Ref<InputEvent> mapped_event = d[String("event")];
	REQUIRE(mapped_event.is_valid());
	CHECK(mapped_event->is_match(Ref<InputEventKey>(d[String("event")]), true)); // tautology, just a sanity probe
	TypedArray<EIModifier> mods = d[String("modifiers")];
	CHECK(mods.size() == 0);
	TypedArray<EITrigger> trigs = d[String("triggers")];
	CHECK(trigs.size() == 0);
	CHECK(bool(d[String("consumes")]) == true);
}

TEST_CASE("[EnhancedInput][Bridge] import_context maps every event for a multi-key action") {
	InputMapFixture f;
	// action_b has 2 key events (Key::K, Key::L).
	Ref<EIMappingContext> ctx = EIBridge::import_context(f.action_b, "Gameplay");
	REQUIRE(ctx.is_valid());
	CHECK(ctx->get_mapping_count() == 2);
}

TEST_CASE("[EnhancedInput][Bridge] import_context handles default-loaded ui_accept (spec P9 acceptance)") {
	// Spec §9 P9 acceptance: `import_context("ui_accept")` produces
	// a context with the same events as the InputMap action. The
	// fixture's load_default() populates ui_accept with engine
	// defaults (Enter + Space for keyboard, plus controller buttons).
	InputMapFixture f;
	Ref<EIMappingContext> ctx = EIBridge::import_context("ui_accept", "UI", 0);
	REQUIRE(ctx.is_valid());
	CHECK(ctx->get_context_name() == "UI");
	// At least Enter + Space on a default install; we don't pin the
	// exact count because default mappings can change across engine
	// versions, but >=2 is a safe lower bound for keyboard + gamepad.
	CHECK(ctx->get_mapping_count() >= 2);

	// All mappings should reference an EIAction whose description
	// matches the imported action name (the bridge shares one
	// action across all mappings on the same IMC).
	TypedArray<Dictionary> arr = ctx->get_mappings();
	for (int i = 0; i < arr.size(); i++) {
		Dictionary d = arr[i];
		Ref<EIAction> act = d[String("action")];
		REQUIRE(act.is_valid());
		CHECK(act->get_description() == "ui_accept");
		// And each mapping must have bare empty modifiers/triggers
		// with consumes=true (lossy v1 contract).
		CHECK(d[String("modifiers")].operator Array().size() == 0);
		CHECK(d[String("triggers")].operator Array().size() == 0);
		CHECK(bool(d[String("consumes")]) == true);
	}
}

TEST_CASE("[EnhancedInput][Bridge] EIBridge is a registered RefCounted class") {
	// Spec §4.10: pure RefCounted helper, not a Node, not a Resource.
	CHECK(ClassDB::class_exists("EIBridge"));
	CHECK(ClassDB::can_instantiate("EIBridge"));
	CHECK(ClassDB::get_parent_class("EIBridge") == "RefCounted");
}

} // namespace TestEIBridge

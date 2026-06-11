/**************************************************************************/
/*  test_ei_mapping_context.h                                            */
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
#include "../core/ei_input_event_sampler.h"
#include "../core/ei_mapping_context.h"
#include "../core/ei_value.h"

#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "tests/test_macros.h"

namespace TestEIMappingContext {

using namespace ei;

static bool approx_eq(float a, float b, float eps = 1e-5f) {
	return Math::abs(a - b) < eps;
}

// ---------------------------------------------------------------------------
// EIMappingContext
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][MappingContext] Add and inspect mappings") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	ctx->set_context_name("Gameplay");

	CHECK(ctx->get_context_name() == "Gameplay");
	CHECK(ctx->get_mapping_count() == 0);

	Ref<EIAction> jump;
	jump.instantiate();
	jump->set_value_type(EIAction::VALUE_TYPE_BOOL);

	Ref<InputEventKey> space_key;
	space_key.instantiate();
	space_key->set_pressed(true);
	space_key->set_keycode(Key::SPACE);

	ctx->add_mapping(space_key, jump, TypedArray<EIModifier>(), TypedArray<EITrigger>(), /*consumes=*/true);
	CHECK(ctx->get_mapping_count() == 1);

	// Inspector surface: get_mappings returns a Dictionary per mapping.
	TypedArray<Dictionary> arr = ctx->get_mappings();
	CHECK(arr.size() == 1);
	Dictionary d = arr[0];
	CHECK(d.has(String("event")));
	CHECK(d.has(String("action")));
	CHECK(d.has(String("modifiers")));
	CHECK(d.has(String("triggers")));
	CHECK(d.has(String("consumes")));
	Ref<InputEvent> e_roundtrip = d[String("event")];
	Ref<EIAction> a_roundtrip = d[String("action")];
	CHECK(e_roundtrip == space_key);
	CHECK(a_roundtrip == jump);
	CHECK(bool(d[String("consumes")]) == true);
}

TEST_CASE("[EnhancedInput][MappingContext] Remove by (event, action) pair") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();

	Ref<EIAction> a;
	a.instantiate();
	Ref<EIAction> b;
	b.instantiate();

	Ref<InputEventKey> k1;
	k1.instantiate();
	k1->set_keycode(Key::A);
	Ref<InputEventKey> k2;
	k2.instantiate();
	k2->set_keycode(Key::B);

	ctx->add_mapping(k1, a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	ctx->add_mapping(k1, b, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	ctx->add_mapping(k2, a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	CHECK(ctx->get_mapping_count() == 3);

	// Remove only the (k1, a) pair — k1->b and k2->a remain.
	ctx->remove_mapping(k1, a);
	CHECK(ctx->get_mapping_count() == 2);

	// remove_mapping on a non-existent pair is a silent no-op.
	ctx->remove_mapping(k1, a);
	CHECK(ctx->get_mapping_count() == 2);

	ctx->remove_mapping(k1, b);
	ctx->remove_mapping(k2, a);
	CHECK(ctx->get_mapping_count() == 0);
}

TEST_CASE("[EnhancedInput][MappingContext] clear_mappings drops all") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	Ref<EIAction> a;
	a.instantiate();
	Ref<InputEventKey> k;
	k.instantiate();

	for (int i = 0; i < 5; i++) {
		ctx->add_mapping(k, a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	}
	CHECK(ctx->get_mapping_count() == 5);

	ctx->clear_mappings();
	CHECK(ctx->get_mapping_count() == 0);
}

TEST_CASE("[EnhancedInput][MappingContext] add_mapping silently rejects null inputs") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	Ref<EIAction> a;
	a.instantiate();
	Ref<InputEventKey> k;
	k.instantiate();

	ctx->add_mapping(Ref<InputEvent>(), a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	CHECK(ctx->get_mapping_count() == 0);
	ctx->add_mapping(k, Ref<EIAction>(), TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	CHECK(ctx->get_mapping_count() == 0);

	// Valid add still works after the rejects.
	ctx->add_mapping(k, a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);
	CHECK(ctx->get_mapping_count() == 1);
}

TEST_CASE("[EnhancedInput][MappingContext] set_mappings replaces, get_mappings is a snapshot") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();

	// Build a dict-array as if the inspector had just saved it.
	TypedArray<Dictionary> arr;
	Ref<EIAction> a;
	a.instantiate();
	Ref<InputEventKey> k;
	k.instantiate();
	Dictionary d;
	d["event"] = k;
	d["action"] = a;
	d["modifiers"] = TypedArray<EIModifier>();
	d["triggers"] = TypedArray<EITrigger>();
	d["consumes"] = false;
	arr.push_back(d);

	ctx->set_mappings(arr);
	CHECK(ctx->get_mapping_count() == 1);

	// The consumes=false we passed in is preserved.
	TypedArray<Dictionary> out = ctx->get_mappings();
	Dictionary d0 = out[0];
	CHECK(bool(d0[String("consumes")]) == false);

	// Mutating the local copy does not affect the internal state
	// (Dictionary is a value type). To "save" changes, call
	// set_mappings again with the modified array.
	Dictionary mutated = out[0];
	mutated[String("consumes")] = true;
	Dictionary internal = ctx->get_mappings()[0];
	CHECK(bool(internal[String("consumes")]) == false); // internal unchanged

	// Round-trip via set_mappings DOES update the internal state.
	ctx->set_mappings(out);
	Dictionary d1 = ctx->get_mappings()[0];
	CHECK(bool(d1[String("consumes")]) == true); // now the modified value
}

TEST_CASE("[EnhancedInput][MappingContext] forward compat: dict with missing fields gets defaults") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();

	// A .tres saved by an older version might lack fields. The parser
	// should fill in defaults rather than crash.
	TypedArray<Dictionary> arr;
	Dictionary d; // empty
	arr.push_back(d);
	ctx->set_mappings(arr);

	CHECK(ctx->get_mapping_count() == 1);
	const EIMappingContext::Mapping &m = ctx->get_mappings_internal()[0];
	CHECK(m.event.is_null()); // no event -> unmatchable
	CHECK(m.action.is_null()); // no action -> null
	CHECK(m.modifiers.size() == 0);
	CHECK(m.triggers.size() == 0);
	CHECK(m.consumes == true); // spec default
}

TEST_CASE("[EnhancedInput][MappingContext] get_mappings_internal gives typed access for the dispatcher") {
	Ref<EIMappingContext> ctx;
	ctx.instantiate();
	Ref<EIAction> a;
	a.instantiate();
	a->set_value_type(EIAction::VALUE_TYPE_AXIS2D);
	Ref<InputEventKey> k;
	k.instantiate();

	ctx->add_mapping(k, a, TypedArray<EIModifier>(), TypedArray<EITrigger>(), true);

	const Vector<EIMappingContext::Mapping> &internal = ctx->get_mappings_internal();
	CHECK(internal.size() == 1);
	CHECK(internal[0].action == a);
	CHECK(internal[0].event == k);
	CHECK(internal[0].action->get_value_type() == EIAction::VALUE_TYPE_AXIS2D);
	CHECK(internal[0].consumes == true);
}

TEST_CASE("[EnhancedInput][MappingContext] EIMappingContext is registered as Resource") {
	// Spec §4.5: a Resource, instantiable, .tres-serializable.
	CHECK(ClassDB::class_exists("EIMappingContext"));
	CHECK(ClassDB::can_instantiate("EIMappingContext"));
	// Parent class is Resource.
	CHECK(ClassDB::get_parent_class("EIMappingContext") == "Resource");
}

// ---------------------------------------------------------------------------
// EIInputEventSampler
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Sampler] Key event press -> Axis1D(1.0), release -> Axis1D(0.0)") {
	Ref<InputEventKey> key;
	key.instantiate();
	key->set_pressed(true);
	EIValue v_pressed = EIInputEventSampler::sample(key, EIAction::VALUE_TYPE_AXIS1D);
	CHECK(v_pressed.get_type() == EIValue::TYPE_AXIS1D);
	CHECK(approx_eq(v_pressed.get_axis1d(), 1.0f));

	key->set_pressed(false);
	EIValue v_released = EIInputEventSampler::sample(key, EIAction::VALUE_TYPE_AXIS1D);
	CHECK(approx_eq(v_released.get_axis1d(), 0.0f));
}

TEST_CASE("[EnhancedInput][Sampler] Key event promoted to Bool action") {
	Ref<InputEventKey> key;
	key.instantiate();
	key->set_pressed(true);

	EIValue v = EIInputEventSampler::sample(key, EIAction::VALUE_TYPE_BOOL);
	CHECK(v.get_type() == EIValue::TYPE_BOOL);
	CHECK(v.get_bool() == true);
}

TEST_CASE("[EnhancedInput][Sampler] Key event promoted to Axis2D action (X=1, Y=0)") {
	Ref<InputEventKey> key;
	key.instantiate();
	key->set_pressed(true);

	EIValue v = EIInputEventSampler::sample(key, EIAction::VALUE_TYPE_AXIS2D);
	CHECK(v.get_type() == EIValue::TYPE_AXIS2D);
	Vector2 axis2d = v.get_axis2d();
	CHECK(approx_eq(axis2d.x, 1.0f));
	CHECK(approx_eq(axis2d.y, 0.0f));
}

TEST_CASE("[EnhancedInput][Sampler] Mouse button mirrors key semantics") {
	Ref<InputEventMouseButton> mb;
	mb.instantiate();
	mb->set_pressed(true);

	EIValue v_pressed = EIInputEventSampler::sample(mb, EIAction::VALUE_TYPE_AXIS1D);
	CHECK(approx_eq(v_pressed.get_axis1d(), 1.0f));

	mb->set_pressed(false);
	EIValue v_released = EIInputEventSampler::sample(mb, EIAction::VALUE_TYPE_AXIS1D);
	CHECK(approx_eq(v_released.get_axis1d(), 0.0f));
}

TEST_CASE("[EnhancedInput][Sampler] Mouse motion produces Axis2D (relative vector)") {
	Ref<InputEventMouseMotion> mm;
	mm.instantiate();
	mm->set_relative(Vector2(3.5f, -2.0f));

	// For an Axis2D action, return the relative motion as-is.
	EIValue v_2d = EIInputEventSampler::sample(mm, EIAction::VALUE_TYPE_AXIS2D);
	CHECK(v_2d.get_type() == EIValue::TYPE_AXIS2D);
	Vector2 motion = v_2d.get_axis2d();
	CHECK(approx_eq(motion.x, 3.5f));
	CHECK(approx_eq(motion.y, -2.0f));

	// For an Axis3D action, promote: (x, y, 0).
	EIValue v_3d = EIInputEventSampler::sample(mm, EIAction::VALUE_TYPE_AXIS3D);
	Vector3 motion3 = v_3d.get_axis3d();
	CHECK(approx_eq(motion3.x, 3.5f));
	CHECK(approx_eq(motion3.y, -2.0f));
	CHECK(approx_eq(motion3.z, 0.0f));
}

TEST_CASE("[EnhancedInput][Sampler] Null event returns typed zero") {
	Ref<InputEvent> null_ev;
	EIValue v = EIInputEventSampler::sample(null_ev, EIAction::VALUE_TYPE_AXIS2D);
	CHECK(v.get_type() == EIValue::TYPE_AXIS2D);
	CHECK(v.get_axis2d() == Vector2());
}

TEST_CASE("[EnhancedInput][Sampler] Unhandled InputEventAction returns typed zero") {
	// InputEventAction is a concrete InputEvent subtype that the
	// sampler doesn't specifically handle — it has a different role
	// (referencing an InputMap action by name) than a raw physical
	// event. The sampler should fall through to the "typed zero"
	// fallback, same as for the null-event case.
	Ref<InputEventAction> action_ev;
	action_ev.instantiate();
	action_ev->set_action("ui_accept");
	action_ev->set_pressed(true);

	EIValue v = EIInputEventSampler::sample(action_ev, EIAction::VALUE_TYPE_AXIS2D);
	CHECK(v.get_type() == EIValue::TYPE_AXIS2D);
	CHECK(v.get_axis2d() == Vector2());
}

} // namespace TestEIMappingContext

/**************************************************************************/
/*  ei_mapping_context.cpp                                               */
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

#include "ei_mapping_context.h"

#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"

namespace ei {

// ---------------------------------------------------------------------------
// Programmatic API
// ---------------------------------------------------------------------------

void EIMappingContext::add_mapping(const Ref<InputEvent> &p_event,
		const Ref<EIAction> &p_action,
		const TypedArray<EIModifier> &p_mods,
		const TypedArray<EITrigger> &p_trigs,
		bool p_consumes) {
	// Reject silently on null inputs — there's no meaningful "default"
	// to apply and the dispatcher would crash on null deref later.
	if (p_event.is_null() || p_action.is_null()) {
		return;
	}
	Mapping m;
	m.event = p_event;
	m.action = p_action;
	m.modifiers = p_mods;
	m.triggers = p_trigs;
	m.consumes = p_consumes;
	_mappings.push_back(m);
}

void EIMappingContext::remove_mapping(const Ref<InputEvent> &p_event, const Ref<EIAction> &p_action) {
	if (p_event.is_null() || p_action.is_null()) {
		return;
	}
	for (int i = 0; i < _mappings.size(); i++) {
		const Mapping &m = _mappings[i];
		if (m.event == p_event && m.action == p_action) {
			_mappings.remove_at(i);
			return; // remove only the first match; spec doesn't say "all"
		}
	}
}

void EIMappingContext::clear_mappings() {
	_mappings.clear();
}

// ---------------------------------------------------------------------------
// Inspector / serialization surface
// ---------------------------------------------------------------------------

// Convert one Mapping into a Dictionary with the canonical key set used
// by both the inspector and .tres round-trip:
//
//   { "event": Ref<InputEvent>, "action": Ref<EIAction>,
//     "modifiers": TypedArray<EIModifier>, "triggers": TypedArray<EITrigger>,
//     "consumes": bool }
//
// Using String keys (rather than StringName) to keep the dictionary
// inspector-friendly; Godot's variant dispatch handles either.
static Dictionary _mapping_to_dict(const EIMappingContext::Mapping &m) {
	Dictionary d;
	d["event"] = m.event;
	d["action"] = m.action;
	d["modifiers"] = m.modifiers;
	d["triggers"] = m.triggers;
	d["consumes"] = m.consumes;
	return d;
}

// Inverse: parse a Dictionary back into a Mapping. Missing fields get
// defaults (empty arrays, consumes=true). Bad field types are silently
// dropped so .tres forward-compat works (spec §6.3).
static EIMappingContext::Mapping _dict_to_mapping(const Dictionary &p_d) {
	EIMappingContext::Mapping m;
	const Variant *event_v = p_d.getptr("event");
	if (event_v && event_v->get_type() == Variant::OBJECT) {
		// Variant has no operator Ref<T> directly; extract the
		// Object* and cast to the expected InputEvent subtype. The
		// cast_to<InputEvent> handles the cross-type case gracefully
		// (returns nullptr for non-input-event objects).
		Ref<InputEvent> e = Object::cast_to<InputEvent>(*event_v);
		if (e.is_valid()) {
			m.event = e;
		}
	}
	const Variant *action_v = p_d.getptr("action");
	if (action_v && action_v->get_type() == Variant::OBJECT) {
		Ref<EIAction> a = Object::cast_to<EIAction>(*action_v);
		if (a.is_valid()) {
			m.action = a;
		}
	}
	const Variant *mods_v = p_d.getptr("modifiers");
	if (mods_v && mods_v->get_type() == Variant::ARRAY) {
		Array arr = *mods_v;
		TypedArray<EIModifier> typed;
		for (int i = 0; i < arr.size(); i++) {
			typed.push_back(arr[i]);
		}
		m.modifiers = typed;
	}
	const Variant *trigs_v = p_d.getptr("triggers");
	if (trigs_v && trigs_v->get_type() == Variant::ARRAY) {
		Array arr = *trigs_v;
		TypedArray<EITrigger> typed;
		for (int i = 0; i < arr.size(); i++) {
			typed.push_back(arr[i]);
		}
		m.triggers = typed;
	}
	const Variant *consumes_v = p_d.getptr("consumes");
	if (consumes_v && consumes_v->get_type() == Variant::BOOL) {
		m.consumes = (bool)*consumes_v;
	} else {
		m.consumes = true; // spec default
	}
	return m;
}

void EIMappingContext::set_mappings(const TypedArray<Dictionary> &p_mappings) {
	_mappings.clear();
	_mappings.reserve(p_mappings.size());
	for (int i = 0; i < p_mappings.size(); i++) {
		Dictionary d = p_mappings[i];
		_mappings.push_back(_dict_to_mapping(d));
	}
}

TypedArray<Dictionary> EIMappingContext::get_mappings() const {
	TypedArray<Dictionary> out;
	for (int i = 0; i < _mappings.size(); i++) {
		out.push_back(_mapping_to_dict(_mappings[i]));
	}
	return out;
}

int EIMappingContext::get_mapping_count() const {
	return _mappings.size();
}

// ---------------------------------------------------------------------------
// Internal access for the dispatcher
// ---------------------------------------------------------------------------

const Vector<EIMappingContext::Mapping> &EIMappingContext::get_mappings_internal() const {
	return _mappings;
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

void EIMappingContext::set_context_name(const String &p_name) {
	_context_name = p_name;
}

String EIMappingContext::get_context_name() const {
	return _context_name;
}

// ---------------------------------------------------------------------------
// Method binding
// ---------------------------------------------------------------------------

void EIMappingContext::_bind_methods() {
	// modifiers / triggers default to empty, consumes defaults to true so
	// the common 2-arg call `add_mapping(event, action)` works from GDScript
	// (matches the README quick-start). DEFVAL order is right-to-left.
	ClassDB::bind_method(D_METHOD("add_mapping", "event", "action", "modifiers", "triggers", "consumes"), &EIMappingContext::add_mapping,
			DEFVAL(TypedArray<EIModifier>()), DEFVAL(TypedArray<EITrigger>()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("remove_mapping", "event", "action"), &EIMappingContext::remove_mapping);
	ClassDB::bind_method(D_METHOD("clear_mappings"), &EIMappingContext::clear_mappings);

	ClassDB::bind_method(D_METHOD("set_mappings", "mappings"), &EIMappingContext::set_mappings);
	ClassDB::bind_method(D_METHOD("get_mappings"), &EIMappingContext::get_mappings);
	ClassDB::bind_method(D_METHOD("get_mapping_count"), &EIMappingContext::get_mapping_count);

	ClassDB::bind_method(D_METHOD("set_context_name", "name"), &EIMappingContext::set_context_name);
	ClassDB::bind_method(D_METHOD("get_context_name"), &EIMappingContext::get_context_name);

	// Inspector: a single array-of-dictionaries property. The hint
	// tells the inspector to render each dict as a row; the array type
	// hint is the element dict's schema.
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "mappings", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"),
			"set_mappings", "get_mappings");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "context_name"), "set_context_name", "get_context_name");
}

} // namespace ei

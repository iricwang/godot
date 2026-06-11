/**************************************************************************/
/* ei_action.cpp */
/**************************************************************************/
/* This file is part of: */
/* GODOT ENGINE */
/* https://godotengine.org */
/**************************************************************************/
/* Copyright (c)2014-present Godot Engine contributors (see AUTHORS.md). */
/* */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the */
/* "Software"), to deal in the Software without restriction, including */
/* without limitation the rights to use, copy, modify, merge, publish, */
/* distribute, sublicense, and/or sell copies of the Software, and to */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions: */
/* */
/* The above copyright notice and this permission notice shall be */
/* included in all copies or substantial portions of the Software. */
/* */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/**************************************************************************/

#include "ei_action.h"

#include "core/object/class_db.h"

namespace ei {

// --- ValueType -----------------------------------------------------------

void EIAction::set_value_type(ValueType p_t) {
	_value_type = p_t;
}

EIAction::ValueType EIAction::get_value_type() const {
	return _value_type;
}

// --- default_modifiers ---------------------------------------------------

void EIAction::set_default_modifiers(const TypedArray<EIModifier> &p_mods) {
	_default_modifiers = p_mods;
}

TypedArray<EIModifier> EIAction::get_default_modifiers() const {
	return _default_modifiers;
}

// --- default_triggers (P4: narrowed to TypedArray<EITrigger>) -----------

void EIAction::set_default_triggers(const TypedArray<EITrigger> &p_trigs) {
	_default_triggers = p_trigs;
}

TypedArray<EITrigger> EIAction::get_default_triggers() const {
	return _default_triggers;
}

// --- description ---------------------------------------------------------

void EIAction::set_description(const String &p_desc) {
	_description = p_desc;
}

String EIAction::get_description() const {
	return _description;
}

// --- bindings ------------------------------------------------------------

void EIAction::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value_type", "value_type"), &EIAction::set_value_type);
	ClassDB::bind_method(D_METHOD("get_value_type"), &EIAction::get_value_type);

	ClassDB::bind_method(D_METHOD("set_default_modifiers", "modifiers"), &EIAction::set_default_modifiers);
	ClassDB::bind_method(D_METHOD("get_default_modifiers"), &EIAction::get_default_modifiers);

	ClassDB::bind_method(D_METHOD("set_default_triggers", "triggers"), &EIAction::set_default_triggers);
	ClassDB::bind_method(D_METHOD("get_default_triggers"), &EIAction::get_default_triggers);

	ClassDB::bind_method(D_METHOD("set_description", "description"), &EIAction::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &EIAction::get_description);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "value_type", PROPERTY_HINT_ENUM, "Bool,Axis1D,Axis2D,Axis3D"), "set_value_type", "get_value_type");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "default_modifiers", PROPERTY_HINT_ARRAY_TYPE, "EIModifier"), "set_default_modifiers", "get_default_modifiers");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "default_triggers", PROPERTY_HINT_ARRAY_TYPE, "EITrigger"), "set_default_triggers", "get_default_triggers");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description"), "set_description", "get_description");
}

} // namespace ei

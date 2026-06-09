/**************************************************************************/
/*  value_converter.cpp                                                    */
/**************************************************************************/

#include "value_converter.h"

#include "core/object/class_db.h"
#include "core/string/ustring.h"

// ---- ValueConverter base ----

Variant ValueConverter::_convert(const Variant &p_value) {
	return p_value; // Default: no conversion.
}

Variant ValueConverter::_convert_back(const Variant &p_value) {
	return p_value; // Default: no conversion.
}

Variant ValueConverter::convert(const Variant &p_value) {
	// GDScript 4 cannot override C++ virtuals, so we duck-type:
	// if a GDScript subclass defines `_convert`, dispatch to it via `call()`.
	if (get_script_instance() != nullptr && has_method("_convert")) {
		return call("_convert", p_value);
	}
	return _convert(p_value);
}

Variant ValueConverter::convert_back(const Variant &p_value) {
	if (get_script_instance() != nullptr && has_method("_convert_back")) {
		return call("_convert_back", p_value);
	}
	return _convert_back(p_value);
}

void ValueConverter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("convert", "value"), &ValueConverter::convert);
	ClassDB::bind_method(D_METHOD("convert_back", "value"), &ValueConverter::convert_back);
}

// ---- IntToStringConverter ----

Variant IntToStringConverter::_convert(const Variant &p_value) {
	if (p_value.get_type() == Variant::INT) {
		return String::num_int64((int64_t)p_value);
	}
	return p_value;
}

Variant IntToStringConverter::_convert_back(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING) {
		return p_value.operator String().to_int();
	}
	return 0;
}

void IntToStringConverter::_bind_methods() {
}

// ---- FloatToPercentConverter ----

Variant FloatToPercentConverter::_convert(const Variant &p_value) {
	if (p_value.get_type() == Variant::FLOAT) {
		float f = p_value;
		return vformat("%.0f%%", f * 100.0f);
	}
	return p_value;
}

Variant FloatToPercentConverter::_convert_back(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING) {
		String s = p_value;
		s = s.trim_suffix("%").strip_edges();
		return s.to_float() / 100.0f;
	}
	return 0.0f;
}

void FloatToPercentConverter::_bind_methods() {
}

// ---- BoolToTextConverter ----

Variant BoolToTextConverter::_convert(const Variant &p_value) {
	if (p_value.get_type() == Variant::BOOL) {
		return (bool)p_value ? true_text : false_text;
	}
	return p_value;
}

Variant BoolToTextConverter::_convert_back(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING) {
		String s = p_value;
		if (s == true_text) return true;
		if (s == false_text) return false;
	}
	return false;
}

void BoolToTextConverter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_true_text", "text"), &BoolToTextConverter::set_true_text);
	ClassDB::bind_method(D_METHOD("get_true_text"), &BoolToTextConverter::get_true_text);
	ClassDB::bind_method(D_METHOD("set_false_text", "text"), &BoolToTextConverter::set_false_text);
	ClassDB::bind_method(D_METHOD("get_false_text"), &BoolToTextConverter::get_false_text);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "true_text"), "set_true_text", "get_true_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "false_text"), "set_false_text", "get_false_text");
}

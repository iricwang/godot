/**************************************************************************/
/*  value_converter.h                                                      */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"

// ValueConverter interface for data transformation in bindings.
//
// Design rationale:
//   GDScript 4 cannot override C++ virtual methods, so a "pure virtual interface"
//   approach would not work. Instead, we use a duck-typed dispatch:
//   * C++ subclasses: override the protected virtual `_convert` / `_convert_back`
//     directly. The public `convert` / `convert_back` template method delegates.
//   * GDScript subclasses: define a method named `_convert` / `_convert_back`.
//     The public `convert` / `convert_back` template method detects this via
//     `has_method()` and dispatches via `call()`.
//
// The leading underscore on the hook names is intentional — it makes the
// "subclass responsibility" nature clear and avoids the GDScript parser's
// "method overrides native class method" warning (Godot 4 only warns when
// the names match exactly).
//
// Usage in GDScript:
//   class MyConverter extends ValueConverter:
//       func _convert(value) -> String:
//           return "Count: %d" % value
//       func _convert_back(value) -> int:
//           return int(value.trim_prefix("Count: ")))
//
//   @bind_property("text", "Label", converter=MyConverter.new())
//   var count: int
class ValueConverter : public RefCounted {
	GDCLASS(ValueConverter, RefCounted);

protected:
	static void _bind_methods();

	// Hooks for C++ subclasses. Default implementations are identity.
	virtual Variant _convert(const Variant &p_value);
	virtual Variant _convert_back(const Variant &p_value);

public:
	// Convert ViewModel value → View display value (e.g., int → "75%").
	// Template method: dispatches to GDScript `_convert` if defined, else to C++ virtual `_convert`.
	Variant convert(const Variant &p_value);

	// Convert View value → ViewModel value (e.g., "75%" → 0.75).
	Variant convert_back(const Variant &p_value);
};

// Built-in converters (C++ — override the protected virtual hooks).
class IntToStringConverter : public ValueConverter {
	GDCLASS(IntToStringConverter, ValueConverter);

protected:
	static void _bind_methods();

public:
	Variant _convert(const Variant &p_value) override;
	Variant _convert_back(const Variant &p_value) override;
};

class FloatToPercentConverter : public ValueConverter {
	GDCLASS(FloatToPercentConverter, ValueConverter);

protected:
	static void _bind_methods();

public:
	Variant _convert(const Variant &p_value) override;
	Variant _convert_back(const Variant &p_value) override;
};

class BoolToTextConverter : public ValueConverter {
	GDCLASS(BoolToTextConverter, ValueConverter);

	String true_text = "Yes";
	String false_text = "No";

protected:
	static void _bind_methods();

public:
	void set_true_text(const String &p_text) { true_text = p_text; }
	String get_true_text() const { return true_text; }
	void set_false_text(const String &p_text) { false_text = p_text; }
	String get_false_text() const { return false_text; }

	Variant _convert(const Variant &p_value) override;
	Variant _convert_back(const Variant &p_value) override;
};
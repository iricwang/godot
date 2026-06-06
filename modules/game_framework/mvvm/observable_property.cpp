/**************************************************************************/
/*  observable_property.cpp                                               */
/**************************************************************************/

#include "observable_property.h"

#include "core/object/class_db.h"

void ObservableProperty::set_value(const Variant &p_value) {
	if (value == p_value) {
		return; // no change, no notification
	}
	value = p_value;
	emit_signal("value_changed", value);
}

Variant ObservableProperty::get_value() const {
	return value;
}

void ObservableProperty::subscribe(const Callable &p_callable) {
	if (!p_callable.is_valid()) {
		return;
	}
	if (!is_connected("value_changed", p_callable)) {
		connect("value_changed", p_callable);
	}
}

void ObservableProperty::unsubscribe(const Callable &p_callable) {
	if (is_connected("value_changed", p_callable)) {
		disconnect("value_changed", p_callable);
	}
}

void ObservableProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "value"), &ObservableProperty::set_value);
	ClassDB::bind_method(D_METHOD("get_value"), &ObservableProperty::get_value);
	ClassDB::bind_method(D_METHOD("subscribe", "callable"), &ObservableProperty::subscribe);
	ClassDB::bind_method(D_METHOD("unsubscribe", "callable"), &ObservableProperty::unsubscribe);

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::NIL, "value")));
}

/**************************************************************************/
/*  observable_property.cpp                                               */
/**************************************************************************/

#include "observable_property.h"

#include "core/object/class_db.h"

void ObservableProperty::set_value(const Variant &p_value) {
	if (value == p_value) {
		return; // no change, no notification
	}

	if (_bulk_update) {
		// Always update the underlying value so get_value() returns the latest,
		// but suppress the notification until end_bulk_update(). Notification is fired
		// only if the final value differs from _bulk_start_value.
		value = p_value;
		return;
	}

	value = p_value;
	emit_signal("value_changed", value);
}

Variant ObservableProperty::get_value() const {
	return value;
}

void ObservableProperty::begin_bulk_update() {
	_bulk_update = true;
	_bulk_start_value = value;
}

void ObservableProperty::end_bulk_update() {
	_bulk_update = false;
	// Only emit if the final value actually differs from what the subscribers last saw.
	if (value != _bulk_start_value) {
		emit_signal("value_changed", value);
	}
	_bulk_start_value = Variant();
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
	ClassDB::bind_method(D_METHOD("begin_bulk_update"), &ObservableProperty::begin_bulk_update);
	ClassDB::bind_method(D_METHOD("end_bulk_update"), &ObservableProperty::end_bulk_update);
	ClassDB::bind_method(D_METHOD("is_in_bulk_update"), &ObservableProperty::is_in_bulk_update);
	ClassDB::bind_method(D_METHOD("subscribe", "callable"), &ObservableProperty::subscribe);
	ClassDB::bind_method(D_METHOD("unsubscribe", "callable"), &ObservableProperty::unsubscribe);

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::NIL, "value")));
}

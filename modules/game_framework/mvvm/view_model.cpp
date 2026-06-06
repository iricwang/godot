/**************************************************************************/
/*  view_model.cpp                                                        */
/**************************************************************************/

#include "view_model.h"

#include "core/object/class_db.h"

Ref<ObservableProperty> ViewModel::get_property(const StringName &p_name) {
	HashMap<StringName, Ref<ObservableProperty>>::Iterator it = props.find(p_name);
	if (it) {
		return it->value;
	}
	Ref<ObservableProperty> prop;
	prop.instantiate();
	props[p_name] = prop;
	return prop;
}

bool ViewModel::has_property(const StringName &p_name) const {
	return props.has(p_name);
}

void ViewModel::set_property(const StringName &p_name, const Variant &p_value) {
	get_property(p_name)->set_value(p_value);
}

Variant ViewModel::get_value(const StringName &p_name) const {
	HashMap<StringName, Ref<ObservableProperty>>::ConstIterator it = props.find(p_name);
	return it ? it->value->get_value() : Variant();
}

void ViewModel::subscribe_property(const StringName &p_name, const Callable &p_callable) {
	get_property(p_name)->subscribe(p_callable);
}

PackedStringArray ViewModel::get_property_names() const {
	PackedStringArray out;
	for (const KeyValue<StringName, Ref<ObservableProperty>> &E : props) {
		out.push_back(String(E.key));
	}
	return out;
}

void ViewModel::dispose() {
	props.clear();
}

// ---- Godot Object overrides: native GDScript property syntax ----

bool ViewModel::_set(const StringName &p_name, const Variant &p_value) {
	set_property(p_name, p_value);
	return true; // handled — don't fall back to default Object behavior
}

bool ViewModel::_get(const StringName &p_name, Variant &r_ret) const {
	HashMap<StringName, Ref<ObservableProperty>>::ConstIterator it = props.find(p_name);
	if (it) {
		r_ret = it->value->get_value();
		return true;
	}
	return false; // not found — fall back to default
}

void ViewModel::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const KeyValue<StringName, Ref<ObservableProperty>> &E : props) {
		PropertyInfo pi;
		pi.name = E.key;
		pi.type = E.value->get_value().get_type();
		pi.usage = PROPERTY_USAGE_DEFAULT;
		p_list->push_back(pi);
	}
}

void ViewModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_property", "name"), &ViewModel::get_property);
	ClassDB::bind_method(D_METHOD("has_property", "name"), &ViewModel::has_property);
	ClassDB::bind_method(D_METHOD("set_property", "name", "value"), &ViewModel::set_property);
	ClassDB::bind_method(D_METHOD("get_value", "name"), &ViewModel::get_value);
	ClassDB::bind_method(D_METHOD("subscribe_property", "name", "callable"), &ViewModel::subscribe_property);
	ClassDB::bind_method(D_METHOD("get_property_names"), &ViewModel::get_property_names);
	ClassDB::bind_method(D_METHOD("dispose"), &ViewModel::dispose);
}

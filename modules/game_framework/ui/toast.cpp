/**************************************************************************/
/*  toast.cpp                                                             */
/**************************************************************************/

#include "toast.h"

#include "core/object/class_db.h"
#include "core/object/object.h"

Ref<Toast> Toast::make_text(const String &p_text, double p_duration) {
	Ref<Toast> toast;
	toast.instantiate();
	toast->text = p_text;
	toast->duration = p_duration;
	return toast;
}

void Toast::set_text(const String &p_text) {
	text = p_text;
}

String Toast::get_text() const {
	return text;
}

void Toast::set_duration(double p_duration) {
	duration = p_duration;
}

double Toast::get_duration() const {
	return duration;
}

void Toast::set_owner(Object *p_owner) {
	owner_id = p_owner ? p_owner->get_instance_id() : ObjectID();
}

Object *Toast::get_owner() const {
	if (owner_id.is_valid()) {
		return ObjectDB::get_instance(owner_id);
	}
	return nullptr;
}

bool Toast::is_owned_by(ObjectID p_id) const {
	return owner_id == p_id;
}

void Toast::set_custom_scene(const String &p_path) {
	custom_scene = p_path;
}

String Toast::get_custom_scene() const {
	return custom_scene;
}

void Toast::_bind_methods() {
	ClassDB::bind_static_method("Toast", D_METHOD("make_text", "text", "duration"), &Toast::make_text, DEFVAL(2.0));

	ClassDB::bind_method(D_METHOD("set_text", "text"), &Toast::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &Toast::get_text);
	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &Toast::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &Toast::get_duration);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "set_duration", "get_duration");

	ClassDB::bind_method(D_METHOD("set_owner", "owner"), &Toast::set_owner);
	ClassDB::bind_method(D_METHOD("get_owner"), &Toast::get_owner);
	ClassDB::bind_method(D_METHOD("set_custom_scene", "path"), &Toast::set_custom_scene);
	ClassDB::bind_method(D_METHOD("get_custom_scene"), &Toast::get_custom_scene);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "custom_scene"), "set_custom_scene", "get_custom_scene");
}

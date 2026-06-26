/**************************************************************************/
/*  intent.cpp                                                            */
/**************************************************************************/

#include "intent.h"

#include "core/object/class_db.h"

void Intent::set_action(const String &p_action) {
	action = p_action;
}

String Intent::get_action() const {
	return action;
}

void Intent::set_data(const String &p_data) {
	data = p_data;
}

String Intent::get_data() const {
	return data;
}

void Intent::set_extras(const Dictionary &p_extras) {
	extras = p_extras;
}

Dictionary Intent::get_extras() const {
	return extras;
}

void Intent::set_flags(int p_flags) {
	flags = p_flags;
}

int Intent::get_flags() const {
	return flags;
}

void Intent::set_flag(Flag p_flag) {
	flags |= (int)p_flag;
}

bool Intent::has_flag(Flag p_flag) const {
	return (flags & (int)p_flag) != 0;
}

Ref<Intent> Intent::create(const String &p_action, int p_flags, const Dictionary &p_extras) {
	Ref<Intent> intent = memnew(Intent);
	intent->action = p_action;
	intent->flags = p_flags;
	intent->extras = p_extras;
	return intent;
}

void Intent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &Intent::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &Intent::get_action);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &Intent::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &Intent::get_data);
	ClassDB::bind_method(D_METHOD("set_extras", "extras"), &Intent::set_extras);
	ClassDB::bind_method(D_METHOD("get_extras"), &Intent::get_extras);
	ClassDB::bind_method(D_METHOD("set_flags", "flags"), &Intent::set_flags);
	ClassDB::bind_method(D_METHOD("get_flags"), &Intent::get_flags);
	ClassDB::bind_method(D_METHOD("set_flag", "flag"), &Intent::set_flag);
	ClassDB::bind_method(D_METHOD("has_flag", "flag"), &Intent::has_flag);
	ClassDB::bind_static_method("Intent", D_METHOD("create", "action", "flags", "extras"), &Intent::create, DEFVAL(0), DEFVAL(Dictionary()));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data"), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "extras"), "set_extras", "get_extras");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "flags", PROPERTY_HINT_FLAGS, "SingleTop:1,ClearTop:2,NoHistory:4,ReorderToFront:8,NewClear:16,LoadSync:32"), "set_flags", "get_flags");

	BIND_ENUM_CONSTANT(FLAG_SINGLE_TOP);
	BIND_ENUM_CONSTANT(FLAG_CLEAR_TOP);
	BIND_ENUM_CONSTANT(FLAG_NO_HISTORY);
	BIND_ENUM_CONSTANT(FLAG_REORDER_TO_FRONT);
	BIND_ENUM_CONSTANT(FLAG_NEW_CLEAR);
	BIND_ENUM_CONSTANT(FLAG_LOAD_SYNC);
	BIND_ENUM_CONSTANT(FLAG_SCENE);
}

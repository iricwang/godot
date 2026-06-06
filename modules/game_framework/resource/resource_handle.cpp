/**************************************************************************/
/*  resource_handle.cpp                                                   */
/**************************************************************************/

#include "resource_handle.h"

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

void ResourceHandle::set_path(const String &p_path) {
	path = p_path;
}

String ResourceHandle::get_path() const {
	return path;
}

ResourceHandle::State ResourceHandle::get_state() const {
	return state;
}

bool ResourceHandle::is_loaded() const {
	return state == STATE_LOADED && resource.is_valid();
}

Ref<Resource> ResourceHandle::get_resource() const {
	return resource;
}

Ref<Resource> ResourceHandle::load_sync() {
	if (is_loaded()) {
		return resource;
	}
	state = STATE_LOADING;
	Error err = OK;
	Ref<Resource> res = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	if (res.is_valid()) {
		resource = res;
		state = STATE_LOADED;
		emit_signal("loaded", res);
	} else {
		state = STATE_FAILED;
		emit_signal("failed", path);
	}
	return res;
}

void ResourceHandle::_set_state(State p_state) {
	state = p_state;
}

void ResourceHandle::_set_resource(const Ref<Resource> &p_resource) {
	resource = p_resource;
}

void ResourceHandle::_emit_loaded(const Ref<Resource> &p_resource) {
	emit_signal("loaded", p_resource);
}

void ResourceHandle::_emit_failed() {
	emit_signal("failed", path);
}

void ResourceHandle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_path"), &ResourceHandle::get_path);
	ClassDB::bind_method(D_METHOD("get_state"), &ResourceHandle::get_state);
	ClassDB::bind_method(D_METHOD("is_loaded"), &ResourceHandle::is_loaded);
	ClassDB::bind_method(D_METHOD("get_resource"), &ResourceHandle::get_resource);
	ClassDB::bind_method(D_METHOD("load_sync"), &ResourceHandle::load_sync);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_state");

	ADD_SIGNAL(MethodInfo("loaded", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
	ADD_SIGNAL(MethodInfo("failed", PropertyInfo(Variant::STRING, "path")));

	BIND_ENUM_CONSTANT(STATE_UNLOADED);
	BIND_ENUM_CONSTANT(STATE_LOADING);
	BIND_ENUM_CONSTANT(STATE_LOADED);
	BIND_ENUM_CONSTANT(STATE_FAILED);
}

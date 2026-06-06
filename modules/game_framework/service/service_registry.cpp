/**************************************************************************/
/*  service_registry.cpp                                                  */
/**************************************************************************/

#include "service_registry.h"

#include "core/object/class_db.h"

void ServiceRegistry::register_service(const StringName &p_name, Object *p_service) {
	ERR_FAIL_NULL_MSG(p_service, "Cannot register a null service.");
	services[p_name] = p_service;
}

void ServiceRegistry::unregister_service(const StringName &p_name) {
	services.erase(p_name);
}

Object *ServiceRegistry::get_service(const StringName &p_name) const {
	HashMap<StringName, Object *>::ConstIterator it = services.find(p_name);
	return it ? it->value : nullptr;
}

bool ServiceRegistry::has_service(const StringName &p_name) const {
	return services.has(p_name);
}

PackedStringArray ServiceRegistry::get_service_names() const {
	PackedStringArray out;
	for (const KeyValue<StringName, Object *> &E : services) {
		out.push_back(String(E.key));
	}
	return out;
}

void ServiceRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_service", "name", "service"), &ServiceRegistry::register_service);
	ClassDB::bind_method(D_METHOD("unregister_service", "name"), &ServiceRegistry::unregister_service);
	ClassDB::bind_method(D_METHOD("get_service", "name"), &ServiceRegistry::get_service);
	ClassDB::bind_method(D_METHOD("has_service", "name"), &ServiceRegistry::has_service);
	ClassDB::bind_method(D_METHOD("get_service_names"), &ServiceRegistry::get_service_names);
}

ServiceRegistry::ServiceRegistry() {
}

ServiceRegistry::~ServiceRegistry() {
}

/**************************************************************************/
/*  service_registry.h                                                    */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

// A plain Object owned by Application that acts as a compile-time service registry.
// Modules register their services here by name, and other modules discover them at runtime
// by name + interface. This replaces native dynamic plugin loading (which is not available
// on mobile/web/console) while still giving the "code decoupling" the design asked for.
class ServiceRegistry : public Object {
	GDCLASS(ServiceRegistry, Object);

	HashMap<StringName, Object *> services;

protected:
	static void _bind_methods();

public:
	void register_service(const StringName &p_name, Object *p_service);
	void unregister_service(const StringName &p_name);
	Object *get_service(const StringName &p_name) const;
	bool has_service(const StringName &p_name) const;
	PackedStringArray get_service_names() const;

	ServiceRegistry();
	~ServiceRegistry();
};

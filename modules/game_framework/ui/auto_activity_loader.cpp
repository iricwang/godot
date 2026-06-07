/**************************************************************************/
/*  auto_activity_loader.cpp                                              */
/**************************************************************************/

#include "auto_activity_loader.h"

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

void AutoActivityLoader::set_base_path(const String &p_path) {
	base_path = p_path.trim_suffix("/");
}

String AutoActivityLoader::get_base_path() const {
	return base_path;
}

String AutoActivityLoader::resolve(const String &p_action) const {
	const String base = base_path;

	// Split "dir/name" → dir prefix + bare name.
	// e.g. "game/battle" → dir="game/", name="battle"
	//      "main"        → dir="",      name="main"
	String dir_prefix;
	String name;
	const int sep = p_action.rfind("/");
	if (sep >= 0) {
		dir_prefix = p_action.substr(0, sep + 1); // includes trailing slash
		name = p_action.substr(sep + 1);
	} else {
		name = p_action;
	}

	// Candidate list (first match wins):
	const String candidates[] = {
		base + "/" + dir_prefix + name + "/" + name + "_activity.tscn",
		base + "/" + dir_prefix + name + "/" + name + ".tscn",
		base + "/" + dir_prefix + name + "_activity.tscn",
		base + "/" + dir_prefix + name + ".tscn",
	};

	for (const String &candidate : candidates) {
		if (ResourceLoader::exists(candidate, "PackedScene")) {
			return candidate;
		}
	}

	return String();
}

void AutoActivityLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_path", "path"), &AutoActivityLoader::set_base_path);
	ClassDB::bind_method(D_METHOD("get_base_path"), &AutoActivityLoader::get_base_path);
	ClassDB::bind_method(D_METHOD("resolve", "action"), &AutoActivityLoader::resolve);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_path"), "set_base_path", "get_base_path");
}


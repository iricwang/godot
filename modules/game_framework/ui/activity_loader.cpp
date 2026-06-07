/**************************************************************************/
/*  activity_loader.cpp                                                   */
/**************************************************************************/

#include "activity_loader.h"

#include "core/object/class_db.h"

String ActivityLoader::resolve(const String &p_action) const {
	String result;
	GDVIRTUAL_CALL(_resolve, p_action, result);
	return result;
}

void ActivityLoader::_bind_methods() {
	GDVIRTUAL_BIND(_resolve, "action");
}


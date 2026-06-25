/**************************************************************************/
/*  activity_proxy.cpp                                                    */
/**************************************************************************/

#include "activity_proxy.h"

#include "../../context/application.h"
#include "../activity.h"

#include "core/object/class_db.h"

Activity *ActivityProxy::get_activity() const {
	return Object::cast_to<Activity>(ObjectDB::get_instance(instance_id));
}

void ActivityProxy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_no_history", "no_history"), &ActivityProxy::set_no_history);
	ClassDB::bind_method(D_METHOD("get_no_history"), &ActivityProxy::get_no_history);
	ClassDB::bind_method(D_METHOD("has_pending_new_intent"), &ActivityProxy::has_pending_new_intent);
	ClassDB::bind_method(D_METHOD("get_pending_new_intent"), &ActivityProxy::get_pending_new_intent);
	ClassDB::bind_method(D_METHOD("get_lifecycle_stage"), &ActivityProxy::get_lifecycle_stage);
	ClassDB::bind_method(D_METHOD("get_activity"), &ActivityProxy::get_activity);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_history", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_no_history");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lifecycle_stage", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_lifecycle_stage");

	BIND_ENUM_CONSTANT(LIFECYCLE_NONE);
	BIND_ENUM_CONSTANT(LIFECYCLE_STARTED);
	BIND_ENUM_CONSTANT(LIFECYCLE_RESUMED);
	BIND_ENUM_CONSTANT(LIFECYCLE_PAUSED);
	BIND_ENUM_CONSTANT(LIFECYCLE_STOPPED);
	BIND_ENUM_CONSTANT(LIFECYCLE_DESTROYED);
}

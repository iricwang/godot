/**************************************************************************/
/*  context_proxy.cpp                                                     */
/**************************************************************************/

#include "context_proxy.h"

#include "../../context/application.h"
#include "core/object/class_db.h"
#include "scene/main/node.h"

Node *ContextProxy::get_instance_node() const {
	if (instance_id.is_null()) {
		return nullptr;
	}
	Object *o = ObjectDB::get_instance(instance_id);
	return Object::cast_to<Node>(o);
}

void ContextProxy::_set_owner(Object *p_owner) {
	owner_id = p_owner ? p_owner->get_instance_id() : ObjectID();
}

void ContextProxy::_set_instance(Object *p_instance) {
	instance_id = p_instance ? p_instance->get_instance_id() : ObjectID();
}

void ContextProxy::_set_pause_owner(Object *p_owner) {
	pause_owner_id = p_owner ? p_owner->get_instance_id() : ObjectID();
}

void ContextProxy::cancel() {
	if (state == STATE_PENDING || state == STATE_LOADING) {
		state = STATE_CANCELLED;
		// Emit so [ActivityManager] can drain us from its stack/queue and resume
		// the activity we paused at push time. Idempotent on the manager side.
		emit_signal(SNAME("cancelled"));
	}
}

void ContextProxy::_emit_ready(Node *p_node) {
	emit_signal(SNAME("ready"), p_node);
}

void ContextProxy::_emit_failed(const String &p_reason) {
	emit_signal(SNAME("failed"), p_reason);
}

void ContextProxy::_emit_cancelled() {
	emit_signal(SNAME("cancelled"));
}

void ContextProxy::_emit_finished() {
	emit_signal(SNAME("finished"));
}

void ContextProxy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_state"), &ContextProxy::get_state);
	ClassDB::bind_method(D_METHOD("is_pending"), &ContextProxy::is_pending);
	ClassDB::bind_method(D_METHOD("is_loading"), &ContextProxy::is_loading);
	ClassDB::bind_method(D_METHOD("is_ready"), &ContextProxy::is_ready);
	ClassDB::bind_method(D_METHOD("is_terminal"), &ContextProxy::is_terminal);

	ClassDB::bind_method(D_METHOD("get_intent"), &ContextProxy::get_intent);
	ClassDB::bind_method(D_METHOD("get_action"), &ContextProxy::get_action);
	ClassDB::bind_method(D_METHOD("get_scene_path"), &ContextProxy::get_scene_path);
	ClassDB::bind_method(D_METHOD("get_application"), &ContextProxy::get_application);
	ClassDB::bind_method(D_METHOD("get_instance_node"), &ContextProxy::get_instance_node);

	ClassDB::bind_method(D_METHOD("cancel"), &ContextProxy::cancel);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_state");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "intent", PROPERTY_HINT_RESOURCE_TYPE, "Intent", PROPERTY_USAGE_READ_ONLY), "", "get_intent");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_scene_path");

	ADD_SIGNAL(MethodInfo("ready", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NODE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("failed", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("cancelled"));
	ADD_SIGNAL(MethodInfo("finished"));

	BIND_ENUM_CONSTANT(STATE_PENDING);
	BIND_ENUM_CONSTANT(STATE_LOADING);
	BIND_ENUM_CONSTANT(STATE_READY);
	BIND_ENUM_CONSTANT(STATE_FAILED);
	BIND_ENUM_CONSTANT(STATE_CANCELLED);
	BIND_ENUM_CONSTANT(STATE_FINISHED);
}

/**************************************************************************/
/*  dialog_proxy.cpp                                                      */
/**************************************************************************/

#include "dialog_proxy.h"

#include "../../context/application.h"
#include "../dialog.h"

#include "core/object/class_db.h"

Dialog *DialogProxy::get_dialog() const {
	return Object::cast_to<Dialog>(ObjectDB::get_instance(instance_id));
}

void DialogProxy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_dialog"), &DialogProxy::get_dialog);
}

/**************************************************************************/
/*  toast_proxy.cpp                                                       */
/**************************************************************************/

#include "toast_proxy.h"

#include "../../context/application.h"
#include "../toast.h"

#include "core/object/class_db.h"

Toast *ToastProxy::get_toast() const {
	return Object::cast_to<Toast>(ObjectDB::get_instance(instance_id));
}

void ToastProxy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_toast"), &ToastProxy::get_toast);
}

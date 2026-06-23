/**************************************************************************/
/*  context_base.inl  — template definitions, include in implementer .cpp */
/**************************************************************************/
#pragma once

#include "context_base.h"

#include "application.h"
#include "resource/resource_handle.h"
#include "resource/resource_manager.h"
#include "service/service_registry.h"
#include "ui/activity_manager.h"
#include "ui/intent.h"
#include "ui/toast.h"

template <typename Self>
Object *ContextBase<Self>::get_service(const StringName &p_name) const {
	Application *app = _ctx_app();
	ERR_FAIL_NULL_V(app, nullptr);
	return app->get_service_registry()->get_service(p_name);
}

template <typename Self>
bool ContextBase<Self>::has_service(const StringName &p_name) const {
	Application *app = _ctx_app();
	ERR_FAIL_NULL_V(app, false);
	return app->get_service_registry()->has_service(p_name);
}

template <typename Self>
Ref<ResourceHandle> ContextBase<Self>::get_resource_handle(const String &p_path) {
	Application *app = _ctx_app();
	ERR_FAIL_NULL_V(app, Ref<ResourceHandle>());
	return app->get_resource_manager()->get_handle(p_path);
}

template <typename Self>
Ref<Resource> ContextBase<Self>::load_resource_sync(const String &p_path) {
	Application *app = _ctx_app();
	ERR_FAIL_NULL_V(app, Ref<Resource>());
	return app->get_resource_manager()->load_sync(p_path);
}

template <typename Self>
void ContextBase<Self>::load_resource_async(const String &p_path, const Callable &p_callback, int p_priority) {
	Application *app = _ctx_app();
	ERR_FAIL_NULL(app);
	app->get_resource_manager()->load_async(p_path, p_callback, p_priority);
}

template <typename Self>
void ContextBase<Self>::start_activity(const Ref<Intent> &p_intent) {
	Application *app = _ctx_app();
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->start_activity(p_intent);
}

template <typename Self>
void ContextBase<Self>::start_activity_with(const String &p_action, int p_flags, const Dictionary &p_extras) {
	start_activity(Intent::create(p_action, p_flags, p_extras));
}

template <typename Self>
void ContextBase<Self>::finish_top() {
	Application *app = _ctx_app();
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->finish_top();
}

template <typename Self>
bool ContextBase<Self>::back() {
	Application *app = _ctx_app();
	ERR_FAIL_NULL_V(app, false);
	return app->get_activity_manager()->back();
}

template <typename Self>
void ContextBase<Self>::show_dialog(const Ref<Intent> &p_intent) {
	Application *app = _ctx_app();
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_dialog_with_owner(p_intent, static_cast<Self *>(this)->as_object());
}

template <typename Self>
void ContextBase<Self>::show_toast(Toast *p_toast) {
	Application *app = _ctx_app();
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_toast_with_owner(p_toast, static_cast<Self *>(this)->as_object());
}

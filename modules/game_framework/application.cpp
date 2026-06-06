/**************************************************************************/
/*  application.cpp                                                        */
/**************************************************************************/

#include "application.h"

#include "resource/resource_manager.h"
#include "service/service_registry.h"
#include "ui/activity_manager.h"
#include "ui/scene_service.h"

#include "core/object/class_db.h"
#include "scene/gui/control.h"

ServiceRegistry *Application::get_service_registry() const {
	return service_registry;
}

ResourceManager *Application::get_resource_manager() const {
	return resource_manager;
}

ActivityManager *Application::get_activity_manager() const {
	return activity_manager;
}

SceneService *Application::get_scene_service() const {
	return scene_service;
}

void Application::initialize(Control *p_root) {
	ERR_FAIL_COND_MSG(service_registry, "Application is already initialized. Call shutdown() first before re-initializing.");

	service_registry = memnew(ServiceRegistry);
	resource_manager = memnew(ResourceManager);
	activity_manager = memnew(ActivityManager);

	activity_manager->set_root(p_root);
	activity_manager->set_application(this);

	scene_service = memnew(SceneService);
	scene_service->set_application(this);
	scene_service->set_root(p_root);
}

void Application::shutdown() {
	if (activity_manager) {
		activity_manager->cleanup_all();
		memdelete(activity_manager);
		activity_manager = nullptr;
	}
	if (scene_service) {
		memdelete(scene_service);
		scene_service = nullptr;
	}
	if (resource_manager) {
		memdelete(resource_manager);
		resource_manager = nullptr;
	}
	if (service_registry) {
		memdelete(service_registry);
		service_registry = nullptr;
	}
}

void Application::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_service_registry"), &Application::get_service_registry);
	ClassDB::bind_method(D_METHOD("get_resource_manager"), &Application::get_resource_manager);
	ClassDB::bind_method(D_METHOD("get_activity_manager"), &Application::get_activity_manager);
	ClassDB::bind_method(D_METHOD("get_scene_service"), &Application::get_scene_service);

	ClassDB::bind_method(D_METHOD("initialize", "root"), &Application::initialize);
	ClassDB::bind_method(D_METHOD("shutdown"), &Application::shutdown);
}

Application::Application() {
	set_application(this);
}

Application::~Application() {
	if (activity_manager || scene_service || resource_manager || service_registry) {
		shutdown();
	}
}

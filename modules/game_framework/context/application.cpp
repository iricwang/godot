/**************************************************************************/
/*  application.cpp                                                        */
/**************************************************************************/

#include "application.h"

#include "../resource/resource_manager.h"
#include "../service/service_registry.h"
#include "../ui/activity_manager.h"
#include "../ui/scene_service.h"

#include "core/object/class_db.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

// ============================================================
// Application — _notification (receives OS events directly)
// ============================================================

void Application::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_EXIT_TREE: {
			// Auto-shutdown when the node leaves the scene tree.
			if (service_registry) {
				shutdown();
			}
		} break;

		case NOTIFICATION_OS_MEMORY_WARNING:
			dispatch_low_memory();
			break;
		case NOTIFICATION_TRANSLATION_CHANGED:
			dispatch_configuration_changed();
			break;
		case NOTIFICATION_APPLICATION_RESUMED:
			dispatch_resume();
			break;
		case NOTIFICATION_APPLICATION_PAUSED:
			dispatch_pause();
			break;
		case NOTIFICATION_APPLICATION_FOCUS_IN:
			dispatch_focus_in();
			break;
		case NOTIFICATION_APPLICATION_FOCUS_OUT:
			dispatch_focus_out();
			break;
		case NOTIFICATION_APPLICATION_PIP_MODE_ENTERED:
			dispatch_pip_entered();
			break;
		case NOTIFICATION_APPLICATION_PIP_MODE_EXITED:
			dispatch_pip_exited();
			break;
	}
}

// ============================================================
// Application — manager accessors
// ============================================================

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

// ============================================================
// Application — lifecycle dispatch
// ============================================================

void Application::dispatch_create() {
	emit_signal(SNAME("created"));
	GDVIRTUAL_CALL(_on_create);
}

void Application::dispatch_terminate() {
	emit_signal(SNAME("terminated"));
	GDVIRTUAL_CALL(_on_terminate);
}

void Application::dispatch_low_memory() {
	emit_signal(SNAME("low_memory"));
	GDVIRTUAL_CALL(_on_low_memory);
}

void Application::dispatch_configuration_changed() {
	emit_signal(SNAME("configuration_changed"));
	GDVIRTUAL_CALL(_on_configuration_changed);
}

void Application::dispatch_resume() {
	emit_signal(SNAME("resumed"));
	GDVIRTUAL_CALL(_on_resume);
}

void Application::dispatch_pause() {
	emit_signal(SNAME("paused"));
	GDVIRTUAL_CALL(_on_pause);
}

void Application::dispatch_focus_in() {
	emit_signal(SNAME("focus_in"));
	GDVIRTUAL_CALL(_on_focus_in);
}

void Application::dispatch_focus_out() {
	emit_signal(SNAME("focus_out"));
	GDVIRTUAL_CALL(_on_focus_out);
}

void Application::dispatch_pip_entered() {
	emit_signal(SNAME("pip_entered"));
	GDVIRTUAL_CALL(_on_pip_entered);
}

void Application::dispatch_pip_exited() {
	emit_signal(SNAME("pip_exited"));
	GDVIRTUAL_CALL(_on_pip_exited);
}

// ============================================================
// Application — resolution
// ============================================================

void Application::set_design_resolution(const Vector2i &p_size) {
	ERR_FAIL_NULL_MSG(get_tree(), "set_design_resolution() must be called after the node is in the scene tree.");
	get_tree()->get_root()->set_content_scale_size(p_size);
}

Vector2i Application::get_design_resolution() const {
	ERR_FAIL_NULL_V_MSG(get_tree(), Vector2i(), "get_design_resolution() must be called after the node is in the scene tree.");
	return get_tree()->get_root()->get_content_scale_size();
}

Vector2i Application::get_screen_size(int p_screen) const {
	DisplayServer *ds = DisplayServer::get_singleton();
	ERR_FAIL_NULL_V(ds, Vector2i());
	// p_screen == -1 equals DisplayServerEnums::SCREEN_OF_MAIN_WINDOW — resolved by DisplayServer.
	return ds->screen_get_size(p_screen);
}

void Application::set_window_size(const Vector2i &p_size) {
	DisplayServer *ds = DisplayServer::get_singleton();
	ERR_FAIL_NULL(ds);

	// Use the usable rect of the screen that currently hosts the main window
	// (excludes OS taskbar, dock, notch, etc.).
	const int screen = ds->window_get_current_screen();
	const Rect2i usable = ds->screen_get_usable_rect(screen);

	// If the requested size fits, use it as-is; otherwise scale it down proportionally.
	Vector2i size = p_size;
	if (size.x > usable.size.x || size.y > usable.size.y) {
		const float sx = static_cast<float>(usable.size.x) / static_cast<float>(size.x);
		const float sy = static_cast<float>(usable.size.y) / static_cast<float>(size.y);
		const float scale = MIN(sx, sy);
		size = Vector2i(int(static_cast<float>(size.x) * scale), int(static_cast<float>(size.y) * scale));
	}

	ds->window_set_size(size);

	// Re-center within the usable area.
	const Point2i centered = usable.position + (usable.size - size) / 2;
	ds->window_set_position(centered);
}

Vector2i Application::get_window_size() const {
	DisplayServer *ds = DisplayServer::get_singleton();
	ERR_FAIL_NULL_V(ds, Vector2i());
	return ds->window_get_size();
}

// ============================================================
// Application — initialize / shutdown
// ============================================================

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

	dispatch_create();
}

void Application::shutdown() {
	// Notify before tearing down — callbacks can still access managers safely.
	dispatch_terminate();

	if (activity_manager) {
		activity_manager->cleanup_all();
		memdelete(activity_manager);
		activity_manager = nullptr;
	}
	if (scene_service) {
		memdelete(scene_service);
		scene_service = nullptr;
	}
	if (service_registry) {
		memdelete(service_registry);
		service_registry = nullptr;
	}
	if (resource_manager) {
		memdelete(resource_manager);
		resource_manager = nullptr;
	}
}

// ============================================================
// Application — _bind_methods
// ============================================================

void Application::_bind_methods() {
	// Manager accessors
	ClassDB::bind_method(D_METHOD("get_service_registry"), &Application::get_service_registry);
	ClassDB::bind_method(D_METHOD("get_resource_manager"), &Application::get_resource_manager);
	ClassDB::bind_method(D_METHOD("get_activity_manager"), &Application::get_activity_manager);
	ClassDB::bind_method(D_METHOD("get_scene_service"), &Application::get_scene_service);

	// Core methods
	ClassDB::bind_method(D_METHOD("initialize", "root"), &Application::initialize);
	ClassDB::bind_method(D_METHOD("shutdown"), &Application::shutdown);

	// Resolution
	ClassDB::bind_method(D_METHOD("set_design_resolution", "size"), &Application::set_design_resolution);
	ClassDB::bind_method(D_METHOD("get_design_resolution"), &Application::get_design_resolution);
	ClassDB::bind_method(D_METHOD("get_screen_size", "screen"), &Application::get_screen_size, DEFVAL(-1));

	// Window size
	ClassDB::bind_method(D_METHOD("set_window_size", "size"), &Application::set_window_size);
	ClassDB::bind_method(D_METHOD("get_window_size"), &Application::get_window_size);

	// Signals — observer / connect() pattern
	ADD_SIGNAL(MethodInfo("created"));
	ADD_SIGNAL(MethodInfo("terminated"));
	ADD_SIGNAL(MethodInfo("low_memory"));
	ADD_SIGNAL(MethodInfo("configuration_changed"));
	ADD_SIGNAL(MethodInfo("resumed"));
	ADD_SIGNAL(MethodInfo("paused"));
	ADD_SIGNAL(MethodInfo("focus_in"));
	ADD_SIGNAL(MethodInfo("focus_out"));
	ADD_SIGNAL(MethodInfo("pip_entered"));
	ADD_SIGNAL(MethodInfo("pip_exited"));

	// Virtual methods — subclass / override pattern
	GDVIRTUAL_BIND(_on_create);
	GDVIRTUAL_BIND(_on_terminate);
	GDVIRTUAL_BIND(_on_low_memory);
	GDVIRTUAL_BIND(_on_configuration_changed);
	GDVIRTUAL_BIND(_on_resume);
	GDVIRTUAL_BIND(_on_pause);
	GDVIRTUAL_BIND(_on_focus_in);
	GDVIRTUAL_BIND(_on_focus_out);
	GDVIRTUAL_BIND(_on_pip_entered);
	GDVIRTUAL_BIND(_on_pip_exited);
}

// ============================================================
// Application — ctor / dtor
// ============================================================

Application::Application() {
	set_application(this);
}

Application::~Application() {
	if (activity_manager || scene_service || resource_manager || service_registry) {
		shutdown();
	}
}

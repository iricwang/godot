/**************************************************************************/
/*  application.h                                                          */
/**************************************************************************/
#pragma once

#include "context.h"

class ServiceRegistry;
class ResourceManager;
class ActivityManager;
class SceneService;
class Control;

// The root of the Android-style Context hierarchy. Application extends Context and owns all
// plugin / service instances (ServiceRegistry, ResourceManager, ActivityManager, etc.) as
// member variables instead of global singletons.
//
// Usage (GDScript):
//   var app = Application.new()
//   app.initialize($UIRoot)
//   app.register_activity("main", "res://...")
//   app.start_activity(Intent.create("main"))
class Application : public Context {
	GDCLASS(Application, Context);

	ServiceRegistry *service_registry = nullptr;
	ResourceManager *resource_manager = nullptr;
	ActivityManager *activity_manager = nullptr;
	SceneService *scene_service = nullptr;

protected:
	static void _bind_methods();

public:
	// Access owned managers (non-null after initialize()).
	ServiceRegistry *get_service_registry() const;
	ResourceManager *get_resource_manager() const;
	ActivityManager *get_activity_manager() const;
	SceneService *get_scene_service() const;

	// Set up all managers, wire them together, and set the root container.
	void initialize(Control *p_root);

	// Reverse of initialize(): drain all activities / dialogs, then destroy managers.
	void shutdown();

	Application();
	~Application();
};

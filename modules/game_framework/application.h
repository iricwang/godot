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

// Application — root of the Android-style Context hierarchy.
// Extends Context (which extends Node), so it can be added directly to the scene
// tree as any other node: drag it in, set it as scene root, or use it as an AutoLoad.
//
// Android-style lifecycle — override in GDScript or connect to the matching signal:
//   _on_create()                — call initialize() to trigger (also auto-fires on READY
//                                 if ui_root is set via set_ui_root())
//   _on_terminate()             — fires on EXIT_TREE (or explicit shutdown())
//   _on_low_memory()            — NOTIFICATION_OS_MEMORY_WARNING
//   _on_configuration_changed() — NOTIFICATION_TRANSLATION_CHANGED
//   _on_resume()                — NOTIFICATION_APPLICATION_RESUMED
//   _on_pause()                 — NOTIFICATION_APPLICATION_PAUSED
//   _on_focus_in()              — NOTIFICATION_APPLICATION_FOCUS_IN
//   _on_focus_out()             — NOTIFICATION_APPLICATION_FOCUS_OUT
//   _on_pip_entered()           — NOTIFICATION_APPLICATION_PIP_MODE_ENTERED
//   _on_pip_exited()            — NOTIFICATION_APPLICATION_PIP_MODE_EXITED
//
// Typical scene setup:
//   - Add an Application node to your scene (or use it as the root).
//   - Call initialize($UIContainer) from _ready() to wire up managers.
//   - Application auto-shuts-down when it exits the scene tree.
class Application : public Context {
	GDCLASS(Application, Context);

	ServiceRegistry *service_registry = nullptr;
	ResourceManager *resource_manager = nullptr;
	ActivityManager *activity_manager = nullptr;
	SceneService *scene_service = nullptr;

protected:
	static void _bind_methods();

	// Receives OS / window notifications directly (Application IS a Node now).
	void _notification(int p_what);

	// GDScript-overridable Android Application lifecycle hooks.
	GDVIRTUAL0(_on_create)
	GDVIRTUAL0(_on_terminate)
	GDVIRTUAL0(_on_low_memory)
	GDVIRTUAL0(_on_configuration_changed)
	GDVIRTUAL0(_on_resume)
	GDVIRTUAL0(_on_pause)
	GDVIRTUAL0(_on_focus_in)
	GDVIRTUAL0(_on_focus_out)
	GDVIRTUAL0(_on_pip_entered)
	GDVIRTUAL0(_on_pip_exited)

public:
	// Access owned managers (non-null after initialize()).
	ServiceRegistry *get_service_registry() const;
	ResourceManager *get_resource_manager() const;
	ActivityManager *get_activity_manager() const;
	SceneService *get_scene_service() const;

	// Lifecycle dispatch (called internally; may also be called manually in tests).
	void dispatch_create();
	void dispatch_terminate();
	void dispatch_low_memory();
	void dispatch_configuration_changed();
	void dispatch_resume();
	void dispatch_pause();
	void dispatch_focus_in();
	void dispatch_focus_out();
	void dispatch_pip_entered();
	void dispatch_pip_exited();

	// ---- Resolution ----
	// The design resolution is the virtual size all UI / game content is authored at.
	// The engine's viewport scaling system maps it to the physical display automatically.
	// Must be called after the node is in the tree (get_tree() must be valid).
	void set_design_resolution(const Vector2i &p_size);
	Vector2i get_design_resolution() const;

	// Returns the physical pixel size of the given screen.
	// Pass -1 (default) to query the screen that hosts the main window.
	Vector2i get_screen_size(int p_screen = -1) const;

	// ---- Window size ----
	// Sets the OS window size to p_size, then:
	//   • clamps it (preserving aspect ratio) so it never exceeds the usable screen area,
	//   • re-centers the window on the screen.
	// Useful for preview / desktop builds where the design resolution would be too large.
	void set_window_size(const Vector2i &p_size);
	Vector2i get_window_size() const;

	// Wire up managers and mark the Control that hosts activities / dialogs / toasts.
	// Call this from _ready() after the node is in the tree.
	void initialize(Control *p_root);

	// Tear down managers explicitly. Called automatically on EXIT_TREE if not already done.
	void shutdown();

	Application();
	~Application();
};

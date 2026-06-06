/**************************************************************************/
/*  scene_service.h                                                        */
/**************************************************************************/
#pragma once

#include "core/object/object.h"

class Application;
class Control;
class Node;
class PackedScene;

// Manages scene-graph tree replacement under the root Control. Owned by Application.
// Unlike Godot's get_tree().change_scene_to_file(), SceneService keeps the root
// Control alive — it only swaps the child subtree. This means Application (and the
// GDScript that owns it) survives scene changes intact.
//
// Usage from GDScript:
//   app.get_context().change_scene("res://scenes/gameplay.tscn")
//   # root's child is now the instantiated gameplay scene
//   # app, ServiceRegistry, ResourceManager, ActivityManager all still valid
class SceneService : public Object {
	GDCLASS(SceneService, Object);

	Application *app = nullptr;
	Control *root = nullptr;        // the permanent root Control (never freed)
	Node *current_root = nullptr;   // the current scene subtree child
	String current_scene;           // path of the current scene
	String next_scene;              // path being loaded (empty = not loading)
	double loading_progress = 0.0;  // 0.0 → 1.0
	bool polling_connected = false;

	void _poll();
	void _ensure_polling();
	void _apply_scene(Node *p_new_root, const String &p_path);

protected:
	static void _bind_methods();

public:
	void set_application(Application *p_app);
	void set_root(Control *p_root);

	// Async load + replace root child when ready.
	void change_scene(const String &p_path);
	void change_scene_with_callback(const String &p_path, const Callable &p_callback);

	// Synchronous load (blocks; use only for tiny scenes or loading screens).
	void change_scene_sync(const String &p_path);

	// Preload a scene into cache without swapping.
	void preload_scene(const String &p_path);
	void release_preload(const String &p_path);

	// Query
	String get_current_scene() const;
	double get_loading_progress() const;
	bool is_loading() const;

	SceneService();
};

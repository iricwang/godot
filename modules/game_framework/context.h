/**************************************************************************/
/*  context.h                                                              */
/**************************************************************************/
#pragma once

#include "scene/main/node.h"

class Application;
class Activity;
class Dialog;
class ResourceHandle;
class ViewModel;
class Node;
class Resource;
class Intent;
class Toast;

// Android-style Context. Provides a unified access point for application-wide services.
// Application extends Context and owns all plugin instances; Activity / Dialog / Toast
// hold a Context* to reach those services through a common API.
//
// Context extends Node so that Application (and any future Context subclass) can live
// in the scene tree and receive engine notifications directly — no proxy needed.
class Context : public Node {
	GDCLASS(Context, Node);

	Application *app = nullptr;

protected:
	static void _bind_methods();

public:
	// ---- Application linkage ----
	void set_application(Application *p_app);
	Application *get_application() const;

	// ---- Service lookup ----
	Object *get_service(const StringName &p_name) const;
	bool has_service(const StringName &p_name) const;

	// ---- Resource loading ----
	Ref<ResourceHandle> get_resource_handle(const String &p_path);
	Ref<Resource> load_resource_sync(const String &p_path);
	void load_resource_async(const String &p_path, const Callable &p_callback = Callable(), int p_priority = 0);

	// ---- Navigation ----
	void start_activity(const Ref<Intent> &p_intent);
	// Convenience: build an Intent and start it in one call.
	void start_activity_with(const String &p_action, int p_flags = 0, const Dictionary &p_extras = Dictionary());
	void finish_activity(Activity *p_activity);
	void finish_top();
	bool back();
	Activity *get_current_activity() const;
	int get_stack_size() const;

	// ---- Overlays ----
	void show_dialog(const Ref<Intent> &p_intent);
	void show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner);
	void dismiss_dialog(Dialog *p_dialog);
	void show_toast(const Ref<Toast> &p_toast);
	void show_toast_with_owner(const Ref<Toast> &p_toast, Object *p_owner);
	void clear_all_toasts();
	void clear_toasts_by_owner(Object *p_owner);

	// ---- Activity registration (delegates to ActivityManager) ----
	void register_activity(const String &p_action, const String &p_scene_path);

	// ---- Scene switching (delegates to SceneService) ----
	void change_scene(const String &p_path);
	void change_scene_sync(const String &p_path);
	String get_current_scene() const;

	// ---- MVVM (static, delegates to BindingEngine) ----
	static void bind(Node *p_view, ViewModel *p_vm);
	static void bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property);
	static void bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method);

	// ---- Toast factory (static) ----
	static Ref<Toast> make_toast(const String &p_text, double p_duration = 2.0);
};

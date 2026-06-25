/**************************************************************************/
/*  context.h                                                              */
/**************************************************************************/
#pragma once

#include "context_base.h"

#include "scene/main/node.h"

class Activity;
class Dialog;
class ResourceHandle;
class ViewModel;
class ValueConverter;
class Node;
class Resource;
class Intent;
class Toast;
class ActivityProxy;
class DialogProxy;
class ToastProxy;

// Android-style Context. Provides a unified access point for application-wide services.
// Application extends Context and owns all plugin instances; Activity / Dialog also implement
// IContext (via ContextBase<Self>) so they accept the same call sites.
//
// Inheritance: Context inherits ContextBase<Context> to get the 12 generic delegates
// (start_activity / show_toast / get_service / load_resource_async / ...) for free.
// Context-specific methods (state queries, owner overloads, scene service, MVVM statics)
// are still implemented here directly.
//
// Why GDCLASS still inherits Node (not IContext): Application needs to live in the SceneTree
// so it can receive engine notifications. GDCLASS allows only single inheritance, so the
// IContext side is non-GDCLASS — GDScript-level `is Context` checks rely on the Context
// class chain, not IContext.
class Context : public Node, public ContextBase<Context> {
	GDCLASS(Context, Node);

	Application *app = nullptr;

protected:
	static void _bind_methods();

	void try_launch();

public:
	// ---- IContext implementation ----
	Application *get_application() const override { return app; }
	Object *as_object() override { return this; }

	// ---- Application linkage ----
	void set_application(Application *p_app);

	// ---- State queries (Context-only — Activity/Dialog don't need these) ----
	Activity *get_current_activity() const;
	int get_stack_size() const;

	// ---- Owner / explicit-target overloads (Application-level operations) ----
	void finish_activity(Activity *p_activity);
	Ref<DialogProxy> show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner);
	void dismiss_dialog(Dialog *p_dialog);
	Ref<ToastProxy> show_toast_with_owner(Toast *p_toast, Object *p_owner);
	void clear_all_toasts();
	void clear_toasts_by_owner(Object *p_owner);

	// ---- Activity registration (Application configuration) ----
	void register_activity(const String &p_action, const String &p_scene_path);

	// ---- Scene switching (delegates to SceneService) ----
	void change_scene(const String &p_path);
	void change_scene_sync(const String &p_path);
	String get_current_scene() const;

	// ---- MVVM (static, delegates to BindingEngine) ----
	static void bind(Node *p_view, ViewModel *p_vm);
	static void bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property, int p_mode = 0, const Ref<ValueConverter> &p_converter = Ref<ValueConverter>());
	static void bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method);

	// ---- Toast factory (static) ----
	static Toast *make_toast(const String &p_text, double p_duration = 2.0);
};

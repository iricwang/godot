/**************************************************************************/
/*  context_base.h                                                        */
/**************************************************************************/
#pragma once

#include "context_interface.h"

#include "core/error/error_macros.h"
#include "core/io/resource.h"
#include "core/string/string_name.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

// Forward declarations only — concrete types resolved at template-instantiation time
// in the implementing class's .cpp. This avoids a header cycle through
// application.h → context.h → context_base.h.
class Application;
class Intent;
class Toast;
class ResourceHandle;
class ActivityProxy;
class DialogProxy;
class ToastProxy;

// ContextBase — CRTP mixin providing the full Context-style delegate API.
//
// Inherit as `class Foo : public Control, public ContextBase<Foo>`. Foo automatically
// gets all the convenience methods (start_activity / show_toast / get_service / ...)
// that route through `static_cast<Foo *>(this)->get_application()`.
//
// Foo MUST implement (both come from IContext, both pure virtual):
//   * Application *get_application() const  — which Application owns me
//   * Object *as_object()                   — `return this;` — used as the
//                                              lifecycle owner for spawned Dialogs / Toasts
//
// Why CRTP: show_toast / show_dialog need to pass `this` (the most-derived pointer) as the
// lifecycle owner so ActivityManager can dismiss them when the owner is destroyed. CRTP lets
// us forward `static_cast<Self *>(this)` directly — no dynamic_cast at call sites.
//
// All methods route through `app->get_xxx_manager()->yyy()`. The implementations live in
// context_base.inl, which the implementer's .cpp must include after pulling in application.h
// and the relevant manager headers. (Template definitions only need to be visible at the
// point of instantiation, not declaration.)
template <typename Self>
class ContextBase : public IContext {
public:
	// ---- Service lookup ----
	Object *get_service(const StringName &p_name) const;
	bool has_service(const StringName &p_name) const;

	// ---- Resource loading ----
	Ref<ResourceHandle> get_resource_handle(const String &p_path);
	Ref<Resource> load_resource_sync(const String &p_path);
	void load_resource_async(const String &p_path, const Callable &p_callback = Callable(), int p_priority = 0);

	// ---- Navigation ----
	Ref<ActivityProxy> start_activity(const Ref<Intent> &p_intent);
	Ref<ActivityProxy> start_activity_with(const String &p_action, int p_flags = 0, const Dictionary &p_extras = Dictionary());
	void finish_top();
	bool back();

	// ---- Overlays (pass `this` as the lifecycle owner) ----
	Ref<DialogProxy> show_dialog(const Ref<Intent> &p_intent);
	Ref<ToastProxy> show_toast(Toast *p_toast);

protected:
	Application *_ctx_app() const { return static_cast<const Self *>(this)->get_application(); }
};

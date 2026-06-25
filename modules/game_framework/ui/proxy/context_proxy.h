/**************************************************************************/
/*  context_proxy.h                                                       */
/**************************************************************************/
#pragma once

#include "core/object/object_id.h"
#include "core/object/ref_counted.h"
#include "core/string/string_name.h"

#include "../intent.h"

class Application;
class Node;

// Lightweight async record that ActivityManager holds in its stack/dialogs/toast_queue
// instead of (or before) the real Activity/Dialog/Toast Node*. A ContextProxy is created
// synchronously at the call site, so flag-matching (SINGLE_TOP/CLEAR_TOP/...) and
// stack-size queries are immediately consistent even while the underlying scene is still
// loading on a background thread.
//
// State machine:
//   PENDING -> LOADING -> READY ----+
//                  |                |
//                  v                v
//              CANCELLED        FINISHED
//                  ^                ^
//                  |                |
//                  +---- FAILED ----+
//
// PENDING:    proxy exists in a queue but its load hasn't started yet (Toast SERIAL queue).
// LOADING:    ResourceLoader::load_threaded_request is in flight. Toasts never enter
//             this state — they're constructed by the caller as fully-built Nodes and
//             go PENDING (in the SERIAL queue) → READY (when pumped to `_present_toast`).
// READY:      node has been instantiated, parented and dispatched _on_create.
// FAILED:     load/instantiation failed; emit_failed has fired.
// CANCELLED:  user finished/dismissed/cleared before LOADING completed.
// FINISHED:   node was READY and has now been destroyed via the normal lifecycle.
//
// Subclasses store the concrete Node* in instance_id and provide a typed get_instance().
class ContextProxy : public RefCounted {
	GDCLASS(ContextProxy, RefCounted);

public:
	enum State {
		STATE_PENDING,
		STATE_LOADING,
		STATE_READY,
		STATE_FAILED,
		STATE_CANCELLED,
		STATE_FINISHED,
	};

protected:
	State state = STATE_PENDING;
	Ref<Intent> intent;
	String scene_path;
	Application *app = nullptr;
	ObjectID owner_id; // optional lifecycle owner (Activity/Application/Dialog) — same semantics as today
	ObjectID instance_id; // populated when the node is attached
	ObjectID pause_owner_id; // Activity that was paused when this proxy was pushed — needs resume on rollback/cancel

	static void _bind_methods();

public:
	// ---- State ----
	State get_state() const { return state; }
	bool is_pending() const { return state == STATE_PENDING; }
	bool is_loading() const { return state == STATE_LOADING; }
	bool is_ready() const { return state == STATE_READY; }
	bool is_terminal() const { return state == STATE_FAILED || state == STATE_CANCELLED || state == STATE_FINISHED; }

	// ---- Identity ----
	Ref<Intent> get_intent() const { return intent; }
	String get_action() const { return intent.is_valid() ? intent->get_action() : String(); }
	String get_scene_path() const { return scene_path; }

	// ---- Linkage ----
	Application *get_application() const { return app; }
	ObjectID get_owner_id() const { return owner_id; }
	ObjectID get_instance_id_cached() const { return instance_id; }
	ObjectID get_pause_owner_id() const { return pause_owner_id; }
	Node *get_instance_node() const; // returns the node if still alive, else nullptr

	// ---- Mutators used by ActivityManager (kept public so subclasses + manager can drive). ----
	void _set_state(State p_state) { state = p_state; }
	void _set_intent(const Ref<Intent> &p_intent) { intent = p_intent; }
	void _set_scene_path(const String &p_path) { scene_path = p_path; }
	void _set_application(Application *p_app) { app = p_app; }
	void _set_owner(Object *p_owner);
	void _set_instance(Object *p_instance);
	void _set_pause_owner(Object *p_owner);

	// Request cancellation. Only meaningful in PENDING/LOADING — terminal states ignore.
	// Sets state=CANCELLED and emits the `cancelled` signal so [ActivityManager] can drain
	// the proxy from its stack/queue and resume the previously-paused Activity.
	void cancel();

	// Emit helpers (so ActivityManager doesn't have to know signal names).
	void _emit_ready(Node *p_node);
	void _emit_failed(const String &p_reason);
	void _emit_cancelled();
	void _emit_finished();
};

VARIANT_ENUM_CAST(ContextProxy::State);

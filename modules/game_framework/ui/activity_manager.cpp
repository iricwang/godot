/**************************************************************************/
/*  activity_manager.cpp                                                  */
/**************************************************************************/

#include "activity_manager.h"

#include "../context/application.h"
#include "activity.h"
#include "auto_activity_loader.h"
#include "dialog.h"
#include "toast.h"
#include "transition.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/animation/tween.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/main/canvas_item.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"

ActivityManager::ActivityManager() {
	loader = Ref<ActivityLoader>(memnew(AutoActivityLoader));
}

ActivityManager::~ActivityManager() {
}

void ActivityManager::set_application(Application *p_app) {
	app = p_app;
}

Application *ActivityManager::get_application() const {
	return app;
}

void ActivityManager::set_root(Control *p_root) {
	root = p_root;
}

Control *ActivityManager::get_root() const {
	return root;
}

void ActivityManager::register_activity(const String &p_action, const String &p_scene_path) {
	registry[p_action] = p_scene_path;
}

void ActivityManager::set_loader(const Ref<ActivityLoader> &p_loader) {
	loader = p_loader;
}

Ref<ActivityLoader> ActivityManager::get_loader() const {
	return loader;
}

// ---- Internal: proxy helpers ----

int ActivityManager::_stack_index_of_action(const String &p_action) const {
	for (int i = 0; i < stack.size(); ++i) {
		if (stack[i]->get_action() == p_action) {
			return i;
		}
	}
	return -1;
}

int ActivityManager::_stack_index_of_activity(Activity *p_act) const {
	if (!p_act) {
		return -1;
	}
	ObjectID id = p_act->get_instance_id();
	for (int i = 0; i < stack.size(); ++i) {
		if (stack[i]->get_instance_id_cached() == id) {
			return i;
		}
	}
	return -1;
}

int ActivityManager::_dialogs_index_of_dialog(Dialog *p_dlg) const {
	if (!p_dlg) {
		return -1;
	}
	ObjectID id = p_dlg->get_instance_id();
	for (int i = 0; i < dialogs.size(); ++i) {
		if (dialogs[i]->get_instance_id_cached() == id) {
			return i;
		}
	}
	return -1;
}

String ActivityManager::_resolve_scene_path(const String &p_action) const {
	if (registry.has(p_action)) {
		return registry[p_action];
	}
	if (loader.is_valid()) {
		return loader->resolve(p_action);
	}
	return String();
}

void ActivityManager::_begin_exit(Activity *p_act) {
	Ref<Transition> tout = p_act->get_transition_out();
	Ref<Tween> tween = tout.is_valid() ? tout->play_exit(p_act) : Ref<Tween>();
	if (tween.is_valid()) {
		tween->connect("finished", callable_mp((Node *)p_act, &Node::queue_free), Object::CONNECT_ONE_SHOT);
	} else {
		p_act->queue_free();
	}
}

void ActivityManager::_drop_stack_entry(int p_idx, bool p_dispatch_lifecycle) {
	if (p_idx < 0 || p_idx >= stack.size()) {
		return;
	}
	Ref<ActivityProxy> proxy = stack[p_idx];
	stack.remove_at(p_idx);

	if (proxy->is_ready()) {
		Activity *a = proxy->get_activity();
		if (a) {
			if (p_dispatch_lifecycle) {
				_transition_to_destroyed(proxy);
			}
			_dismiss_owned_dialogs(a);
			_cancel_owned_toasts(a);
			_begin_exit(a);
		}
		proxy->_set_state(ContextProxy::STATE_FINISHED);
		proxy->_emit_finished();
	} else if (proxy->is_pending() || proxy->is_loading()) {
		proxy->_set_state(ContextProxy::STATE_CANCELLED);
		proxy->_emit_cancelled();
	}

	// If this entry was a scene curtain, lift it AFTER the manager state
	// has settled. Dialogs owned by the scene activity itself were already
	// dismissed by _dismiss_owned_dialogs above; the lift only restores
	// dialogs that existed BEFORE this scene was pushed.
	if (proxy->is_scene_curtain()) {
		_lift_scene_curtain(proxy);
	}
}

// ---- Scene curtain (Intent::FLAG_SCENE) ----

void ActivityManager::_drape_scene_curtain(const Ref<ActivityProxy> &p_new_scene_proxy) {
	// Stop+hide every Activity already in the stack. This is the visual
	// difference vs a normal push: normally the previous top stays visible
	// behind the new activity until _attach_activity hides it; here we hide
	// EVERYTHING right away, including activities mid-stack that the user
	// pushed earlier but were already hidden -- idempotent because
	// _transition_to_stopped_hidden is guarded on lifecycle stage.
	for (int i = 0; i < stack.size(); ++i) {
		_transition_to_stopped_hidden(stack[i]);
	}
	// Pause+hide every dialog, and record its node id on the new proxy so
	// we can selectively restore at lift time. Iterate by index because
	// dialogs may be modified by side effects (none expected, but cheap
	// to defend against).
	for (int i = 0; i < dialogs.size(); ++i) {
		const Ref<DialogProxy> &dp = dialogs[i];
		if (!dp->is_ready()) {
			continue;
		}
		Dialog *d = dp->get_dialog();
		if (!d) {
			continue;
		}
		p_new_scene_proxy->_add_suspended_dialog(d->get_instance_id());
		// Dialog::dispatch_pause is idempotent -- a second scene curtain on
		// top of this one will re-record the same dialog id but won't
		// double-fire _on_pause.
		d->dispatch_pause();
	}
}

void ActivityManager::_lift_scene_curtain(const Ref<ActivityProxy> &p_popped) {
	// If another scene curtain is now the top, leave the dialogs hidden
	// -- they're still under SOMEONE'S curtain (the still-active one)
	// and will be lifted when it pops.
	if (!stack.is_empty()) {
		Ref<ActivityProxy> new_top = stack[stack.size() - 1];
		if (new_top.is_valid() && new_top->is_scene_curtain()) {
			return;
		}
	}
	for (ObjectID id : p_popped->get_suspended_dialog_ids()) {
		Object *obj = ObjectDB::get_instance(id);
		if (!obj) {
			continue; // dialog was already freed (e.g. owner destroyed)
		}
		Dialog *d = Object::cast_to<Dialog>(obj);
		if (!d) {
			continue;
		}
		// dispatch_resume is idempotent (no-op when not paused) so a dialog
		// that was already lifted by an earlier path is safe.
		d->dispatch_resume();
	}
	p_popped->_clear_suspended_dialogs();
}

// ---- Lifecycle transition helpers ----
//
// All helpers are no-ops if the proxy isn't READY or has been destroyed, so call
// sites can blast them at any proxy without re-checking. They keep
// ActivityProxy::lifecycle_stage in sync with the dispatch calls so we never
// double-pause or stop-without-pause.

void ActivityManager::_transition_to_paused(const Ref<ActivityProxy> &p_proxy) {
	if (p_proxy.is_null() || !p_proxy->is_ready()) {
		return; // LOADING/PENDING: nothing to dispatch; is_top check at attach handles visibility.
	}
	Activity *a = p_proxy->get_activity();
	if (!a) {
		return;
	}
	if (p_proxy->get_lifecycle_stage() == ActivityProxy::LIFECYCLE_RESUMED) {
		a->dispatch_pause();
		p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_PAUSED);
	}
}

void ActivityManager::_transition_to_stopped_hidden(const Ref<ActivityProxy> &p_proxy) {
	if (p_proxy.is_null() || !p_proxy->is_ready()) {
		return;
	}
	Activity *a = p_proxy->get_activity();
	if (!a) {
		return;
	}
	switch (p_proxy->get_lifecycle_stage()) {
		case ActivityProxy::LIFECYCLE_RESUMED:
			a->dispatch_pause();
			a->dispatch_stop();
			p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_STOPPED);
			break;
		case ActivityProxy::LIFECYCLE_PAUSED:
			a->dispatch_stop();
			p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_STOPPED);
			break;
		case ActivityProxy::LIFECYCLE_STARTED:
			// Mid-stack attach that never resumed — go straight to stop. We
			// intentionally skip dispatch_pause: per Android semantics onPause
			// requires a preceding onResume that never happened here.
			a->dispatch_stop();
			p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_STOPPED);
			break;
		default:
			break;
	}
	a->set_visible(false);
}

void ActivityManager::_transition_to_resumed(const Ref<ActivityProxy> &p_proxy) {
	if (p_proxy.is_null() || !p_proxy->is_ready()) {
		return;
	}
	Activity *a = p_proxy->get_activity();
	if (!a) {
		return;
	}
	switch (p_proxy->get_lifecycle_stage()) {
		case ActivityProxy::LIFECYCLE_PAUSED:
		case ActivityProxy::LIFECYCLE_STOPPED:
		case ActivityProxy::LIFECYCLE_STARTED:
			// Preserves the existing convention: coming back from stopped does NOT
			// re-dispatch start — only resume. (See PROGRESS.md §5 Test 5.)
			a->set_visible(true);
			a->dispatch_resume();
			p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_RESUMED);
			break;
		default:
			break;
	}
}

void ActivityManager::_transition_to_destroyed(const Ref<ActivityProxy> &p_proxy) {
	if (p_proxy.is_null() || !p_proxy->is_ready()) {
		return;
	}
	Activity *a = p_proxy->get_activity();
	if (!a) {
		return;
	}
	switch (p_proxy->get_lifecycle_stage()) {
		case ActivityProxy::LIFECYCLE_RESUMED:
			a->dispatch_pause();
			a->dispatch_stop();
			a->dispatch_destroy();
			break;
		case ActivityProxy::LIFECYCLE_PAUSED:
			a->dispatch_stop();
			a->dispatch_destroy();
			break;
		case ActivityProxy::LIFECYCLE_STARTED:
		case ActivityProxy::LIFECYCLE_STOPPED:
			a->dispatch_destroy();
			break;
		default:
			break;
	}
	p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_DESTROYED);
}

void ActivityManager::_resume_pause_owner_if_top(const Ref<ActivityProxy> &p_cancelled_proxy) {
	if (p_cancelled_proxy.is_null() || stack.is_empty()) {
		return;
	}
	const ObjectID pause_id = p_cancelled_proxy->get_pause_owner_id();
	if (pause_id.is_null()) {
		return;
	}
	Ref<ActivityProxy> new_top = stack[stack.size() - 1];
	if (new_top->get_instance_id_cached() != pause_id) {
		// Something newer is on top now — that activity is the one that should
		// be visible/resumed, not our pause owner. Leave it alone.
		return;
	}
	_transition_to_resumed(new_top);
}

// Standalone bootstrap callback — see header doc.
void ActivityManager::_mark_adopted_ready(Object *p_instance) {
	if (!p_instance) {
		return;
	}
	const ObjectID id = p_instance->get_instance_id();
	for (int i = 0; i < stack.size(); ++i) {
		if (stack[i]->get_instance_id_cached() == id) {
			Ref<ActivityProxy> p = stack[i];
			if (p->is_ready()) {
				return; // already marked
			}
			p->_set_state(ContextProxy::STATE_READY);
			p->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_RESUMED);
			Node *n = Object::cast_to<Node>(p_instance);
			p->_emit_ready(n);
			return;
		}
	}
	for (int i = 0; i < dialogs.size(); ++i) {
		if (dialogs[i]->get_instance_id_cached() == id) {
			Ref<DialogProxy> p = dialogs[i];
			if (p->is_ready()) {
				return;
			}
			p->_set_state(ContextProxy::STATE_READY);
			Node *n = Object::cast_to<Node>(p_instance);
			p->_emit_ready(n);
			return;
		}
	}
	for (int i = 0; i < active_toast_proxies.size(); ++i) {
		if (active_toast_proxies[i]->get_instance_id_cached() == id) {
			Ref<ToastProxy> p = active_toast_proxies[i];
			if (p->is_ready()) {
				return;
			}
			p->_set_state(ContextProxy::STATE_READY);
			Node *n = Object::cast_to<Node>(p_instance);
			p->_emit_ready(n);
			return;
		}
	}
}

// Signal handler — runs when ContextProxy::cancel() fires from outside (e.g. GDScript).
// Manager-side cancellation paths (_drop_stack_entry / cleanup_all / _dismiss_owned_*)
// remove the proxy from its container BEFORE emitting cancelled, so this scan finds
// nothing for those; only external cancels need draining.
void ActivityManager::_on_proxy_cancelled() {
	// Drain CANCELLED ActivityProxies and remember their pause owners so we can
	// resume them once the scan is done (resume order matters: only the new top
	// gets resumed, not every cancelled-proxy's owner).
	for (int i = stack.size() - 1; i >= 0; --i) {
		Ref<ActivityProxy> p = stack[i];
		if (p->get_state() == ContextProxy::STATE_CANCELLED) {
			stack.remove_at(i);
		}
	}
	// Drain CANCELLED DialogProxies.
	for (int i = dialogs.size() - 1; i >= 0; --i) {
		if (dialogs[i]->get_state() == ContextProxy::STATE_CANCELLED) {
			dialogs.remove_at(i);
		}
	}
	// Drain CANCELLED ToastProxies from the pending queue (free their orphan nodes).
	for (int i = toast_queue.size() - 1; i >= 0; --i) {
		Ref<ToastProxy> tp = toast_queue[i];
		if (tp->get_state() == ContextProxy::STATE_CANCELLED) {
			Toast *t = tp->get_toast();
			if (t) {
				memdelete(t);
			}
			toast_queue.remove_at(i);
		}
	}
	// After all cancels drained, ensure the new top is resumed (no-op if already).
	if (!stack.is_empty()) {
		_transition_to_resumed(stack[stack.size() - 1]);
	}
}

// ============================================================
// start_activity
// ============================================================

Ref<ActivityProxy> ActivityManager::start_activity(const Ref<Intent> &p_intent) {
	ERR_FAIL_COND_V_MSG(p_intent.is_null(), Ref<ActivityProxy>(), "Intent is null.");
	ERR_FAIL_NULL_V_MSG(root, Ref<ActivityProxy>(), "ActivityManager root not set. Call set_root(control) before starting activities.");

	const String action = p_intent->get_action();
	Ref<ActivityProxy> top = stack.is_empty() ? Ref<ActivityProxy>() : stack[stack.size() - 1];

	// Priority 1: FLAG_NEW_CLEAR — clear the entire stack (and all dialogs) before proceeding.
	if (p_intent->has_flag(Intent::FLAG_NEW_CLEAR)) {
		while (!dialogs.is_empty()) {
			Ref<DialogProxy> d = dialogs[dialogs.size() - 1];
			dialogs.remove_at(dialogs.size() - 1);
			if (d->is_ready()) {
				Dialog *node = d->get_dialog();
				if (node) {
					node->dispatch_dismiss();
					node->queue_free();
				}
				d->_set_state(ContextProxy::STATE_FINISHED);
				d->_emit_finished();
			} else if (!d->is_terminal()) {
				d->_set_state(ContextProxy::STATE_CANCELLED);
				d->_emit_cancelled();
			}
		}
		while (!stack.is_empty()) {
			_drop_stack_entry(stack.size() - 1, true);
		}
		top = Ref<ActivityProxy>();
	}

	// Priority 2: FLAG_SINGLE_TOP — if the top activity has the same action, reuse it.
	if (p_intent->has_flag(Intent::FLAG_SINGLE_TOP) && top.is_valid() && top->get_action() == action) {
		if (top->is_ready()) {
			Activity *t = top->get_activity();
			if (t) {
				t->dispatch_new_intent(p_intent);
			}
		} else {
			// Still LOADING — capture the intent so it dispatches on READY.
			top->set_pending_new_intent(p_intent);
			top->_set_intent(p_intent);
		}
		return top;
	}

	// Priority 3: FLAG_CLEAR_TOP — pop everything above a matching activity and reuse it.
	if (p_intent->has_flag(Intent::FLAG_CLEAR_TOP)) {
		const int found = _stack_index_of_action(action);
		if (found >= 0) {
			for (int i = stack.size() - 1; i > found; --i) {
				_drop_stack_entry(i, true);
			}
			Ref<ActivityProxy> exist = stack[stack.size() - 1];
			if (exist->is_ready()) {
				Activity *e = exist->get_activity();
				if (e) {
					e->dispatch_new_intent(p_intent);
				}
				_transition_to_resumed(exist);
			} else {
				exist->set_pending_new_intent(p_intent);
				exist->_set_intent(p_intent);
			}
			return exist;
		}
	}

	// Priority 4: FLAG_REORDER_TO_FRONT — move an existing activity to the top without destroying anything.
	if (p_intent->has_flag(Intent::FLAG_REORDER_TO_FRONT)) {
		const int found = _stack_index_of_action(action);
		if (found >= 0) {
			if (found == stack.size() - 1) {
				if (top->is_ready()) {
					Activity *t = top->get_activity();
					if (t) {
						t->dispatch_new_intent(p_intent);
					}
				} else {
					top->set_pending_new_intent(p_intent);
					top->_set_intent(p_intent);
				}
				return top;
			}
			Ref<ActivityProxy> target = stack[found];
			// Pause + stop old top (guarded by lifecycle stage).
			_transition_to_stopped_hidden(top);
			stack.remove_at(found);
			stack.push_back(target);
			if (target->is_ready()) {
				Activity *target_node = target->get_activity();
				if (target_node) {
					target_node->dispatch_new_intent(p_intent);
				}
				_transition_to_resumed(target);
			} else {
				target->set_pending_new_intent(p_intent);
				target->_set_intent(p_intent);
			}
			return target;
		}
		// Not found in stack — fall through to normal creation.
	}

	// Priority 5: Normal creation of a new Activity.
	// If the current top is a no_history activity, finish it before pushing.
	if (top.is_valid() && top->get_no_history()) {
		_drop_stack_entry(stack.size() - 1, true);
		top = stack.is_empty() ? Ref<ActivityProxy>() : stack[stack.size() - 1];
	}

	const String scene_path = _resolve_scene_path(action);
	ERR_FAIL_COND_V_MSG(scene_path.is_empty(),
			Ref<ActivityProxy>(),
			"No activity registered for action '" + action + "' and the loader could not resolve it. "
					"Register it manually via register_activity(), or place a scene at e.g. res://activities/" +
					action + ".tscn");

	// Build the proxy synchronously.
	Ref<ActivityProxy> proxy;
	proxy.instantiate();
	proxy->connect(SNAME("cancelled"), callable_mp(this, &ActivityManager::_on_proxy_cancelled));
	proxy->_set_intent(p_intent);
	proxy->_set_scene_path(scene_path);
	proxy->_set_application(app);
	if (p_intent->has_flag(Intent::FLAG_NO_HISTORY)) {
		proxy->set_no_history(true);
	}
	// Remember who we paused so cancel/FAILED rollback can resume the right activity.
	if (top.is_valid()) {
		Activity *top_node = top->is_ready() ? top->get_activity() : nullptr;
		if (top_node) {
			proxy->_set_pause_owner(top_node);
		}
	}
	stack.push_back(proxy);

	// Scene-curtain push: stop+hide every existing activity and pause+hide
	// every existing dialog BEFORE the normal _transition_to_paused(top)
	// below. The latter becomes a no-op for the now-STOPPED top (guarded
	// by lifecycle stage) so the ordering is safe.
	if (p_intent->has_flag(Intent::FLAG_SCENE)) {
		proxy->_set_scene_curtain(true);
		_drape_scene_curtain(proxy);
	}

	// Immediately pause the old top — guarded so a LOADING/mid-stack top is a no-op.
	// We intentionally do NOT change set_visible: the user keeps seeing the previous
	// activity until the new one is READY (hidden in _attach_activity).
	_transition_to_paused(top);
	Activity *pause_top = (top.is_valid() && top->is_ready()) ? top->get_activity() : nullptr;

	_begin_activity_load(proxy, pause_top);
	return proxy;
}

// ---- Async / sync load orchestration ----

void ActivityManager::_begin_activity_load(const Ref<ActivityProxy> &p_proxy, Activity *p_pause_top) {
	const String scene_path = p_proxy->get_scene_path();
	const bool force_sync = p_proxy->get_intent().is_valid() && p_proxy->get_intent()->has_flag(Intent::FLAG_LOAD_SYNC);
	const bool can_async = OS::get_singleton()->has_feature("threads");

	if (force_sync || !can_async) {
		Ref<PackedScene> packed = ResourceLoader::load(scene_path, "PackedScene");
		_attach_activity(p_proxy, packed);
		return;
	}

	const Error err = ResourceLoader::load_threaded_request(scene_path);
	if (err != OK) {
		p_proxy->_set_state(ContextProxy::STATE_FAILED);
		p_proxy->_emit_failed("ResourceLoader::load_threaded_request failed for " + scene_path);
		// Roll back: drop the proxy from the stack and resume whoever we paused
		// (only if they're now back on top — same rule as cancel-drain).
		int idx = stack.find(p_proxy);
		if (idx >= 0) {
			stack.remove_at(idx);
		}
		_resume_pause_owner_if_top(p_proxy);
		return;
	}

	p_proxy->_set_state(ContextProxy::STATE_LOADING);
	LoadingTask task;
	task.proxy = p_proxy;
	task.scene_path = scene_path;
	loading.push_back(task);
	_ensure_polling();
}

void ActivityManager::_attach_activity(const Ref<ActivityProxy> &p_proxy, const Ref<PackedScene> &p_packed) {
	// If the proxy was cancelled while loading was in flight, discard the packed scene.
	// The cancel-signal handler (_on_proxy_cancelled) already drained us from `stack`
	// and resumed the new top, so we just early-return.
	if (p_proxy->is_terminal()) {
		return;
	}
	// FAILED rollback helper — drop from stack, resume the activity we paused at push time.
	auto rollback = [&](const String &reason) {
		p_proxy->_set_state(ContextProxy::STATE_FAILED);
		p_proxy->_emit_failed(reason);
		int idx = stack.find(p_proxy);
		if (idx >= 0) {
			stack.remove_at(idx);
		}
		// Find the proxy whose Activity we paused at push time and, if it's now
		// the top of the stack, resume it. _transition_to_resumed is a guarded
		// no-op for proxies that aren't paused, so we can be liberal.
		const ObjectID pause_id = p_proxy->get_pause_owner_id();
		if (!pause_id.is_null() && !stack.is_empty()) {
			Ref<ActivityProxy> new_top = stack[stack.size() - 1];
			if (new_top->get_instance_id_cached() == pause_id) {
				_transition_to_resumed(new_top);
			}
		}
	};

	if (p_packed.is_null()) {
		rollback("Failed to load activity scene: " + p_proxy->get_scene_path());
		return;
	}

	Node *inst = p_packed->instantiate();
	Activity *act = Object::cast_to<Activity>(inst);
	if (!act) {
		if (inst) {
			memdelete(inst);
		}
		rollback("Activity scene root is not an Activity: " + p_proxy->get_scene_path());
		return;
	}

	Ref<Intent> use_intent = p_proxy->get_intent();
	act->set_intent(use_intent);
	if (app) {
		act->set_application(app);
	}
	if (p_proxy->get_no_history()) {
		act->set_no_history(true);
	}

	root->add_child(act);
	act->set_position(Vector2(0, 0));
	act->set_size(root->get_size());

	p_proxy->_set_instance(act);
	p_proxy->_set_state(ContextProxy::STATE_READY);

	act->dispatch_create(use_intent.is_valid() ? use_intent->get_extras() : Dictionary());
	act->dispatch_start();
	p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_STARTED);

	// Is this proxy the current top? If something newer was pushed (or we
	// somehow loaded out of order), this is a mid-stack attach and must NOT
	// dispatch_resume — it stays invisible until the proxy above it is
	// finished/cancelled.
	int idx = stack.find(p_proxy);
	const bool is_top = (idx >= 0 && idx == stack.size() - 1);

	if (is_top) {
		Ref<Transition> tin = act->get_transition_in();
		if (tin.is_valid()) {
			tin->play_enter(act);
		}
		act->dispatch_resume();
		p_proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_RESUMED);

		// Hide the previous READY proxy (transition guarded by its lifecycle stage).
		if (idx > 0) {
			Ref<ActivityProxy> prev = stack[idx - 1];
			_transition_to_stopped_hidden(prev);
		}
	} else {
		// Mid-stack attach (a newer proxy is already above us). Stay invisible —
		// our Activity exists in STARTED but isn't on screen.
		act->set_visible(false);
	}

	// Honour any pending new-intent queued while LOADING (SINGLE_TOP retarget).
	if (p_proxy->has_pending_new_intent()) {
		Ref<Intent> pending = p_proxy->get_pending_new_intent();
		p_proxy->clear_pending_new_intent();
		act->dispatch_new_intent(pending);
	}

	p_proxy->_emit_ready(act);
}

void ActivityManager::finish_activity(Activity *p_activity) {
	if (!p_activity) {
		return;
	}
	const int idx = _stack_index_of_activity(p_activity);
	if (idx < 0) {
		return;
	}
	const bool is_top = (idx == stack.size() - 1);
	Ref<ActivityProxy> proxy = stack[idx];

	_transition_to_destroyed(proxy);
	stack.remove_at(idx);

	// Clean up dialogs and toasts owned by this Activity.
	_dismiss_owned_dialogs(p_activity);
	_cancel_owned_toasts(p_activity);

	proxy->_set_state(ContextProxy::STATE_FINISHED);
	proxy->_emit_finished();

	// Scene-curtain lift: restore the pre-scene dialogs (if no other scene
	// is still on top). Must happen BEFORE _transition_to_resumed below so
	// the new top's _on_resume sees the dialogs already visible+resumed.
	if (proxy->is_scene_curtain()) {
		_lift_scene_curtain(proxy);
	}

	const bool was_standalone_root = p_activity->is_standalone() && stack.size() == 0;
	// Standalone bootstrap root finishing → quit the SceneTree instead of
	// playing an exit transition into the void. The user expects F6 +
	// "close" to end the run, just like any normal scene would.
	if (was_standalone_root) {
		SceneTree *st = p_activity->get_tree();
		if (st) {
			st->quit();
		}
		// Leave the Activity in the tree — SceneTree::quit() drains the
		// whole graph at exit, so queue_free here would race with that.
		return;
	}

	_begin_exit(p_activity);

	if (is_top && !stack.is_empty()) {
		_transition_to_resumed(stack[stack.size() - 1]);
	}
}

Ref<ActivityProxy> ActivityManager::adopt_running_activity(Activity *p_activity, const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL_V(p_activity, Ref<ActivityProxy>());
	int idx = _stack_index_of_activity(p_activity);
	if (idx >= 0) {
		return stack[idx]; // already adopted
	}
	if (p_intent.is_valid()) {
		p_activity->set_intent(p_intent);
	}
	if (app) {
		p_activity->set_application(app);
	}
	Ref<ActivityProxy> proxy;
	proxy.instantiate();
	proxy->connect(SNAME("cancelled"), callable_mp(this, &ActivityManager::_on_proxy_cancelled));
	proxy->_set_intent(p_intent.is_valid() ? p_intent : p_activity->get_intent());
	proxy->_set_application(app);
	proxy->_set_instance(p_activity);
	// Adopted = lifecycle is driven by the caller (e.g. standalone bootstrap
	// dispatches create/start/resume on a deferred frame). Until that runs we
	// stay at LOADING; _dispatch_standalone_lifecycle's trampoline flips us to
	// READY+RESUMED via _mark_adopted_ready. Standalone is preview/test only,
	// so the one-frame gap is acceptable.
	proxy->_set_state(ContextProxy::STATE_LOADING);
	proxy->_set_lifecycle_stage(ActivityProxy::LIFECYCLE_NONE);
	stack.push_back(proxy);
	return proxy;
}

void ActivityManager::finish_top() {
	if (stack.is_empty()) {
		return;
	}
	Ref<ActivityProxy> top = stack[stack.size() - 1];
	if (top->is_ready()) {
		Activity *a = top->get_activity();
		if (a) {
			finish_activity(a);
			return;
		}
	}
	// LOADING/PENDING top — cancel and resume whoever is below.
	_drop_stack_entry(stack.size() - 1, false);
	if (!stack.is_empty()) {
		_transition_to_resumed(stack[stack.size() - 1]);
	}
}

bool ActivityManager::back() {
	if (stack.is_empty()) {
		return false;
	}
	Ref<ActivityProxy> top = stack[stack.size() - 1];
	if (top->is_ready()) {
		Activity *a = top->get_activity();
		if (a) {
			if (a->dispatch_back_pressed()) {
				return true;
			}
			finish_activity(a);
			return true;
		}
	}
	// LOADING — cancel and resume below.
	finish_top();
	return true;
}

Activity *ActivityManager::get_current_activity() const {
	if (stack.is_empty()) {
		return nullptr;
	}
	Ref<ActivityProxy> p = stack[stack.size() - 1];
	return p->is_ready() ? p->get_activity() : nullptr;
}

Ref<ActivityProxy> ActivityManager::get_current_activity_proxy() const {
	if (stack.is_empty()) {
		return Ref<ActivityProxy>();
	}
	return stack[stack.size() - 1];
}

int ActivityManager::get_stack_size() const {
	return stack.size();
}

Activity *ActivityManager::get_stack_activity(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, stack.size(), nullptr);
	Ref<ActivityProxy> p = stack[p_idx];
	return p->is_ready() ? p->get_activity() : nullptr;
}

Ref<ActivityProxy> ActivityManager::get_stack_proxy(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, stack.size(), Ref<ActivityProxy>());
	return stack[p_idx];
}

int ActivityManager::get_dialog_count() const {
	return dialogs.size();
}

Dialog *ActivityManager::get_dialog(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, dialogs.size(), nullptr);
	Ref<DialogProxy> p = dialogs[p_idx];
	return p->is_ready() ? p->get_dialog() : nullptr;
}

Ref<DialogProxy> ActivityManager::get_dialog_proxy(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, dialogs.size(), Ref<DialogProxy>());
	return dialogs[p_idx];
}

int ActivityManager::get_toast_queue_count() const {
	return toast_queue.size();
}

Toast *ActivityManager::get_toast_queue_item(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, toast_queue.size(), nullptr);
	Ref<ToastProxy> p = toast_queue[p_idx];
	return p->get_toast();
}

Ref<ToastProxy> ActivityManager::get_toast_queue_proxy(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, toast_queue.size(), Ref<ToastProxy>());
	return toast_queue[p_idx];
}

bool ActivityManager::is_toast_active() const {
	return toast_active;
}

Object *ActivityManager::_resolve_default_owner() {
	if (!stack.is_empty()) {
		Ref<ActivityProxy> top = stack[stack.size() - 1];
		if (top->is_ready()) {
			return top->get_activity();
		}
	}
	return app; // fall back to Application
}

// ============================================================
// Dialogs
// ============================================================

Ref<DialogProxy> ActivityManager::show_dialog(const Ref<Intent> &p_intent) {
	return show_dialog_with_owner(p_intent, _resolve_default_owner());
}

Ref<DialogProxy> ActivityManager::show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner) {
	ERR_FAIL_COND_V_MSG(p_intent.is_null(), Ref<DialogProxy>(), "Intent is null.");
	ERR_FAIL_NULL_V_MSG(root, Ref<DialogProxy>(), "ActivityManager root not set.");

	if (!p_owner) {
		p_owner = _resolve_default_owner();
	}

	const String action = p_intent->get_action();
	const String scene_path = _resolve_scene_path(action);
	ERR_FAIL_COND_V_MSG(scene_path.is_empty(), Ref<DialogProxy>(), "No scene registered for action: " + action);

	Ref<DialogProxy> proxy;
	proxy.instantiate();
	proxy->connect(SNAME("cancelled"), callable_mp(this, &ActivityManager::_on_proxy_cancelled));
	proxy->_set_intent(p_intent);
	proxy->_set_scene_path(scene_path);
	proxy->_set_application(app);
	proxy->_set_owner(p_owner);
	dialogs.push_back(proxy);

	_begin_dialog_load(proxy);
	return proxy;
}

void ActivityManager::_begin_dialog_load(const Ref<DialogProxy> &p_proxy) {
	const String scene_path = p_proxy->get_scene_path();
	const bool force_sync = p_proxy->get_intent().is_valid() && p_proxy->get_intent()->has_flag(Intent::FLAG_LOAD_SYNC);
	const bool can_async = OS::get_singleton()->has_feature("threads");

	if (force_sync || !can_async) {
		Ref<PackedScene> packed = ResourceLoader::load(scene_path, "PackedScene");
		_attach_dialog(p_proxy, packed);
		return;
	}

	const Error err = ResourceLoader::load_threaded_request(scene_path);
	if (err != OK) {
		p_proxy->_set_state(ContextProxy::STATE_FAILED);
		p_proxy->_emit_failed("ResourceLoader::load_threaded_request failed for " + scene_path);
		int idx = dialogs.find(p_proxy);
		if (idx >= 0) {
			dialogs.remove_at(idx);
		}
		return;
	}

	p_proxy->_set_state(ContextProxy::STATE_LOADING);
	LoadingTask task;
	task.proxy = p_proxy;
	task.scene_path = scene_path;
	loading.push_back(task);
	_ensure_polling();
}

void ActivityManager::_attach_dialog(const Ref<DialogProxy> &p_proxy, const Ref<PackedScene> &p_packed) {
	if (p_proxy->is_terminal()) {
		return;
	}
	if (p_packed.is_null()) {
		p_proxy->_set_state(ContextProxy::STATE_FAILED);
		p_proxy->_emit_failed("Failed to load dialog scene: " + p_proxy->get_scene_path());
		int idx = dialogs.find(p_proxy);
		if (idx >= 0) {
			dialogs.remove_at(idx);
		}
		return;
	}

	Node *inst = p_packed->instantiate();
	Dialog *dlg = Object::cast_to<Dialog>(inst);
	if (!dlg) {
		if (inst) {
			memdelete(inst);
		}
		p_proxy->_set_state(ContextProxy::STATE_FAILED);
		p_proxy->_emit_failed("Dialog scene root is not a Dialog: " + p_proxy->get_scene_path());
		int idx = dialogs.find(p_proxy);
		if (idx >= 0) {
			dialogs.remove_at(idx);
		}
		return;
	}

	Ref<Intent> use_intent = p_proxy->get_intent();
	dlg->set_intent(use_intent);
	if (app) {
		dlg->set_application(app);
	}
	Object *owner_obj = ObjectDB::get_instance(p_proxy->get_owner_id());
	if (owner_obj) {
		dlg->set_lifecycle_owner(owner_obj);
	}
	root->add_child(dlg);
	dlg->set_position(Vector2(0, 0));
	dlg->set_size(root->get_size());
	dlg->dispatch_create(use_intent.is_valid() ? use_intent->get_extras() : Dictionary());

	Ref<Transition> tin = dlg->get_transition_in();
	if (tin.is_valid()) {
		tin->play_enter(dlg);
	}

	p_proxy->_set_instance(dlg);
	p_proxy->_set_state(ContextProxy::STATE_READY);
	p_proxy->_emit_ready(dlg);
}

void ActivityManager::dismiss_dialog(Dialog *p_dialog) {
	if (!p_dialog) {
		return;
	}
	const int idx = _dialogs_index_of_dialog(p_dialog);
	if (idx < 0) {
		return;
	}
	Ref<DialogProxy> proxy = dialogs[idx];
	dialogs.remove_at(idx);

	// Standalone-root Dialog closing (F6'd preview with nothing else running) →
	// quit the SceneTree instead of playing an exit transition into the void,
	// mirroring finish_activity's was_standalone_root path.
	const bool was_standalone_root = p_dialog->is_standalone() && stack.is_empty();

	p_dialog->dispatch_dismiss();
	proxy->_set_state(ContextProxy::STATE_FINISHED);
	proxy->_emit_finished();

	if (was_standalone_root) {
		SceneTree *st = p_dialog->get_tree();
		if (st) {
			st->quit();
		}
		// Leave the Dialog in the tree — SceneTree::quit() drains the graph.
		return;
	}

	Ref<Transition> tout = p_dialog->get_transition_out();
	Ref<Tween> tween = tout.is_valid() ? tout->play_exit(p_dialog) : Ref<Tween>();
	if (tween.is_valid()) {
		tween->connect("finished", callable_mp((Node *)p_dialog, &Node::queue_free), Object::CONNECT_ONE_SHOT);
	} else {
		p_dialog->queue_free();
	}
}

Ref<DialogProxy> ActivityManager::adopt_running_dialog(Dialog *p_dialog, const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL_V(p_dialog, Ref<DialogProxy>());
	int idx = _dialogs_index_of_dialog(p_dialog);
	if (idx >= 0) {
		return dialogs[idx]; // already adopted
	}
	if (p_intent.is_valid()) {
		p_dialog->set_intent(p_intent);
	}
	if (app) {
		p_dialog->set_application(app);
		// Owner = Application so the dialog is not auto-dismissed by stack churn.
		p_dialog->set_lifecycle_owner(app);
	}
	Ref<DialogProxy> proxy;
	proxy.instantiate();
	proxy->connect(SNAME("cancelled"), callable_mp(this, &ActivityManager::_on_proxy_cancelled));
	proxy->_set_intent(p_intent.is_valid() ? p_intent : p_dialog->get_intent());
	proxy->_set_application(app);
	proxy->_set_owner(app);
	proxy->_set_instance(p_dialog);
	// Adopted = lifecycle dispatched on a deferred frame by Dialog's
	// _dispatch_standalone_lifecycle, which calls _mark_adopted_ready when done.
	proxy->_set_state(ContextProxy::STATE_LOADING);
	dialogs.push_back(proxy);
	return proxy;
}

// ============================================================
// Toasts
// ============================================================

Ref<ToastProxy> ActivityManager::adopt_running_toast(Toast *p_toast, const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL_V(p_toast, Ref<ToastProxy>());
	if (p_intent.is_valid()) {
		p_toast->set_intent(p_intent);
	}
	if (app) {
		p_toast->set_application(app);
		// Owner = Application so it is not auto-cancelled by stack churn.
		p_toast->set_lifecycle_owner(app);
	}
	Ref<ToastProxy> proxy;
	proxy.instantiate();
	proxy->connect(SNAME("cancelled"), callable_mp(this, &ActivityManager::_on_proxy_cancelled));
	proxy->_set_intent(p_intent.is_valid() ? p_intent : p_toast->get_intent());
	proxy->_set_application(app);
	proxy->_set_owner(app);
	proxy->_set_instance(p_toast);
	proxy->_set_state(ContextProxy::STATE_READY);
	// Standalone preview: already reparented + shown by the bootstrap; bookkeeping only.
	proxy->_emit_ready(p_toast);
	return proxy;
}

void ActivityManager::dismiss_toast(Toast *p_toast) {
	if (!p_toast) {
		return;
	}
	const bool was_standalone_root = p_toast->is_standalone() && stack.is_empty();
	p_toast->dispatch_dismiss();

	ObjectID id = p_toast->get_instance_id();
	for (int i = active_toast_proxies.size() - 1; i >= 0; --i) {
		if (active_toast_proxies[i]->get_instance_id_cached() == id) {
			active_toast_proxies[i]->_set_state(ContextProxy::STATE_FINISHED);
			active_toast_proxies[i]->_emit_finished();
			active_toast_proxies.remove_at(i);
		}
	}

	if (was_standalone_root) {
		SceneTree *st = p_toast->get_tree();
		if (st) {
			st->quit();
		}
		return;
	}
	p_toast->queue_free();
}

Ref<ToastProxy> ActivityManager::show_toast(Toast *p_toast) {
	return show_toast_with_owner(p_toast, _resolve_default_owner());
}

Ref<ToastProxy> ActivityManager::show_toast_with_owner(Toast *p_toast, Object *p_owner) {
	if (!p_toast) {
		return Ref<ToastProxy>();
	}
	Object *owner = p_owner ? p_owner : _resolve_default_owner();
	if (owner) {
		p_toast->set_lifecycle_owner(owner);
	}

	Ref<ToastProxy> proxy;
	proxy.instantiate();
	proxy->connect(SNAME("cancelled"), callable_mp(this, &ActivityManager::_on_proxy_cancelled));
	proxy->_set_intent(p_toast->get_intent());
	proxy->_set_application(app);
	proxy->_set_owner(owner);
	proxy->_set_instance(p_toast);
	// PARALLEL: present immediately (which sets READY + emits ready in _present_toast).
	// SERIAL: pending until pumped.
	if (toast_mode == PARALLEL) {
		_present_toast(proxy);
		return proxy;
	}

	proxy->_set_state(ContextProxy::STATE_PENDING);
	toast_queue.push_back(proxy);
	if (!toast_active) {
		_show_next_toast();
	}
	return proxy;
}

void ActivityManager::clear_all_toasts() {
	// Dismiss active toast panels.
	for (int i = active_toast_proxies.size() - 1; i >= 0; --i) {
		Ref<ToastProxy> proxy = active_toast_proxies[i];
		Toast *t = proxy->get_toast();
		if (t) {
			t->queue_free();
		}
		proxy->_set_state(ContextProxy::STATE_FINISHED);
		proxy->_emit_finished();
	}
	active_toast_proxies.clear();
	// Pending toasts are orphan nodes (never added to the tree) — free directly.
	for (int i = 0; i < toast_queue.size(); ++i) {
		Ref<ToastProxy> proxy = toast_queue[i];
		Toast *t = proxy->get_toast();
		if (t) {
			memdelete(t);
		}
		proxy->_set_state(ContextProxy::STATE_CANCELLED);
		proxy->_emit_cancelled();
	}
	toast_queue.clear();
	toast_active = false;
}

void ActivityManager::clear_toasts_by_owner(Object *p_owner) {
	if (!p_owner) {
		return;
	}
	ObjectID owner_id = p_owner->get_instance_id();

	// Remove matching toasts from the pending queue (orphan nodes → memdelete).
	for (int i = toast_queue.size() - 1; i >= 0; --i) {
		Ref<ToastProxy> proxy = toast_queue[i];
		if (proxy->get_owner_id() == owner_id) {
			Toast *t = proxy->get_toast();
			if (t) {
				memdelete(t);
			}
			toast_queue.remove_at(i);
			proxy->_set_state(ContextProxy::STATE_CANCELLED);
			proxy->_emit_cancelled();
		}
	}

	// Dismiss active toast panels owned by this owner (parallel mode).
	for (int i = active_toast_proxies.size() - 1; i >= 0; --i) {
		Ref<ToastProxy> proxy = active_toast_proxies[i];
		if (proxy->get_owner_id() == owner_id) {
			Toast *t = proxy->get_toast();
			if (t) {
				t->queue_free();
			}
			active_toast_proxies.remove_at(i);
			proxy->_set_state(ContextProxy::STATE_FINISHED);
			proxy->_emit_finished();
		}
	}
}

void ActivityManager::set_toast_display_mode(ToastDisplayMode p_mode) {
	toast_mode = p_mode;
}

ActivityManager::ToastDisplayMode ActivityManager::get_toast_display_mode() const {
	return toast_mode;
}

void ActivityManager::_dismiss_owned_dialogs(Object *p_owner) {
	if (!p_owner) {
		return;
	}
	ObjectID id = p_owner->get_instance_id();
	// Iterate in reverse since we may remove items.
	for (int i = dialogs.size() - 1; i >= 0; --i) {
		Ref<DialogProxy> proxy = dialogs[i];
		if (proxy->get_owner_id() != id) {
			continue;
		}
		dialogs.remove_at(i);
		if (proxy->is_ready()) {
			Dialog *d = proxy->get_dialog();
			if (d) {
				d->dispatch_dismiss();
				d->queue_free();
			}
			proxy->_set_state(ContextProxy::STATE_FINISHED);
			proxy->_emit_finished();
		} else if (!proxy->is_terminal()) {
			proxy->_set_state(ContextProxy::STATE_CANCELLED);
			proxy->_emit_cancelled();
		}
	}
}

void ActivityManager::_cancel_owned_toasts(Object *p_owner) {
	if (!p_owner) {
		return;
	}
	ObjectID owner_id = p_owner->get_instance_id();
	for (int i = toast_queue.size() - 1; i >= 0; --i) {
		Ref<ToastProxy> proxy = toast_queue[i];
		if (proxy->get_owner_id() != owner_id) {
			continue;
		}
		Toast *t = proxy->get_toast();
		if (t) {
			memdelete(t);
		}
		// Remove BEFORE emitting the cancelled signal so the connected
		// _on_proxy_cancelled scan finds nothing left to drain — otherwise
		// the handler races us and we end up double-removing.
		toast_queue.remove_at(i);
		proxy->_set_state(ContextProxy::STATE_CANCELLED);
		proxy->_emit_cancelled();
	}
}

void ActivityManager::_show_next_toast() {
	if (!root || toast_queue.is_empty()) {
		toast_active = false;
		return;
	}
	toast_active = true;
	Ref<ToastProxy> proxy = toast_queue[0];
	toast_queue.remove_at(0);
	_present_toast(proxy);
}

void ActivityManager::_present_toast(const Ref<ToastProxy> &p_proxy) {
	ERR_FAIL_NULL(root);
	Toast *toast = p_proxy->get_toast();
	if (!toast) {
		p_proxy->_set_state(ContextProxy::STATE_FAILED);
		p_proxy->_emit_failed("Toast node was freed before presentation");
		return;
	}

	if (app) {
		toast->set_application(app);
	}

	// Anchor the Toast node near the bottom-center of the root.
	toast->set_anchor(SIDE_LEFT, 0.5);
	toast->set_anchor(SIDE_RIGHT, 0.5);
	toast->set_anchor(SIDE_TOP, 1.0);
	toast->set_anchor(SIDE_BOTTOM, 1.0);
	toast->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
	toast->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
	toast->set_offset(SIDE_BOTTOM, -60);

	root->add_child(toast);

	// The Toast node builds its own visual (default panel+label, or a subclass
	// scene that overrides _on_create).
	toast->dispatch_create(toast->get_intent().is_valid() ? toast->get_intent()->get_extras() : Dictionary());

	p_proxy->_set_state(ContextProxy::STATE_READY);
	p_proxy->_emit_ready(toast);

	// Parallel mode: track active proxy for clear_all_toasts.
	if (toast_mode == PARALLEL) {
		active_toast_proxies.push_back(p_proxy);
	}

	toast->set_modulate(Color(1, 1, 1, 0));

	Ref<Tween> tween = toast->create_tween();
	if (tween.is_valid()) {
		tween->tween_property(toast, NodePath("modulate:a"), 1.0, 0.2);
		tween->tween_interval(toast->get_duration());
		tween->tween_property(toast, NodePath("modulate:a"), 0.0, 0.3);
		tween->tween_callback(callable_mp(this, &ActivityManager::_on_toast_finished).bind(toast));
	} else {
		_on_toast_finished(toast);
	}
}

void ActivityManager::_on_toast_finished(Object *p_panel) {
	Toast *toast = Object::cast_to<Toast>(p_panel);
	Node *n = Object::cast_to<Node>(p_panel);
	if (n) {
		if (toast) {
			// Find the proxy and emit finished.
			ObjectID id = toast->get_instance_id();
			for (int i = active_toast_proxies.size() - 1; i >= 0; --i) {
				if (active_toast_proxies[i]->get_instance_id_cached() == id) {
					active_toast_proxies[i]->_set_state(ContextProxy::STATE_FINISHED);
					active_toast_proxies[i]->_emit_finished();
					active_toast_proxies.remove_at(i);
					break;
				}
			}
			toast->dispatch_dismiss();
		}
		n->queue_free();
	}

	if (toast_mode == PARALLEL) {
		return; // parallel: no chain — each toast is self-contained
	}
	// Serial: pump next in FIFO.
	_show_next_toast();
}

// ---- Async polling ----

void ActivityManager::_ensure_polling() {
	if (polling_connected) {
		return;
	}
	SceneTree *st = SceneTree::get_singleton();
	if (!st) {
		return;
	}
	st->connect("process_frame", callable_mp(this, &ActivityManager::_poll_loads));
	polling_connected = true;
}

void ActivityManager::_poll_loads() {
	for (int i = loading.size() - 1; i >= 0; --i) {
		const String path = loading[i].scene_path;
		const ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(path);

		if (status == ResourceLoader::THREAD_LOAD_LOADED) {
			Error err = OK;
			Ref<Resource> res = ResourceLoader::load_threaded_get(path, &err);
			Ref<PackedScene> packed = res;
			Ref<ContextProxy> proxy = loading[i].proxy;
			loading.remove_at(i);
			if (Ref<ActivityProxy> ap = proxy; ap.is_valid()) {
				_attach_activity(ap, packed);
			} else if (Ref<DialogProxy> dp = proxy; dp.is_valid()) {
				_attach_dialog(dp, packed);
			} else {
				// Unknown ContextProxy subclass in the loading queue —
				// future-proofing: emit failed so the diagnostic is visible
				// instead of silently dropping the loaded resource.
				if (!proxy->is_terminal()) {
					proxy->_set_state(ContextProxy::STATE_FAILED);
					proxy->_emit_failed("Unknown ContextProxy subclass in loading queue: " + path);
				}
			}
		} else if (status == ResourceLoader::THREAD_LOAD_FAILED || status == ResourceLoader::THREAD_LOAD_INVALID_RESOURCE) {
			Ref<ContextProxy> proxy = loading[i].proxy;
			loading.remove_at(i);
			if (proxy->is_terminal()) {
				continue;
			}
			proxy->_set_state(ContextProxy::STATE_FAILED);
			proxy->_emit_failed("Failed to thread-load scene: " + path);
			if (Ref<ActivityProxy> ap = proxy; ap.is_valid()) {
				int idx = stack.find(ap);
				if (idx >= 0) {
					stack.remove_at(idx);
				}
			} else if (Ref<DialogProxy> dp = proxy; dp.is_valid()) {
				int idx = dialogs.find(dp);
				if (idx >= 0) {
					dialogs.remove_at(idx);
				}
			}
		}
		// THREAD_LOAD_IN_PROGRESS: keep polling next frame.
	}
}

void ActivityManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root", "root"), &ActivityManager::set_root);
	ClassDB::bind_method(D_METHOD("get_root"), &ActivityManager::get_root);
	ClassDB::bind_method(D_METHOD("register_activity", "action", "scene_path"), &ActivityManager::register_activity);
	ClassDB::bind_method(D_METHOD("set_loader", "loader"), &ActivityManager::set_loader);
	ClassDB::bind_method(D_METHOD("get_loader"), &ActivityManager::get_loader);
	ClassDB::bind_method(D_METHOD("start_activity", "intent"), &ActivityManager::start_activity);
	ClassDB::bind_method(D_METHOD("adopt_running_activity", "activity", "intent"), &ActivityManager::adopt_running_activity);
	ClassDB::bind_method(D_METHOD("finish_activity", "activity"), &ActivityManager::finish_activity);
	ClassDB::bind_method(D_METHOD("finish_top"), &ActivityManager::finish_top);
	ClassDB::bind_method(D_METHOD("back"), &ActivityManager::back);
	ClassDB::bind_method(D_METHOD("get_current_activity"), &ActivityManager::get_current_activity);
	ClassDB::bind_method(D_METHOD("get_current_activity_proxy"), &ActivityManager::get_current_activity_proxy);
	ClassDB::bind_method(D_METHOD("get_stack_size"), &ActivityManager::get_stack_size);
	ClassDB::bind_method(D_METHOD("get_stack_activity", "idx"), &ActivityManager::get_stack_activity);
	ClassDB::bind_method(D_METHOD("get_stack_proxy", "idx"), &ActivityManager::get_stack_proxy);
	ClassDB::bind_method(D_METHOD("get_dialog_count"), &ActivityManager::get_dialog_count);
	ClassDB::bind_method(D_METHOD("get_dialog", "idx"), &ActivityManager::get_dialog);
	ClassDB::bind_method(D_METHOD("get_dialog_proxy", "idx"), &ActivityManager::get_dialog_proxy);
	ClassDB::bind_method(D_METHOD("get_toast_queue_count"), &ActivityManager::get_toast_queue_count);
	ClassDB::bind_method(D_METHOD("get_toast_queue_item", "idx"), &ActivityManager::get_toast_queue_item);
	ClassDB::bind_method(D_METHOD("get_toast_queue_proxy", "idx"), &ActivityManager::get_toast_queue_proxy);
	ClassDB::bind_method(D_METHOD("is_toast_active"), &ActivityManager::is_toast_active);

	ClassDB::bind_method(D_METHOD("show_dialog", "intent"), &ActivityManager::show_dialog);
	ClassDB::bind_method(D_METHOD("show_dialog_with_owner", "intent", "owner"), &ActivityManager::show_dialog_with_owner);
	ClassDB::bind_method(D_METHOD("dismiss_dialog", "dialog"), &ActivityManager::dismiss_dialog);
	ClassDB::bind_method(D_METHOD("adopt_running_dialog", "dialog", "intent"), &ActivityManager::adopt_running_dialog);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), &ActivityManager::show_toast);
	ClassDB::bind_method(D_METHOD("show_toast_with_owner", "toast", "owner"), &ActivityManager::show_toast_with_owner);
	ClassDB::bind_method(D_METHOD("adopt_running_toast", "toast", "intent"), &ActivityManager::adopt_running_toast);
	ClassDB::bind_method(D_METHOD("dismiss_toast", "toast"), &ActivityManager::dismiss_toast);
	ClassDB::bind_method(D_METHOD("clear_all_toasts"), &ActivityManager::clear_all_toasts);
	ClassDB::bind_method(D_METHOD("clear_toasts_by_owner", "owner"), &ActivityManager::clear_toasts_by_owner);

	BIND_ENUM_CONSTANT(ToastDisplayMode::SERIAL);
	BIND_ENUM_CONSTANT(PARALLEL);
	ClassDB::bind_method(D_METHOD("set_toast_display_mode", "mode"), &ActivityManager::set_toast_display_mode);
	ClassDB::bind_method(D_METHOD("get_toast_display_mode"), &ActivityManager::get_toast_display_mode);

	ClassDB::bind_method(D_METHOD("set_application", "application"), &ActivityManager::set_application);
	ClassDB::bind_method(D_METHOD("get_application"), &ActivityManager::get_application);
	ClassDB::bind_method(D_METHOD("cleanup_all"), &ActivityManager::cleanup_all);
}

void ActivityManager::cleanup_all() {
	// Drain dialogs first (they depend on activity stack being intact).
	while (!dialogs.is_empty()) {
		Ref<DialogProxy> d = dialogs[dialogs.size() - 1];
		dialogs.remove_at(dialogs.size() - 1);
		if (d->is_ready()) {
			Dialog *node = d->get_dialog();
			if (node) {
				node->dispatch_dismiss();
				node->queue_free();
			}
			d->_set_state(ContextProxy::STATE_FINISHED);
			d->_emit_finished();
		} else if (!d->is_terminal()) {
			d->_set_state(ContextProxy::STATE_CANCELLED);
			d->_emit_cancelled();
		}
	}
	// Drain activity stack bottom-to-top so destroy is called in reverse creation order.
	while (!stack.is_empty()) {
		Ref<ActivityProxy> a = stack[stack.size() - 1];
		stack.remove_at(stack.size() - 1);
		if (a->is_ready()) {
			Activity *node = a->get_activity();
			if (node) {
				node->dispatch_pause();
				node->dispatch_stop();
				node->dispatch_destroy();
				node->queue_free();
			}
			a->_set_state(ContextProxy::STATE_FINISHED);
			a->_emit_finished();
		} else if (!a->is_terminal()) {
			a->_set_state(ContextProxy::STATE_CANCELLED);
			a->_emit_cancelled();
		}
	}
	// Pending toasts are orphan nodes — free them directly.
	for (int i = 0; i < toast_queue.size(); ++i) {
		Ref<ToastProxy> proxy = toast_queue[i];
		Toast *t = proxy->get_toast();
		if (t) {
			memdelete(t);
		}
		proxy->_set_state(ContextProxy::STATE_CANCELLED);
		proxy->_emit_cancelled();
	}
	toast_queue.clear();
	toast_active = false;
	active_toast_proxies.clear();
	// Drain in-flight async loads: tell ResourceLoader to retire each slot
	// (otherwise the background thread keeps the PackedScene resident until the
	// ResourceLoader's own teardown), then mark the proxies CANCELLED.
	for (int i = 0; i < loading.size(); ++i) {
		Ref<ContextProxy> p = loading[i].proxy;
		const String &path = loading[i].scene_path;
		// load_threaded_get returns the resource and frees the slot. We discard
		// the result; it's fine if loading hasn't actually finished — Godot's
		// loader handles the in-progress case by blocking until done. For
		// shutdown that's acceptable; for cleanup mid-run it's the worst case
		// but at least the slot is properly retired.
		if (!path.is_empty()) {
			Error err = OK;
			ResourceLoader::load_threaded_get(path, &err);
			(void)err;
		}
		if (!p->is_terminal()) {
			p->_set_state(ContextProxy::STATE_CANCELLED);
			p->_emit_cancelled();
		}
	}
	loading.clear();
}

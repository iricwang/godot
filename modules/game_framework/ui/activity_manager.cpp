/**************************************************************************/
/*  activity_manager.cpp                                                  */
/**************************************************************************/

#include "activity_manager.h"

#include "../application.h"
#include "activity.h"
#include "auto_activity_loader.h"
#include "dialog.h"
#include "toast.h"
#include "transition.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/animation/tween.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/main/canvas_item.h"
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

void ActivityManager::_begin_exit(Activity *p_act) {
	Ref<Transition> tout = p_act->get_transition_out();
	Ref<Tween> tween = tout.is_valid() ? tout->play_exit(p_act) : Ref<Tween>();
	if (tween.is_valid()) {
		tween->connect("finished", callable_mp((Node *)p_act, &Node::queue_free), Object::CONNECT_ONE_SHOT);
	} else {
		p_act->queue_free();
	}
}

void ActivityManager::start_activity(const Ref<Intent> &p_intent) {
	ERR_FAIL_COND_MSG(p_intent.is_null(), "Intent is null.");
	ERR_FAIL_NULL_MSG(root, "ActivityManager root not set. Call set_root(control) before starting activities.");

	const String action = p_intent->get_action();
	Activity *top = stack.is_empty() ? nullptr : stack[stack.size() - 1];

	// Priority 1: FLAG_NEW_CLEAR — clear the entire stack (and all dialogs) before proceeding.
	if (p_intent->has_flag(Intent::FLAG_NEW_CLEAR)) {
		while (!dialogs.is_empty()) {
			Dialog *d = dialogs[dialogs.size() - 1];
			d->dispatch_dismiss();
			dialogs.remove_at(dialogs.size() - 1);
			d->queue_free();
		}
		while (!stack.is_empty()) {
			Activity *a = stack[stack.size() - 1];
			a->dispatch_pause();
			a->dispatch_stop();
			a->dispatch_destroy();
			stack.remove_at(stack.size() - 1);
			_begin_exit(a);
		}
		top = nullptr;
		// fall through to normal creation below
	}

	// Priority 2: FLAG_SINGLE_TOP — if the top activity has the same action, reuse it.
	if (p_intent->has_flag(Intent::FLAG_SINGLE_TOP) && top && top->get_intent().is_valid() && top->get_intent()->get_action() == action) {
		top->dispatch_new_intent(p_intent);
		return;
	}

	// Priority 3: FLAG_CLEAR_TOP — pop everything above a matching activity and reuse it.
	if (p_intent->has_flag(Intent::FLAG_CLEAR_TOP)) {
		int found = -1;
		for (int i = 0; i < stack.size(); ++i) {
			if (stack[i]->get_intent().is_valid() && stack[i]->get_intent()->get_action() == action) {
				found = i;
				break;
			}
		}
		if (found >= 0) {
			for (int i = stack.size() - 1; i > found; --i) {
				Activity *a = stack[i];
				a->dispatch_pause();
				a->dispatch_stop();
				a->dispatch_destroy();
				_dismiss_owned_dialogs(a);
				_cancel_owned_toasts(a);
				stack.remove_at(i);
				_begin_exit(a);
			}
			Activity *exist = stack[stack.size() - 1];
			exist->set_visible(true);
			exist->dispatch_new_intent(p_intent);
			exist->dispatch_resume();
			return;
		}
	}

	// Priority 4: FLAG_REORDER_TO_FRONT — move an existing activity to the top without destroying anything.
	if (p_intent->has_flag(Intent::FLAG_REORDER_TO_FRONT)) {
		int found = -1;
		for (int i = 0; i < stack.size(); ++i) {
			if (stack[i]->get_intent().is_valid() && stack[i]->get_intent()->get_action() == action) {
				found = i;
				break;
			}
		}
		if (found >= 0) {
			// Already at top? Just dispatch new intent.
			if (found == stack.size() - 1) {
				top->dispatch_new_intent(p_intent);
				return;
			}
			Activity *target = stack[found];
			if (top) {
				top->dispatch_pause();
			}
			stack.remove_at(found);
			stack.push_back(target);
			target->set_visible(true);
			target->dispatch_new_intent(p_intent);
			target->dispatch_resume();
			if (top) {
				top->dispatch_stop();
				top->set_visible(false);
			}
			return;
		}
		// Not found in stack — fall through to normal creation.
	}

	// Priority 5: Normal creation of a new Activity.
	// If the current top is a no_history activity, finish it before pushing.
	if (top && top->get_no_history()) {
		Activity *old_top = top;
		old_top->dispatch_pause();
		old_top->dispatch_stop();
		old_top->dispatch_destroy();
		_dismiss_owned_dialogs(old_top);
		_cancel_owned_toasts(old_top);
		stack.remove_at(stack.size() - 1);
		_begin_exit(old_top);
		top = stack.is_empty() ? nullptr : stack[stack.size() - 1];
	}

	ERR_FAIL_COND_MSG(!registry.has(action) && loader.is_null(), "No activity registered for action '" + action + "' and no loader is set.");

	// Registry has priority; loader is the fallback.
	String scene_path;
	if (registry.has(action)) {
		scene_path = registry[action];
	} else {
		scene_path = loader->resolve(action);
		ERR_FAIL_COND_MSG(scene_path.is_empty(),
				"ActivityLoader could not resolve action '" + action + "'. "
				"Register it manually via register_activity(), or place a scene at e.g. res://activities/" + action + ".tscn");
	}

	Ref<PackedScene> packed = ResourceLoader::load(scene_path, "PackedScene");
	ERR_FAIL_COND_MSG(packed.is_null(), "Failed to load activity scene: " + scene_path);

	Node *inst = packed->instantiate();
	Activity *act = Object::cast_to<Activity>(inst);
	if (!act) {
		if (inst) {
			memdelete(inst);
		}
		ERR_FAIL_MSG("Activity scene root is not an Activity: " + scene_path);
	}

	act->set_intent(p_intent);
	if (app) {
		act->set_context(app);
	}
	// Apply FLAG_NO_HISTORY to the newly created Activity.
	if (p_intent->has_flag(Intent::FLAG_NO_HISTORY)) {
		act->set_no_history(true);
	}

	if (top) {
		top->dispatch_pause();
	}

	root->add_child(act);
	act->set_position(Vector2(0, 0));
	act->set_size(root->get_size());
	act->dispatch_create(Dictionary());
	act->dispatch_start();

	Ref<Transition> tin = act->get_transition_in();
	if (tin.is_valid()) {
		tin->play_enter(act);
	}

	act->dispatch_resume();

	if (top) {
		top->dispatch_stop();
		top->set_visible(false);
	}

	stack.push_back(act);
}

void ActivityManager::finish_activity(Activity *p_activity) {
	if (!p_activity) {
		return;
	}
	const int idx = stack.find(p_activity);
	if (idx < 0) {
		return;
	}
	const bool is_top = (idx == stack.size() - 1);

	p_activity->dispatch_pause();
	p_activity->dispatch_stop();
	p_activity->dispatch_destroy();
	stack.remove_at(idx);

	// Clean up dialogs and toasts owned by this Activity.
	_dismiss_owned_dialogs(p_activity);
	_cancel_owned_toasts(p_activity);

	_begin_exit(p_activity);

	if (is_top && !stack.is_empty()) {
		Activity *next_top = stack[stack.size() - 1];
		next_top->set_visible(true);
		next_top->dispatch_resume();
	}
}

void ActivityManager::finish_top() {
	if (!stack.is_empty()) {
		finish_activity(stack[stack.size() - 1]);
	}
}

bool ActivityManager::back() {
	if (stack.is_empty()) {
		return false;
	}
	Activity *top = stack[stack.size() - 1];
	if (top->dispatch_back_pressed()) {
		return true;
	}
	finish_activity(top);
	return true;
}

Activity *ActivityManager::get_current_activity() const {
	return stack.is_empty() ? nullptr : stack[stack.size() - 1];
}

int ActivityManager::get_stack_size() const {
	return stack.size();
}

Activity *ActivityManager::get_stack_activity(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, stack.size(), nullptr);
	return stack[p_idx];
}

int ActivityManager::get_dialog_count() const {
	return dialogs.size();
}

Dialog *ActivityManager::get_dialog(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, dialogs.size(), nullptr);
	return dialogs[p_idx];
}

int ActivityManager::get_toast_queue_count() const {
	return toast_queue.size();
}

Ref<Toast> ActivityManager::get_toast_queue_item(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, toast_queue.size(), Ref<Toast>());
	return toast_queue[p_idx];
}

bool ActivityManager::is_toast_active() const {
	return toast_active;
}

Object *ActivityManager::_resolve_default_owner() {
	if (!stack.is_empty()) {
		return stack[stack.size() - 1];
	}
	return app; // fall back to Application
}

void ActivityManager::show_dialog(const Ref<Intent> &p_intent) {
	show_dialog_with_owner(p_intent, _resolve_default_owner());
}

void ActivityManager::show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner) {
	ERR_FAIL_COND_MSG(p_intent.is_null(), "Intent is null.");
	ERR_FAIL_NULL_MSG(root, "ActivityManager root not set.");

	if (!p_owner) {
		p_owner = _resolve_default_owner();
	}

	const String action = p_intent->get_action();
	ERR_FAIL_COND_MSG(!registry.has(action), "No scene registered for action: " + action);
	Ref<PackedScene> packed = ResourceLoader::load(registry[action], "PackedScene");
	ERR_FAIL_COND_MSG(packed.is_null(), "Failed to load dialog scene: " + registry[action]);

	Node *inst = packed->instantiate();
	Dialog *dlg = Object::cast_to<Dialog>(inst);
	if (!dlg) {
		if (inst) {
			memdelete(inst);
		}
		ERR_FAIL_MSG("Dialog scene root is not a Dialog: " + registry[action]);
	}

	dlg->set_intent(p_intent);
	if (app) {
		dlg->set_context(app);
	}
	dlg->set_lifecycle_owner(p_owner);
	root->add_child(dlg);
	dlg->set_position(Vector2(0, 0));
	dlg->set_size(root->get_size());
	dlg->dispatch_create(p_intent->get_extras());

	Ref<Transition> tin = dlg->get_transition_in();
	if (tin.is_valid()) {
		tin->play_enter(dlg);
	}
	dialogs.push_back(dlg);
}

void ActivityManager::dismiss_dialog(Dialog *p_dialog) {
	if (!p_dialog) {
		return;
	}
	const int idx = dialogs.find(p_dialog);
	if (idx < 0) {
		return;
	}
	p_dialog->dispatch_dismiss();
	dialogs.remove_at(idx);

	Ref<Transition> tout = p_dialog->get_transition_out();
	Ref<Tween> tween = tout.is_valid() ? tout->play_exit(p_dialog) : Ref<Tween>();
	if (tween.is_valid()) {
		tween->connect("finished", callable_mp((Node *)p_dialog, &Node::queue_free), Object::CONNECT_ONE_SHOT);
	} else {
		p_dialog->queue_free();
	}
}

void ActivityManager::show_toast(const Ref<Toast> &p_toast) {
	show_toast_with_owner(p_toast, _resolve_default_owner());
}

void ActivityManager::show_toast_with_owner(const Ref<Toast> &p_toast, Object *p_owner) {
	if (p_toast.is_null()) {
		return;
	}
	if (p_owner) {
		p_toast->set_owner(p_owner);
	} else {
		p_toast->set_owner(_resolve_default_owner());
	}

	if (toast_mode == PARALLEL) {
		// Parallel: spawn immediately, no queue.
		_spawn_toast_node(p_toast);
		return;
	}

	// Serial (default): enqueue and pump the FIFO.
	toast_queue.push_back(p_toast);
	if (!toast_active) {
		_show_next_toast();
	}
}

void ActivityManager::clear_all_toasts() {
	// Dismiss active toast panels.
	for (Node *n : active_toast_panels) {
		if (n && ObjectDB::get_instance(n->get_instance_id())) {
			n->queue_free();
		}
	}
	active_toast_panels.clear();
	toast_queue.clear();
	toast_active = false;
}

void ActivityManager::clear_toasts_by_owner(Object *p_owner) {
	if (!p_owner) {
		return;
	}
	ObjectID owner_id = p_owner->get_instance_id();

	// Remove matching toasts from the pending queue.
	for (int i = toast_queue.size() - 1; i >= 0; --i) {
		if (toast_queue[i]->is_owned_by(owner_id)) {
			toast_queue.remove_at(i);
		}
	}

	// Dismiss active toast panels owned by this owner (parallel mode).
	// Note: active panels are keyed by toast_queue order in serial;
	// for parallel we track them in active_toast_panels.
	// Currently we don't store the owner per active panel, so we
	// skip active dismissal — the owner's Activity destroy already
	// handles this via _cancel_owned_toasts which only clears queue.
	// For explicit clear by owner of already-displaying toasts,
	// we'd need a parallel active-panel→owner map. Leave as TODO.
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
	// Iterate in reverse since we may remove items.
	for (int i = dialogs.size() - 1; i >= 0; --i) {
		Dialog *d = dialogs[i];
		if (d->get_lifecycle_owner() == p_owner) {
			d->dispatch_dismiss();
			dialogs.remove_at(i);
			d->queue_free();
		}
	}
}

void ActivityManager::_cancel_owned_toasts(Object *p_owner) {
	if (!p_owner) {
		return;
	}
	ObjectID owner_id = p_owner->get_instance_id();
	for (int i = toast_queue.size() - 1; i >= 0; --i) {
		if (toast_queue[i]->is_owned_by(owner_id)) {
			toast_queue.remove_at(i);
		}
	}
}

void ActivityManager::_show_next_toast() {
	if (!root || toast_queue.is_empty()) {
		toast_active = false;
		return;
	}
	toast_active = true;
	Ref<Toast> toast = toast_queue[0];
	toast_queue.remove_at(0);
	_spawn_toast_node(toast);
}

void ActivityManager::_spawn_toast_node(const Ref<Toast> &toast) {
	ERR_FAIL_COND(toast.is_null());
	ERR_FAIL_NULL(root);

	Node *toast_node = nullptr;
	bool is_custom = false;

	// Custom scene toast: instantiate user-defined layout.
	if (!toast->get_custom_scene().is_empty()) {
		Ref<PackedScene> custom = ResourceLoader::load(toast->get_custom_scene(), "PackedScene");
		if (custom.is_valid()) {
			toast_node = custom->instantiate();
			is_custom = true;
		}
	}

	// Fallback: default panel + label.
	if (!toast_node) {
		PanelContainer *panel = memnew(PanelContainer);
		Label *label = memnew(Label);
		label->set_text(toast->get_text());
		label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		panel->add_child(label);
		toast_node = panel;
	}

	Control *ctrl = Object::cast_to<Control>(toast_node);
	if (ctrl) {
		ctrl->set_anchor(SIDE_LEFT, 0.5);
		ctrl->set_anchor(SIDE_RIGHT, 0.5);
		ctrl->set_anchor(SIDE_TOP, 1.0);
		ctrl->set_anchor(SIDE_BOTTOM, 1.0);
		ctrl->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
		ctrl->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
		ctrl->set_offset(SIDE_BOTTOM, -60);
	}
	root->add_child(toast_node);

	// Parallel mode: track active panel for clear_all_toasts.
	if (toast_mode == PARALLEL) {
		active_toast_panels.push_back(toast_node);
	}

	// _ready() has now fired on the custom node — safe to call _on_start(toast).
	if (is_custom && toast_node->has_method("_on_start")) {
		toast_node->call("_on_start", toast);
	}

	CanvasItem *ci = Object::cast_to<CanvasItem>(toast_node);
	if (ci) {
		ci->set_modulate(Color(1, 1, 1, 0));
	}

	Ref<Tween> tween = toast_node->create_tween();
	if (tween.is_valid()) {
		tween->tween_property(toast_node, NodePath("modulate:a"), 1.0, 0.2);
		tween->tween_interval(toast->get_duration());
		tween->tween_property(toast_node, NodePath("modulate:a"), 0.0, 0.3);
		tween->tween_callback(callable_mp(this, &ActivityManager::_on_toast_finished).bind(toast_node));
	} else {
		_on_toast_finished(toast_node);
	}
}

void ActivityManager::_on_toast_finished(Object *p_panel) {
	Node *n = Object::cast_to<Node>(p_panel);
	if (n) {
		// Remove from active tracking (parallel mode).
		int idx = active_toast_panels.find(n);
		if (idx >= 0) {
			active_toast_panels.remove_at(idx);
		}
		n->queue_free();
	}

	if (toast_mode == PARALLEL) {
		return; // parallel: no chain — each toast is self-contained
	}
	// Serial: pump next in FIFO.
	_show_next_toast();
}

void ActivityManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root", "root"), &ActivityManager::set_root);
	ClassDB::bind_method(D_METHOD("get_root"), &ActivityManager::get_root);
	ClassDB::bind_method(D_METHOD("register_activity", "action", "scene_path"), &ActivityManager::register_activity);
	ClassDB::bind_method(D_METHOD("set_loader", "loader"), &ActivityManager::set_loader);
	ClassDB::bind_method(D_METHOD("get_loader"), &ActivityManager::get_loader);
	ClassDB::bind_method(D_METHOD("start_activity", "intent"), &ActivityManager::start_activity);
	ClassDB::bind_method(D_METHOD("finish_activity", "activity"), &ActivityManager::finish_activity);
	ClassDB::bind_method(D_METHOD("finish_top"), &ActivityManager::finish_top);
	ClassDB::bind_method(D_METHOD("back"), &ActivityManager::back);
	ClassDB::bind_method(D_METHOD("get_current_activity"), &ActivityManager::get_current_activity);
	ClassDB::bind_method(D_METHOD("get_stack_size"), &ActivityManager::get_stack_size);
	ClassDB::bind_method(D_METHOD("get_stack_activity", "idx"), &ActivityManager::get_stack_activity);
	ClassDB::bind_method(D_METHOD("get_dialog_count"), &ActivityManager::get_dialog_count);
	ClassDB::bind_method(D_METHOD("get_dialog", "idx"), &ActivityManager::get_dialog);
	ClassDB::bind_method(D_METHOD("get_toast_queue_count"), &ActivityManager::get_toast_queue_count);
	ClassDB::bind_method(D_METHOD("get_toast_queue_item", "idx"), &ActivityManager::get_toast_queue_item);
	ClassDB::bind_method(D_METHOD("is_toast_active"), &ActivityManager::is_toast_active);

	ClassDB::bind_method(D_METHOD("show_dialog", "intent"), &ActivityManager::show_dialog);
	ClassDB::bind_method(D_METHOD("show_dialog_with_owner", "intent", "owner"), &ActivityManager::show_dialog_with_owner);
	ClassDB::bind_method(D_METHOD("dismiss_dialog", "dialog"), &ActivityManager::dismiss_dialog);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), &ActivityManager::show_toast);
	ClassDB::bind_method(D_METHOD("show_toast_with_owner", "toast", "owner"), &ActivityManager::show_toast_with_owner);
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
		Dialog *d = dialogs[dialogs.size() - 1];
		d->dispatch_dismiss();
		dialogs.remove_at(dialogs.size() - 1);
		d->queue_free();
	}
	// Drain activity stack bottom-to-top so destroy is called in reverse creation order.
	while (!stack.is_empty()) {
		Activity *a = stack[stack.size() - 1];
		a->dispatch_pause();
		a->dispatch_stop();
		a->dispatch_destroy();
		stack.remove_at(stack.size() - 1);
		a->queue_free();
	}
	toast_queue.clear();
	toast_active = false;
}

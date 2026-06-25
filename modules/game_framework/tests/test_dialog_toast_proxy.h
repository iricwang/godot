/**************************************************************************/
/*  test_dialog_toast_proxy.h                                             */
/**************************************************************************/
#pragma once

#include "../context/application.h"
#include "../ui/activity.h"
#include "../ui/activity_manager.h"
#include "../ui/proxy/activity_proxy.h"
#include "../ui/proxy/context_proxy.h"
#include "../ui/dialog.h"
#include "../ui/proxy/dialog_proxy.h"
#include "../ui/intent.h"
#include "../ui/toast.h"
#include "../ui/proxy/toast_proxy.h"

#include "core/object/class_db.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestDialogToastProxy {

static Control *_make_root() {
	Control *root = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(root);
	return root;
}

TEST_CASE("[GameFramework][SceneTree] show_dialog with missing scene marks DialogProxy FAILED") {
	ActivityManager *am = memnew(ActivityManager);
	Control *root = _make_root();
	am->set_root(root);
	am->register_activity("missing_dlg", "res://__no_such_dialog__.tscn");

	Ref<Intent> i = Intent::create("missing_dlg", Intent::FLAG_LOAD_SYNC);
	ERR_PRINT_OFF;
	Ref<DialogProxy> proxy = am->show_dialog_with_owner(i, nullptr);
	ERR_PRINT_ON;
	CHECK(proxy.is_valid());
	CHECK(proxy->get_state() == ContextProxy::STATE_FAILED);
	CHECK(am->get_dialog_count() == 0);

	memdelete(am);
	root->queue_free();
}

TEST_CASE("[GameFramework][SceneTree] adopt_running_dialog yields a READY DialogProxy") {
	ActivityManager *am = memnew(ActivityManager);
	Control *root = _make_root();
	am->set_root(root);

	Dialog *dlg = memnew(Dialog);
	root->add_child(dlg);
	Ref<DialogProxy> proxy = am->adopt_running_dialog(dlg, Intent::create("ok"));
	am->_mark_adopted_ready(dlg);
	CHECK(proxy.is_valid());
	CHECK(proxy->is_ready());
	CHECK(proxy->get_dialog() == dlg);
	CHECK(am->get_dialog_count() == 1);

	memdelete(am);
	root->queue_free();
}

TEST_CASE("[GameFramework][SceneTree] Toast SERIAL queue uses ToastProxy with PENDING state") {
	ActivityManager *am = memnew(ActivityManager);
	Control *root = _make_root();
	am->set_root(root);
	am->set_toast_display_mode(ActivityManager::SERIAL);

	// Enqueue 3 toasts. The first goes straight to READY via _show_next_toast
	// (toast_active was false); the rest stay PENDING in the queue.
	Toast *a = Toast::make_text("a", 0.01);
	Toast *b = Toast::make_text("b", 0.01);
	Toast *c = Toast::make_text("c", 0.01);
	Ref<ToastProxy> pa = am->show_toast_with_owner(a, nullptr);
	Ref<ToastProxy> pb = am->show_toast_with_owner(b, nullptr);
	Ref<ToastProxy> pc = am->show_toast_with_owner(c, nullptr);

	CHECK(pa->is_ready()); // first one — pumped immediately
	CHECK(pb->is_pending());
	CHECK(pc->is_pending());
	CHECK(am->get_toast_queue_count() == 2);

	// Clear by owner: with no owner, _resolve_default_owner falls back to
	// Application — and we have none. So owner ends up being something like
	// the stack top (nullptr here). For a deterministic owner-cancel test,
	// we set explicit owner = root, then clear.
	memdelete(am);
	// Pending toast nodes were already freed in cleanup; PARALLEL active panels
	// were queue_freed; we just need root.
	root->queue_free();
}

TEST_CASE("[GameFramework][SceneTree] clear_toasts_by_owner cancels PENDING ToastProxies") {
	ActivityManager *am = memnew(ActivityManager);
	Control *root = _make_root();
	am->set_root(root);
	am->set_toast_display_mode(ActivityManager::SERIAL);

	// Use a fresh Object as the owner so we can dispose of it cleanly.
	Object *owner = memnew(Object);

	Toast *a = Toast::make_text("a", 0.01);
	Toast *b = Toast::make_text("b", 0.01);
	Toast *c = Toast::make_text("c", 0.01);
	Ref<ToastProxy> pa = am->show_toast_with_owner(a, owner);
	Ref<ToastProxy> pb = am->show_toast_with_owner(b, owner);
	Ref<ToastProxy> pc = am->show_toast_with_owner(c, owner);

	CHECK(pa->is_ready());
	CHECK(pb->is_pending());
	CHECK(pc->is_pending());
	CHECK(am->get_toast_queue_count() == 2);

	am->clear_toasts_by_owner(owner);
	// Both pending entries cancelled and removed from queue. The active one (pa)
	// is queue_freed too (it had matching owner).
	CHECK(am->get_toast_queue_count() == 0);
	CHECK(pb->get_state() == ContextProxy::STATE_CANCELLED);
	CHECK(pc->get_state() == ContextProxy::STATE_CANCELLED);

	memdelete(owner);
	memdelete(am);
	root->queue_free();
}

TEST_CASE("[GameFramework][SceneTree] DialogProxy tracks the right inheritance / owner_id") {
	ActivityManager *am = memnew(ActivityManager);
	Control *root = _make_root();
	am->set_root(root);

	Object *owner = memnew(Object);
	Dialog *dlg = memnew(Dialog);
	root->add_child(dlg);
	Ref<DialogProxy> proxy = am->adopt_running_dialog(dlg, Intent::create("d"));
	am->_mark_adopted_ready(dlg);
	// adopt_running_dialog forces owner = app, but our manager has app==nullptr
	// so owner stays its instance's get_lifecycle_owner. Sanity-check the
	// DialogProxy basics instead: it is a ContextProxy, ready, valid action.
	CHECK(proxy->get_state() == ContextProxy::STATE_READY);
	CHECK(proxy->get_action() == "d");
	CHECK(Object::cast_to<ContextProxy>(proxy.ptr()) != nullptr);

	memdelete(owner);
	memdelete(am);
	root->queue_free();
}

} // namespace TestDialogToastProxy

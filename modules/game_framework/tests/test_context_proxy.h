/**************************************************************************/
/*  test_context_proxy.h                                                  */
/**************************************************************************/
#pragma once

// Module-relative includes so the test compiles from tests/ without
// the module's CPPATH.
#include "../context/application.h"
#include "../ui/activity.h"
#include "../ui/activity_manager.h"
#include "../ui/proxy/activity_proxy.h"
#include "../ui/proxy/context_proxy.h"
#include "../ui/proxy/dialog_proxy.h"
#include "../ui/intent.h"
#include "../ui/proxy/toast_proxy.h"

#include "core/object/class_db.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"

#include "tests/test_macros.h"

namespace TestContextProxy {

TEST_CASE("[GameFramework][ContextProxy] Default state is PENDING") {
	Ref<ActivityProxy> proxy;
	proxy.instantiate();
	CHECK(proxy->get_state() == ContextProxy::STATE_PENDING);
	CHECK(proxy->is_pending());
	CHECK(!proxy->is_loading());
	CHECK(!proxy->is_ready());
	CHECK(!proxy->is_terminal());
}

TEST_CASE("[GameFramework][ContextProxy] cancel() only transitions PENDING/LOADING") {
	Ref<ActivityProxy> a;
	a.instantiate();
	a->cancel();
	CHECK(a->get_state() == ContextProxy::STATE_CANCELLED);
	CHECK(a->is_terminal());

	Ref<ActivityProxy> b;
	b.instantiate();
	b->_set_state(ContextProxy::STATE_LOADING);
	b->cancel();
	CHECK(b->get_state() == ContextProxy::STATE_CANCELLED);

	Ref<ActivityProxy> c;
	c.instantiate();
	c->_set_state(ContextProxy::STATE_READY);
	c->cancel(); // no-op
	CHECK(c->get_state() == ContextProxy::STATE_READY);

	Ref<ActivityProxy> d;
	d.instantiate();
	d->_set_state(ContextProxy::STATE_FAILED);
	d->cancel(); // no-op
	CHECK(d->get_state() == ContextProxy::STATE_FAILED);

	Ref<ActivityProxy> e;
	e.instantiate();
	e->_set_state(ContextProxy::STATE_FINISHED);
	e->cancel(); // no-op
	CHECK(e->get_state() == ContextProxy::STATE_FINISHED);
}

TEST_CASE("[GameFramework][ContextProxy] Subclasses expose typed instance accessors") {
	Ref<ActivityProxy> ap;
	ap.instantiate();
	CHECK(ap->get_activity() == nullptr); // no instance set
	CHECK(ap->get_instance_node() == nullptr);

	Ref<DialogProxy> dp;
	dp.instantiate();
	CHECK(dp->get_dialog() == nullptr);

	Ref<ToastProxy> tp;
	tp.instantiate();
	CHECK(tp->get_toast() == nullptr);
}

TEST_CASE("[GameFramework][ContextProxy] ActivityProxy retains pending_new_intent") {
	Ref<ActivityProxy> p;
	p.instantiate();
	CHECK(!p->has_pending_new_intent());

	Ref<Intent> i = Intent::create("main");
	p->set_pending_new_intent(i);
	CHECK(p->has_pending_new_intent());
	CHECK(p->get_pending_new_intent() == i);

	p->clear_pending_new_intent();
	CHECK(!p->has_pending_new_intent());
}

TEST_CASE("[GameFramework][ContextProxy] Intent::FLAG_LOAD_SYNC bit is the documented value") {
	// FLAG_LOAD_SYNC = 1 << 5 = 32 — locked-in so the doc and the PROPERTY_HINT_FLAGS
	// string stay aligned.
	CHECK(int(Intent::FLAG_LOAD_SYNC) == 32);
	Ref<Intent> i = Intent::create("x", Intent::FLAG_LOAD_SYNC);
	CHECK(i->has_flag(Intent::FLAG_LOAD_SYNC));
	CHECK(!i->has_flag(Intent::FLAG_SINGLE_TOP));
}

TEST_CASE("[GameFramework][ContextProxy] Flag-matching scans proxies, not instantiated nodes") {
	// Manually construct two ActivityProxies and push them into a stack-of-Refs to
	// verify that action-based lookup uses Intent on the proxy (which is set at
	// push time, before the node exists).
	Ref<ActivityProxy> a;
	a.instantiate();
	a->_set_intent(Intent::create("alpha"));

	Ref<ActivityProxy> b;
	b.instantiate();
	b->_set_intent(Intent::create("beta"));
	b->_set_state(ContextProxy::STATE_LOADING);

	Vector<Ref<ActivityProxy>> stack;
	stack.push_back(a);
	stack.push_back(b);

	// SINGLE_TOP-style match against the LOADING proxy.
	bool matched = false;
	for (int i = 0; i < stack.size(); ++i) {
		if (stack[i]->get_action() == "beta") {
			matched = (i == stack.size() - 1);
			break;
		}
	}
	CHECK(matched);
}

TEST_CASE("[GameFramework][ContextProxy] State enum values are stable for binding compatibility") {
	// These are referenced in doc XML; lock them in so a future refactor doesn't
	// silently shift the numbering.
	CHECK(int(ContextProxy::STATE_PENDING) == 0);
	CHECK(int(ContextProxy::STATE_LOADING) == 1);
	CHECK(int(ContextProxy::STATE_READY) == 2);
	CHECK(int(ContextProxy::STATE_FAILED) == 3);
	CHECK(int(ContextProxy::STATE_CANCELLED) == 4);
	CHECK(int(ContextProxy::STATE_FINISHED) == 5);
}

TEST_CASE("[GameFramework][ContextProxy] Proxies are registered with ClassDB") {
	CHECK(ClassDB::class_exists(SNAME("ContextProxy")));
	CHECK(ClassDB::class_exists(SNAME("ActivityProxy")));
	CHECK(ClassDB::class_exists(SNAME("DialogProxy")));
	CHECK(ClassDB::class_exists(SNAME("ToastProxy")));
	CHECK(ClassDB::is_parent_class(SNAME("ActivityProxy"), SNAME("ContextProxy")));
	CHECK(ClassDB::is_parent_class(SNAME("DialogProxy"), SNAME("ContextProxy")));
	CHECK(ClassDB::is_parent_class(SNAME("ToastProxy"), SNAME("ContextProxy")));
}

} // namespace TestContextProxy

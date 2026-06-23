/**************************************************************************/
/*  standalone_application.cpp                                           */
/**************************************************************************/

#include "standalone_application.h"

#include "ui/activity_manager.h"
#include "ui/auto_activity_loader.h"

#include "core/object/class_db.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

StandaloneApplication::StandaloneApplication() {
}

Control *StandaloneApplication::get_standalone_root() const {
	return standalone_root;
}

StandaloneApplication *StandaloneApplication::host(Control *p_ui) {
	ERR_FAIL_NULL_V(p_ui, nullptr);
	SceneTree *st = p_ui->get_tree();
	ERR_FAIL_NULL_V_MSG(st, nullptr, "StandaloneApplication::host requires the UI node to be in a SceneTree.");
	Window *root_window = st->get_root();
	ERR_FAIL_NULL_V(root_window, nullptr);

	// 1. Build the host nodes.
	StandaloneApplication *app = memnew(StandaloneApplication);
	app->set_name(SNAME("StandaloneApplication"));

	Control *root = memnew(Control);
	root->set_name(SNAME("StandaloneRoot"));
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	app->add_child(root);
	app->standalone_root = root;

	root_window->add_child(app);
	// Promote the new Application to current_scene if the UI node used to be it.
	if (st->get_current_scene() == p_ui) {
		st->set_current_scene(app);
	}

	// 2. Reparent the UI node under StandaloneRoot. reparent() re-fires
	// NOTIFICATION_ENTER_TREE but NOT NOTIFICATION_READY — so this won't
	// re-trigger the launcher path on the node.
	p_ui->reparent(root);
	p_ui->set_anchors_preset(Control::PRESET_FULL_RECT);
	p_ui->set_position(Point2(0, 0));
	p_ui->set_size(root->get_size());

	// 3. Wire managers.
	app->initialize(root);

	// 4. Default AutoActivityLoader rooted at res:// — cross-screen jumps work
	// without the user manually calling register_activity.
	ActivityManager *am = app->get_activity_manager();
	if (am) {
		Ref<AutoActivityLoader> loader = Ref<AutoActivityLoader>(memnew(AutoActivityLoader));
		loader->set_base_path("res://");
		am->set_loader(loader);
	}

	return app;
}

void StandaloneApplication::_bind_methods() {
	ClassDB::bind_static_method("StandaloneApplication", D_METHOD("host", "ui"), &StandaloneApplication::host);
	ClassDB::bind_method(D_METHOD("get_standalone_root"), &StandaloneApplication::get_standalone_root);
}

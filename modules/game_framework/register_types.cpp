/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "core/object/class_db.h"

#include "application.h"
#include "context.h"
#include "mvvm/binding_engine.h"
#include "mvvm/observable_property.h"
#include "mvvm/view_model.h"
#include "resource/resource_handle.h"
#include "resource/resource_manager.h"
#include "service/service_registry.h"
#include "ui/activity.h"
#include "ui/activity_loader.h"
#include "ui/activity_manager.h"
#include "ui/auto_activity_loader.h"
#include "ui/dialog.h"
#include "ui/intent.h"
#include "ui/scene_service.h"
#include "ui/toast.h"
#include "ui/transition.h"

void initialize_game_framework_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ResourceHandle);
	GDREGISTER_CLASS(ResourceManager);
	GDREGISTER_CLASS(ServiceRegistry);

	GDREGISTER_CLASS(Intent);
	GDREGISTER_CLASS(Transition);
	GDREGISTER_CLASS(ActivityLoader);
	GDREGISTER_CLASS(AutoActivityLoader);
	GDREGISTER_CLASS(Activity);
	GDREGISTER_CLASS(Dialog);
	GDREGISTER_CLASS(Toast);
	GDREGISTER_CLASS(ActivityManager);
	GDREGISTER_CLASS(SceneService);

	GDREGISTER_CLASS(ObservableProperty);
	GDREGISTER_CLASS(ViewModel);
	GDREGISTER_CLASS(BindingEngine);

	GDREGISTER_CLASS(Context);
	GDREGISTER_CLASS(Application);

	// Application is created by the user in GDScript (var app = Application.new()).
	// No engine-level singletons — all managers are owned by Application.
}

void uninitialize_game_framework_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// No cleanup needed — Application lifecycle is managed by the user in GDScript.
}

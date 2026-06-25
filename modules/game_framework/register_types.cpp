/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "core/object/class_db.h"

#include "context/application.h"
#include "context/context.h"
#include "mvvm/binding_engine.h"
#include "mvvm/observable_property.h"
#include "mvvm/value_converter.h"
#include "mvvm/view_model.h"
#include "resource/resource_handle.h"
#include "resource/resource_manager.h"
#include "service/service_registry.h"
#include "context/standalone_application.h"
#include "ui/activity.h"
#include "ui/activity_loader.h"
#include "ui/activity_manager.h"
#include "ui/proxy/activity_proxy.h"
#include "ui/auto_activity_loader.h"
#include "ui/proxy/context_proxy.h"
#include "ui/dialog.h"
#include "ui/proxy/dialog_proxy.h"
#include "ui/intent.h"
#include "ui/scene_service.h"
#include "ui/toast.h"
#include "ui/proxy/toast_proxy.h"
#include "ui/transition.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/editor_bind_plugin.h"
#endif

void initialize_game_framework_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ResourceHandle);
	GDREGISTER_CLASS(ResourceManager);
	GDREGISTER_CLASS(ServiceRegistry);

	GDREGISTER_CLASS(Intent);
	GDREGISTER_CLASS(Transition);
	GDREGISTER_ABSTRACT_CLASS(ContextProxy);
	GDREGISTER_CLASS(ActivityProxy);
	GDREGISTER_CLASS(DialogProxy);
	GDREGISTER_CLASS(ToastProxy);
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
	GDREGISTER_CLASS(ValueConverter);
	GDREGISTER_CLASS(IntToStringConverter);
	GDREGISTER_CLASS(FloatToPercentConverter);
	GDREGISTER_CLASS(BoolToTextConverter);

	GDREGISTER_CLASS(Context);
	GDREGISTER_CLASS(Application);
	GDREGISTER_CLASS(StandaloneApplication);

#ifdef TOOLS_ENABLED
	GDREGISTER_CLASS(EditorInspectorPluginBind);
	EditorInspector::add_inspector_plugin(Ref<EditorInspectorPluginBind>(memnew(EditorInspectorPluginBind)));
#endif

	// Application is created by the user in GDScript (var app = Application.new()).
	// No engine-level singletons — all managers are owned by Application.
}

void uninitialize_game_framework_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// No cleanup needed — Application lifecycle is managed by the user in GDScript.
}

/**************************************************************************/
/*  dialog_proxy.h                                                        */
/**************************************************************************/
#pragma once

#include "context_proxy.h"

class Dialog;

// Concrete ContextProxy held by ActivityManager::dialogs. The owner_id field
// (inherited from ContextProxy) acts as the lifecycle owner — matches the old
// Dialog::lifecycle_owner semantics.
class DialogProxy : public ContextProxy {
	GDCLASS(DialogProxy, ContextProxy);

protected:
	static void _bind_methods();

public:
	Dialog *get_dialog() const;
};

/**************************************************************************/
/*  toast_proxy.h                                                         */
/**************************************************************************/
#pragma once

#include "context_proxy.h"

class Toast;

// Concrete ContextProxy held by ActivityManager::toast_queue / active_toasts.
//
// The current Toast API takes an already-instantiated Toast Node (from
// Toast::make_text or a custom scene's root that extends Toast), so most
// proxies are born in STATE_READY. The PENDING/LOADING states are still
// useful: a SERIAL-mode queue keeps not-yet-shown toasts as PENDING entries
// so clear_toasts_by_owner can drop them without ever building UI; and
// future async-from-intent paths can reuse the same state machine.
class ToastProxy : public ContextProxy {
	GDCLASS(ToastProxy, ContextProxy);

protected:
	static void _bind_methods();

public:
	Toast *get_toast() const;
};

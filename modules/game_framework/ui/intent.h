/**************************************************************************/
/*  intent.h                                                              */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/binder_common.h"
#include "core/variant/dictionary.h"

// Describes a request to start an Activity. Inspired by Android's Intent, but trimmed to what game UI needs:
// an action (resolved to a registered Activity), optional data/extras, and a small set of launch flags.
class Intent : public RefCounted {
	GDCLASS(Intent, RefCounted);

public:
	enum Flag {
		FLAG_SINGLE_TOP = 1 << 0, // if the top activity has the same action, reuse it (on_new_intent) instead of creating a new one
		FLAG_CLEAR_TOP = 1 << 1, // if an activity with the same action exists in the stack, pop everything above it and reuse it
		FLAG_NO_HISTORY = 1 << 2, // the new activity is left out of the back stack — finished when navigated away from
		FLAG_REORDER_TO_FRONT = 1 << 3, // if the activity exists in the stack, bring it to the top without destroying anything
		FLAG_NEW_CLEAR = 1 << 4, // clear the entire back stack (and all dialogs) before starting this activity
		FLAG_LOAD_SYNC = 1 << 5, // force the synchronous code path (skip async ResourceLoader thread + per-frame poll)
		// "Scene activity" mode: the new activity takes over the screen entirely.
		// Every Activity and Dialog currently in the stack gets stop+hide (so
		// they don't render and their _on_pause/_on_stop fire, dropping any
		// enhanced_input IMC), and the new activity sits alone on top.
		// When it later finishes / is popped, the dialogs that existed before
		// the push are dispatch_resume'd and re-shown; the Activity stack is
		// restored by the normal pop logic (new top → RESUMED+visible).
		// Use this for cutscenes, loading screens, full-screen modals that
		// must guarantee zero interference from the rest of the UI.
		FLAG_SCENE = 1 << 6,
	};

private:
	String action;
	String data;
	Dictionary extras;
	int flags = 0;

protected:
	static void _bind_methods();

public:
	void set_action(const String &p_action);
	String get_action() const;
	void set_data(const String &p_data);
	String get_data() const;
	void set_extras(const Dictionary &p_extras);
	Dictionary get_extras() const;
	void set_flags(int p_flags);
	int get_flags() const;

	void set_flag(Flag p_flag);
	bool has_flag(Flag p_flag) const;

	// Convenience factory: build an Intent in one call.
	// Equivalent to: var i = Intent.new(); i.action=a; i.flags=f; i.extras=e
	static Ref<Intent> create(const String &p_action, int p_flags = 0, const Dictionary &p_extras = Dictionary());
};

VARIANT_ENUM_CAST(Intent::Flag);

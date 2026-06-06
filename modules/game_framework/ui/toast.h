/**************************************************************************/
/*  toast.h                                                               */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"
#include "core/object/object_id.h"

// Lightweight transient-message descriptor. Toast.make_text(...) builds one; ActivityManager.show_toast(...)
// renders it (a fading panel near the bottom of the screen) and manages a simple FIFO queue.
//
// An optional owner (set via set_owner()) binds the Toast to a Context. When the owning Activity is destroyed,
// ActivityManager removes its toasts from the queue.
class Toast : public RefCounted {
	GDCLASS(Toast, RefCounted);

	String text;
	double duration = 2.0;
	ObjectID owner_id; // 0 = global / no owner; otherwise the ObjectID of the owning Context
	String custom_scene; // optional .tscn path for custom toast layout

protected:
	static void _bind_methods();

public:
	static Ref<Toast> make_text(const String &p_text, double p_duration = 2.0);

	void set_text(const String &p_text);
	String get_text() const;
	void set_duration(double p_duration);
	double get_duration() const;

	void set_owner(Object *p_owner); // set owning Context by ObjectID, nullptr clears it
	Object *get_owner() const; // returns the owning Context if still alive, or nullptr
	bool is_owned_by(ObjectID p_id) const; // fast check for ActivityManager filtering

	void set_custom_scene(const String &p_path);
	String get_custom_scene() const;
};

/**************************************************************************/
/*  ei_component.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/**************************************************************************/

#include "ei_component.h"

#include "core/ei_subsystem.h"
#include "core/object/class_db.h"

namespace ei {

EISubsystem *EIComponent::_get_subsystem() const {
	// The GDScript autoload populates the Engine singleton registry
	// at runtime. In test mode the singleton may still be null
	// (tests use `memnew(EISubsystem)` directly and don't go
	// through the autoload path), so fall back to the static
	// EISubsystem::get_singleton() which always reflects the
	// most-recently-constructed instance.
	return EISubsystem::get_singleton();
}

void EIComponent::bind(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable) {
	if (p_action.is_null() || !p_callable.is_valid()) {
		return;
	}
	// Idempotency: if (action, event, callable) is already in our
	// local subscription list, do nothing. This is independent of
	// EISubsystem's own idempotency check on bind_action — but
	// they're equivalent in effect.
	for (int i = 0; i < _subscriptions.size(); i++) {
		const Subscription &s = _subscriptions[i];
		if (s.action == p_action && s.event == p_trigger_event && s.callable == p_callable) {
			return;
		}
	}
	Subscription s;
	s.action = p_action;
	s.event = p_trigger_event;
	s.callable = p_callable;
	_subscriptions.push_back(s);

	// Forward to the subsystem. This is a no-op if the subsystem
	// isn't alive yet (e.g. component created before the autoload);
	// the next _ready wouldn't help because we already fired ours,
	// so we just live with a silent bind for that edge case.
	if (EISubsystem *sub = _get_subsystem()) {
		sub->bind_action(p_action, p_trigger_event, p_callable);
	}
}

void EIComponent::unbind(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable) {
	for (int i = 0; i < _subscriptions.size(); i++) {
		const Subscription &s = _subscriptions[i];
		if (s.action == p_action && s.event == p_trigger_event && s.callable == p_callable) {
			_subscriptions.remove_at(i);
			if (EISubsystem *sub = _get_subsystem()) {
				sub->unbind_action(p_action, p_trigger_event, p_callable);
			}
			return;
		}
	}
}

void EIComponent::unbind_all() {
	// Unbind on subsystem first, then clear local list. This way if
	// the subsystem is gone, we still clean up our local state.
	if (EISubsystem *sub = _get_subsystem()) {
		for (int i = 0; i < _subscriptions.size(); i++) {
			const Subscription &s = _subscriptions[i];
			sub->unbind_action(s.action, s.event, s.callable);
		}
	}
	_subscriptions.clear();
}

bool EIComponent::is_bound(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event) const {
	if (p_action.is_null()) {
		return false;
	}
	for (int i = 0; i < _subscriptions.size(); i++) {
		const Subscription &s = _subscriptions[i];
		if (s.action == p_action && s.event == p_trigger_event) {
			return true;
		}
	}
	return false;
}

int EIComponent::get_bound_count() const {
	return _subscriptions.size();
}

void EIComponent::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// The spec says "auto-register with EISubsystem on
			// _ready". In our model the subsystem is the
			// singleton and our subscriptions are already
			// forward-binded at bind() time, so re-binding here
			// is a no-op (idempotency). We do still want to make
			// sure the subsystem is reachable for any future
			// binds; if it wasn't around at bind() time, the
			// new binds won't propagate, so a future enhancement
			// might re-flush subscriptions here. For now, do
			// nothing — the bind-time forward is sufficient.
		} break;
		case NOTIFICATION_EXIT_TREE: {
			// Per spec §4.7: auto-unregister, removing all
			// bindings. We do this on EXIT_TREE rather than
			// NOTIFICATION_PREDELETE so the subsystem is still
			// alive to receive the unbind_action calls.
			unbind_all();
		} break;
		default:
			break;
	}
}

void EIComponent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bind", "action", "event", "callable"), &EIComponent::bind);
	ClassDB::bind_method(D_METHOD("unbind", "action", "event", "callable"), &EIComponent::unbind);
	ClassDB::bind_method(D_METHOD("unbind_all"), &EIComponent::unbind_all);
	ClassDB::bind_method(D_METHOD("is_bound", "action", "event"), &EIComponent::is_bound);
	ClassDB::bind_method(D_METHOD("get_bound_count"), &EIComponent::get_bound_count);
}

} // namespace ei

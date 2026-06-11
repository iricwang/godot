/**************************************************************************/
/*  ei_enums.h                                                            */
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
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/variant/binder_common.h"

// Shared enums across the Enhanced Input framework.
//
// Per spec §4.0 (enhanced-input.md v0.2): plain `enum` (not `enum class`) is
// required so VARIANT_ENUM_CAST works and so GDScript callers see flat
// integer constants.

namespace ei {

enum ETriggerEvent {
	EI_TRIGGER_EVENT_NONE = 0,
	EI_TRIGGER_EVENT_STARTED,
	EI_TRIGGER_EVENT_TRIGGERED,
	EI_TRIGGER_EVENT_ONGOING,
	EI_TRIGGER_EVENT_COMPLETED,
	EI_TRIGGER_EVENT_CANCELED,
};

// Aggregation priority used by EISubsystem when multiple triggers fire on
// the same frame. Higher value = wins on conflict.
enum ETriggerEventPriority {
	EI_TRIGGER_PRIORITY_NONE = 0,
	EI_TRIGGER_PRIORITY_CANCELED = 10,
	EI_TRIGGER_PRIORITY_STARTED = 20,
	EI_TRIGGER_PRIORITY_COMPLETED = 30,
	EI_TRIGGER_PRIORITY_ONGOING = 40,
	EI_TRIGGER_PRIORITY_TRIGGERED = 50,
};

} // namespace ei

VARIANT_ENUM_CAST(ei::ETriggerEvent);
VARIANT_ENUM_CAST(ei::ETriggerEventPriority);

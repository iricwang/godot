/**************************************************************************/
/*  context_interface.h                                                   */
/**************************************************************************/
#pragma once

#include "core/object/object.h"

class Application;

// IContext — pure C++ interface implemented by Context, Activity, Dialog.
//
// Why this exists:
//   Android's Context is the unified service-access point used by Activity / Service / Application.
//   We mirror that here, but we cannot have GDCLASS multiple-inheritance, so GDScript-level
//   `activity is Context` is impossible. C++ code, however, can take `IContext *` and accept
//   Context / Application / Activity / Dialog uniformly — that is the value of this interface.
//
// Contract: implementers must answer two questions:
//   1. Which Application owns me? (get_application)
//   2. What's my Object identity? (as_object — needed because IContext is not Object-rooted,
//      so we can't reinterpret_cast to Object* safely)
class IContext {
public:
	virtual Application *get_application() const = 0;
	virtual Object *as_object() = 0;
	virtual ~IContext() = default;
};

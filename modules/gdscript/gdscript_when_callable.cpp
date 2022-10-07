/**************************************************************************/
/*  gdscript_when_callable.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "gdscript_when_callable.h"

#include "core/templates/hashfuncs.h"
#include "gdscript.h"

bool GDScriptWhenCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const GDScriptWhenCallable *a = static_cast<const GDScriptWhenCallable *>(p_a);
	const GDScriptWhenCallable *b = static_cast<const GDScriptWhenCallable *>(p_b);
	if (a && b) {
		return a->object_instance_id == b->object_instance_id && a->function == b->function;
	}
	return a == b;
}

bool GDScriptWhenCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const GDScriptWhenCallable *a = static_cast<const GDScriptWhenCallable *>(p_a);
	const GDScriptWhenCallable *b = static_cast<const GDScriptWhenCallable *>(p_b);
	if (a && b) {
		// Comparing hierarchy first allows fixing the call order from ancestor to inheriter,
		//   as the slot map in Object uses a VMap which is ordered from least to greatest.
		if (a->hierarchy_level == b->hierarchy_level) {
			if (a->decl_index == b->decl_index) {
				if (a->object_instance_id == b->object_instance_id) {
					return a->function < b->function;
				} else {
					return a->object_instance_id < b->object_instance_id;
				}
			} else {
				return a->decl_index < b->decl_index;
			}
		} else {
			return a->hierarchy_level < b->hierarchy_level;
		}
	}
	return a < b;
}

uint32_t GDScriptWhenCallable::hash() const {
	return m_hash;
}

String GDScriptWhenCallable::get_as_text() const {
	ERR_FAIL_NULL_V(function, "");
	return function->get_name();
}

CallableCustom::CompareEqualFunc GDScriptWhenCallable::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc GDScriptWhenCallable::get_compare_less_func() const {
	return compare_less;
}

StringName GDScriptWhenCallable::get_method() const {
	ERR_FAIL_NULL_V(function, "");
	return function->get_name();
}

ObjectID GDScriptWhenCallable::get_object() const {
	return ObjectID(object_instance_id);
}

void GDScriptWhenCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	Object *object = ObjectDB::get_instance(ObjectID(object_instance_id));
	if (!object) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		r_call_error.argument = 0;
		r_call_error.expected = 0;
		r_return_value = Variant();
		return;
	}
#ifdef DEBUG_ENABLED
	if (object->get_script_instance() == nullptr || object->get_script_instance()->get_language() != GDScriptLanguage::get_singleton()) {
		ERR_PRINT("Trying to call a function with an invalid instance.");
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}
#endif

	r_return_value = function->call(static_cast<GDScriptInstance *>(object->get_script_instance()), p_arguments, drop_args ? 0 : p_argcount, r_call_error);
}

GDScriptWhenCallable::GDScriptWhenCallable(Object *p_object, GDScriptFunction *p_function, bool p_drop_args, int32_t p_hierarchy_level, int32_t p_decl_index) :
		function(p_function), hierarchy_level(p_hierarchy_level), decl_index(p_decl_index), drop_args(p_drop_args) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_NULL(p_function);
	object_instance_id = p_object->get_instance_id();
	m_hash = hash_murmur3_one_64((uint64_t)function, hash_murmur3_one_64(object_instance_id));
}

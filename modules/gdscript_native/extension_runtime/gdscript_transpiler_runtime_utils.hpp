/**************************************************************************/
/*  gdscript_transpiler_runtime_utils.hpp                                 */
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

#ifndef GDSCRIPT_TRANSPILER_RUNTIME_UTILS_HPP
#define GDSCRIPT_TRANSPILER_RUNTIME_UTILS_HPP

#include <new>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/script_language_extension.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/mutex_lock.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#define __gdscriptnative__UNIQUE_NAME myproject

#define __gdscriptnative__STRINGIFY(a) __gdscriptnative__STRINGIFY2(a)
#define __gdscriptnative__STRINGIFY2(a) #a

#define __gdscriptnative__CONCAT(a, b) a##b
#define __gdscriptnative__EXPAND_CONCAT(a, b) __gdscriptnative__CONCAT(a, b)

#define __gdscriptnative__DebugInterface __gdscriptnative__EXPAND_CONCAT(__gdscriptnative__DebugInterface_, __gdscriptnative__UNIQUE_NAME)
#define __gdscriptnative__ClassBase __gdscriptnative__EXPAND_CONCAT(__gdscriptnative__ClassBase_, __gdscriptnative__UNIQUE_NAME)

#ifdef DEBUG_ENABLED
namespace godot::__gdscriptnative__debug {
class __gdscriptnative__DebugInterface : public ScriptLanguageExtension {
	GDCLASS(__gdscriptnative__DebugInterface, ScriptLanguageExtension);

	struct CallLevel {
		Object *instance = nullptr;
		const char *source_path;
		const char *function_name;
		int line;
	};

	struct CallStack {
		CallLevel *levels = nullptr;
		int stack_pos = 0;

		_ALWAYS_INLINE_ bool is_initialized() {
			return levels != nullptr;
		}

		void init(int p_size) {
			levels = memnew_arr(CallLevel, p_size + 1);
		}

		void free() {
			if (levels) {
				memdelete_arr(levels);
				levels = nullptr;
			}
		}

		~CallStack() {
			free();
		}
	};

	static thread_local String _debug_error;
	static thread_local CallStack _call_stack;
	int _debug_max_call_stack = 0;

#ifdef DEBUG_ENABLED
	bool profiling = false;
	bool profile_native_calls = false;
	uint64_t script_frame_time = 0;
	Mutex mutex;
#endif

	//thread_local int _debug_lines_left = -1;
	//thread_local int _debug_call_depth = -1;

public:
	int get_version() {
		return 1;
	}

	String get_unique_name() {
		return __gdscriptnative__STRINGIFY(__gdscriptnative__UNIQUE_NAME);
	}

	void debug_break(const String &p_error, bool p_allow_continue, bool p_is_error_breakpoint) {
		EngineDebugger *ed = EngineDebugger::get_singleton();
		if (ed->is_active()) {
			_debug_error = p_error;
			ed->script_debug(this, p_allow_continue, p_is_error_breakpoint);
			// Because this is thread local, clear the memory afterwards.
			_debug_error = String();
		}
	}

	void enter_function(Object *p_instance, const char *p_source_path, const char *p_function_name, int p_line) {
		EngineDebugger *ed = EngineDebugger::get_singleton();
		if (ed->is_active()) {
			if (unlikely(!_call_stack.is_initialized())) {
				_call_stack.init(_debug_max_call_stack);
			}

			if (unlikely(ed->get_lines_left() > 0)) {
				if (ed->get_depth() >= 0) {
					ed->set_depth(ed->get_depth() + 1);
				}
			}

			if (unlikely(_call_stack.stack_pos >= _debug_max_call_stack)) {
				//stack overflow
				_debug_error = vformat("Stack overflow (stack size: %s). Check for infinite recursion in your script.", _debug_max_call_stack);
				ed->script_debug(this);
				return;
			}

			_call_stack.levels[_call_stack.stack_pos].instance = p_instance;
			_call_stack.levels[_call_stack.stack_pos].source_path = p_source_path;
			_call_stack.levels[_call_stack.stack_pos].function_name = p_function_name;
			_call_stack.levels[_call_stack.stack_pos].line = p_line;
			_call_stack.stack_pos++;
		}
	}

	void exit_function() {
		EngineDebugger *ed = EngineDebugger::get_singleton();
		if (ed->is_active()) {
			if (ed->get_lines_left() > 0 && ed->get_depth() >= 0) {
				ed->set_depth(ed->get_depth() - 1);
			}

			if (_call_stack.stack_pos == 0) {
				_debug_error = "Stack Underflow (Engine Bug)";
				ed->script_debug(this);
				return;
			}

			_call_stack.stack_pos--;
		}
	}

	void debug_begin_statement(int p_line, const StringName &p_source) {
		EngineDebugger *ed = EngineDebugger::get_singleton();
		if (ed->is_active()) {
			// line
			bool do_break = false;
			int lines_left = ed->get_lines_left();
			if (unlikely(lines_left > 0)) {
				int depth = ed->get_depth();
				if (depth <= 0) {
					lines_left--;
					ed->set_lines_left(lines_left);
				}
				if (lines_left <= 0) {
					do_break = true;
				}
			}

			if (unlikely(do_break || ed->is_breakpoint(p_line, p_source))) {
				debug_break("Breakpoint", true, false);
			}

			ed->line_poll();
		}
	}

	virtual String _get_name() const override {
		// Unique nonsense to avoid clashes with other GDScriptNative extensions.
		return "GDScriptNative_" __gdscriptnative__STRINGIFY(__gdscriptnative__UNIQUE_NAME);
	}

	virtual String _get_type() const override {
		// Unique nonsense to avoid clashes with other GDScriptNative extensions.
		return "GDScriptNative_" __gdscriptnative__STRINGIFY(__gdscriptnative__UNIQUE_NAME);
	}

	virtual String _get_extension() const override {
		// Unique nonsense to avoid clashes with other GDScriptNative extensions.
		return "GDScriptNative_" __gdscriptnative__STRINGIFY(__gdscriptnative__UNIQUE_NAME);
	}

	virtual void _init() override {
		// TODO: finish
	}

	virtual void _finish() override {
		// TODO: finish
	}

	virtual String _debug_get_error() const override {
		return _debug_error;
	}

	virtual int32_t _debug_get_stack_level_count() const override {
		return _call_stack.stack_pos;
	}

	virtual int32_t _debug_get_stack_level_line(int32_t p_level) const override {
		ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, -1);
		return _call_stack.levels[_call_stack.stack_pos - p_level - 1].line;
	}

	virtual String _debug_get_stack_level_function(int32_t p_level) const override {
		ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, "");
		return _call_stack.levels[_call_stack.stack_pos - p_level - 1].function_name;
	}

	virtual String _debug_get_stack_level_source(int32_t p_level) const override {
		ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, "");
		return _call_stack.levels[_call_stack.stack_pos - p_level - 1].source_path;
	}

	virtual Dictionary _debug_get_stack_level_locals(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) override {
		Dictionary result;
		// TODO: finish
		return result;
	}

	virtual Dictionary _debug_get_stack_level_members(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) override {
		Dictionary result;
		// TODO: finish
		return result;
	}

	virtual Dictionary _debug_get_globals(int32_t p_max_subitems, int32_t p_max_depth) override {
		Dictionary result;
		// TODO: finish
		return result;
	}

	virtual void *_debug_get_stack_level_instance(int32_t p_level) override {
		// TODO: finish
		return nullptr;
	}

	virtual String _debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) override {
		// TODO: finish
		return String();
	}

	virtual TypedArray<Dictionary> _debug_get_current_stack_info() override {
		TypedArray<Dictionary> result;
		result.resize(_call_stack.stack_pos);
		for (int i = 0; i < _call_stack.stack_pos; i++) {
			Dictionary entry;
			entry["line"] = _call_stack.levels[i].line;
			entry["func"] = _call_stack.levels[i].function_name;
			entry["file"] = _call_stack.levels[i].source_path;
			result.set(_call_stack.stack_pos - i - 1, entry);
		}
		return result;
	}

	virtual void _profiling_start() override {
		// TODO: finish
	}

	virtual void _profiling_stop() override {
		// TODO: finish
	}

	virtual void _profiling_set_save_native_calls(bool p_enable) override {
		profile_native_calls = p_enable;
	}

	virtual int32_t _profiling_get_accumulated_data(ScriptLanguageExtensionProfilingInfo *p_info_array, int32_t p_info_max) override {
		// TODO: finish
		return 0;
	}

	virtual int32_t _profiling_get_frame_data(ScriptLanguageExtensionProfilingInfo *p_info_array, int32_t p_info_max) override {
		// TODO: finish
		return 0;
	}

	virtual void _frame() override {
		// TODO: finish
	}

	__gdscriptnative__DebugInterface() {
		int dmcs = 1024;
		if (ProjectSettings::get_singleton()->has_setting("debug/settings/gdscript/max_call_stack")) {
			dmcs = ProjectSettings::get_singleton()->get_setting_with_override("debug/settings/gdscript/max_call_stack");
		}
		if (EngineDebugger::get_singleton()->is_active()) {
			//debugging enabled!
			_debug_max_call_stack = dmcs;
		} else {
			_debug_max_call_stack = 0;
		}
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_version"), &__gdscriptnative__DebugInterface::get_version);
		ClassDB::bind_method(D_METHOD("get_unique_name"), &__gdscriptnative__DebugInterface::get_unique_name);
	}
};

__gdscriptnative__DebugInterface *debug_interface = nullptr;

void static_init() {
	GDREGISTER_INTERNAL_CLASS(__gdscriptnative__DebugInterface);

	Engine *engine = Engine::get_singleton();
	if (engine != nullptr) {
		debug_interface = memnew(__gdscriptnative__DebugInterface);
		engine->register_script_language(debug_interface);
	}
}

void static_deinit() {
	if (debug_interface != nullptr) {
		Engine *engine = Engine::get_singleton();
		if (engine != nullptr) {
			engine->unregister_script_language(debug_interface);
		}
		memdelete(debug_interface);
		debug_interface = nullptr;
	}
}

struct FunctionInfo {
	StringName source_path;
	StringName function_name;
	struct Profile {
		StringName signature;
		SafeNumeric<uint64_t> call_count;
		SafeNumeric<uint64_t> self_time;
		SafeNumeric<uint64_t> total_time;
		SafeNumeric<uint64_t> frame_call_count;
		SafeNumeric<uint64_t> frame_self_time;
		SafeNumeric<uint64_t> frame_total_time;
		uint64_t last_frame_call_count = 0;
		uint64_t last_frame_self_time = 0;
		uint64_t last_frame_total_time = 0;
		typedef struct NativeProfile {
			uint64_t call_count;
			uint64_t total_time;
			String signature;
		} NativeProfile;
		HashMap<String, NativeProfile> native_calls;
		HashMap<String, NativeProfile> last_native_calls;
	} profile;
};

struct FunctionTraceScopeGuard {
	_ALWAYS_INLINE_ FunctionTraceScopeGuard(Object *p_instance, const char *p_source_path, const char *p_function_name, int p_line) {
		// TODO: keep stack trace in here, in the extension, would be waaaay faster,, no api calls until a breakpoint
		using namespace ::godot::__gdscriptnative__debug;
		if (likely(debug_interface != nullptr)) {
			debug_interface->enter_function(p_instance, p_source_path, p_function_name, p_line);
		}
	}

	_ALWAYS_INLINE_ ~FunctionTraceScopeGuard() {
		using namespace ::godot::__gdscriptnative__debug;
		if (likely(debug_interface != nullptr)) {
			debug_interface->exit_function();
		}
	}
};
} //namespace godot::__gdscriptnative__debug
#endif

#undef DEBUG_ENABLED

#ifdef DEBUG_ENABLED

#define __gdscriptnative__BREAKPOINT                                                               \
	if (::godot::__gdscriptnative__debug::debug_interface != nullptr) {                            \
		::godot::__gdscriptnative__debug::debug_interface->debug_break("Breakpoint", true, false); \
	} else                                                                                         \
		((void)0)

#define __gdscriptnative__ASSERT(cond)                                                                   \
	if (!(cond) && ::godot::__gdscriptnative__debug::debug_interface != nullptr) {                       \
		::godot::__gdscriptnative__debug::debug_interface->debug_break("Assertion failed.", true, true); \
	} else                                                                                               \
		((void)0)

#define __gdscriptnative__ASSERT_MSG(cond, msg)                                                                                 \
	if (!(cond) && ::godot::__gdscriptnative__debug::debug_interface != nullptr) {                                              \
		::godot::__gdscriptnative__debug::debug_interface->debug_break(String("Assertion failed: ") + String(msg), true, true); \
	} else                                                                                                                      \
		((void)0)

#define __gdscriptnative__ASSERT_INTERNAL(cond, msg)                                             \
	if (!(cond) && ::godot::__gdscriptnative__debug::debug_interface != nullptr) {               \
		::godot::__gdscriptnative__debug::debug_interface->debug_break(String(msg), true, true); \
	} else                                                                                       \
		((void)0)

#define __gdscriptnative__BEGIN_STATEMENT(line, source_file)                                             \
	if (likely(::godot::__gdscriptnative__debug::debug_interface != nullptr)) {                          \
		::godot::__gdscriptnative__debug::debug_interface->debug_begin_statement((line), (source_file)); \
	} else                                                                                               \
		((void)0)

#define __gdscriptnative__FUNCTION(instance, source_path, function_name, line)

#else

#define __gdscriptnative__BREAKPOINT ((void)0)
#define __gdscriptnative__ASSERT(cond) ((void)0)
#define __gdscriptnative__ASSERT_MSG(cond, msg) ((void)0)
#define __gdscriptnative__ASSERT_INTERNAL(cond, msg) ((void)0)
#define __gdscriptnative__BEGIN_STATEMENT(line, source_file) ((void)0)
#define __gdscriptnative__FUNCTION(instance, source_path, function_name, line) ((void)0)

#endif

namespace godot {
class __gdscriptnative__ClassBase : public RefCounted {
	GDCLASS(__gdscriptnative__ClassBase, RefCounted);

	StringName name;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("new"), &__gdscriptnative__ClassBase::_new);
		ClassDB::bind_method(D_METHOD("can_instantiate"), &__gdscriptnative__ClassBase::can_instantiate);
		ClassDB::bind_method(D_METHOD("get_base_script"), &__gdscriptnative__ClassBase::get_base_script);
		ClassDB::bind_method(D_METHOD("get_instance_base_type"), &__gdscriptnative__ClassBase::get_instance_base_type);
		ClassDB::bind_method(D_METHOD("get_property_default_value", "property"), &__gdscriptnative__ClassBase::get_property_default_value);
		ClassDB::bind_method(D_METHOD("get_script_constant_map"), &__gdscriptnative__ClassBase::get_script_constant_map);
		ClassDB::bind_method(D_METHOD("get_script_method_list"), &__gdscriptnative__ClassBase::get_script_method_list);
		ClassDB::bind_method(D_METHOD("get_script_property_list"), &__gdscriptnative__ClassBase::get_script_property_list);
		ClassDB::bind_method(D_METHOD("get_script_signal_list"), &__gdscriptnative__ClassBase::get_script_signal_list);
		ClassDB::bind_method(D_METHOD("has_script_signal", "signal_name"), &__gdscriptnative__ClassBase::has_script_signal);
		ClassDB::bind_method(D_METHOD("has_source_code"), &__gdscriptnative__ClassBase::has_source_code);
		ClassDB::bind_method(D_METHOD("instance_has", "base_object"), &__gdscriptnative__ClassBase::instance_has);
		ClassDB::bind_method(D_METHOD("is_abstract"), &__gdscriptnative__ClassBase::is_abstract);
		ClassDB::bind_method(D_METHOD("is_tool"), &__gdscriptnative__ClassBase::is_tool);
		ClassDB::bind_method(D_METHOD("reload", "keep_state"), &__gdscriptnative__ClassBase::reload, DEFVAL(false));

		ClassDB::bind_method(D_METHOD("set_source_code", "source"), &__gdscriptnative__ClassBase::set_source_code);
		ClassDB::bind_method(D_METHOD("get_source_code"), &__gdscriptnative__ClassBase::get_source_code);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_code", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_source_code", "get_source_code");
	}

public:
	Variant _new() const {
		Object *o = ClassDB::instantiate(name);
		ERR_FAIL_COND_V_MSG(!o, Variant(), "Class type: '" + String(name) + "' is not instantiable.");

		RefCounted *rc = Object::cast_to<RefCounted>(o);
		if (rc) {
			return Ref<RefCounted>(rc);
		} else {
			return o;
		}
	}

	bool can_instantiate() const {
		return ClassDB::can_instantiate(name);
	}

	virtual Ref<__gdscriptnative__ClassBase> get_base_script() const = 0;
	virtual StringName get_instance_base_type() const = 0;
	virtual Variant get_property_default_value() = 0;
	virtual Dictionary get_script_constant_map() = 0;
	virtual TypedArray<Dictionary> get_script_method_list() = 0;
	virtual TypedArray<Dictionary> get_script_property_list() = 0;
	virtual TypedArray<Dictionary> get_script_signal_list() = 0;
	virtual bool has_script_signal(StringName p_signal_name) const = 0;
	virtual bool is_tool() const = 0;

	bool has_source_code() const { return false; }
	bool instance_has(Object *p_base_object) const { ERR_FAIL_V_MSG(false, "'instance_has': unsupported in GDScriptNative"); }
	bool is_abstract() const { return false; }
	bool reload(bool p_keep_state = false) const { ERR_FAIL_V_MSG(false, "'reload': unsupported in GDScriptNative"); }

	void set_source_code(String p_value) {}
	String get_source_code() { return U""; }
};
} //namespace godot

namespace godot::__gdscriptnative__ {
namespace detail {
/*template<typename T, bool V>
struct is_object_impl { constexpr value = false; };
template<typename T, bool V>
struct is_object_impl<T *, V> { constexpr value = is_object_impl<T, V>; };
template<typename T>
struct is_object_impl<T, (static_cast<T *, const Object *>, true)> { return true; };

template<typename T>
constexpr bool is_object = is_object_impl<T, true>::value;*/

template <typename T>
class NoConstruct {
	alignas(T) char data[sizeof(T)] = { 0 };

public:
	template <typename... Args>
	void init(Args &&...p_args) { new (reinterpret_cast<void *>(&data)) T(p_args...); }
	void deinit() { reinterpret_cast<T *>(&data)->~T(); }

	T *operator->() { return reinterpret_cast<T *>(&data); }
	const T *operator->() const { return reinterpret_cast<const T *>(&data); }
	T &operator*() { return *reinterpret_cast<T *>(&data); }
	const T &operator*() const { return *reinterpret_cast<const T *>(&data); }
};

struct SNames {
	StringName _new{ "new" };
	StringName _iter_init{ "_iter_init" };
	StringName _iter_next{ "_iter_next" };
	StringName _iter_get{ "_iter_get" };
};
NoConstruct<SNames> snames;
} //namespace detail

void static_init() {
	detail::snames.init();
	GDREGISTER_INTERNAL_CLASS(__gdscriptnative__ClassBase);
}

void static_deinit() {
	detail::snames.deinit();
}

namespace detail {
template <typename T>
class PtrClassMember {
	T *ptr = nullptr;

public:
	void init(T *p) { ptr = p; }
	void deinit() {}

	// The operator-> just skips straight to the pointer to allow for an easier to write transpiler.
	T *operator->() { return ptr; }
	const T *operator->() const { return ptr; }

	T *operator*() { return ptr; }
	const T *operator*() const { return ptr; }
};

template <typename T>
class RefClassMember {
	Ref<T> ref;

public:
	void init(Ref<T> r) { ref = r; }
	void deinit() { ref.unref(); }

	// The operator-> just skips straight to the pointer to allow for an easier to write transpiler.
	T *operator->() { return ref.ptr(); }
	const T *operator->() const { return ref.ptr(); }

	Ref<T> &operator*() { return ref; }
	const Ref<T> &operator*() const { return ref; }
};

template <typename T>
struct ClassMember {
	using type = NoConstruct<T>;
};

template <typename T>
struct ClassMember<T *> {
	using type = PtrClassMember<T>;
};

template <typename T>
struct ClassMember<Ref<T>> {
	using type = RefClassMember<T>;
};
} //namespace detail

template <typename T>
using ClassMember = typename detail::ClassMember<T>::type;

namespace detail {
template <typename T>
struct to_array;

template <typename T>
struct to_array<Vector<T>> {
	Vector<T> &call(Vector<T> &v) {
		return v;
	}
	const Vector<T> &call(const Vector<T> &v) {
		return v;
	}
};

template <typename T>
struct to_array<TypedArray<T>> {
	TypedArray<T> &call(TypedArray<T> &v) {
		return v;
	}
	const TypedArray<T> &call(const TypedArray<T> &v) {
		return v;
	}
};

template <>
struct to_array<Array> {
	Array &call(Array &v) {
		return v;
	}
	const Array &call(const Array &v) {
		return v;
	}
};

template <>
struct to_array<Variant> {
	Array call(const Variant &v) {
		return v;
	}
};
} //namespace detail

template <typename T>
auto to_array(T &&v) {
	return detail::to_array<T>::call(v);
}

template <typename T>
auto to_dictionary(T &&v);

template <typename T>
auto to_dictionary(Dictionary &v) {
	return v;
}

template <typename T>
auto to_dictionary(const Dictionary &v) {
	return v;
}

template <typename T>
auto to_dictionary(const Variant &v) {
	return Dictionary(v);
}

namespace detail {
class Sentinel {};

class VariantIterator {
	Variant container;
	Variant iter;
	bool should_continue = false;

public:
	VariantIterator(const Variant &p_container) :
			container(p_container) {
		bool valid = false;
		should_continue = container.iter_init(iter, valid);
		__gdscriptnative__ASSERT_INTERNAL(valid, "Unable to iterate on object of type '" + Variant::get_type_name(container.get_type()) + "'.");
	}

	VariantIterator &operator++() {
		bool valid = false;
		should_continue = container.iter_next(iter, valid);
		__gdscriptnative__ASSERT_INTERNAL(valid, "Unable to iterate on object of type '" + Variant::get_type_name(container.get_type()) + "'.");
		return *this;
	}

	Variant operator*() {
		bool valid = false;
		Variant ret = container.iter_get(iter, valid);
		__gdscriptnative__ASSERT_INTERNAL(valid, "Unable to obtain iterator object of type '" + Variant::get_type_name(container.get_type()) + "'.");
		return ret;
	}

	operator bool() {
		return should_continue;
	}
};

template <Variant::Type type>
class Iterator;

template <Variant::Type type>
bool operator!=(Iterator<type> iter, Sentinel) {
	return iter;
}

template <>
class Iterator<Variant::INT> {
	int64_t iter = 0;
	int64_t end;

public:
	_ALWAYS_INLINE_ Iterator(int64_t p_value) :
			end(p_value) {}
	_ALWAYS_INLINE_ void operator++() { iter += 1; }
	_ALWAYS_INLINE_ auto operator*() { return iter; }
	_ALWAYS_INLINE_ operator bool() { return iter < end; }
};

template <>
class Iterator<Variant::FLOAT> {
	double iter = 0;
	double end;

public:
	_ALWAYS_INLINE_ Iterator(double p_value) :
			end(p_value) {}
	_ALWAYS_INLINE_ void operator++() { iter += 1; }
	_ALWAYS_INLINE_ auto operator*() { return iter; }
	_ALWAYS_INLINE_ operator bool() { return iter < end; }
};

template <>
class Iterator<Variant::VECTOR2> {
	double iter;
	double end;

public:
	_ALWAYS_INLINE_ Iterator(Vector2 p_value) :
			iter(p_value.x), end(p_value.y) {}
	_ALWAYS_INLINE_ void operator++() { iter += 1; }
	_ALWAYS_INLINE_ auto operator*() { return iter; }
	_ALWAYS_INLINE_ operator bool() { return iter < end; }
};

template <>
class Iterator<Variant::VECTOR2I> {
	int64_t iter;
	int64_t end;

public:
	_ALWAYS_INLINE_ Iterator(Vector2i p_value) :
			iter(p_value.x), end(p_value.y) {}
	_ALWAYS_INLINE_ void operator++() { iter += 1; }
	_ALWAYS_INLINE_ auto operator*() { return iter; }
	_ALWAYS_INLINE_ operator bool() { return iter < end; }
};

template <>
class Iterator<Variant::VECTOR3> {
	double iter;
	double end;
	double step;

public:
	_ALWAYS_INLINE_ Iterator(Vector3 p_value) :
			iter(p_value.x), end(p_value.y), step(p_value.z) {}
	_ALWAYS_INLINE_ void operator++() { iter += step; }
	_ALWAYS_INLINE_ auto operator*() { return iter; }
	_ALWAYS_INLINE_ operator bool() { return iter < end; }
};

template <>
class Iterator<Variant::VECTOR3I> {
	int64_t iter;
	int64_t end;
	int64_t step;

public:
	_ALWAYS_INLINE_ Iterator(Vector3i p_value) :
			iter(p_value.x), end(p_value.y), step(p_value.z) {}
	_ALWAYS_INLINE_ void operator++() { iter += step; }
	_ALWAYS_INLINE_ auto operator*() { return iter; }
	_ALWAYS_INLINE_ operator bool() { return iter < end; }
};

template <>
class Iterator<Variant::OBJECT> {
	Object *obj;
	Ref<RefCounted> ref;
#ifdef DEBUG_ENABLED
	ObjectID obj_id;
#endif
	Array iter_array;
	bool should_continue = false;

public:
	bool validate_obj() {
		if (!obj) {
			__gdscriptnative__ASSERT_INTERNAL(valid, "Trying to iterate on a null value.");
			return false;
		}
#ifdef DEBUG_ENABLED
		if (!obj_id.is_ref_counted() && ObjectDB::get_instance(obj_id) == nullptr) {
			__gdscriptnative__ASSERT_INTERNAL(valid, "Trying to iterate on a previously freed object.");
			return false;
		}
#endif
		return true;
	}

	bool iter_init(bool &valid) {
		if (!validate_obj()) {
			valid = false;
			return false;
		}

		Variant ret = obj->call(detail::snames->_iter_init, iter_array);

		if (iter_array.size() != 1) {
			valid = false;
			return false;
		}

		return ret;
	}

	bool iter_next(bool &valid) {
		if (!validate_obj()) {
			valid = false;
			return false;
		}

		Variant ret = obj->call(detail::snames->_iter_next, iter_array);

		if (iter_array.size() != 1) {
			valid = false;
			return false;
		}

		return ret;
	}

	Variant iter_get(bool &r_valid) {
		if (!validate_obj()) {
			r_valid = false;
			return false;
		}

		Variant ret = obj->call(detail::snames->_iter_get, iter_array);

		if (iter_array.size() != 1) {
			r_valid = false;
			return Variant();
		}

		return ret;
	}

	String get_object_as_string() {
		if (obj) {
#ifdef DEBUG_ENABLED
			if (!obj_id.is_ref_counted() && ObjectDB::get_instance(obj_id) == nullptr) {
				return "<Freed Object>";
			}
#endif

			return obj->to_string();
		} else {
			return "<Object#null>";
		}
	}

public:
	Iterator(Object *p_obj) :
			obj(p_obj) {
#ifdef DEBUG_ENABLED
		obj_id = obj->get_instance_id();
#endif
		ref.reference_ptr(obj);
		bool valid = false;
		should_continue = iter_init(valid);
		__gdscriptnative__ASSERT_INTERNAL(valid, vformat("There was an error calling '_iter_init' on iterator object of type %s.", get_object_as_string()));
	}

	Iterator &operator++() {
		bool valid = false;
		should_continue = iter_next(valid);
		__gdscriptnative__ASSERT_INTERNAL(valid, vformat("There was an error calling '_iter_next' on iterator object of type %s.", get_object_as_string()));
		return *this;
	}

	Variant operator*() {
		bool valid = false;
		Variant ret = iter_get(valid);
		__gdscriptnative__ASSERT_INTERNAL(valid, vformat("There was an error calling '_iter_get' on iterator object of type %s.", get_object_as_string()));
		return ret;
	}

	operator bool() {
		return should_continue;
	}
};

template <>
class Iterator<Variant::STRING> {
	const String str;
	int64_t iter = 0;

public:
	_ALWAYS_INLINE_ Iterator(const String &p_value) :
			str(p_value) {}
	_ALWAYS_INLINE_ void operator++() { ++iter; }
	_ALWAYS_INLINE_ auto operator*() { return str[iter]; }
	_ALWAYS_INLINE_ operator bool() { return iter < str.length(); }
};

template <>
class Iterator<Variant::DICTIONARY> {
	const Dictionary dict;
	const Array keys;
	int64_t iter = 0;
	bool should_continue = false;

public:
	Iterator(const Dictionary &p_value) :
			dict(p_value), keys(dict.keys()), should_continue(!keys.is_empty()) {}

	void operator++() {
		iter++;
		if (iter >= keys.size()) {
			should_continue = false;
		}
	}

	_ALWAYS_INLINE_ auto operator*() { return keys[iter]; }
	_ALWAYS_INLINE_ operator bool() { return should_continue; }
};

template <>
class Iterator<Variant::ARRAY> {
	const Array list;
	int64_t index = 0;

public:
	_ALWAYS_INLINE_ Iterator(const Array &p_value) :
			list(p_value) {}
	_ALWAYS_INLINE_ void operator++() { ++index; }
	_ALWAYS_INLINE_ auto operator*() { return list[index]; }
	_ALWAYS_INLINE_ operator bool() { return index < list.size(); }
};

#define PACKED_ARRAY_ITERATOR_DEF(variant_type, Type)                    \
	template <>                                                          \
	class Iterator<variant_type> {                                       \
		const Type list;                                                 \
		int64_t index = 0;                                               \
                                                                         \
	public:                                                              \
		_ALWAYS_INLINE_ Iterator(const Type &p_value) : list(p_value) {} \
		_ALWAYS_INLINE_ void operator++() {                              \
			++index;                                                     \
		}                                                                \
		_ALWAYS_INLINE_ auto operator*() {                               \
			return list[index];                                          \
		}                                                                \
		_ALWAYS_INLINE_ operator bool() {                                \
			return index < list.size();                                  \
		}                                                                \
	}

PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_BYTE_ARRAY, PackedByteArray);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_INT32_ARRAY, PackedInt32Array);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_INT64_ARRAY, PackedInt64Array);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_FLOAT32_ARRAY, PackedFloat32Array);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_FLOAT64_ARRAY, PackedFloat64Array);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_STRING_ARRAY, PackedStringArray);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_VECTOR2_ARRAY, PackedVector2Array);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_VECTOR3_ARRAY, PackedVector3Array);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_COLOR_ARRAY, PackedColorArray);
PACKED_ARRAY_ITERATOR_DEF(Variant::PACKED_VECTOR4_ARRAY, PackedVector4Array);

template <Variant::Type type>
class ForRangeT {
	Iterator<type> iter;

public:
	_ALWAYS_INLINE_ ForRangeT(const T &list) :
			iter(list) {}

	_ALWAYS_INLINE_ auto &begin() {
		return iter;
	}

	_ALWAYS_INLINE_ Sentinel end() {
		return Sentinel{};
	}
};

template <>
class ForRangeT<Variant::STRING> {
	String m_str;

public:
	_ALWAYS_INLINE_ ForRangeT(const String &p_str) :
			m_str(p_str) {}

	_ALWAYS_INLINE_ const char32_t *begin() {
		return m_str.ptr();
	}

	_ALWAYS_INLINE_ const char32_t *end() {
		return m_str.ptr() + m_str.length();
	}
};

class ForRangeVariantT {
	VariantIterator iter;

public:
	_ALWAYS_INLINE_ ForRangeVariantT(const Variant &list) :
			iter(list) {}

	_ALWAYS_INLINE_ auto &begin() {
		return iter;
	}

	_ALWAYS_INLINE_ Sentinel end() {
		return Sentinel{};
	}
};
} //namespace detail

//template<typename T1, typename T2, typename T3>
//auto range(T1 p_begin, T2 p_end, T3 p_step) {
//	return detail::Iterator<T>(p_container, r_valid);
//}

template <typename T>
constexpr Variant::Type variant_type_of = variant_type_of<std::remove_cv_t<std::remove_reference_t<T>>>;
template <>
constexpr Variant::Type variant_type_of<void> = Variant::NIL;
template <>
constexpr Variant::Type variant_type_of<bool> = Variant::BOOL;
template <>
constexpr Variant::Type variant_type_of<int32_t> = Variant::INT;
template <>
constexpr Variant::Type variant_type_of<int64_t> = Variant::INT;
template <>
constexpr Variant::Type variant_type_of<uint32_t> = Variant::INT;
template <>
constexpr Variant::Type variant_type_of<uint64_t> = Variant::INT;
template <>
constexpr Variant::Type variant_type_of<double> = Variant::FLOAT;
template <>
constexpr Variant::Type variant_type_of<float> = Variant::FLOAT;
template <>
constexpr Variant::Type variant_type_of<String> = Variant::STRING;
template <>
constexpr Variant::Type variant_type_of<const char *> = Variant::STRING;
template <>
constexpr Variant::Type variant_type_of<const char16_t *> = Variant::STRING;
template <>
constexpr Variant::Type variant_type_of<const char32_t *> = Variant::STRING;
template <>
constexpr Variant::Type variant_type_of<const wchar_t *> = Variant::STRING;
template <>
constexpr Variant::Type variant_type_of<Vector2> = Variant::VECTOR2;
template <>
constexpr Variant::Type variant_type_of<Vector2i> = Variant::VECTOR2I;
template <>
constexpr Variant::Type variant_type_of<Rect2> = Variant::RECT2;
template <>
constexpr Variant::Type variant_type_of<Rect2i> = Variant::RECT2I;
template <>
constexpr Variant::Type variant_type_of<Vector3> = Variant::VECTOR3;
template <>
constexpr Variant::Type variant_type_of<Vector3i> = Variant::VECTOR3I;
template <>
constexpr Variant::Type variant_type_of<Transform2D> = Variant::TRANSFORM2D;
template <>
constexpr Variant::Type variant_type_of<Vector4> = Variant::VECTOR4;
template <>
constexpr Variant::Type variant_type_of<Vector4i> = Variant::VECTOR4I;
template <>
constexpr Variant::Type variant_type_of<Plane> = Variant::PLANE;
template <>
constexpr Variant::Type variant_type_of<Quaternion> = Variant::QUATERNION;
template <>
constexpr Variant::Type variant_type_of<godot::AABB> = Variant::AABB;
template <>
constexpr Variant::Type variant_type_of<Basis> = Variant::BASIS;
template <>
constexpr Variant::Type variant_type_of<Transform3D> = Variant::TRANSFORM3D;
template <>
constexpr Variant::Type variant_type_of<Projection> = Variant::PROJECTION;
template <>
constexpr Variant::Type variant_type_of<Color> = Variant::COLOR;
template <>
constexpr Variant::Type variant_type_of<StringName> = Variant::STRING_NAME;
template <>
constexpr Variant::Type variant_type_of<NodePath> = Variant::NODE_PATH;
template <>
constexpr Variant::Type variant_type_of<godot::RID> = Variant::RID;
template <>
constexpr Variant::Type variant_type_of<ObjectID> = Variant::OBJECT;
template <>
constexpr Variant::Type variant_type_of<Object *> = Variant::OBJECT;
template <>
constexpr Variant::Type variant_type_of<Callable> = Variant::CALLABLE;
template <>
constexpr Variant::Type variant_type_of<Signal> = Variant::SIGNAL;
template <>
constexpr Variant::Type variant_type_of<Dictionary> = Variant::DICTIONARY;
template <>
constexpr Variant::Type variant_type_of<Array> = Variant::ARRAY;
// TODO: add TypedArray
template <>
constexpr Variant::Type variant_type_of<PackedByteArray> = Variant::PACKED_BYTE_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedInt32Array> = Variant::PACKED_INT32_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedInt64Array> = Variant::PACKED_INT64_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedFloat32Array> = Variant::PACKED_FLOAT32_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedFloat64Array> = Variant::PACKED_FLOAT64_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedStringArray> = Variant::PACKED_STRING_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedVector2Array> = Variant::PACKED_VECTOR2_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedVector3Array> = Variant::PACKED_VECTOR3_ARRAY;
template <>
constexpr Variant::Type variant_type_of<PackedColorArray> = Variant::PACKED_COLOR_ARRAY;

template <typename T>
_ALWAYS_INLINE_ Variant::Type get_expr_type(const T &value) {
	return variant_type_of<T>;
}
template <>
_ALWAYS_INLINE_ Variant::Type get_expr_type<Variant>(const Variant &value) {
	return value.get_type();
}

template <typename T>
auto for_range(T &&v) {
	return detail::ForRangeT<variant_type_of<std::remove_cv_t<std::remove_reference_t<T>>>>(v);
}

template <typename T>
auto for_range(const Variant &v) {
	return detail::ForRangeVariantT(v);
}

} //namespace godot::__gdscriptnative__

#endif // GDSCRIPT_TRANSPILER_RUNTIME_UTILS_HPP

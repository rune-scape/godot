/**************************************************************************/
/*  gdscript_transpiler.h                                                 */
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

#ifndef GDSCRIPT_TRANSPILER_H
#define GDSCRIPT_TRANSPILER_H

#include "core/object/ref_counted.h"
#include "core/templates/hash_set.h"

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_parser.h"

#include "code_builder.h"

class GDScriptTranspiler : public Object {
	GDCLASS(GDScriptTranspiler, Object);

	static GDScriptTranspiler *singleton;

protected:
	static void _bind_methods();

public:
	enum AccessSpecifier {
		ACCESS_UNKNOWN,
		ACCESS_PUBLIC,
		ACCESS_PROTECTED,
		ACCESS_PRIVATE,
	};

	template <typename T>
	class VariableScopeGuard {
		T &target;
		T prev_value;

	public:
		VariableScopeGuard(T &p_target, T p_value) :
				target(p_target) {
			prev_value = p_target;
			p_target = p_value;
		}
		~VariableScopeGuard() {
			target = prev_value;
		}
	};

	template <typename Fn>
	class Finally {
		Fn fn;

	public:
		Finally(Fn &&p_fn) :
				fn(p_fn) {}
		~Finally() { fn(); }
	};

	enum DatatypeUsage {
		DECLARATION,
		INHERITS,
		INTEROP,
	};

	enum DeclUsage {
		NONSTATIC,
		STATIC,
		VIRTUAL,
		LAMBDA,
	};

	enum Mutability {
		CONSTANT,
		MUTABLE,
	};

	Ref<GDScriptParserRef> parser_ref;
	String script_path;

	Ref<GDScript> current_script = nullptr;
	const GDScriptParser::ClassNode *current_class = nullptr;
	const GDScriptParser::FunctionNode *current_function = nullptr;
	const GDScriptParser::EnumNode *current_enum = nullptr;
	AccessSpecifier current_access_spec = ACCESS_UNKNOWN;

	HashSet<String> dependant_types;
	HashSet<GDScriptParser::EnumNode *> rendered_enums;
	HashMap<String, int> getters;
	CodeBuilder header_builder;
	CodeBuilder source_builder;
	CodeBuilder binder_builder;
	CodeBuilder class_header_builder;
	CodeBuilder class_source_builder;
	CodeBuilder class_binder_builder;
	Vector<String> init_stmts;
	Vector<String> deinit_stmts;
	Vector<String> static_init_stmts;
	Vector<String> static_deinit_stmts;

	//static const String get_global_notification_name(int64_t p_value);
	//static const String get_notification_name(Ref<GDScript> p_script, int64_t p_value);

	static String get_class_cpp_name(const GDScriptParser::ClassNode *p_class);
	static String get_class_cpp_meta_name(const GDScriptParser::ClassNode *p_class);
	static String get_class_file_basename(const GDScriptParser::ClassNode *p_class);
	static String get_class_header_filename(const GDScriptParser::ClassNode *p_class);
	static String get_class_source_filename(const GDScriptParser::ClassNode *p_class);

	static String variant_type_to_value(Variant::Type p_type);
	static String variant_type_to_cpp_type(Variant::Type p_type);
	static String datatype_to_cpp_type(DatatypeUsage p_usage, const GDScriptParser::DataType &p_type);
	static Variant::Type get_pattern_type(const GDScriptParser::PatternNode *p_pattern);

	template <typename... Args>
	void add_init(Args &&...p_init_stmts) {
		Vector<String> new_init_stmts;
		(new_init_stmts.push_back(p_init_stmts), ...);
		init_stmts.append_array(new_init_stmts);
	}
	template <typename... Args>
	void add_deinit(Args &&...p_deinit_stmts) {
		Vector<String> new_deinit_stmts;
		(new_deinit_stmts.push_back(p_deinit_stmts), ...);
		new_deinit_stmts.append_array(deinit_stmts);
		deinit_stmts = new_deinit_stmts;
	}

	template <typename... Args>
	void add_static_init(Args &&...p_init_stmts) {
		Vector<String> new_static_init_stmts;
		(new_static_init_stmts.push_back(p_init_stmts), ...);
		static_init_stmts.append_array(new_static_init_stmts);
	}
	template <typename... Args>
	void add_static_deinit(Args &&...p_deinit_stmts) {
		Vector<String> new_static_deinit_stmts;
		(new_static_deinit_stmts.push_back(p_deinit_stmts), ...);
		new_static_deinit_stmts.append_array(static_deinit_stmts);
		static_deinit_stmts = new_static_deinit_stmts;
	}

	void set_access_spec(AccessSpecifier p_access_spec);
	void begin_function(DeclUsage p_static, const String &p_return_type, const String &p_name, const String &p_param_list = "");
	void end_function();
	void declare_member_var(DeclUsage p_static, GDScriptParser::DataType p_datatype, String p_name, String p_init_expr);
	void declare_member_var(DeclUsage p_static, String p_datatype, String p_name, String p_init_expr);
	void declare_local_var(DeclUsage p_static, GDScriptParser::DataType p_datatype, String p_name, String p_init_expr);
	void declare_local_var(DeclUsage p_static, String p_datatype, String p_name, String p_init_expr);
	void declare_param(const GDScriptParser::ParameterNode *p_param);
	void bind_member_var(DeclUsage p_static, PropertyInfo p_propinfo, String p_setter, String p_getter);
	void bind_member_var(DeclUsage p_static, PropertyInfo p_propinfo) { bind_member_var(p_static, p_propinfo, "", ""); }
	void bind_member_const(const String &p_name);
	void bind_member_func(MethodInfo p_methodinfo);
	void bind_static_member_func(MethodInfo p_methodinfo);
	String make_header_includes();
	String make_source_includes();

	void transpile_class(const GDScriptParser::ClassNode *p_class);
	void transpile_class_member(const GDScriptParser::ClassNode::Member &p_member);
	void transpile_function(const GDScriptParser::FunctionNode *p_function);
	void transpile_signal(const GDScriptParser::SignalNode *p_signal);
	void transpile_enum(const GDScriptParser::EnumNode *p_enum);
	void transpile_member_const(const GDScriptParser::ConstantNode *p_constant);
	void transpile_member_var(const GDScriptParser::VariableNode *p_variable);
	void transpile_suite(const GDScriptParser::SuiteNode *p_suite);
	void transpile_statement(const GDScriptParser::Node *p_node);
	void transpile_pattern(const String &p_test_var_name, const String *p_test_type_var_id, const GDScriptParser::PatternNode *p_pattern);
	void transpile_pattern_type_check(const String &p_test_var_name, const String *p_test_type_var_id, String p_op, String p_type_expr);
	String transpile_literal(const Variant &value);
	String transpile_expression(const GDScriptParser::ExpressionNode *p_expression);

	void clear();

	static void initialize();
	static void uninitialize();
	static GDScriptTranspiler *get_singleton() { return singleton; }

	Dictionary transpile_script(const Ref<GDScript> &p_script);
	Dictionary transpile_script_from_path(const String &p_path);

private:
	GDScriptTranspiler() = default;
	~GDScriptTranspiler() = default;
};

#endif // GDSCRIPT_TRANSPILER_H

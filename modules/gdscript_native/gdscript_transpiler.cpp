/**************************************************************************/
/*  gdscript_transpiler.cpp                                               */
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

#include "gdscript_transpiler.h"

#include "core/io/dir_access.h"
//#include "core/string/ucaps.h"
#include "core/string/char_utils.h"
#include "modules/gdscript/gdscript_analyzer.h"
#include "modules/gdscript/gdscript_parser.h"

#include <cctype>

static GDScriptParser::DataType make_builtin_type(Variant::Type t) {
	GDScriptParser::DataType result;
	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	result.kind = GDScriptParser::DataType::BUILTIN;
	result.builtin_type = t;
	return result;
}

static GDScriptParser::DataType make_native_type(const StringName &p_class_name) {
	GDScriptParser::DataType result;
	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	result.kind = GDScriptParser::DataType::NATIVE;
	result.builtin_type = Variant::OBJECT;
	result.native_type = p_class_name;
	return result;
}

static String convert_to_readable_unique_id(const String &p_str) {
	FastStringBuilder result;
	result += "_";

	int64_t str_len = p_str.length();
	for (int i = 0; i < str_len; ++i) {
		char32_t c = p_str[i];
		if (is_ascii_identifier_char(c)) {
			result += c;
		} else {
			result += "_";
		}
	}

	result += "_";
	result += String::num_uint64(p_str.hash64(), 16).lpad(16, "0");

	return result;
}

// Implementation borrowed from:
// https://github.com/godotengine/godot/blob/27ddb27da8a78b21a83ee7ee049fe92d32c66727/modules/mono/editor/bindings_generator.cpp#L117-L183

/*static String snake_to_pascal_case(const String & p_identifier, bool p_input_is_upper = false) {
	FastStringBuilder ret;
	Vector<String> parts = p_identifier.split("_", true);

	for (int i = 0; i < parts.size(); i++) {
		String part = parts[i];

		if (part.length()) {
			part[0] = _find_upper(part[0]);
			if (p_input_is_upper) {
				for (int j = 1; j < part.length(); j++)
					part[j] = _find_lower(part[j]);
			}
			ret += part;
		} else {
			if (i == 0 || i == (parts.size() - 1)) {
				// Preserve underscores at the beginning and end
				ret += "_";
			} else {
				// Preserve contiguous underscores
				if (parts[i - 1].length()) {
					ret += "__";
				} else {
					ret += "_";
				}
			}
		}
	}

	return ret;
}

static String snake_to_camel_case(const String & p_identifier, bool p_input_is_upper = false) {
	FastStringBuilder ret;
	Vector<String> parts = p_identifier.split("_", true);

	for (int i = 0; i < parts.size(); i++) {
		String part = parts[i];

		if (part.length()) {
			if (i != 0) {
				part[0] = _find_upper(part[0]);
			}
			if (p_input_is_upper) {
				for (int j = i != 0 ? 1 : 0; j < part.length(); j++)
					part[j] = _find_lower(part[j]);
			}
			ret += part;
		} else {
			if (i == 0 || i == (parts.size() - 1)) {
				// Preserve underscores at the beginning and end
				ret += "_";
			} else {
				// Preserve contiguous underscores
				if (parts[i - 1].length()) {
					ret += "__";
				} else {
					ret += "_";
				}
			}
		}
	}

	return ret;
}

static String camel_to_snake_case(const String & p_identifier) {
	if (p_identifier.is_empty()) {
		return p_identifier;
	}
	FastStringBuilder result;
	result += _find_lower(p_identifier[0]);

	// TODO: finish
	int str_len = p_identifier.length();
	for (int i = 1; i < str_len; ++i) {
		char32_t c = p_identifier[i];
		char32_t lc = p_identifier[i-1];
		if (is_ascii_lower_case(lc) && is_ascii_upper_case(c)) {
			result += '_';
		}
		result += _find_lower(c);
	}
	return result;
}*/

static String escape_string_cpp(const String &p_str, const char *p_prefix) {
	FastStringBuilder result;
	result += p_prefix;
	result += "\"";

	int64_t len = p_str.length();
	for (int i = 0; i < len; ++i) {
		const char32_t c = p_str[i];

		switch (c) {
			case U'"':
				result += "\\\"";
				continue;
			case U'\\':
				result += "\\\\";
				continue;
			case U'\a':
				result += "\\a";
				continue;
			case U'\b':
				result += "\\b";
				continue;
			case U'\f':
				result += "\\f";
				continue;
			case U'\n':
				result += "\\n";
				continue;
			case U'\r':
				result += "\\r";
				continue;
			case U'\t':
				result += "\\t";
				continue;
			case U'\v':
				result += "\\v";
				continue;
		}

		// Characters that don't need to be escaped, except '?', ':', and '%' to avoid trigraphs and such.
		if ((' ' <= c && c <= '~') && (c != '?' && c != ':' && c != '%')) {
			result += c;
			continue;
		}

		const char hexchars[] = "0123456789abcdef";
		if (c <= 0xff) {
			char buffer[] = { '\\', hexchars[(c >> 6) & 0x7], hexchars[(c >> 3) & 0x7], hexchars[(c >> 0) & 0x7], U'\0' };
			result += buffer;
		} else if (c <= U'\uffff') {
			char buffer[] = { '\\', 'u', hexchars[(c >> 12) & 0xf], hexchars[(c >> 8) & 0xf], hexchars[(c >> 4) & 0xf], hexchars[(c >> 0) & 0xf], U'\0' };
			result += buffer;
		} else {
			char buffer[] = { '\\', 'U', hexchars[(c >> 28) & 0xf], hexchars[(c >> 24) & 0xf], hexchars[(c >> 20) & 0xf], hexchars[(c >> 16) & 0xf], hexchars[(c >> 12) & 0xf], hexchars[(c >> 8) & 0xf], hexchars[(c >> 4) & 0xf], hexchars[(c >> 0) & 0xf], U'\0' };
			result += buffer;
		}
	}
	result += "\"";
	return result;
}

static String escape_char32_string_cpp(const String &p_str) {
	return escape_string_cpp(p_str, "U");
}

static String escape_cstr_cpp(const String &p_str) {
	return escape_string_cpp(p_str, "");
}

String GDScriptTranspiler::make_source_includes() {
	// TODO: actually pick n choose includes for faster compiles
	return vformat("\n#include \"%s\"\n", get_class_header_filename(parser_ref->get_parser()->get_tree()));
}

String GDScriptTranspiler::make_header_includes() {
	// TODO: actually pick n choose includes for faster compiles
	return vformat("\n#include \"%s\"\n", "gdscript_transpiler_runtime_utils.hpp");
}

void GDScriptTranspiler::transpile_class(const GDScriptParser::ClassNode *p_class) {
	{
		VariableScopeGuard class_guard(current_class, p_class);
		//VariableScopeGuard script_guard(current_script, Ref<GDScript>(root_script->find_class(p_class->fqcn)));

		String class_name = get_class_cpp_name(p_class);
		StringName meta_class_name = get_class_cpp_meta_name(p_class);
		String base_type = datatype_to_cpp_type(INHERITS, p_class->base_type);

		{
			header_builder += vformat("class %s;", meta_class_name);
			header_builder += vformat("class %s : public %s {", class_name, base_type);
			header_builder.indent();
			header_builder += vformat("GDCLASS(%s, %s);", class_name, base_type);

			set_access_spec(ACCESS_PUBLIC);
			declare_member_var(STATIC, make_builtin_type(Variant::STRING_NAME), "__gdscriptnative__class_name", escape_cstr_cpp(class_name));
			declare_member_var(STATIC, make_builtin_type(Variant::STRING_NAME), "__gdscriptnative__instance_base_type", escape_cstr_cpp(p_class->get_datatype().native_type));
			declare_member_var(STATIC, make_builtin_type(Variant::STRING_NAME), "__gdscriptnative__source_file_path", escape_cstr_cpp(script_path));
			declare_member_var(STATIC, "const char *", "__gdscriptnative__source_file_path_cstr", escape_cstr_cpp(script_path));
			declare_member_var(STATIC, make_native_type(meta_class_name), "__gdscriptnative__class", vformat("memnew(%s)", meta_class_name));

			for (const KeyValue<StringName, int> &KV : p_class->members_indices) {
				transpile_class_member(p_class->members[KV.value]);
			}

			set_access_spec(ACCESS_PROTECTED);
			begin_function(STATIC, "void", "_bind_methods");
			source_builder += binder_builder;
			end_function();

			set_access_spec(ACCESS_PUBLIC);
			begin_function(STATIC, "void", "__gdscriptnative__init");
			for (String stmt : static_init_stmts) {
				source_builder += stmt;
			}
			static_init_stmts.clear();
			end_function();

			begin_function(STATIC, "void", "__gdscriptnative__deinit");
			for (String stmt : static_deinit_stmts) {
				source_builder += stmt;
			}
			static_deinit_stmts.clear();
			end_function();

			begin_function(STATIC, "void", "__gdscriptnative__static_init");
			for (String stmt : static_init_stmts) {
				source_builder += stmt;
			}
			static_init_stmts.clear();
			end_function();

			begin_function(STATIC, "void", "__gdscriptnative__static_deinit");
			for (String stmt : static_deinit_stmts) {
				source_builder += stmt;
			}
			static_deinit_stmts.clear();
			end_function();

			header_builder.dedent();
			header_builder += "};";
		}

		{
			header_builder += vformat("class %s : public __gdscriptnative__ClassBase {", meta_class_name);
			header_builder.indent();
			header_builder += vformat("GDCLASS(%s, __gdscriptnative__ClassBase);", meta_class_name);
			set_access_spec(ACCESS_PROTECTED);
			header_builder += "static void _bind_methods();";
			set_access_spec(ACCESS_PUBLIC);
			header_builder += vformat("%s::%s() : __gdscriptnative__ClassBase(%s) {}", meta_class_name, meta_class_name, transpile_literal(class_name));
			header_builder += "Ref<__gdscriptnative__ClassBase> get_base_script() const override;";
			header_builder += "StringName get_instance_base_type() const override;";
			header_builder += "Variant get_property_default_value() override;";
			header_builder += "Dictionary get_script_constant_map() override;";
			header_builder += "TypedArray<Dictionary> get_script_method_list() override;";
			header_builder += "TypedArray<Dictionary> get_script_property_list() override;";
			header_builder += "TypedArray<Dictionary> get_script_signal_list() override;";
			header_builder += "bool has_script_signal(StringName p_signal_name) const override;";
			header_builder += "bool is_tool() const override;";
			header_builder += class_header_builder;
			header_builder.dedent();
			header_builder += "};";
		}

		{
			source_builder += vformat("void %s::_bind_methods() {", meta_class_name);
			source_builder.indent();
			source_builder += class_binder_builder;
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("Ref<__gdscriptnative__ClassBase> %s::get_base_script() const {", meta_class_name);
			source_builder.indent();
			if (p_class->base_type.kind == GDScriptParser::DataType::CLASS) {
				String base_class_name = get_class_cpp_name(p_class->base_type.class_type);
				source_builder += vformat("return %s::__gdscriptnative__class;", base_class_name);
			} else {
				source_builder += "return nullptr;";
			}
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("StringName %s::get_instance_base_type() const {", meta_class_name);
			source_builder.indent();
			source_builder += vformat("return %s::__gdscriptnative__instance_base_type;", class_name);
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("Variant %s::get_property_default_value() {", meta_class_name);
			source_builder.indent();
			source_builder += "// TODO: implement";
			source_builder += "return Variant();";
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("Dictionary %s::get_script_constant_map() {", meta_class_name);
			source_builder.indent();
			source_builder += "// TODO: implement";
			source_builder += "return Dictionary();";
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("TypedArray<Dictionary> %s::get_script_method_list() {", meta_class_name);
			source_builder.indent();
			source_builder += "// TODO: implement";
			source_builder += "return TypedArray<Dictionary>();";
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("TypedArray<Dictionary> %s::get_script_property_list() {", meta_class_name);
			source_builder.indent();
			source_builder += "// TODO: implement";
			source_builder += "return TypedArray<Dictionary>();";
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("TypedArray<Dictionary> %s::get_script_signal_list() {", meta_class_name);
			source_builder.indent();
			source_builder += "// TODO: implement";
			source_builder += "return TypedArray<Dictionary>();";
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("bool %s::has_script_signal(StringName p_signal_name) const {", meta_class_name);
			source_builder.indent();
			source_builder += "// TODO: implement";
			source_builder += "return false;";
			source_builder.dedent();
			source_builder += "}";

			source_builder += vformat("bool %s::is_tool() const {", meta_class_name);
			source_builder.indent();
			source_builder += vformat("return %s;", transpile_literal(parser_ref->get_parser()->is_tool()));
			source_builder.dedent();
			source_builder += "}";
		}
	}

	for (int i = 0; i < p_class->members.size(); ++i) {
		const GDScriptParser::ClassNode::Member &member = p_class->members[i];
		if (member.type == GDScriptParser::ClassNode::Member::CLASS) {
			transpile_class(member.m_class);
		}
	}
}

void GDScriptTranspiler::transpile_class_member(const GDScriptParser::ClassNode::Member &p_member) {
	String class_name = get_class_cpp_name(current_class);

	switch (p_member.type) {
		case GDScriptParser::ClassNode::Member::UNDEFINED: {
			ERR_FAIL_MSG("internal error: undefined class member!");
		} break;
		case GDScriptParser::ClassNode::Member::CLASS: {
			String member_class_cpp_name = get_class_cpp_name(p_member.m_class);
			StringName member_name = p_member.get_name();
			declare_member_var(STATIC, p_member.m_class->get_datatype(), member_name, vformat("%s::__gdscriptnative__class", member_class_cpp_name));
			bind_member_var(STATIC, PropertyInfo(Variant::OBJECT, member_name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, member_class_cpp_name));
		} break;
		case GDScriptParser::ClassNode::Member::CONSTANT: {
			const GDScriptParser::ConstantNode *c = p_member.constant;
			transpile_member_const(c);
		} break;
		case GDScriptParser::ClassNode::Member::FUNCTION: {
			const GDScriptParser::FunctionNode *fn = p_member.function;
			transpile_function(fn);
		} break;
		case GDScriptParser::ClassNode::Member::SIGNAL: {
			const GDScriptParser::SignalNode *signal = p_member.signal;
			transpile_signal(signal);
		} break;
		case GDScriptParser::ClassNode::Member::VARIABLE: {
			const GDScriptParser::VariableNode *var = p_member.variable;
			transpile_member_var(var);
		} break;
		case GDScriptParser::ClassNode::Member::ENUM: {
			const GDScriptParser::EnumNode *enumm = p_member.m_enum;
			transpile_enum(enumm);
		} break;
		case GDScriptParser::ClassNode::Member::ENUM_VALUE: { // For unnamed enums.
			if (!rendered_enums.has(p_member.enum_value.parent_enum)) {
				rendered_enums.insert(p_member.enum_value.parent_enum);
				transpile_enum(p_member.enum_value.parent_enum);
			}
		} break;
		case GDScriptParser::ClassNode::Member::GROUP: { // For member grouping.
			const GDScriptParser::AnnotationNode *annotation = p_member.annotation;

			switch (annotation->export_info.usage) {
				case PROPERTY_USAGE_CATEGORY: {
					binder_builder += vformat(R"(ADD_PROPERTY(PropertyInfo(Variant::NIL, %s, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CATEGORY), "", "");)", escape_char32_string_cpp(annotation->export_info.name));
				} break;
				case PROPERTY_USAGE_GROUP: {
					binder_builder += vformat(R"(ADD_GROUP(%s, %s);)", escape_char32_string_cpp(annotation->export_info.name), escape_char32_string_cpp(annotation->export_info.hint_string));
				} break;
				case PROPERTY_USAGE_SUBGROUP: {
					binder_builder += vformat(R"(ADD_SUBGROUP(%s, %s);)", escape_char32_string_cpp(annotation->export_info.name), escape_char32_string_cpp(annotation->export_info.hint_string));
				} break;
			}
		} break;
	}
}

void GDScriptTranspiler::transpile_function(const GDScriptParser::FunctionNode *p_function) {
	ERR_FAIL_NULL(p_function);
	VariableScopeGuard fn_guard(current_function, p_function);

	PackedStringArray param_names;
	PackedStringArray parameters;
	for (int i = 0; i < p_function->parameters.size(); ++i) {
		GDScriptParser::ParameterNode *param = p_function->parameters[i];
		ERR_FAIL_NULL(param);
		String type = datatype_to_cpp_type(DECLARATION, param->get_datatype());
		ERR_FAIL_NULL(param->identifier);
		String name = param->identifier->name;
		param_names.push_back(vformat(R"(U"%s")", name));
		if (param->initializer != nullptr) {
			parameters.push_back(vformat("%s %s = %s", type, name, transpile_expression(param->initializer)));
		} else {
			parameters.push_back(vformat("%s %s", type, name));
		}
	}

	String return_type = datatype_to_cpp_type(DECLARATION, p_function->get_datatype());
	String class_name = get_class_cpp_name(current_class);
	String parameter_list = String(", ").join(parameters);

	DeclUsage usage = NONSTATIC;
	String function_name = p_function->identifier ? p_function->identifier->name : "";
	if (p_function->source_lambda != nullptr) {
		usage = LAMBDA;
	} else if (p_function->is_static) {
		usage = STATIC;
		bind_static_member_func(p_function->info);
	} else {
		usage = VIRTUAL;
		bind_member_func(p_function->info);
	}
	begin_function(usage, return_type, function_name, parameter_list);
	// TODO: static instance ptr could point to special class type
	String instance_str = p_function->is_static ? U"nullptr" : U"this";
	String debug_function_name = p_function->identifier != nullptr ? String(p_function->identifier->name) : String(U"<anonymous lambda>");
	source_builder += vformat(U"__gdscriptnative__FUNCTION(%s, %s, %s, %s);", instance_str, U"__gdscriptnative__source_file_path_cstr", escape_cstr_cpp(debug_function_name), p_function->start_line);
	transpile_suite(p_function->body);
	end_function();
}

void GDScriptTranspiler::transpile_signal(const GDScriptParser::SignalNode *p_signal) {
	ERR_FAIL_NULL(p_signal);
	PackedStringArray method_info_params;
	for (const GDScriptParser::ParameterNode *param : p_signal->parameters) {
		GDScriptParser::DataType datatype = param->get_datatype();
		// TODO: add more type info
		method_info_params.push_back(vformat("PropertyInfo(%s, %s)", variant_type_to_value(datatype.builtin_type), param->identifier->name));
	}
	binder_builder += vformat("ADD_SIGNAL(MethodInfo(%s, %s));", p_signal->identifier->name, String(", ").join(method_info_params));
	// TODO: finish
}

void GDScriptTranspiler::transpile_enum(const GDScriptParser::EnumNode *p_enum) {
	ERR_FAIL_NULL(p_enum);
	VariableScopeGuard enum_guard(current_enum, p_enum);

	if (p_enum->identifier != nullptr) {
		header_builder += vformat("enum %s : int64_t {", p_enum->identifier->name);
	} else {
		header_builder += "enum : int64_t {";
	}

	for (const GDScriptParser::EnumNode::Value &value : p_enum->values) {
		ERR_FAIL_NULL(value.identifier);
		ERR_FAIL_COND(!value.resolved);
		header_builder += vformat("%s = %s,", value.identifier->name, value.value);
		binder_builder += vformat("BIND_CONSTANT(%s);", value.identifier->name);
	}

	header_builder += "}";
	// TODO: finish
}

void GDScriptTranspiler::set_access_spec(AccessSpecifier p_access_spec) {
	if (current_access_spec != p_access_spec) {
		current_access_spec = p_access_spec;
		header_builder.dedent();
		switch (p_access_spec) {
			case ACCESS_UNKNOWN:
				ERR_FAIL_MSG("Invalid access specifier ACCESS_UNKNOWN");
				break;
			case ACCESS_PUBLIC:
				header_builder += "public:";
				break;
			case ACCESS_PROTECTED:
				header_builder += "protected:";
				break;
			case ACCESS_PRIVATE:
				header_builder += "private:";
				break;
		}
		header_builder.indent();
	}
}

void GDScriptTranspiler::begin_function(DeclUsage p_usage, const String &p_return_type, const String &p_name, const String &p_param_list) {
	switch (p_usage) {
		case NONSTATIC:
			ERR_FAIL_COND(p_name.is_empty());
			header_builder += vformat("%s %s(%s);", p_return_type, p_name, p_param_list);
			break;
		case STATIC:
			ERR_FAIL_COND(p_name.is_empty());
			header_builder += vformat("static %s %s(%s);", p_return_type, p_name, p_param_list);
			break;
		case VIRTUAL:
			ERR_FAIL_COND(p_name.is_empty());
			header_builder += vformat("virtual %s %s(%s);", p_return_type, p_name, p_param_list);
			break;
		case LAMBDA:
			break;
	}

	source_builder.push_newline_enabled(true);
	if (p_usage == LAMBDA) {
		source_builder += vformat("[&](%s) -> %s {", p_param_list, p_return_type);
	} else {
		source_builder += vformat("%s %s::%s(%s) {", p_return_type, get_class_cpp_name(current_class), p_name, p_param_list);
	}
	source_builder.indent();
}

void GDScriptTranspiler::end_function() {
	source_builder.dedent();
	source_builder.pop_newline_enabled();
	source_builder += "}";
}

void GDScriptTranspiler::declare_member_var(DeclUsage p_usage, GDScriptParser::DataType p_datatype, String p_name, String p_init_expr) {
	declare_member_var(p_usage, datatype_to_cpp_type(DECLARATION, p_datatype), p_name, p_init_expr);
}

void GDScriptTranspiler::declare_member_var(DeclUsage p_usage, String p_datatype, String p_name, String p_init_expr) {
	String staticness = p_usage == STATIC ? "static " : "";

	String init_stmt = vformat("%s.init(%s);", p_name, p_init_expr);
	String deinit_stmt = vformat("%s.deinit();", p_name);
	switch (p_usage) {
		case STATIC: {
			add_static_init(init_stmt);
			add_static_deinit(deinit_stmt);
		} break;
		case NONSTATIC: {
			add_init(init_stmt);
			add_deinit(deinit_stmt);
		} break;
		case VIRTUAL:
			ERR_FAIL();
			break;
		case LAMBDA:
			ERR_FAIL();
			break;
	}

	header_builder += staticness + vformat("::godot::__gdscriptnative__::ClassMember<%s> %s;", p_datatype, p_name);
}

void GDScriptTranspiler::declare_local_var(DeclUsage p_usage, GDScriptParser::DataType p_datatype, String p_name, String p_init_expr) {
	declare_local_var(p_usage, datatype_to_cpp_type(DECLARATION, p_datatype), p_name, p_init_expr);
}

void GDScriptTranspiler::declare_local_var(DeclUsage p_usage, String p_datatype, String p_name, String p_init_expr) {
	String staticness = p_usage == STATIC ? "static " : "";
	if (p_init_expr.is_empty()) {
		source_builder += staticness + vformat("%s %s;", p_datatype, p_name);
	} else {
		source_builder += staticness + vformat("%s %s = %s;", p_datatype, p_name, p_init_expr);
	}
}

void GDScriptTranspiler::declare_param(const GDScriptParser::ParameterNode *p_param) {
	// TODO: finish
}

void GDScriptTranspiler::bind_member_var(DeclUsage p_static, PropertyInfo p_propinfo, String p_setter, String p_getter) {
	// TODO: finish
}

void GDScriptTranspiler::bind_member_const(const String &p_name) {
	binder_builder += vformat("BIND_CONSTANT(%s);", p_name);
}

void GDScriptTranspiler::bind_member_func(MethodInfo p_methodinfo) {
	// TODO: finish
}

void GDScriptTranspiler::bind_static_member_func(MethodInfo p_methodinfo) {
	// TODO: finish
}

void GDScriptTranspiler::transpile_member_const(const GDScriptParser::ConstantNode *p_constant) {
	ERR_FAIL_NULL(p_constant);
	ERR_FAIL_NULL(p_constant->initializer);
	ERR_FAIL_COND(!p_constant->initializer->is_constant);

	GDScriptParser::DataType datatype = p_constant->get_datatype();
	ERR_FAIL_COND(!datatype.is_constant);

	declare_member_var(STATIC, datatype, p_constant->identifier->name, transpile_literal(p_constant->initializer->reduced_value));
	bind_member_const(p_constant->identifier->name);
}

void GDScriptTranspiler::transpile_member_var(const GDScriptParser::VariableNode *p_variable) {
	ERR_FAIL_NULL(p_variable);
	String name = p_variable->identifier->name;
	DeclUsage static_usage = p_variable->is_static ? STATIC : NONSTATIC;

	GDScriptParser::DataType datatype = p_variable->get_datatype();
	declare_member_var(static_usage, datatype, p_variable->identifier->name, transpile_expression(p_variable->initializer));
	bind_member_var(static_usage, p_variable->export_info);
}

void GDScriptTranspiler::transpile_suite(const GDScriptParser::SuiteNode *p_suite) {
	ERR_FAIL_NULL(p_suite);
	for (const GDScriptParser::Node *n : p_suite->statements) {
		transpile_statement(n);
	}
}

void GDScriptTranspiler::transpile_statement(const GDScriptParser::Node *p_node) {
	ERR_FAIL_NULL(p_node);

	source_builder += vformat("__gdscriptnative__BEGIN_STATEMENT(%s, __gdscriptnative__source_file_path);", p_node->start_line);
	switch (p_node->type) {
		case GDScriptParser::Node::MATCH: {
			const GDScriptParser::MatchNode *match = static_cast<const GDScriptParser::MatchNode *>(p_node);
			ERR_FAIL_NULL(match->test);

			String test_var_name = vformat("__gdscriptnative__match%s_%s_test", match->start_line, match->start_column);
			source_builder += vformat("%s %s = %s;", datatype_to_cpp_type(DECLARATION, match->test->get_datatype()), test_var_name, transpile_expression(match->test));

			String test_type_var_name = vformat("%s_type_", test_var_name);
			source_builder += vformat("const Variant::Type %s = ::godot::__gdscriptnative__::get_expr_type(%s);", test_type_var_name, test_var_name);

			for (int i = 0; i < match->branches.size(); ++i) {
				const GDScriptParser::MatchBranchNode *branch = match->branches[i];
				ERR_FAIL_NULL(branch);
				for (const GDScriptParser::PatternNode *pattern : branch->patterns) {
					ERR_FAIL_NULL(pattern);
					transpile_pattern(test_var_name, &test_type_var_name, pattern);
					source_builder += "{";
					source_builder.indent();
					transpile_suite(branch->block);
					source_builder.dedent();
					source_builder += "}";
				}
			}
		} break;
		case GDScriptParser::Node::IF: {
			const GDScriptParser::IfNode *if_n = static_cast<const GDScriptParser::IfNode *>(p_node);

			source_builder += vformat("if (%s) {", transpile_expression(if_n->condition));
			source_builder.indent();
			transpile_suite(if_n->true_block);

			const GDScriptParser::IfNode *elseif_parent = if_n;
			while (elseif_parent->false_block != nullptr && elseif_parent->false_block->statements.size() == 1 && elseif_parent->false_block->statements[0]->type == GDScriptParser::Node::IF) {
				const GDScriptParser::IfNode *elseif_node = static_cast<const GDScriptParser::IfNode *>(elseif_parent->false_block->statements[0]);
				source_builder.dedent();
				source_builder += vformat("} else if (%s) {", transpile_expression(elseif_node->condition));
				source_builder.indent();
				transpile_suite(elseif_node->true_block);
				elseif_parent = elseif_node;
			}

			if (elseif_parent->false_block != nullptr) {
				source_builder.dedent();
				source_builder += "} else {";
				source_builder.indent();
				transpile_suite(elseif_parent->false_block);
			}

			source_builder.dedent();
			source_builder += "}";
		} break;
		case GDScriptParser::Node::FOR: {
			const GDScriptParser::ForNode *for_n = static_cast<const GDScriptParser::ForNode *>(p_node);

			ERR_FAIL_NULL(for_n->variable);

			source_builder += vformat("for (auto && %s : ::godot::__gdscriptnative__::for_range(%s)) {", for_n->variable->name, transpile_expression(for_n->list));
			source_builder.indent();
			transpile_suite(for_n->loop);
			source_builder.dedent();
			source_builder += "}";
		} break;
		case GDScriptParser::Node::WHILE: {
			const GDScriptParser::WhileNode *while_n = static_cast<const GDScriptParser::WhileNode *>(p_node);

			source_builder += vformat("while (%s) {", transpile_expression(while_n->condition));
			source_builder.indent();
			transpile_suite(while_n->loop);
			source_builder.dedent();
			source_builder += "}";

		} break;
		case GDScriptParser::Node::BREAK: {
			source_builder += "break;";
		} break;
		case GDScriptParser::Node::CONTINUE: {
			source_builder += "continue;";
		} break;
		case GDScriptParser::Node::RETURN: {
			const GDScriptParser::ReturnNode *return_n = static_cast<const GDScriptParser::ReturnNode *>(p_node);

			source_builder += vformat("return %s;", transpile_expression(return_n->return_value));
		} break;
		case GDScriptParser::Node::ASSERT: {
			const GDScriptParser::AssertNode *assert_n = static_cast<const GDScriptParser::AssertNode *>(p_node);

			String cond_expr = transpile_expression(assert_n->condition);
			if (assert_n->message != nullptr) {
				source_builder += vformat("__gdscriptnative__ASSERT_MSG(%s, %s);", cond_expr, transpile_expression(assert_n->message));
			} else {
				source_builder += vformat("__gdscriptnative__ASSERT(%s);", cond_expr);
			}
		} break;
		case GDScriptParser::Node::BREAKPOINT: {
			source_builder += "__gdscriptnative__BREAKPOINT;";
		} break;
		case GDScriptParser::Node::VARIABLE: {
			const GDScriptParser::VariableNode *lv = static_cast<const GDScriptParser::VariableNode *>(p_node);

			String type_str = datatype_to_cpp_type(DECLARATION, lv->get_datatype());
			ERR_FAIL_NULL(lv->identifier);
			String var_name = lv->identifier->name;
			if (lv->initializer != nullptr) {
				source_builder += vformat("%s %s = %s;", type_str, var_name, transpile_expression(lv->initializer));
			} else {
				source_builder += vformat("%s %s;", type_str, var_name);
			}
		} break;
		case GDScriptParser::Node::CONSTANT: {
			// Local constants.
			const GDScriptParser::ConstantNode *lc = static_cast<const GDScriptParser::ConstantNode *>(p_node);

			String type_str = datatype_to_cpp_type(DECLARATION, lc->get_datatype());
			ERR_FAIL_NULL(lc->identifier);
			String var_name = lc->identifier->name;
			ERR_FAIL_NULL(lc->initializer);
			ERR_FAIL_COND_MSG(!lc->initializer->is_constant, "Local constant must have a constant value as initializer.");

			source_builder += vformat("%s %s = %s;", type_str, var_name, transpile_literal(lc->initializer->reduced_value));
		} break;
		case GDScriptParser::Node::PASS:
			// Nothing to do.
			break;
		default: {
			// Expression.
			if (p_node->is_expression()) {
				source_builder += vformat("%s;", transpile_expression(static_cast<const GDScriptParser::ExpressionNode *>(p_node)));
			} else {
				ERR_FAIL_MSG("Bug in transpiler, unexpected node in parse tree while parsing statement."); // Unreachable code.
			}
		} break;
	}
}

void GDScriptTranspiler::transpile_pattern(const String &p_test_var_id, const String *p_test_type_var_id, const GDScriptParser::PatternNode *p_pattern) {
	ERR_FAIL_NULL(p_pattern);
	// TODO: implement

	String local_test_type_var_id;

	switch (p_pattern->pattern_type) {
		case GDScriptParser::PatternNode::PT_LITERAL: {
			transpile_pattern_type_check(p_test_var_id, p_test_type_var_id, "==", variant_type_to_value(p_pattern->literal->value.get_type()));
			ERR_FAIL_NULL(p_pattern->literal);
			source_builder += vformat("if ((%s) == (%s))", p_test_var_id, transpile_literal(p_pattern->literal->value));
		} break;
		case GDScriptParser::PatternNode::PT_EXPRESSION: {
			String pattern_expr_var_id = vformat("%s_expr%s_%s", p_test_var_id, p_pattern->start_line, p_pattern->start_column);
			source_builder += vformat("if (auto %s = %s; true)", pattern_expr_var_id, transpile_expression(p_pattern->expression));
			transpile_pattern_type_check(p_test_var_id, p_test_type_var_id, "==", vformat("::godot::__gdscriptnative__::get_expr_type(%s)", pattern_expr_var_id));
			source_builder += vformat("if ((%s) == (%s))", p_test_var_id, pattern_expr_var_id);
		} break;
		case GDScriptParser::PatternNode::PT_BIND: {
			ERR_FAIL_NULL(p_pattern->bind);
			source_builder += vformat("if (auto %s = %s; true)", p_test_var_id, p_pattern->bind->name, p_test_var_id);
		} break;
		case GDScriptParser::PatternNode::PT_ARRAY: {
			transpile_pattern_type_check(p_test_var_id, p_test_type_var_id, ">=", "Variant::ARRAY");

			String test_var_array_name = vformat("%s_array%s_%s", p_test_var_id, p_pattern->start_line, p_pattern->start_column);
			String size_test_expr;
			if (p_pattern->rest_used) {
				size_test_expr = vformat("%s.size() >= %s", test_var_array_name, p_pattern->array.size() - 1);
			} else {
				size_test_expr = vformat("%s.size() == %s", test_var_array_name, p_pattern->array.size());
			}
			source_builder += vformat("if (auto %s = ::godot::__gdscriptnative__::to_array(%s); %s)", test_var_array_name, p_test_var_id, size_test_expr);
			for (int i = 0; i < p_pattern->array.size(); ++i) {
				const GDScriptParser::PatternNode *sub_pattern = p_pattern->array[i];
				ERR_FAIL_NULL(sub_pattern);
				if (sub_pattern->pattern_type == GDScriptParser::PatternNode::PT_REST) {
					break;
				}
				String test_element_name = vformat("%s_v%s", test_var_array_name, i);
				source_builder += vformat("if (auto %s = %s[%s]; true)", test_element_name, test_var_array_name, i);
				transpile_pattern(test_element_name, nullptr, sub_pattern);
			}
		} break;
		case GDScriptParser::PatternNode::PT_DICTIONARY: {
			transpile_pattern_type_check(p_test_var_id, p_test_type_var_id, "==", "Variant::DICTIONARY");

			String test_var_dict_name = vformat("%s_dict%s_%s", p_test_var_id, p_pattern->start_line, p_pattern->start_column);
			String size_test_expr;
			if (p_pattern->rest_used) {
				size_test_expr = vformat("%s.size() >= %s", test_var_dict_name, p_pattern->dictionary.size() - 1);
			} else {
				size_test_expr = vformat("%s.size() == %s", test_var_dict_name, p_pattern->dictionary.size());
			}
			source_builder += vformat("if (auto %s = ::godot::__gdscriptnative__::to_dictionary(%s); %s)", test_var_dict_name, p_test_var_id, size_test_expr);
			for (int i = 0; i < p_pattern->dictionary.size(); ++i) {
				const GDScriptParser::PatternNode::Pair &element = p_pattern->dictionary[i];
				if (element.value_pattern != nullptr && element.value_pattern->pattern_type == GDScriptParser::PatternNode::PT_REST) {
					break;
				}

				ERR_FAIL_NULL(element.key);
				String element_key_name = vformat("%s_k%s_", test_var_dict_name, i);
				source_builder += vformat("if (auto %s = %s; %s.has(%s))", element_key_name, transpile_expression(element.key), test_var_dict_name, element_key_name);
				if (element.value_pattern != nullptr) {
					String element_value_name = vformat("%s_v%s_", test_var_dict_name, i);
					source_builder += vformat("if (auto %s = %s[%s]; true)", element_value_name, test_var_dict_name, element_key_name);
					transpile_pattern(element_value_name, nullptr, element.value_pattern);
				}
			}
		} break;
		case GDScriptParser::PatternNode::PT_REST:
			ERR_FAIL_MSG("Unreachable");
		case GDScriptParser::PatternNode::PT_WILDCARD:
			break;
	}
}

void GDScriptTranspiler::transpile_pattern_type_check(const String &p_test_var_name, const String *p_test_type_var_id, String p_op, String p_type_expr) {
	if (p_test_type_var_id != nullptr) {
		source_builder += vformat("if ((%s) %s (%s))", *p_test_type_var_id, p_op, p_type_expr);
	} else {
		String test_type_var_name = p_test_var_name + "_type";
		source_builder += vformat("if (Variant::Type %s = ::godot::__gdscriptnative__::get_expr_type(%s); (%s) %s (%s))", test_type_var_name, p_test_var_name, test_type_var_name, p_op, p_type_expr);
	}
}

String GDScriptTranspiler::transpile_literal(const Variant &value) {
	switch (value.get_type()) {
		case Variant::NIL:
			return "nullptr";

			// Atomic types.
		case Variant::BOOL:
		case Variant::INT:
			return value.operator String();
		case Variant::FLOAT:
			char buffer[256];
			snprintf(buffer, 256, "%a", value.operator double());
			return String(buffer);
		case Variant::STRING:
			return escape_char32_string_cpp(value);

			// Math types.
		case Variant::VECTOR2: /* {
			 Vector2 tv(value);
			 int write_size = snprintf(buffer, buf_size, "Vector2(%a, %a)", tv.x, tv.y);
			 ERR_FAIL_COND(write_size >= buf_size);
			 return String(buffer);
		 } break;*/
		case Variant::VECTOR2I:
		case Variant::RECT2:
		case Variant::RECT2I:
		case Variant::TRANSFORM2D:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
		case Variant::PLANE:
		case Variant::AABB:
		case Variant::QUATERNION:
		case Variant::BASIS:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
			break;

			// Miscellaneous types.
		case Variant::COLOR:
		case Variant::RID:
		case Variant::OBJECT:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
			break;
		case Variant::STRING_NAME:
			return String(U"SNAME(") + escape_char32_string_cpp(value.operator String()) + String(U")");
		case Variant::NODE_PATH:
		case Variant::DICTIONARY:
		case Variant::ARRAY:
			break;

			// Arrays.
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
			break;
		default:
			break;
	}

	ERR_FAIL_V_MSG("ErrorValue", vformat("Unsupported literal type %s", Variant::get_type_name(value.get_type())));

	// TODO: implement
	return "ErrorValue";
}

String GDScriptTranspiler::transpile_expression(const GDScriptParser::ExpressionNode *p_expression) {
	using Node = GDScriptParser::Node;
	switch (p_expression->type) {
		case Node::NONE:
			break;
		case Node::ANNOTATION:
		case Node::ARRAY:
		case Node::ASSERT:
		case Node::ASSIGNMENT:
		case Node::AWAIT:
		case Node::BINARY_OPERATOR:
		case Node::BREAK:
		case Node::BREAKPOINT:
		case Node::CALL:
		case Node::CAST:
		case Node::CLASS:
		case Node::CONSTANT:
		case Node::CONTINUE:
		case Node::DICTIONARY:
		case Node::ENUM:
		case Node::FOR:
		case Node::FUNCTION:
		case Node::GET_NODE:
		case Node::IDENTIFIER:
		case Node::IF:
		case Node::LAMBDA:
		case Node::LITERAL:
		case Node::MATCH:
		case Node::MATCH_BRANCH:
		case Node::PARAMETER:
		case Node::PASS:
		case Node::PATTERN:
		case Node::PRELOAD:
		case Node::RETURN:
		case Node::SELF:
		case Node::SIGNAL:
		case Node::SUBSCRIPT:
		case Node::SUITE:
		case Node::TERNARY_OPERATOR:
		case Node::TYPE:
		case Node::TYPE_TEST:
		case Node::UNARY_OPERATOR:
		case Node::VARIABLE:
		case Node::WHILE:
	}

	// TODO: implement
	return "";
}

/*inline const String GDScriptTranspiler::get_global_notification_name(int64_t p_value) {
	// TODO: make sure its ok that it doesn't get refreshed
	static HashMap<int64_t, String> notification_names = [] {
		HashMap<int64_t, String> map;
		auto& named_globals = GDScriptLanguage::get_singleton()->get_named_globals_map();
		for (auto& KV : named_globals) {
			if (KV.value.get_type() == Variant::INT && String(KV.key).begins_with("NOTIFICATION_")) {
				map.insert(KV.value, KV.key);
			}
		}
		auto& global_names = GDScriptLanguage::get_singleton()->get_global_map();
		Variant* global_values = GDScriptLanguage::get_singleton()->get_global_array();
		for (auto& KV : global_names) {
			auto value = global_values[KV.value];
			if (value.get_type() == Variant::INT && String(KV.key).begins_with("NOTIFICATION_")) {
				map.insert(value, KV.key);
			}
		}
		return map;
		}();

		return notification_names[p_value];
}

inline const String GDScriptTranspiler::get_notification_name(Ref<GDScript> p_script, int64_t p_value) {
	if (String global = get_global_notification_name(p_value); !global.is_empty()) {
		return global;
	}

	static HashMap<String, HashMap<int64_t, String>> notification_names_map;
	String fqcn = p_script->get_fully_qualified_name();
	auto it = notification_names_map.find(fqcn);
	if (!it) {
		notification_names_map.insert(fqcn, {}); // insert
		HashMap<int64_t, String> & notification_names = it->value;
		for (Ref<GDScript> sptr = p_script.ptr(); sptr != nullptr; sptr = sptr->get_base()) {
			auto& script_constant_names = sptr->get_constants();
			for (auto& KV : script_constant_names) {
				if (KV.value.get_type() == Variant::INT && String(KV.key).begins_with("NOTIFICATION_")) {
					notification_names.insert(KV.value, KV.key);
				}
			}
			if (sptr->get_base() == nullptr) {
				StringName native_base_name = sptr->get_native()->get_name();
				List<String> class_constant_names;
				// this gives strings instead of stringnames..  >:c  inefficient
				ClassDB::get_integer_constant_list(native_base_name, &class_constant_names);
				for (String name : class_constant_names) {
					if (name.begins_with("NOTIFICATION_")) {
						int64_t value = ClassDB::get_integer_constant(native_base_name, name);
						notification_names[value] = name;
					}
				}
				break;
			}
		}
		it = notification_names_map.find(fqcn);
	}

	return it->value;
}*/

String GDScriptTranspiler::get_class_cpp_name(const GDScriptParser::ClassNode *p_class) {
	if (p_class->identifier != nullptr && p_class->fqcn == p_class->identifier->name) {
		return p_class->fqcn;
	}
	return String(U"__gdscriptnative__") + convert_to_readable_unique_id(p_class->fqcn);
}

String GDScriptTranspiler::get_class_cpp_meta_name(const GDScriptParser::ClassNode *p_class) {
	String class_name = get_class_cpp_name(p_class);
	if (!class_name.begins_with(U"__gdscriptnative__")) {
		class_name = String(U"__gdscriptnative__") + class_name;
	}
	return class_name + U"_Class";
}

String GDScriptTranspiler::get_class_file_basename(const GDScriptParser::ClassNode *p_class) {
	return get_class_cpp_name(p_class).to_lower();
}

String GDScriptTranspiler::get_class_header_filename(const GDScriptParser::ClassNode *p_class) {
	return get_class_file_basename(p_class) + U".hpp";
}

String GDScriptTranspiler::get_class_source_filename(const GDScriptParser::ClassNode *p_class) {
	return get_class_file_basename(p_class) + U".cpp";
}

String GDScriptTranspiler::variant_type_to_cpp_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::NIL:
			return "void";

			// Atomic types.
		case Variant::BOOL:
			return "bool";
		case Variant::INT:
			return "int";
		case Variant::FLOAT:
			return "float";
		case Variant::STRING:
			return "String";

			// Math types.
		case Variant::VECTOR2:
			return "Vector2";
		case Variant::VECTOR2I:
			return "Vector2i";
		case Variant::RECT2:
			return "Rect2";
		case Variant::RECT2I:
			return "Rect2i";
		case Variant::TRANSFORM2D:
			return "Transform2D";
		case Variant::VECTOR3:
			return "Vector3";
		case Variant::VECTOR3I:
			return "Vector3i";
		case Variant::VECTOR4:
			return "Vector4";
		case Variant::VECTOR4I:
			return "Vector4i";
		case Variant::PLANE:
			return "Plane";
		case Variant::AABB:
			return "AABB";
		case Variant::QUATERNION:
			return "Quaternion";
		case Variant::BASIS:
			return "Basis";
		case Variant::TRANSFORM3D:
			return "Transform3D";
		case Variant::PROJECTION:
			return "Projection";

			// Miscellaneous types.
		case Variant::COLOR:
			return "Color";
		case Variant::RID:
			return "RID";
		case Variant::OBJECT:
			return "Object";
		case Variant::CALLABLE:
			return "Callable";
		case Variant::SIGNAL:
			return "Signal";
		case Variant::STRING_NAME:
			return "StringName";
		case Variant::NODE_PATH:
			return "NodePath";
		case Variant::DICTIONARY:
			return "Dictionary";
		case Variant::ARRAY:
			return "Array";

			// Arrays.
		case Variant::PACKED_BYTE_ARRAY:
			return "PackedByteArray";
		case Variant::PACKED_INT32_ARRAY:
			return "PackedInt32Array";
		case Variant::PACKED_INT64_ARRAY:
			return "PackedInt64Array";
		case Variant::PACKED_FLOAT32_ARRAY:
			return "PackedFloat32Array";
		case Variant::PACKED_FLOAT64_ARRAY:
			return "PackedFloat64Array";
		case Variant::PACKED_STRING_ARRAY:
			return "PackedStringArray";
		case Variant::PACKED_VECTOR2_ARRAY:
			return "PackedVector2Array";
		case Variant::PACKED_VECTOR3_ARRAY:
			return "PackedVector3Array";
		case Variant::PACKED_COLOR_ARRAY:
			return "PackedColorArray";
		case Variant::PACKED_VECTOR4_ARRAY:
			return "PackedVector4Array";
		case Variant::VARIANT_MAX:
			ERR_FAIL_V_MSG("ErrorType", "Unreachable");
	}
}

String GDScriptTranspiler::variant_type_to_value(Variant::Type p_type) {
	switch (p_type) {
		case Variant::NIL:
			return "Variant::NIL";

			// Atomic types.
		case Variant::BOOL:
			return "Variant::BOOL";
		case Variant::INT:
			return "Variant::INT";
		case Variant::FLOAT:
			return "Variant::FLOAT";
		case Variant::STRING:
			return "Variant::STRING";

			// Math types.
		case Variant::VECTOR2:
			return "Variant::VECTOR2";
		case Variant::VECTOR2I:
			return "Variant::VECTOR2I";
		case Variant::RECT2:
			return "Variant::RECT2";
		case Variant::RECT2I:
			return "Variant::RECT2I";
		case Variant::TRANSFORM2D:
			return "Variant::TRANSFORM2D";
		case Variant::VECTOR3:
			return "Variant::VECTOR3";
		case Variant::VECTOR3I:
			return "Variant::VECTOR3I";
		case Variant::VECTOR4:
			return "Variant::VECTOR4";
		case Variant::VECTOR4I:
			return "Variant::VECTOR4I";
		case Variant::PLANE:
			return "Variant::PLANE";
		case Variant::AABB:
			return "Variant::AABB";
		case Variant::QUATERNION:
			return "Variant::QUATERNION";
		case Variant::BASIS:
			return "Variant::BASIS";
		case Variant::TRANSFORM3D:
			return "Variant::TRANSFORM3D";
		case Variant::PROJECTION:
			return "Variant::PROJECTION";

			// Miscellaneous types.
		case Variant::COLOR:
			return "Variant::COLOR";
		case Variant::RID:
			return "Variant::RID";
		case Variant::OBJECT:
			return "Variant::OBJECT";
		case Variant::CALLABLE:
			return "Variant::CALLABLE";
		case Variant::SIGNAL:
			return "Variant::SIGNAL";
		case Variant::STRING_NAME:
			return "Variant::STRING_NAME";
		case Variant::NODE_PATH:
			return "Variant::NODE_PATH";
		case Variant::DICTIONARY:
			return "Variant::DICTIONARY";
		case Variant::ARRAY:
			return "Variant::ARRAY";

			// Arrays.
		case Variant::PACKED_BYTE_ARRAY:
			return "Variant::PACKED_BYTE_ARRAY";
		case Variant::PACKED_INT32_ARRAY:
			return "Variant::PACKED_INT32_ARRAY";
		case Variant::PACKED_INT64_ARRAY:
			return "Variant::PACKED_INT64_ARRAY";
		case Variant::PACKED_FLOAT32_ARRAY:
			return "Variant::PACKED_FLOAT32_ARRAY";
		case Variant::PACKED_FLOAT64_ARRAY:
			return "Variant::PACKED_FLOAT64_ARRAY";
		case Variant::PACKED_STRING_ARRAY:
			return "Variant::PACKED_STRING_ARRAY";
		case Variant::PACKED_VECTOR2_ARRAY:
			return "Variant::PACKED_VECTOR2_ARRAY";
		case Variant::PACKED_VECTOR3_ARRAY:
			return "Variant::PACKED_VECTOR3_ARRAY";
		case Variant::PACKED_COLOR_ARRAY:
			return "Variant::PACKED_COLOR_ARRAY";
		case Variant::PACKED_VECTOR4_ARRAY:
			return "Variant::PACKED_VECTOR4_ARRAY";
		case Variant::VARIANT_MAX:
			ERR_FAIL_V_MSG("((void)0)", "Unreachable");
	}
}

String GDScriptTranspiler::datatype_to_cpp_type(DatatypeUsage p_usage, const GDScriptParser::DataType &p_datatype) {
	String constness = p_datatype.is_constant ? "const " : "";

	switch (p_datatype.kind) {
		case GDScriptParser::DataType::VARIANT: {
			switch (p_usage) {
				case DECLARATION:
					if (p_datatype.is_constant) {
						return "const Variant";
					} else {
						return "Variant";
					}
				case INHERITS:
					ERR_FAIL_V_MSG("", "Cannot inherit Variant!");
				case INTEROP:
					return "Variant";
			}
		} break;
		case GDScriptParser::DataType::BUILTIN: {
			if (p_datatype.builtin_type == Variant::OBJECT) {
				switch (p_usage) {
					case DECLARATION:
						if (p_datatype.is_constant) {
							return "Object *const";
						} else {
							return "Object *";
						}
					case INHERITS:
						return "Object";
					case INTEROP:
						return "Object";
				}
			}

			ERR_FAIL_COND_V_MSG(p_usage == INHERITS, "ErrorType", "Cannot inherit builtin types!");

			if (p_datatype.builtin_type == Variant::NIL) {
				switch (p_usage) {
					case DECLARATION:
						return "void";
					case INTEROP:
						return "Nil";
					case INHERITS:
						break;
				}
			}
			if (p_datatype.builtin_type == Variant::ARRAY && p_datatype.has_container_element_type(0)) {
				switch (p_usage) {
					case DECLARATION: {
						String cpp_type = datatype_to_cpp_type(DECLARATION, p_datatype.get_container_element_type(0));
						if (p_datatype.is_constant) {
							return vformat("const TypedArray<%s>", cpp_type);
						} else {
							return vformat("TypedArray<%s>", cpp_type);
						}
					} break;
					case INTEROP:
						return "Array";
					case INHERITS:
						break;
				}
			}
			return variant_type_to_cpp_type(p_datatype.builtin_type);
		} break;
		case GDScriptParser::DataType::NATIVE: {
			String class_name = p_datatype.is_meta_type ? GDScriptNativeClass::get_class_static() : String(p_datatype.native_type);
			if (p_usage == DECLARATION) {
				if (ClassDB::is_parent_class(class_name, SNAME("RefCounted"))) {
					return vformat(p_datatype.is_constant ? "const Ref<%s>" : "Ref<%s>", class_name);
				} else {
					return vformat(p_datatype.is_constant ? "%s *const" : "%s *", class_name);
				}
			}

			return class_name;
		} break;
		case GDScriptParser::DataType::CLASS: {
			if (p_usage == INTEROP) {
				return p_datatype.is_meta_type ? "GDScript" : p_datatype.class_type->fqcn;
			}

			String class_name = p_datatype.is_meta_type ? get_class_cpp_meta_name(p_datatype.class_type) : get_class_cpp_name(p_datatype.class_type);
			if (p_usage == DECLARATION) {
				if (ClassDB::is_parent_class(p_datatype.class_type->get_datatype().native_type, SNAME("RefCounted"))) {
					return vformat(p_datatype.is_constant ? "const Ref<%s>" : "Ref<%s>", class_name);
				} else {
					return vformat(p_datatype.is_constant ? "%s *const" : "%s *", class_name);
				}
			}

			return class_name;
		} break;
		case GDScriptParser::DataType::SCRIPT: {
			switch (p_usage) {
				case DECLARATION: {
					if (p_datatype.is_meta_type) {
						return p_datatype.is_constant ? "const Ref<Script>" : "Ref<Script>";
					} else {
						return vformat(p_datatype.is_constant ? "%s *const" : "%s *", p_datatype.native_type);
					}
				} break;
				case INHERITS:
					ERR_FAIL_V_MSG("", "Cannot inherit non-GDScript scripts!");
				case INTEROP: {
					String ret;
					if (p_datatype.is_meta_type) {
						ret = p_datatype.script_type != nullptr ? String(p_datatype.script_type->get_class_name()) : "";
					} else {
						ret = p_datatype.script_type != nullptr ? p_datatype.script_type->get_name() : "";
					}

					if (!ret.is_empty()) {
						return ret;
					}

					ret = p_datatype.script_path;
					if (!ret.is_empty()) {
						return ret;
					}

					return String(p_datatype.native_type);
				} break;
			}
		} break;
		case GDScriptParser::DataType::ENUM: {
			switch (p_usage) {
				case DECLARATION:
					// TODO: maybe try to get an actual enum type sometimes
					return constness + "int64_t";
				case INHERITS:
					ERR_FAIL_V_MSG("", "Cannot inherit an enum!");
				case INTEROP:
					// native_type contains either the native class defining the enum
					// or the fully qualified class name of the script defining the enum
					return String(p_datatype.native_type).get_file(); // Remove path, keep filename
			}
		}
		case GDScriptParser::DataType::RESOLVING:
		case GDScriptParser::DataType::UNRESOLVED:
			ERR_FAIL_V_MSG("", "Unresolved type!");
	}

	ERR_FAIL_V("");
}

Variant::Type GDScriptTranspiler::get_pattern_type(const GDScriptParser::PatternNode *p_pattern) {
	ERR_FAIL_NULL_V(p_pattern, Variant::NIL);
	switch (p_pattern->pattern_type) {
		case GDScriptParser::PatternNode::PT_LITERAL:
			ERR_FAIL_NULL_V(p_pattern->literal, Variant::NIL);
			ERR_FAIL_COND_V(!p_pattern->literal->reduced, Variant::NIL);
			return p_pattern->literal->reduced_value.get_type();
		case GDScriptParser::PatternNode::PT_EXPRESSION:
			ERR_FAIL_NULL_V(p_pattern->expression, Variant::NIL);
			return p_pattern->expression->get_datatype().builtin_type;
		case GDScriptParser::PatternNode::PT_BIND:
			return Variant::VARIANT_MAX;
		case GDScriptParser::PatternNode::PT_ARRAY:
			return Variant::ARRAY;
		case GDScriptParser::PatternNode::PT_DICTIONARY:
			return Variant::DICTIONARY;
		case GDScriptParser::PatternNode::PT_REST:
			return Variant::VARIANT_MAX;
		case GDScriptParser::PatternNode::PT_WILDCARD:
			return Variant::VARIANT_MAX;
	}
}

void GDScriptTranspiler::clear() {
	*this = GDScriptTranspiler();
}

Dictionary GDScriptTranspiler::transpile_script(const Ref<GDScript> &p_script) {
	ERR_FAIL_NULL_V(p_script, Dictionary());
	return transpile_script_from_path(p_script->get_script_path());
}

Dictionary GDScriptTranspiler::transpile_script_from_path(const String &p_path) {
	clear();

	script_path = p_path.simplify_path();

	Error err;
	parser_ref = GDScriptCache::get_parser(script_path, GDScriptParserRef::FULLY_SOLVED, err);
	ERR_FAIL_COND_V_MSG(err != OK, Dictionary(), String("Could not parse script: ") + String(error_names[err]));

	if (!parser_ref->get_parser()->get_errors().is_empty()) {
		ERR_PRINT("Errors while parsing:");
		for (const GDScriptParser::ParserError &perr : parser_ref->get_parser()->get_errors()) {
			ERR_PRINT(vformat("  %s(%s): %s", script_path, perr.line, perr.message));
		}
		return Dictionary();
	}

	{
		header_builder += "namespace godot {";
		header_builder += "";

		source_builder += "namespace godot {";
		source_builder += "";

		transpile_class(parser_ref->get_parser()->get_tree());

		header_builder += "";
		header_builder += "}";

		source_builder += "";
		source_builder += "}";
	}

	Dictionary ret;
	ret[get_class_header_filename(parser_ref->get_parser()->get_tree())] = "#pragma once\n" + make_header_includes() + header_builder.as_string();
	ret[get_class_source_filename(parser_ref->get_parser()->get_tree())] = make_source_includes() + source_builder.as_string();

	return ret;
}
/*
void GDScriptTranspiler::transpile_all_scripts_to_files(const String & p_output_dir) {
	String input_dir = p_input_dir.simplify_path();
	String output_dir = p_output_dir.simplify_path();
	Ref<DirAccess> d = DirAccess::_open(input_dir);
	d->list_dir_begin();
	for (String current = d->_get_next(); !current.is_empty(); current = d->_get_next()) {
		if (d->current_is_dir()) {
			transpile_scripts(input_dir + "/" + current, output_dir + "/" + current);
		} else {
			transpile(GDScriptCache::get_shallow_script);
		}
	}
	d->list_dir_end();
}*/

GDScriptTranspiler *GDScriptTranspiler::singleton = nullptr;

void GDScriptTranspiler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("transpile_script", "gdscript"), &GDScriptTranspiler::transpile_script);
	ClassDB::bind_method(D_METHOD("transpile_script_from_path", "path"), &GDScriptTranspiler::transpile_script_from_path);
}

void GDScriptTranspiler::initialize() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "Singleton already exists");
	singleton = memnew(GDScriptTranspiler);
}

void GDScriptTranspiler::uninitialize() {
	singleton = nullptr;
}

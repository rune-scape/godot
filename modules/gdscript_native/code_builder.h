/**************************************************************************/
/*  code_builder.h                                                        */
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

#ifndef CODE_BUILDER_H
#define CODE_BUILDER_H

#include "core/variant/variant.h"

#include "fast_string_builder.h"

class CodeBuilder {
	int indent_level = 0;
	String indent_sequence = "\t";
	Vector<bool> newline_stack;

	FastStringBuilder<1024> code;
	Dictionary map;

	void _append_code(const String &p_string) {
		const String indent_str = get_indent_string();
		const int64_t len = p_string.length();
		const char32_t *const str_ptr = p_string.ptr();

		String code_str;
		{
			FastStringBuilder indented_str;
			int64_t line_start = 0;
			for (int i = 0; i < len; i++) {
				const char32_t c = str_ptr[i];
				if (c == '\n') {
					// Leave empty lines empty.
					if (i != line_start) {
						indented_str += indent_str;
						indented_str += String(str_ptr + line_start, i - line_start);
					}
					indented_str += U'\n';
					line_start = i + 1;
				}
			}

			if (len != line_start) {
				indented_str += indent_str;
				indented_str += String(str_ptr + line_start, len - line_start);
			}
			code_str = indented_str;
		}

		code += code_str;

		if (is_newline_enabled()) {
			code += "\n";
		}
	}

public:
	String get_indent_string() const { return indent_sequence.repeat(indent_level); }

	int get_indent_level() const { return indent_level; }
	String set_indent_level(int p_level) { indent_level = p_level; }

	String get_indent_sequence() const { return indent_sequence; }
	void set_indent_sequence(const String &p_indent_sequence) { indent_sequence = p_indent_sequence; }

	void indent(int p_steps = 1) { indent_level += p_steps; }
	void dedent(int p_steps = 1) { indent_level -= p_steps; }

	void push_newline_enabled(bool p_enabled) { newline_stack.push_back(p_enabled); }

	void pop_newline_enabled() {
		if (!newline_stack.is_empty()) {
			newline_stack.remove_at(newline_stack.size() - 1);
		}
	}

	bool is_newline_enabled() { return newline_stack.is_empty() || newline_stack[newline_stack.size() - 1]; }

	void set_substitute_map(const Dictionary &p_map) { map = p_map; }
	Dictionary get_substitute_map() { return map; }

	void operator+=(const String &p_string) {
		_append_code(p_string);
	}

	void operator+=(const CodeBuilder &p_builder) {
		_append_code(p_builder.code.as_string());
	}

	String as_string() const {
		return code.as_string().format(map);
	}

	CodeBuilder() = default;
};

#endif // CODE_BUILDER_H

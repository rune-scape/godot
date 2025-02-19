/**************************************************************************/
/*  fast_string_builder.h                                                 */
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

#ifndef FAST_STRING_BUILDER_H
#define FAST_STRING_BUILDER_H

#include "core/string/ustring.h"

template <int MaxBufferSize = 255>
class FastStringBuilder {
	Vector<String> completed_buffers;
	int64_t string_length = 0;
	int64_t strings_appended_count = 0;
	int64_t buffer_size = 0;
	char32_t buffer[MaxBufferSize + 1];

	template <typename T>
	void _append(const T *str) {
		strings_appended_count++;
		while (*str) {
			if (buffer_size >= MaxBufferSize) {
				_flush_buffer();
			}
			buffer[buffer_size++] = *(str++);
		}
	}

	void _append(const String &str) {
		strings_appended_count++;
		if ((str.length() + buffer_size) > MaxBufferSize) {
			_flush_buffer();
			completed_buffers.push_back(str);
			string_length += str.length();
		} else {
			int64_t str_len = str.length();
			const char32_t *str_beg = str.ptr();
			const char32_t *str_end = str_beg + str_len;
			for (const char32_t *str_it = str_beg; str_it != str_end; ++str_it) {
				buffer[buffer_size++] = *str_it;
			}
		}
	}

	void _flush_buffer() {
		buffer[buffer_size] = U'\0';
		completed_buffers.push_back(String(buffer));
		string_length += buffer_size;
		buffer_size = 0;
	}

public:
	_FORCE_INLINE_ FastStringBuilder &append(const char *str) {
		_append(str);
		return *this;
	}
	_FORCE_INLINE_ FastStringBuilder &append(const wchar_t *str) {
		_append(str);
		return *this;
	}
	_FORCE_INLINE_ FastStringBuilder &append(const char16_t *str) {
		_append(str);
		return *this;
	}
	_FORCE_INLINE_ FastStringBuilder &append(const char32_t *str) {
		_append(str);
		return *this;
	}
	_FORCE_INLINE_ FastStringBuilder &append(const String &str) {
		_append(str);
		return *this;
	}
	_FORCE_INLINE_ FastStringBuilder &append(char32_t c) {
		char32_t str[] = { c, U'\0' };
		_append(str);
		return *this;
	}

	_FORCE_INLINE_ FastStringBuilder &operator+=(const char *str) { return append(str); }
	_FORCE_INLINE_ FastStringBuilder &operator+=(const wchar_t *str) { return append(str); }
	_FORCE_INLINE_ FastStringBuilder &operator+=(const char16_t *str) { return append(str); }
	_FORCE_INLINE_ FastStringBuilder &operator+=(const char32_t *str) { return append(str); }
	_FORCE_INLINE_ FastStringBuilder &operator+=(const String &str) { return append(str); }
	_FORCE_INLINE_ FastStringBuilder &operator+=(char32_t c) { return append(c); }

	_FORCE_INLINE_ int num_strings_appended() const {
		return strings_appended_count;
	}

	String as_string() const {
		String result;
		result.resize(get_string_length() + 1);

		char32_t *result_buffer = result.ptrw();
		for (const String &str : completed_buffers) {
			int64_t str_len = str.length();
			const char32_t *str_beg = str.ptr();
			const char32_t *str_end = str_beg + str_len;
			for (const char32_t *str_it = str_beg; str_it != str_end; ++str_it) {
				*(result_buffer++) = *str_it;
			}
		}

		const char32_t *buffer_end = buffer + buffer_size;
		for (const char32_t *buffer_it = buffer; buffer_it != buffer_end; ++buffer_it) {
			*(result_buffer++) = *buffer_it;
		}

		*result_buffer = U'\0';

		return result;
	}

	_FORCE_INLINE_ int64_t get_string_length() const {
		return string_length + buffer_size;
	}

	_FORCE_INLINE_ operator String() const {
		return as_string();
	}
};

#endif // FAST_STRING_BUILDER_H

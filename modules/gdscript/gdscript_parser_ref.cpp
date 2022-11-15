/*************************************************************************/
/*  gdscript_cache.cpp                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "gdscript_cache.h"

#include "core/io/file_access.h"
#include "core/templates/vector.h"
#include "gdscript.h"
#include "gdscript_analyzer.h"
#include "gdscript_compiler.h"
#include "gdscript_parser.h"

bool GDScriptParserRef::is_valid() const {
	return parser != nullptr;
}

GDScript::Status GDScriptParserRef::get_status() const {
	return root_script->get_status();
}

GDScriptParser *GDScriptParserRef::get_parser() const {
	return parser;
}

Error GDScriptParserRef::raise_status(GDScript::Status p_new_status, bool p_keep_state) {
	ERR_FAIL_NULL_V(root_script, ERR_INVALID_DATA);
	return root_script->raise_status(p_new_status, p_keep_state);
}

void GDScriptParserRef::clear() {
	if (cleared) {
		return;
	}
	cleared = true;
	root_script = nullptr;

	if (parser != nullptr) {
		memdelete(parser);
	}

	if (analyzer != nullptr) {
		memdelete(analyzer);
	}
}

GDScriptParserRef::~GDScriptParserRef() {
	// TODO: needs cyclic-ref PR
	clear();
}

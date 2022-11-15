/*************************************************************************/
/*  gdscript_parser_ref.h                                                */
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

#ifndef GDSCRIPT_PARSER_REF_H
#define GDSCRIPT_PARSER_REF_H

#include "core/object/ref_counted.h"

class GDScript;
class GDScriptAnalyzer;
class GDScriptCompiler;
class GDScriptParser;

class GDScriptParserRef : public RefCounted {
public:
	enum Status {
		EMPTY,
		PARSED,
		INHERITANCE_SOLVED,
		INTERFACE_SOLVED,
		ANALYZED,
		_POPULATING,
		_POPULATED,
		_COMPILING,
		COMPILED,
	};

private:
	GDScript *root_script = nullptr;
	GDScriptParser *parser = nullptr;
	GDScriptAnalyzer *analyzer = nullptr;
	GDScriptCompiler *compiler = nullptr;
	Error result = OK;
	//bool in_progress = false;
	bool cleared = false;

	friend class GDScript;
	friend class GDScriptCache;

public:
	bool is_valid() const;
	Status get_status() const;
	GDScriptParser *get_parser() const;
	Error raise_status(Status p_new_status, bool p_keep_state = true);
	void clear();

	GDScriptParserRef() {}
	~GDScriptParserRef();
};

#endif // GDSCRIPT_PARSER_REF_H

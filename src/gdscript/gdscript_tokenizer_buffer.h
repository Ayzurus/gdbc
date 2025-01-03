/**************************************************************************/
/*  gdscript_tokenizer_buffer.h                                           */
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

#ifndef GDSCRIPT_TOKENIZER_BUFFER_H
#define GDSCRIPT_TOKENIZER_BUFFER_H

#include "gdscript_tokenizer.h"

#define TOKENIZER_VERSION 100
#define HEADER_SIZE 12

namespace godot {

class GDScriptTokenizerBuffer : public GDScriptTokenizer {
public:
	enum CompressMode {
		COMPRESS_NONE,
		COMPRESS_ZSTD,
	};

	enum {
		TOKEN_BYTE_MASK = 0x80,
		TOKEN_BITS = 8,
		TOKEN_MASK = (1 << (TOKEN_BITS - 1)) - 1,
	};

	Vector<StringName> identifiers;
	Vector<Variant> constants;
	Vector<int> continuation_lines;
	HashMap<int, int> token_lines;
	HashMap<int, int> token_columns;
	Vector<Token> tokens;
	int current = 0;
	uint32_t current_line = 1;

	bool multiline_mode = false;
	List<int> indent_stack;
	List<List<int>> indent_stack_stack; // For lambdas, which require manipulating the indentation point.
	int pending_indents = 0;
	bool last_token_was_newline = false;

	static int _token_to_binary(const Token &p_token, PackedByteArray &r_buffer, int p_start, HashMap<StringName, uint32_t> &r_identifiers_map, HashMap<Variant, uint32_t, VariantHasher, VariantComparator> &r_constants_map);

public:
	static PackedByteArray parse_code_string(const String &p_code, CompressMode p_compress_mode);

	virtual int get_cursor_line() const override;
	virtual int get_cursor_column() const override;
	virtual void set_cursor_position(int p_line, int p_column) override;
	virtual void set_multiline_mode(bool p_state) override;
	virtual bool is_past_cursor() const override;
	virtual void push_expression_indented_block() override; // For lambdas, or blocks inside expressions.
	virtual void pop_expression_indented_block() override; // For lambdas, or blocks inside expressions.
	virtual bool is_text() override { return false; };
	virtual Token scan() override;
};

}

#endif // GDSCRIPT_TOKENIZER_BUFFER_H

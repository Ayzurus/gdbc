/**************************************************************************/
/*  gdscript_tokenizer_buffer.cpp                                         */
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

#include "gdscript_tokenizer_buffer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>

using namespace godot;

int GDScriptTokenizerBuffer::_token_to_binary(const Token &p_token, PackedByteArray &r_buffer, int p_start, HashMap<StringName, uint32_t> &r_identifiers_map, HashMap<Variant, uint32_t, VariantHasher, VariantComparator> &r_constants_map) {
	int pos = p_start;

	int token_type = p_token.type & TOKEN_MASK;

	switch (p_token.type) {
		case GDScriptTokenizer::Token::ANNOTATION:
		case GDScriptTokenizer::Token::IDENTIFIER: {
			// Add identifier to map.
			int identifier_pos;
			StringName id = p_token.get_identifier();
			if (r_identifiers_map.has(id)) {
				identifier_pos = r_identifiers_map[id];
			} else {
				identifier_pos = r_identifiers_map.size();
				r_identifiers_map[id] = identifier_pos;
			}
			token_type |= identifier_pos << TOKEN_BITS;
		} break;
		case GDScriptTokenizer::Token::ERROR:
		case GDScriptTokenizer::Token::LITERAL: {
			// Add literal to map.
			int constant_pos;
			if (r_constants_map.has(p_token.literal)) {
				constant_pos = r_constants_map[p_token.literal];
			} else {
				constant_pos = r_constants_map.size();
				r_constants_map[p_token.literal] = constant_pos;
			}
			token_type |= constant_pos << TOKEN_BITS;
		} break;
		default:
			break;
	}

	// Encode token.
	int token_len;
	if (token_type & TOKEN_MASK) {
		token_len = 8;
		r_buffer.resize(pos + token_len);
		r_buffer.encode_u32(pos, token_type | TOKEN_BYTE_MASK);
		pos += 4;
	} else {
		token_len = 5;
		r_buffer.resize(pos + token_len);
		r_buffer.set(pos, token_type);
		pos++;
	}
	r_buffer.encode_u32(pos, p_token.start_line);
	return token_len;
}

PackedByteArray GDScriptTokenizerBuffer::parse_code_string(const String &p_code, CompressMode p_compress_mode) {
	HashMap<StringName, uint32_t> identifier_map;
	HashMap<Variant, uint32_t, VariantHasher, VariantComparator> constant_map;
	PackedByteArray token_buffer;
	HashMap<uint32_t, uint32_t> token_lines;
	HashMap<uint32_t, uint32_t> token_columns;

	GDScriptTokenizerText tokenizer;
	tokenizer.set_source_code(p_code);
	tokenizer.set_multiline_mode(true); // Ignore whitespace tokens.
	Token current = tokenizer.scan();
	int token_pos = 0;
	int last_token_line = 0;
	int token_counter = 0;

	while (current.type != Token::TK_EOF) {
		int token_len = _token_to_binary(current, token_buffer, token_pos, identifier_map, constant_map);
		token_pos += token_len;
		if (token_counter > 0 && current.start_line > last_token_line) {
			token_lines[token_counter] = current.start_line;
			token_columns[token_counter] = current.start_column;
		}
		last_token_line = current.end_line;

		current = tokenizer.scan();
		token_counter++;
	}

	// Reverse maps.
	Vector<StringName> rev_identifier_map;
	rev_identifier_map.resize(identifier_map.size());
	for (const KeyValue<StringName, uint32_t> &E : identifier_map) {
		rev_identifier_map.set(E.value, E.key);
	}
	Vector<Variant> rev_constant_map;
	rev_constant_map.resize(constant_map.size());
	for (const KeyValue<Variant, uint32_t> &E : constant_map) {
		rev_constant_map.set(E.value, E.key);

	}
	HashMap<uint32_t, uint32_t> rev_token_lines;
	for (const KeyValue<uint32_t, uint32_t> &E : token_lines) {
		rev_token_lines[E.value] = E.key;
	}

	// Remove continuation lines from map.
	for (int line : tokenizer.get_continuation_lines()) {
		if (rev_token_lines.has(line)) {
			token_lines.erase(rev_token_lines[line]);
			token_columns.erase(rev_token_lines[line]);
		}
	}

	PackedByteArray contents;
	contents.resize(20);
	contents.encode_u32(0, identifier_map.size());
	contents.encode_u32(4, constant_map.size());
	contents.encode_u32(8, token_lines.size());
	contents.encode_u32(12, 0);
	contents.encode_u32(16, token_counter);

	int buf_pos = 20;

	// Save identifiers.
	for (const StringName &id : rev_identifier_map) {
		String s = String(id);
		int len = s.length();

		contents.resize(buf_pos + (len + 1) * 4);
		contents.encode_u32(buf_pos, len);
		buf_pos += 4;

		for (int i = 0; i < len; i++) {
			contents.encode_u32(buf_pos, s[i] ^ 0xb6b6b6b6);
			buf_pos += 4;
		}
	}

	// Save constants.
	for (const Variant &v : rev_constant_map) {
		// Objects cannot be constant, never encode objects.
		ERR_FAIL_COND_V_MSG(v.get_type() == Variant::OBJECT, PackedByteArray(), "Error when trying to encode Variant.");
		contents.append_array(UtilityFunctions::var_to_bytes(v));
	}
	buf_pos = contents.size();

	// Save lines and columns.
	contents.resize(buf_pos + token_lines.size() * 16);
	for (const KeyValue<uint32_t, uint32_t> &e : token_lines) {
		contents.encode_u32(buf_pos, e.key);
		buf_pos += 4;
		contents.encode_u32(buf_pos, e.value);
		buf_pos += 4;
	}
	for (const KeyValue<uint32_t, uint32_t> &e : token_columns) {
		contents.encode_u32(buf_pos, e.key);
		buf_pos += 4;
		contents.encode_u32(buf_pos, e.value);
		buf_pos += 4;
	}

	// Store tokens.
	contents.append_array(token_buffer);

	PackedByteArray buf;

	// Save header.
	buf.resize(HEADER_SIZE);
	buf.set(0, 'G');
	buf.set(1, 'D');
	buf.set(2, 'S');
	buf.set(3, 'C');
	buf.encode_u32(4, TOKENIZER_VERSION);

	switch (p_compress_mode) {
		case COMPRESS_NONE:
			buf.encode_u32(8, 0u);
			buf.append_array(contents);
			break;

		case COMPRESS_ZSTD: {
			buf.encode_u32(8, contents.size());
			contents = contents.compress(FileAccess::COMPRESSION_ZSTD);
			buf.append_array(contents);
		} break;
	}

	return buf;
}

int GDScriptTokenizerBuffer::get_cursor_line() const {
	return 0;
}

int GDScriptTokenizerBuffer::get_cursor_column() const {
	return 0;
}

void GDScriptTokenizerBuffer::set_cursor_position(int p_line, int p_column) {
}

void GDScriptTokenizerBuffer::set_multiline_mode(bool p_state) {
	multiline_mode = p_state;
}

bool GDScriptTokenizerBuffer::is_past_cursor() const {
	return false;
}

void GDScriptTokenizerBuffer::push_expression_indented_block() {
	indent_stack_stack.push_back(indent_stack);
}

void GDScriptTokenizerBuffer::pop_expression_indented_block() {
	ERR_FAIL_COND(indent_stack_stack.is_empty());
	indent_stack = indent_stack_stack.back()->get();
	indent_stack_stack.pop_back();
}

GDScriptTokenizer::Token GDScriptTokenizerBuffer::scan() {
	// Add final newline.
	if (current >= tokens.size() && !last_token_was_newline) {
		Token newline;
		newline.type = Token::NEWLINE;
		newline.start_line = current_line;
		newline.end_line = current_line;
		last_token_was_newline = true;
		return newline;
	}

	// Resolve pending indentation change.
	if (pending_indents > 0) {
		pending_indents--;
		Token indent;
		indent.type = Token::INDENT;
		indent.start_line = current_line;
		indent.end_line = current_line;
		return indent;
	} else if (pending_indents < 0) {
		pending_indents++;
		Token dedent;
		dedent.type = Token::DEDENT;
		dedent.start_line = current_line;
		dedent.end_line = current_line;
		return dedent;
	}

	if (current >= tokens.size()) {
		if (!indent_stack.is_empty()) {
			pending_indents -= indent_stack.size();
			indent_stack.clear();
			return scan();
		}
		Token eof;
		eof.type = Token::TK_EOF;
		return eof;
	};

	if (!last_token_was_newline && token_lines.has(current)) {
		current_line = token_lines[current];
		uint32_t current_column = token_columns[current];

		// Check if there's a need to indent/dedent.
		if (!multiline_mode) {
			uint32_t previous_indent = 0;
			if (!indent_stack.is_empty()) {
				previous_indent = indent_stack.back()->get();
			}
			if (current_column - 1 > previous_indent) {
				pending_indents++;
				indent_stack.push_back(current_column - 1);
			} else {
				while (current_column - 1 < previous_indent) {
					pending_indents--;
					indent_stack.pop_back();
					if (indent_stack.is_empty()) {
						break;
					}
					previous_indent = indent_stack.back()->get();
				}
			}

			Token newline;
			newline.type = Token::NEWLINE;
			newline.start_line = current_line;
			newline.end_line = current_line;
			last_token_was_newline = true;

			return newline;
		}
	}

	last_token_was_newline = false;

	Token token = tokens[current++];
	return token;
}

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
#include "io/compression.h"
#include "io/marshalls.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

int GDScriptTokenizerBuffer::_token_to_binary(const Token &p_token, Vector<uint8_t> &r_buffer, int p_start, HashMap<StringName, uint32_t> &r_identifiers_map, HashMap<Variant, uint32_t, VariantHasher, VariantComparator> &r_constants_map) {
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
		encode_uint32(token_type | TOKEN_BYTE_MASK, r_buffer.ptrw() + pos);
		pos += 4;
	} else {
		token_len = 5;
		r_buffer.resize(pos + token_len);
		r_buffer.set(pos, token_type);
		pos++;
	}
	encode_uint32(p_token.start_line, r_buffer.ptrw() + pos);
	return token_len;
}

Vector<uint8_t> GDScriptTokenizerBuffer::parse_code_string(const String &p_code, CompressMode p_compress_mode) {
	HashMap<StringName, uint32_t> identifier_map;
	HashMap<Variant, uint32_t, VariantHasher, VariantComparator> constant_map;
	Vector<uint8_t> token_buffer;
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

	Vector<uint8_t> contents;
	contents.resize(20);
	encode_uint32(identifier_map.size(), contents.ptrw());
	encode_uint32(constant_map.size(), contents.ptrw() + 4);
	encode_uint32(token_lines.size(), contents.ptrw() + 8);
	encode_uint32(token_counter, contents.ptrw() + 16);

	int buf_pos = 20;

	// Save identifiers.
	for (const StringName &id : rev_identifier_map) {
		String s = String(id);
		int len = s.length();

		contents.resize(buf_pos + (len + 1) * 4);

		encode_uint32(len, contents.ptrw() + buf_pos);
		buf_pos += 4;

		for (int i = 0; i < len; i++) {
			uint8_t tmp[4];
			encode_uint32(s[i], tmp);
			contents.set(buf_pos, tmp[0] ^ 0xb6);
			contents.set(buf_pos + 1, tmp[1] ^ 0xb6);
			contents.set(buf_pos + 2, tmp[2] ^ 0xb6);
			contents.set(buf_pos + 3, tmp[3] ^ 0xb6);
			buf_pos += 4;
		}
	}

	// Save constants.
	for (const Variant &v : rev_constant_map) {
		int len;
		// Objects cannot be constant, never encode objects.
		Error err = encode_variant(v, nullptr, len, false);
		ERR_FAIL_COND_V_MSG(err != OK, Vector<uint8_t>(), "Error when trying to encode Variant.");
		contents.resize(buf_pos + len);
		encode_variant(v, contents.ptrw() + buf_pos, len, false);
		buf_pos += len;
	}

	// Save lines and columns.
	contents.resize(buf_pos + token_lines.size() * 16);
	for (const KeyValue<uint32_t, uint32_t> &e : token_lines) {
		encode_uint32(e.key, contents.ptrw() + buf_pos);
		buf_pos += 4;
		encode_uint32(e.value, contents.ptrw() + buf_pos);
		buf_pos += 4;
	}
	for (const KeyValue<uint32_t, uint32_t> &e : token_columns) {
		encode_uint32(e.key, contents.ptrw() + buf_pos);
		buf_pos += 4;
		encode_uint32(e.value, contents.ptrw() + buf_pos);
		buf_pos += 4;
	}

	// Store tokens.
	contents.append_array(token_buffer);

	Vector<uint8_t> buf;

	// Save header.
	buf.resize(HEADER_SIZE);
	buf.set(0, 'G');
	buf.set(1, 'D');
	buf.set(2, 'S');
	buf.set(3, 'C');
	encode_uint32(TOKENIZER_VERSION, buf.ptrw() + 4);

	switch (p_compress_mode) {
		case COMPRESS_NONE:
			encode_uint32(0u, buf.ptrw() + 8);
			buf.append_array(contents);
			break;

		case COMPRESS_ZSTD: {
			encode_uint32(contents.size(), buf.ptrw() + 8);
			Vector<uint8_t> compressed;
			int max_size = Compression::get_max_compressed_buffer_size(contents.size());
			compressed.resize(max_size);

			int compressed_size = Compression::compress(compressed.ptrw(), contents.ptr(), contents.size());
			ERR_FAIL_COND_V_MSG(compressed_size < 0, Vector<uint8_t>(), "Error compressing GDScript tokenizer buffer.");
			compressed.resize(compressed_size);

			buf.append_array(compressed);
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

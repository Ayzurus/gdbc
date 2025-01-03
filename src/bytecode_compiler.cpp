/*
 * Copyright (c) 2024 Ayzurus
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "bytecode_compiler.h"
#include "gdscript/gdscript_tokenizer_buffer.h"
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>

using namespace godot;

void BytecodeCompiler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compile_from_string", "source_code", "compression"),
			&BytecodeCompiler::compile_from_string, DEFVAL(UNCOMPRESSED));
	ClassDB::bind_method(D_METHOD("compile_from_script", "source_script", "compression"),
			&BytecodeCompiler::compile_from_script, DEFVAL(UNCOMPRESSED));
	ClassDB::bind_method(D_METHOD("compress", "bytecode"), &BytecodeCompiler::compress);
	BIND_ENUM_CONSTANT(UNCOMPRESSED);
	BIND_ENUM_CONSTANT(COMPRESSED);
}

PackedByteArray BytecodeCompiler::compile_from_string(
		const String &source_code, CompressionMode compression) {
	// Validate if there is code.
	PackedByteArray bytes;
	if (source_code.is_empty()) {
		UtilityFunctions::push_error(
				"Source code can't be empty. The resulting PackedByteArray will be empty.");
		return bytes;
	}

	// Parse the source code into binary tokens with the tokenizer.
	auto compress_mode = compression == COMPRESSED ? GDScriptTokenizerBuffer::COMPRESS_ZSTD
												   : GDScriptTokenizerBuffer::COMPRESS_NONE;
	GDScriptTokenizerBuffer tokenizer;
	bytes = tokenizer.parse_code_string(source_code, compress_mode);

	for (const auto &token : tokenizer.tokens) {
		if (token.type == GDScriptTokenizer::Token::ERROR) {
			// There was an error during tokenization, return an empty PackedByteArray.
			UtilityFunctions::push_error(vformat(
					"%s. The resulting PackedByteArray will be empty.", String(token.literal)));
			return PackedByteArray();
		}
	}

	if (bytes.is_empty()) {
		// Something went wrong, return anyway.
		UtilityFunctions::push_error(
				"Bytecode compilation failed. The resulting PackedByteArray will be empty.");
	}
	return bytes;
}

PackedByteArray BytecodeCompiler::compile_from_script(
		const Script *source_script, CompressionMode compression) {
	// No null allowed.
	if (source_script == nullptr) {
		UtilityFunctions::push_error(
				"The script can't be null. The resulting PackedByteArray will be empty.");
		return PackedByteArray();
	}

	// If not a valid GDScript, reject as well.
	if (source_script->get_class() != String("GDScript") || !source_script->has_source_code()) {
		UtilityFunctions::push_error(
				"The provided script is not valid. The resulting PackedByteArray will be empty.");
		return PackedByteArray();
	}

	// Otherwise compile the source code from the Script.
	return compile_from_string(source_script->get_source_code(), compression);
}

PackedByteArray BytecodeCompiler::compress(const PackedByteArray bytecode) {
	PackedByteArray compressed_bytecode;
	// Validate size of supposed uncompressed bytecode.
	if (bytecode.size() < HEADER_SIZE) {
		UtilityFunctions::push_error(
				"The bytecode is too small. The resulting PackedByteArray will be empty.");
		return compressed_bytecode;
	}

	// Validate if the header of the bytecode is valid.
	if (bytecode.decode_u8(0) != 'G' || bytecode.decode_u8(1) != 'D' ||
			bytecode.decode_u8(2) != 'S' || bytecode.decode_u8(3) != 'C') {
		UtilityFunctions::push_error(
				"The bytecode seems to be invalid. The resulting PackedByteArray will be empty.");
		return compressed_bytecode;
	}
	if (bytecode.decode_u32(4) != TOKENIZER_VERSION) {
		UtilityFunctions::push_error("The bytecode was generated with a different engine/extension "
									 "version. The resulting PackedByteArray will be empty.");
		return compressed_bytecode;
	}
	if (bytecode.decode_u32(8) > 0) {
		UtilityFunctions::push_warning(
				"The bytecode is already compressed. Returned the same bytecode.");
		return bytecode;
	}

	// Split header and compress binary tokens.
	PackedByteArray contents;
	contents.resize(bytecode.size() - HEADER_SIZE);
	auto content_size = contents.size();
	memcpy(contents.ptrw(), bytecode.ptr() + HEADER_SIZE, content_size);
	contents = contents.compress(FileAccess::COMPRESSION_ZSTD);

	compressed_bytecode.resize(HEADER_SIZE);
	compressed_bytecode.encode_u32(0, bytecode.decode_u32(0));
	compressed_bytecode.encode_u32(4, bytecode.decode_u32(4));
	compressed_bytecode.encode_u32(8, content_size);
	compressed_bytecode.append_array(contents);
	return compressed_bytecode;
}

BytecodeCompiler::BytecodeCompiler() {}

BytecodeCompiler::~BytecodeCompiler() {}

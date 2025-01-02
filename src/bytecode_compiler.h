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

#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class BytecodeCompiler : public RefCounted {
	GDCLASS(BytecodeCompiler, RefCounted)

protected:
	static void _bind_methods();

public:
	enum CompressionMode { UNCOMPRESSED, COMPRESSED };

	PackedByteArray compile_from_string(
			const String &source_code, CompressionMode compression = UNCOMPRESSED);
	PackedByteArray compile_from_script(
			const Script *source_script, CompressionMode compression = UNCOMPRESSED);
	PackedByteArray compress(const PackedByteArray bytecode);
	BytecodeCompiler();
	~BytecodeCompiler();
};

} //namespace godot

VARIANT_ENUM_CAST(BytecodeCompiler::CompressionMode);

#endif // BYTECODE_COMPILER_H

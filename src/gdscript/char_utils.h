/**************************************************************************/
/*  char_utils.h                                                          */
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

#ifndef CHAR_UTILS_H
#define CHAR_UTILS_H

#include "char_range.inc"
#include <godot_cpp/variant/char_utils.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>

#define BSEARCH_CHAR_RANGE(m_array)                      \
	int low = 0;                                         \
	int high = sizeof(m_array) / sizeof(m_array[0]) - 1; \
	int middle;                                          \
                                                         \
	while (low <= high) {                                \
		middle = (low + high) / 2;                       \
                                                         \
		if (c < m_array[middle].start) {                 \
			high = middle - 1;                           \
		} else if (c > m_array[middle].end) {            \
			low = middle + 1;                            \
		} else {                                         \
			return true;                                 \
		}                                                \
	}                                                    \
                                                         \
	return false

static _FORCE_INLINE_ bool is_unicode_identifier_start(char32_t c) {
	BSEARCH_CHAR_RANGE(xid_start);
}

static _FORCE_INLINE_ bool is_unicode_identifier_continue(char32_t c) {
	BSEARCH_CHAR_RANGE(xid_continue);
}

static _FORCE_INLINE_ bool is_unicode_upper_case(char32_t c) {
	BSEARCH_CHAR_RANGE(uppercase_letter);
}

static _FORCE_INLINE_ bool is_unicode_lower_case(char32_t c) {
	BSEARCH_CHAR_RANGE(lowercase_letter);
}

static _FORCE_INLINE_ bool is_unicode_letter(char32_t c) {
	BSEARCH_CHAR_RANGE(unicode_letter);
}

#undef BSEARCH_CHAR_RANGE

// Special utility method to replace lack of String(p_from, len) constructor.
static inline godot::String substring(const char32_t *from, uint64_t len) {
	char32_t buffer[len + 1];
	memcpy(buffer, from, len * sizeof(char32_t));
	buffer[len] = 0;
    return godot::String(buffer);
}

#endif // CHAR_UTILS_H

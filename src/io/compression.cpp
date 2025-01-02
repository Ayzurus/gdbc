/**************************************************************************/
/*  compression.cpp                                                       */
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
#include "compression.h"
#include <zstd.h>

using namespace godot;

int Compression::compress(uint8_t *p_dst, const uint8_t *p_src, int p_src_size) {
	ZSTD_CCtx *cctx = ZSTD_createCCtx();
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, zstd_level);
	if (zstd_long_distance_matching) {
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLongDistanceMatching, 1);
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, zstd_window_log_size);
	}
	int max_dst_size = get_max_compressed_buffer_size(p_src_size);
	int ret = ZSTD_compressCCtx(cctx, p_dst, max_dst_size, p_src, p_src_size, zstd_level);
	ZSTD_freeCCtx(cctx);
	return ret;
}

int Compression::get_max_compressed_buffer_size(int p_src_size) {
	return ZSTD_compressBound(p_src_size);
}

int Compression::zstd_level = 3;
bool Compression::zstd_long_distance_matching = false;
int Compression::zstd_window_log_size = 27; // ZSTD_WINDOWLOG_LIMIT_DEFAULT
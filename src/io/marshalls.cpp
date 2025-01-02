/**************************************************************************/
/*  marshalls.cpp                                                         */
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

#include "marshalls.h"
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <limits.h>
#include <stdio.h>

#define ERR_FAIL_ADD_OF(a, b, err) ERR_FAIL_COND_V(((int32_t)(b)) < 0 || ((int32_t)(a)) < 0 || ((int32_t)(a)) > INT_MAX - ((int32_t)(b)), err)
#define ERR_FAIL_MUL_OF(a, b, err) ERR_FAIL_COND_V(((int32_t)(a)) < 0 || ((int32_t)(b)) <= 0 || ((int32_t)(a)) > INT_MAX / ((int32_t)(b)), err)

// Byte 0: `Variant::Type`, byte 1: unused, bytes 2 and 3: additional data.
#define HEADER_TYPE_MASK 0xFF

// For `Variant::INT`, `Variant::FLOAT` and other math types.
#define HEADER_DATA_FLAG_64 (1 << 16)

// For `Variant::OBJECT`.
#define HEADER_DATA_FLAG_OBJECT_AS_ID (1 << 16)

// For `Variant::ARRAY`.
// Occupies bits 16 and 17.
#define HEADER_DATA_FIELD_TYPED_ARRAY_MASK (0b11 << 16)
#define HEADER_DATA_FIELD_TYPED_ARRAY_NONE (0b00 << 16)
#define HEADER_DATA_FIELD_TYPED_ARRAY_BUILTIN (0b01 << 16)
#define HEADER_DATA_FIELD_TYPED_ARRAY_CLASS_NAME (0b10 << 16)
#define HEADER_DATA_FIELD_TYPED_ARRAY_SCRIPT (0b11 << 16)

#define VARIANT_MAX_RECURSION_DEPTH 1024
#define ENCODED_OBJECT_ID_NAME String("EncodedObjectAsID")
#define GET_RESOURCE_PATH(res) (res->call(StringName("get_path")))

using namespace godot;

static void _encode_string(const String &p_string, uint8_t *&buf, int &r_len) {
	CharString utf8 = p_string.utf8();

	if (buf) {
		encode_uint32(utf8.length(), buf);
		buf += 4;
		memcpy(buf, utf8.get_data(), utf8.length());
		buf += utf8.length();
	}

	r_len += 4 + utf8.length();
	while (r_len % 4) {
		r_len++; //pad
		if (buf) {
			*(buf++) = 0;
		}
	}
}

Error godot::encode_variant(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects, int p_depth) {
	ERR_FAIL_COND_V_MSG(p_depth > VARIANT_MAX_RECURSION_DEPTH, ERR_OUT_OF_MEMORY, "Potential infinite recursion detected. Bailing.");
	uint8_t *buf = r_buffer;

	r_len = 0;

	uint32_t header = p_variant.get_type();

	switch (p_variant.get_type()) {
		case Variant::INT: {
			int64_t val = p_variant;
			if (val > (int64_t)INT_MAX || val < (int64_t)INT_MIN) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		case Variant::FLOAT: {
			double d = p_variant;
			float f = d;
			if (double(f) != d) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		case Variant::OBJECT: {
			// Test for potential wrong values sent by the debugger when it breaks.
			Object *obj = p_variant;
			if (obj) {
				// Object is invalid, send a nullptr instead.
				if (buf) {
					encode_uint32(Variant::NIL, buf);
				}
				r_len += 4;
				return OK;
			}

			if (!p_full_objects) {
				header |= HEADER_DATA_FLAG_OBJECT_AS_ID;
			}
		} break;
		case Variant::ARRAY: {
			Array array = p_variant;
			if (array.is_typed()) {
				Ref<RefCounted> script = array.get_typed_script();
				if (script.is_valid()) {
					header |= p_full_objects ? HEADER_DATA_FIELD_TYPED_ARRAY_SCRIPT : HEADER_DATA_FIELD_TYPED_ARRAY_CLASS_NAME;
				} else if (array.get_typed_class_name() != StringName()) {
					header |= HEADER_DATA_FIELD_TYPED_ARRAY_CLASS_NAME;
				} else {
					// No need to check `p_full_objects` since for `Variant::OBJECT`
					// `array.get_typed_class_name()` should be non-empty.
					header |= HEADER_DATA_FIELD_TYPED_ARRAY_BUILTIN;
				}
			}
		} break;
#ifdef REAL_T_IS_DOUBLE
		case Variant::VECTOR2:
		case Variant::VECTOR3:
		case Variant::VECTOR4:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY:
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
		case Variant::QUATERNION:
		case Variant::PLANE:
		case Variant::BASIS:
		case Variant::RECT2:
		case Variant::AABB: {
			header |= HEADER_DATA_FLAG_64;
		} break;
#endif // REAL_T_IS_DOUBLE
		default: {
		} // nothing to do at this stage
	}

	if (buf) {
		encode_uint32(header, buf);
		buf += 4;
	}
	r_len += 4;

	switch (p_variant.get_type()) {
		case Variant::NIL: {
			//nothing to do
		} break;
		case Variant::BOOL: {
			if (buf) {
				encode_uint32(p_variant.operator bool(), buf);
			}

			r_len += 4;

		} break;
		case Variant::INT: {
			if (header & HEADER_DATA_FLAG_64) {
				//64 bits
				if (buf) {
					encode_uint64(p_variant.operator int64_t(), buf);
				}

				r_len += 8;
			} else {
				if (buf) {
					encode_uint32(p_variant.operator int32_t(), buf);
				}

				r_len += 4;
			}
		} break;
		case Variant::FLOAT: {
			if (header & HEADER_DATA_FLAG_64) {
				if (buf) {
					encode_double(p_variant.operator double(), buf);
				}

				r_len += 8;

			} else {
				if (buf) {
					encode_float(p_variant.operator float(), buf);
				}

				r_len += 4;
			}

		} break;
		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			if (buf) {
				encode_uint32(uint32_t(np.get_name_count()) | 0x80000000, buf); //for compatibility with the old format
				encode_uint32(np.get_subname_count(), buf + 4);
				uint32_t np_flags = 0;
				if (np.is_absolute()) {
					np_flags |= 1;
				}

				encode_uint32(np_flags, buf + 8);

				buf += 12;
			}

			r_len += 12;

			int total = np.get_name_count() + np.get_subname_count();

			for (int i = 0; i < total; i++) {
				String str;

				if (i < np.get_name_count()) {
					str = np.get_name(i);
				} else {
					str = np.get_subname(i - np.get_name_count());
				}

				CharString utf8 = str.utf8();

				int pad = 0;

				if (utf8.length() % 4) {
					pad = 4 - utf8.length() % 4;
				}

				if (buf) {
					encode_uint32(utf8.length(), buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length());
					buf += pad + utf8.length();
				}

				r_len += 4 + utf8.length() + pad;
			}

		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			_encode_string(p_variant, buf, r_len);

		} break;

		// math types
		case Variant::VECTOR2: {
			if (buf) {
				Vector2 v2 = p_variant;
				encode_real(v2.x, &buf[0]);
				encode_real(v2.y, &buf[sizeof(real_t)]);
			}

			r_len += 2 * sizeof(real_t);

		} break;
		case Variant::VECTOR2I: {
			if (buf) {
				Vector2i v2 = p_variant;
				encode_uint32(v2.x, &buf[0]);
				encode_uint32(v2.y, &buf[4]);
			}

			r_len += 2 * 4;

		} break;
		case Variant::RECT2: {
			if (buf) {
				Rect2 r2 = p_variant;
				encode_real(r2.position.x, &buf[0]);
				encode_real(r2.position.y, &buf[sizeof(real_t)]);
				encode_real(r2.size.x, &buf[sizeof(real_t) * 2]);
				encode_real(r2.size.y, &buf[sizeof(real_t) * 3]);
			}
			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::RECT2I: {
			if (buf) {
				Rect2i r2 = p_variant;
				encode_uint32(r2.position.x, &buf[0]);
				encode_uint32(r2.position.y, &buf[4]);
				encode_uint32(r2.size.x, &buf[8]);
				encode_uint32(r2.size.y, &buf[12]);
			}
			r_len += 4 * 4;

		} break;
		case Variant::VECTOR3: {
			if (buf) {
				Vector3 v3 = p_variant;
				encode_real(v3.x, &buf[0]);
				encode_real(v3.y, &buf[sizeof(real_t)]);
				encode_real(v3.z, &buf[sizeof(real_t) * 2]);
			}

			r_len += 3 * sizeof(real_t);

		} break;
		case Variant::VECTOR3I: {
			if (buf) {
				Vector3i v3 = p_variant;
				encode_uint32(v3.x, &buf[0]);
				encode_uint32(v3.y, &buf[4]);
				encode_uint32(v3.z, &buf[8]);
			}

			r_len += 3 * 4;

		} break;
		case Variant::TRANSFORM2D: {
			if (buf) {
				Transform2D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						memcpy(&buf[(i * 2 + j) * sizeof(real_t)], &val.columns[i][j], sizeof(real_t));
					}
				}
			}

			r_len += 6 * sizeof(real_t);

		} break;
		case Variant::VECTOR4: {
			if (buf) {
				Vector4 v4 = p_variant;
				encode_real(v4.x, &buf[0]);
				encode_real(v4.y, &buf[sizeof(real_t)]);
				encode_real(v4.z, &buf[sizeof(real_t) * 2]);
				encode_real(v4.w, &buf[sizeof(real_t) * 3]);
			}

			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::VECTOR4I: {
			if (buf) {
				Vector4i v4 = p_variant;
				encode_uint32(v4.x, &buf[0]);
				encode_uint32(v4.y, &buf[4]);
				encode_uint32(v4.z, &buf[8]);
				encode_uint32(v4.w, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::PLANE: {
			if (buf) {
				Plane p = p_variant;
				encode_real(p.normal.x, &buf[0]);
				encode_real(p.normal.y, &buf[sizeof(real_t)]);
				encode_real(p.normal.z, &buf[sizeof(real_t) * 2]);
				encode_real(p.d, &buf[sizeof(real_t) * 3]);
			}

			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::QUATERNION: {
			if (buf) {
				Quaternion q = p_variant;
				encode_real(q.x, &buf[0]);
				encode_real(q.y, &buf[sizeof(real_t)]);
				encode_real(q.z, &buf[sizeof(real_t) * 2]);
				encode_real(q.w, &buf[sizeof(real_t) * 3]);
			}

			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::AABB: {
			if (buf) {
				AABB aabb = p_variant;
				encode_real(aabb.position.x, &buf[0]);
				encode_real(aabb.position.y, &buf[sizeof(real_t)]);
				encode_real(aabb.position.z, &buf[sizeof(real_t) * 2]);
				encode_real(aabb.size.x, &buf[sizeof(real_t) * 3]);
				encode_real(aabb.size.y, &buf[sizeof(real_t) * 4]);
				encode_real(aabb.size.z, &buf[sizeof(real_t) * 5]);
			}

			r_len += 6 * sizeof(real_t);

		} break;
		case Variant::BASIS: {
			if (buf) {
				Basis val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * sizeof(real_t)], &val.rows[i][j], sizeof(real_t));
					}
				}
			}

			r_len += 9 * sizeof(real_t);

		} break;
		case Variant::TRANSFORM3D: {
			if (buf) {
				Transform3D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * sizeof(real_t)], &val.basis.rows[i][j], sizeof(real_t));
					}
				}

				encode_real(val.origin.x, &buf[sizeof(real_t) * 9]);
				encode_real(val.origin.y, &buf[sizeof(real_t) * 10]);
				encode_real(val.origin.z, &buf[sizeof(real_t) * 11]);
			}

			r_len += 12 * sizeof(real_t);

		} break;
		case Variant::PROJECTION: {
			if (buf) {
				Projection val = p_variant;
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						memcpy(&buf[(i * 4 + j) * sizeof(real_t)], &val.columns[i][j], sizeof(real_t));
					}
				}
			}

			r_len += 16 * sizeof(real_t);

		} break;

		// misc types
		case Variant::COLOR: {
			if (buf) {
				Color c = p_variant;
				encode_float(c.r, &buf[0]);
				encode_float(c.g, &buf[4]);
				encode_float(c.b, &buf[8]);
				encode_float(c.a, &buf[12]);
			}

			r_len += 4 * 4; // Colors should always be in single-precision.

		} break;
		case Variant::RID: {
			RID rid = p_variant;

			if (buf) {
				encode_uint64(rid.get_id(), buf);
			}
			r_len += 8;
		} break;
		case Variant::OBJECT: {
			if (p_full_objects) {
				Object *obj = p_variant;
				if (!obj) {
					if (buf) {
						encode_uint32(0, buf);
					}
					r_len += 4;

				} else {
					ERR_FAIL_COND_V(!ClassDB::can_instantiate(obj->get_class()), ERR_INVALID_PARAMETER);

					_encode_string(obj->get_class(), buf, r_len);

					TypedArray<Dictionary> props = obj->get_property_list();

					int pc = 0;
					for (uint64_t i = 0; i < props.size(); i++) {
						const PropertyInfo &E = PropertyInfo::from_dict(props[i]);
						if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}
						pc++;
					}

					if (buf) {
						encode_uint32(pc, buf);
						buf += 4;
					}

					r_len += 4;

					for (uint64_t i = 0; i < props.size(); i++) {
						const PropertyInfo &E = PropertyInfo::from_dict(props[i]);
						if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}

						_encode_string(E.name, buf, r_len);

						Variant value;

						if (E.name == StringName("script")) {
							Ref<RefCounted> script = obj->get_script();
							if (script.is_valid()) {
								const Resource* res = Object::cast_to<Resource>(*script);
								String path = res->get_path();
								ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), ERR_UNAVAILABLE, "Failed to encode a path to a custom script.");
								value = path;
							}
						} else {
							value = obj->get(E.name);
						}

						int len;
						Error err = encode_variant(value, buf, len, p_full_objects, p_depth + 1);
						ERR_FAIL_COND_V(err, err);
						ERR_FAIL_COND_V(len % 4, ERR_BUG);
						r_len += len;
						if (buf) {
							buf += len;
						}
					}
				}
			} else {
				if (buf) {
					Object *obj = p_variant;
					ObjectID id;
					if (obj) {
						id = obj->get_instance_id();
					}

					encode_uint64(id, buf);
				}

				r_len += 8;
			}

		} break;
		case Variant::CALLABLE: {
		} break;
		case Variant::SIGNAL: {
			Signal signal = p_variant;

			_encode_string(signal.get_name(), buf, r_len);

			if (buf) {
				encode_uint64(signal.get_object_id(), buf);
			}
			r_len += 8;
		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;

			if (buf) {
				encode_uint32(uint32_t(d.size()), buf);
				buf += 4;
			}
			r_len += 4;

			Array keys = d.keys();

			for (uint64_t i = 0; i < keys.size(); i++) {
				const Variant &E = keys[i];
				int len;
				Error err = encode_variant(E, buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
				Variant v = d.get(E, Variant());
				err = encode_variant(v, buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		case Variant::ARRAY: {
			Array array = p_variant;

			if (array.is_typed()) {
				Variant variant = array.get_typed_script();
				Ref<RefCounted> script = variant;
				if (script.is_valid()) {
					if (p_full_objects) {
						const Resource* res = Object::cast_to<Resource>(*script);
						String path = res->get_path();
						ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), ERR_UNAVAILABLE, "Failed to encode a path to a custom script for an array type.");
						_encode_string(path, buf, r_len);
					} else {
						_encode_string(ENCODED_OBJECT_ID_NAME, buf, r_len);
					}
				} else if (array.get_typed_class_name() != StringName()) {
					_encode_string(p_full_objects ? String(array.get_typed_class_name()) : ENCODED_OBJECT_ID_NAME, buf, r_len);
				} else {
					// No need to check `p_full_objects` since for `Variant::OBJECT`
					// `array.get_typed_class_name()` should be non-empty.
					if (buf) {
						encode_uint32(array.get_typed_builtin(), buf);
						buf += 4;
					}
					r_len += 4;
				}
			}

			if (buf) {
				encode_uint32(uint32_t(array.size()), buf);
				buf += 4;
			}
			r_len += 4;

			for (uint64_t i = 0; i < array.size(); i++) {
				const Variant &var = array[i];
				int len;
				Error err = encode_variant(var, buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				if (buf) {
					buf += len;
				}
				r_len += len;
			}

		} break;
		// arrays
		case Variant::PACKED_BYTE_ARRAY: {
			PackedByteArray data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(uint8_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const uint8_t *r = data.ptr();
				if (r) {
					memcpy(buf, &r[0], datalen * datasize);
					buf += datalen * datasize;
				}
			}

			r_len += 4 + datalen * datasize;
			while (r_len % 4) {
				r_len++;
				if (buf) {
					*(buf++) = 0;
				}
			}

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			PackedInt32Array data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const int32_t *r = data.ptr();
				for (int32_t i = 0; i < datalen; i++) {
					encode_uint32(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			PackedInt64Array data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int64_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const int64_t *r = data.ptr();
				for (int64_t i = 0; i < datalen; i++) {
					encode_uint64(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			PackedFloat32Array data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(float);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const float *r = data.ptr();
				for (int i = 0; i < datalen; i++) {
					encode_float(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			PackedFloat64Array data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(double);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const double *r = data.ptr();
				for (int i = 0; i < datalen; i++) {
					encode_double(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			PackedStringArray data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < len; i++) {
				CharString utf8 = data[i].utf8();

				if (buf) {
					encode_uint32(utf8.length() + 1, buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length() + 1);
					buf += utf8.length() + 1;
				}

				r_len += 4 + utf8.length() + 1;
				while (r_len % 4) {
					r_len++; //pad
					if (buf) {
						*(buf++) = 0;
					}
				}
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			PackedVector2Array data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector2 v = data[i];

					encode_real(v.x, &buf[0]);
					encode_real(v.y, &buf[sizeof(real_t)]);
					buf += sizeof(real_t) * 2;
				}
			}

			r_len += sizeof(real_t) * 2 * len;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			PackedVector3Array data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector3 v = data[i];

					encode_real(v.x, &buf[0]);
					encode_real(v.y, &buf[sizeof(real_t)]);
					encode_real(v.z, &buf[sizeof(real_t) * 2]);
					buf += sizeof(real_t) * 3;
				}
			}

			r_len += sizeof(real_t) * 3 * len;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			PackedColorArray data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Color c = data[i];

					encode_float(c.r, &buf[0]);
					encode_float(c.g, &buf[4]);
					encode_float(c.b, &buf[8]);
					encode_float(c.a, &buf[12]);
					buf += 4 * 4; // Colors should always be in single-precision.
				}
			}

			r_len += 4 * 4 * len;

		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			PackedVector4Array data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector4 v = data[i];

					encode_real(v.x, &buf[0]);
					encode_real(v.y, &buf[sizeof(real_t)]);
					encode_real(v.z, &buf[sizeof(real_t) * 2]);
					encode_real(v.w, &buf[sizeof(real_t) * 3]);
					buf += sizeof(real_t) * 4;
				}
			}

			r_len += sizeof(real_t) * 4 * len;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

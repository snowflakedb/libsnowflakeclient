/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include <arrow-glib/basic-data-type.h>
#include <arrow-glib/buffer.h>

G_BEGIN_DECLS

#define GARROW_TYPE_ARRAY (garrow_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowArray,
                         garrow_array,
                         GARROW,
                         ARRAY,
                         GObject)
struct _GArrowArrayClass
{
  GObjectClass parent_class;
};

gboolean       garrow_array_equal       (GArrowArray *array,
                                         GArrowArray *other_array);
gboolean       garrow_array_equal_approx(GArrowArray *array,
                                         GArrowArray *other_array);
gboolean       garrow_array_equal_range (GArrowArray *array,
                                         gint64 start_index,
                                         GArrowArray *other_array,
                                         gint64 other_start_index,
                                         gint64 end_index);

gboolean       garrow_array_is_null     (GArrowArray *array,
                                         gint64 i);
gboolean       garrow_array_is_valid    (GArrowArray *array,
                                         gint64 i);
gint64         garrow_array_get_length  (GArrowArray *array);
gint64         garrow_array_get_offset  (GArrowArray *array);
gint64         garrow_array_get_n_nulls (GArrowArray *array);
GArrowBuffer  *garrow_array_get_null_bitmap(GArrowArray *array);
GArrowDataType *garrow_array_get_value_data_type(GArrowArray *array);
GArrowType     garrow_array_get_value_type(GArrowArray *array);
GArrowArray   *garrow_array_slice       (GArrowArray *array,
                                         gint64 offset,
                                         gint64 length);
gchar         *garrow_array_to_string   (GArrowArray *array,
                                         GError **error);
GARROW_AVAILABLE_IN_0_15
GArrowArray *garrow_array_view(GArrowArray *array,
                               GArrowDataType *return_type,
                               GError **error);
GARROW_AVAILABLE_IN_0_15
gchar *garrow_array_diff_unified(GArrowArray *array,
                                 GArrowArray *other_array);


#define GARROW_TYPE_NULL_ARRAY (garrow_null_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowNullArray,
                         garrow_null_array,
                         GARROW,
                         NULL_ARRAY,
                         GArrowArray)
struct _GArrowNullArrayClass
{
  GArrowArrayClass parent_class;
};

GArrowNullArray *garrow_null_array_new(gint64 length);


#define GARROW_TYPE_PRIMITIVE_ARRAY (garrow_primitive_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowPrimitiveArray,
                         garrow_primitive_array,
                         GARROW,
                         PRIMITIVE_ARRAY,
                         GArrowArray)
struct _GArrowPrimitiveArrayClass
{
  GArrowArrayClass parent_class;
};

GArrowBuffer *garrow_primitive_array_get_buffer(GArrowPrimitiveArray *array);


#define GARROW_TYPE_BOOLEAN_ARRAY (garrow_boolean_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowBooleanArray,
                         garrow_boolean_array,
                         GARROW,
                         BOOLEAN_ARRAY,
                         GArrowPrimitiveArray)
struct _GArrowBooleanArrayClass
{
  GArrowPrimitiveArrayClass parent_class;
};

GArrowBooleanArray *garrow_boolean_array_new(gint64 length,
                                             GArrowBuffer *data,
                                             GArrowBuffer *null_bitmap,
                                             gint64 n_nulls);

gboolean       garrow_boolean_array_get_value (GArrowBooleanArray *array,
                                               gint64 i);
gboolean      *garrow_boolean_array_get_values(GArrowBooleanArray *array,
                                               gint64 *length);

#define GARROW_TYPE_NUMERIC_ARRAY (garrow_numeric_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowNumericArray,
                         garrow_numeric_array,
                         GARROW,
                         NUMERIC_ARRAY,
                         GArrowPrimitiveArray)
struct _GArrowNumericArrayClass
{
  GArrowPrimitiveArrayClass parent_class;
};


#define GARROW_TYPE_INT8_ARRAY (garrow_int8_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowInt8Array,
                         garrow_int8_array,
                         GARROW,
                         INT8_ARRAY,
                         GArrowNumericArray)
struct _GArrowInt8ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowInt8Array *garrow_int8_array_new(gint64 length,
                                       GArrowBuffer *data,
                                       GArrowBuffer *null_bitmap,
                                       gint64 n_nulls);

gint8 garrow_int8_array_get_value(GArrowInt8Array *array,
                                  gint64 i);
const gint8 *garrow_int8_array_get_values(GArrowInt8Array *array,
                                          gint64 *length);


#define GARROW_TYPE_UINT8_ARRAY (garrow_uint8_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowUInt8Array,
                         garrow_uint8_array,
                         GARROW,
                         UINT8_ARRAY,
                         GArrowNumericArray)
struct _GArrowUInt8ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowUInt8Array *garrow_uint8_array_new(gint64 length,
                                         GArrowBuffer *data,
                                         GArrowBuffer *null_bitmap,
                                         gint64 n_nulls);

guint8 garrow_uint8_array_get_value(GArrowUInt8Array *array,
                                    gint64 i);
const guint8 *garrow_uint8_array_get_values(GArrowUInt8Array *array,
                                            gint64 *length);


#define GARROW_TYPE_INT16_ARRAY (garrow_int16_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowInt16Array,
                         garrow_int16_array,
                         GARROW,
                         INT16_ARRAY,
                         GArrowNumericArray)
struct _GArrowInt16ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowInt16Array *garrow_int16_array_new(gint64 length,
                                         GArrowBuffer *data,
                                         GArrowBuffer *null_bitmap,
                                         gint64 n_nulls);

gint16 garrow_int16_array_get_value(GArrowInt16Array *array,
                                    gint64 i);
const gint16 *garrow_int16_array_get_values(GArrowInt16Array *array,
                                            gint64 *length);


#define GARROW_TYPE_UINT16_ARRAY (garrow_uint16_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowUInt16Array,
                         garrow_uint16_array,
                         GARROW,
                         UINT16_ARRAY,
                         GArrowNumericArray)
struct _GArrowUInt16ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowUInt16Array *garrow_uint16_array_new(gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

guint16 garrow_uint16_array_get_value(GArrowUInt16Array *array,
                                      gint64 i);
const guint16 *garrow_uint16_array_get_values(GArrowUInt16Array *array,
                                              gint64 *length);


#define GARROW_TYPE_INT32_ARRAY (garrow_int32_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowInt32Array,
                         garrow_int32_array,
                         GARROW,
                         INT32_ARRAY,
                         GArrowNumericArray)
struct _GArrowInt32ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowInt32Array *garrow_int32_array_new(gint64 length,
                                         GArrowBuffer *data,
                                         GArrowBuffer *null_bitmap,
                                         gint64 n_nulls);

gint32 garrow_int32_array_get_value(GArrowInt32Array *array,
                                    gint64 i);
const gint32 *garrow_int32_array_get_values(GArrowInt32Array *array,
                                            gint64 *length);


#define GARROW_TYPE_UINT32_ARRAY (garrow_uint32_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowUInt32Array,
                         garrow_uint32_array,
                         GARROW,
                         UINT32_ARRAY,
                         GArrowNumericArray)
struct _GArrowUInt32ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowUInt32Array *garrow_uint32_array_new(gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

guint32 garrow_uint32_array_get_value(GArrowUInt32Array *array,
                                      gint64 i);
const guint32 *garrow_uint32_array_get_values(GArrowUInt32Array *array,
                                              gint64 *length);


#define GARROW_TYPE_INT64_ARRAY (garrow_int64_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowInt64Array,
                         garrow_int64_array,
                         GARROW,
                         INT64_ARRAY,
                         GArrowNumericArray)
struct _GArrowInt64ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowInt64Array *garrow_int64_array_new(gint64 length,
                                         GArrowBuffer *data,
                                         GArrowBuffer *null_bitmap,
                                         gint64 n_nulls);

gint64 garrow_int64_array_get_value(GArrowInt64Array *array,
                                    gint64 i);
const gint64 *garrow_int64_array_get_values(GArrowInt64Array *array,
                                            gint64 *length);


#define GARROW_TYPE_UINT64_ARRAY (garrow_uint64_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowUInt64Array,
                         garrow_uint64_array,
                         GARROW,
                         UINT64_ARRAY,
                         GArrowNumericArray)
struct _GArrowUInt64ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowUInt64Array *garrow_uint64_array_new(gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

guint64 garrow_uint64_array_get_value(GArrowUInt64Array *array,
                                      gint64 i);
const guint64 *garrow_uint64_array_get_values(GArrowUInt64Array *array,
                                              gint64 *length);


#define GARROW_TYPE_FLOAT_ARRAY (garrow_float_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowFloatArray,
                         garrow_float_array,
                         GARROW,
                         FLOAT_ARRAY,
                         GArrowNumericArray)
struct _GArrowFloatArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowFloatArray *garrow_float_array_new(gint64 length,
                                         GArrowBuffer *data,
                                         GArrowBuffer *null_bitmap,
                                         gint64 n_nulls);

gfloat garrow_float_array_get_value(GArrowFloatArray *array,
                                    gint64 i);
const gfloat *garrow_float_array_get_values(GArrowFloatArray *array,
                                            gint64 *length);


#define GARROW_TYPE_DOUBLE_ARRAY (garrow_double_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowDoubleArray,
                         garrow_double_array,
                         GARROW,
                         DOUBLE_ARRAY,
                         GArrowNumericArray)
struct _GArrowDoubleArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowDoubleArray *garrow_double_array_new(gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

gdouble garrow_double_array_get_value(GArrowDoubleArray *array,
                                      gint64 i);
const gdouble *garrow_double_array_get_values(GArrowDoubleArray *array,
                                              gint64 *length);


#define GARROW_TYPE_BINARY_ARRAY (garrow_binary_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowBinaryArray,
                         garrow_binary_array,
                         GARROW,
                         BINARY_ARRAY,
                         GArrowArray)
struct _GArrowBinaryArrayClass
{
  GArrowArrayClass parent_class;
};

GArrowBinaryArray *garrow_binary_array_new(gint64 length,
                                           GArrowBuffer *value_offsets,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

GBytes *garrow_binary_array_get_value(GArrowBinaryArray *array,
                                      gint64 i);
GArrowBuffer *garrow_binary_array_get_buffer(GArrowBinaryArray *array);
GArrowBuffer *garrow_binary_array_get_offsets_buffer(GArrowBinaryArray *array);


#define GARROW_TYPE_LARGE_BINARY_ARRAY (garrow_large_binary_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowLargeBinaryArray,
                         garrow_large_binary_array,
                         GARROW,
                         LARGE_BINARY_ARRAY,
                         GArrowArray)
struct _GArrowLargeBinaryArrayClass
{
  GArrowArrayClass parent_class;
};

GARROW_AVAILABLE_IN_0_16
GArrowLargeBinaryArray *garrow_large_binary_array_new(gint64 length,
                                                      GArrowBuffer *value_offsets,
                                                      GArrowBuffer *data,
                                                      GArrowBuffer *null_bitmap,
                                                      gint64 n_nulls);

GARROW_AVAILABLE_IN_0_16
GBytes *garrow_large_binary_array_get_value(GArrowLargeBinaryArray *array,
                                            gint64 i);
GARROW_AVAILABLE_IN_0_16
GArrowBuffer *garrow_large_binary_array_get_buffer(GArrowLargeBinaryArray *array);
GARROW_AVAILABLE_IN_0_16
GArrowBuffer *garrow_large_binary_array_get_offsets_buffer(GArrowLargeBinaryArray *array);


#define GARROW_TYPE_STRING_ARRAY (garrow_string_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowStringArray,
                         garrow_string_array,
                         GARROW,
                         STRING_ARRAY,
                         GArrowBinaryArray)
struct _GArrowStringArrayClass
{
  GArrowBinaryArrayClass parent_class;
};

GArrowStringArray *garrow_string_array_new(gint64 length,
                                           GArrowBuffer *value_offsets,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

gchar *garrow_string_array_get_string(GArrowStringArray *array,
                                      gint64 i);


#define GARROW_TYPE_LARGE_STRING_ARRAY (garrow_large_string_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowLargeStringArray,
                         garrow_large_string_array,
                         GARROW,
                         LARGE_STRING_ARRAY,
                         GArrowLargeBinaryArray)
struct _GArrowLargeStringArrayClass
{
  GArrowLargeBinaryArrayClass parent_class;
};

GARROW_AVAILABLE_IN_0_16
GArrowLargeStringArray *garrow_large_string_array_new(gint64 length,
                                                      GArrowBuffer *value_offsets,
                                                      GArrowBuffer *data,
                                                      GArrowBuffer *null_bitmap,
                                                      gint64 n_nulls);

GARROW_AVAILABLE_IN_0_16
gchar *garrow_large_string_array_get_string(GArrowLargeStringArray *array,
                                            gint64 i);


#define GARROW_TYPE_DATE32_ARRAY (garrow_date32_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowDate32Array,
                         garrow_date32_array,
                         GARROW,
                         DATE32_ARRAY,
                         GArrowNumericArray)
struct _GArrowDate32ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowDate32Array *garrow_date32_array_new(gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

gint32 garrow_date32_array_get_value(GArrowDate32Array *array,
                                     gint64 i);
const gint32 *garrow_date32_array_get_values(GArrowDate32Array *array,
                                             gint64 *length);


#define GARROW_TYPE_DATE64_ARRAY (garrow_date64_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowDate64Array,
                         garrow_date64_array,
                         GARROW,
                         DATE64_ARRAY,
                         GArrowNumericArray)
struct _GArrowDate64ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowDate64Array *garrow_date64_array_new(gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

gint64 garrow_date64_array_get_value(GArrowDate64Array *array,
                                     gint64 i);
const gint64 *garrow_date64_array_get_values(GArrowDate64Array *array,
                                             gint64 *length);


#define GARROW_TYPE_TIMESTAMP_ARRAY (garrow_timestamp_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowTimestampArray,
                         garrow_timestamp_array,
                         GARROW,
                         TIMESTAMP_ARRAY,
                         GArrowNumericArray)
struct _GArrowTimestampArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowTimestampArray *garrow_timestamp_array_new(GArrowTimestampDataType *data_type,
                                                 gint64 length,
                                                 GArrowBuffer *data,
                                                 GArrowBuffer *null_bitmap,
                                                 gint64 n_nulls);

gint64 garrow_timestamp_array_get_value(GArrowTimestampArray *array,
                                        gint64 i);
const gint64 *garrow_timestamp_array_get_values(GArrowTimestampArray *array,
                                                gint64 *length);


#define GARROW_TYPE_TIME32_ARRAY (garrow_time32_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowTime32Array,
                         garrow_time32_array,
                         GARROW,
                         TIME32_ARRAY,
                         GArrowNumericArray)
struct _GArrowTime32ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowTime32Array *garrow_time32_array_new(GArrowTime32DataType *data_type,
                                           gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

gint32 garrow_time32_array_get_value(GArrowTime32Array *array,
                                     gint64 i);
const gint32 *garrow_time32_array_get_values(GArrowTime32Array *array,
                                             gint64 *length);


#define GARROW_TYPE_TIME64_ARRAY (garrow_time64_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowTime64Array,
                         garrow_time64_array,
                         GARROW,
                         TIME64_ARRAY,
                         GArrowNumericArray)
struct _GArrowTime64ArrayClass
{
  GArrowNumericArrayClass parent_class;
};

GArrowTime64Array *garrow_time64_array_new(GArrowTime64DataType *data_type,
                                           gint64 length,
                                           GArrowBuffer *data,
                                           GArrowBuffer *null_bitmap,
                                           gint64 n_nulls);

gint64 garrow_time64_array_get_value(GArrowTime64Array *array,
                                     gint64 i);
const gint64 *garrow_time64_array_get_values(GArrowTime64Array *array,
                                             gint64 *length);


#define GARROW_TYPE_FIXED_SIZE_BINARY_ARRAY (garrow_fixed_size_binary_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowFixedSizeBinaryArray,
                         garrow_fixed_size_binary_array,
                         GARROW,
                         FIXED_SIZE_BINARY_ARRAY,
                         GArrowPrimitiveArray)
struct _GArrowFixedSizeBinaryArrayClass
{
  GArrowPrimitiveArrayClass parent_class;
};


#define GARROW_TYPE_DECIMAL128_ARRAY (garrow_decimal128_array_get_type())
G_DECLARE_DERIVABLE_TYPE(GArrowDecimal128Array,
                         garrow_decimal128_array,
                         GARROW,
                         DECIMAL128_ARRAY,
                         GArrowFixedSizeBinaryArray)
struct _GArrowDecimal128ArrayClass
{
  GArrowFixedSizeBinaryArrayClass parent_class;
};

gchar *garrow_decimal128_array_format_value(GArrowDecimal128Array *array,
                                            gint64 i);
GArrowDecimal128 *garrow_decimal128_array_get_value(GArrowDecimal128Array *array,
                                                    gint64 i);

G_END_DECLS

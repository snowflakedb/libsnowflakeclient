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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <arrow-glib/array-builder.hpp>
#include <arrow-glib/data-type.hpp>
#include <arrow-glib/decimal128.hpp>
#include <arrow-glib/error.hpp>
#include <arrow-glib/type.hpp>

template <typename BUILDER, typename VALUE>
gboolean
garrow_array_builder_append_value(GArrowArrayBuilder *builder,
                                  VALUE value,
                                  GError **error,
                                  const gchar *context)
{
  auto arrow_builder =
    static_cast<BUILDER>(garrow_array_builder_get_raw(builder));
  auto status = arrow_builder->Append(value);
  return garrow_error_check(error, status, context);
}

template <typename BUILDER, typename VALUE>
gboolean
garrow_array_builder_append_values(GArrowArrayBuilder *builder,
                                   VALUE *values,
                                   gint64 values_length,
                                   const gboolean *is_valids,
                                   gint64 is_valids_length,
                                   GError **error,
                                   const gchar *context)
{
  auto arrow_builder =
    static_cast<BUILDER>(garrow_array_builder_get_raw(builder));
  arrow::Status status;
  if (is_valids_length > 0) {
    if (values_length != is_valids_length) {
      g_set_error(error,
                  GARROW_ERROR,
                  GARROW_ERROR_INVALID,
                  "%s: values length and is_valids length must be equal: "
                  "<%" G_GINT64_FORMAT "> != "
                  "<%" G_GINT64_FORMAT ">",
                  context,
                  values_length,
                  is_valids_length);
      return FALSE;
    }

    const gint64 chunk_size = 4096;
    gint64 n_chunks = is_valids_length / chunk_size;
    gint64 n_remains = is_valids_length % chunk_size;
    for (gint64 i = 0; i < n_chunks; ++i) {
      uint8_t valid_bytes[chunk_size];
      gint64 offset = chunk_size * i;
      const gboolean *chunked_is_valids = is_valids + offset;
      for (gint64 j = 0; j < chunk_size; ++j) {
        valid_bytes[j] = chunked_is_valids[j];
      }
      status = arrow_builder->AppendValues(values + offset,
                                           chunk_size,
                                           valid_bytes);
      if (!garrow_error_check(error, status, context)) {
        return FALSE;
      }
    }
    {
      uint8_t valid_bytes[n_remains];
      gint64 offset = chunk_size * n_chunks;
      const gboolean *chunked_is_valids = is_valids + offset;
      for (gint64 i = 0; i < n_remains; ++i) {
        valid_bytes[i] = chunked_is_valids[i];
      }
      status = arrow_builder->AppendValues(values + offset,
                                           n_remains,
                                           valid_bytes);
    }
  } else {
    status = arrow_builder->AppendValues(values, values_length, nullptr);
  }
  return garrow_error_check(error, status, context);
}

template <typename BUILDER>
gboolean
garrow_array_builder_append_values(GArrowArrayBuilder *builder,
                                   GBytes **values,
                                   gint64 values_length,
                                   const gboolean *is_valids,
                                   gint64 is_valids_length,
                                   GError **error,
                                   const gchar *context)
{
  auto arrow_builder =
    static_cast<BUILDER>(garrow_array_builder_get_raw(builder));
  arrow::Status status;
  if (is_valids_length > 0 && values_length != is_valids_length) {
    g_set_error(error,
                GARROW_ERROR,
                GARROW_ERROR_INVALID,
                "%s: values length and is_valids length must be equal: "
                "<%" G_GINT64_FORMAT "> != "
                "<%" G_GINT64_FORMAT ">",
                context,
                values_length,
                is_valids_length);
    return FALSE;
  }

  const gint64 chunk_size = 4096;
  gint64 n_chunks = values_length / chunk_size;
  gint64 n_remains = values_length % chunk_size;
  for (gint64 i = 0; i < n_chunks; ++i) {
    std::vector<std::string> strings;
    uint8_t *valid_bytes = nullptr;
    uint8_t valid_bytes_buffer[chunk_size];
    if (is_valids_length > 0) {
      valid_bytes = valid_bytes_buffer;
    }
    const gint64 offset = chunk_size * i;
    for (gint64 j = 0; j < chunk_size; ++j) {
      auto value = values[offset + j];
      size_t data_size;
      auto raw_data = g_bytes_get_data(value, &data_size);
      strings.push_back(std::string(static_cast<const char *>(raw_data),
                                    data_size));
      if (valid_bytes) {
        valid_bytes_buffer[j] = is_valids[offset + j];
      }
    }
    status = arrow_builder->AppendValues(strings, valid_bytes);
    if (!garrow_error_check(error, status, context)) {
      return FALSE;
    }
  }
  {
    std::vector<std::string> strings;
    uint8_t *valid_bytes = nullptr;
    uint8_t valid_bytes_buffer[chunk_size];
    const gint64 offset = chunk_size * n_chunks;
    if (is_valids_length > 0) {
      valid_bytes = valid_bytes_buffer;
    }
    for (gint64 i = 0; i < n_remains; ++i) {
      auto value = values[offset + i];
      size_t data_size;
      auto raw_data = g_bytes_get_data(value, &data_size);
      strings.push_back(std::string(static_cast<const char *>(raw_data),
                                    data_size));
      if (valid_bytes) {
        valid_bytes_buffer[i] = is_valids[offset + i];
      }
    }
    status = arrow_builder->AppendValues(strings, valid_bytes);
  }
  return garrow_error_check(error, status, context);
}

template <typename BUILDER>
gboolean
garrow_array_builder_append_null(GArrowArrayBuilder *builder,
                                 GError **error,
                                 const gchar *context)
{
  auto arrow_builder =
    static_cast<BUILDER>(garrow_array_builder_get_raw(builder));
  auto status = arrow_builder->AppendNull();
  return garrow_error_check(error, status, context);
}

template <typename BUILDER>
gboolean
garrow_array_builder_append_nulls(GArrowArrayBuilder *builder,
                                  gint64 n,
                                  GError **error,
                                  const gchar *context)
{
  if (n < 0) {
    g_set_error(error,
                GARROW_ERROR,
                GARROW_ERROR_INVALID,
                "%s: the number of nulls must be 0 or larger: "
                "<%" G_GINT64_FORMAT ">",
                context,
                n);
    return FALSE;
  }
  if (n == 0) {
    return TRUE;
  }

  auto arrow_builder =
    static_cast<BUILDER>(garrow_array_builder_get_raw(builder));
  auto status = arrow_builder->AppendNulls(n);
  return garrow_error_check(error, status, context);
}

G_BEGIN_DECLS

/**
 * SECTION: array-builder
 * @section_id: array-builder-classes
 * @title: Array builder classes
 * @include: arrow-glib/arrow-glib.h
 *
 * #GArrowArrayBuilder is a base class for all array builder classes
 * such as #GArrowBooleanArrayBuilder.
 *
 * You need to use array builder class to create a new array.
 *
 * #GArrowNullArrayBuilder is the class to create a new
 * #GArrowNullArray.
 *
 * #GArrowBooleanArrayBuilder is the class to create a new
 * #GArrowBooleanArray.
 *
 * #GArrowIntArrayBuilder is the class to create a new integer
 * array. Integer size is automatically chosen. It's recommend that
 * you use this builder instead of specific integer size builder such
 * as #GArrowInt8ArrayBuilder.
 *
 * #GArrowUIntArrayBuilder is the class to create a new unsigned
 * integer array. Unsigned integer size is automatically chosen. It's
 * recommend that you use this builder instead of specific unsigned
 * integer size builder such as #GArrowUInt8ArrayBuilder.
 *
 * #GArrowInt8ArrayBuilder is the class to create a new
 * #GArrowInt8Array.
 *
 * #GArrowUInt8ArrayBuilder is the class to create a new
 * #GArrowUInt8Array.
 *
 * #GArrowInt16ArrayBuilder is the class to create a new
 * #GArrowInt16Array.
 *
 * #GArrowUInt16ArrayBuilder is the class to create a new
 * #GArrowUInt16Array.
 *
 * #GArrowInt32ArrayBuilder is the class to create a new
 * #GArrowInt32Array.
 *
 * #GArrowUInt32ArrayBuilder is the class to create a new
 * #GArrowUInt32Array.
 *
 * #GArrowInt64ArrayBuilder is the class to create a new
 * #GArrowInt64Array.
 *
 * #GArrowUInt64ArrayBuilder is the class to create a new
 * #GArrowUInt64Array.
 *
 * #GArrowFloatArrayBuilder is the class to creating a new
 * #GArrowFloatArray.
 *
 * #GArrowDoubleArrayBuilder is the class to create a new
 * #GArrowDoubleArray.
 *
 * #GArrowBinaryArrayBuilder is the class to create a new
 * #GArrowBinaryArray.
 *
 * #GArrowLargeBinaryArrayBuilder is the class to create a new
 * #GArrowLargeBinaryArray.
 *
 * #GArrowStringArrayBuilder is the class to create a new
 * #GArrowStringArray.
 *
 * #GArrowLargeStringArrayBuilder is the class to create a new
 * #GArrowLargeStringArray.
 *
 * #GArrowDate32ArrayBuilder is the class to create a new
 * #GArrowDate32Array.
 *
 * #GArrowDate64ArrayBuilder is the class to create a new
 * #GArrowDate64Array.
 *
 * #GArrowTimestampArrayBuilder is the class to create a new
 * #GArrowTimestampArray.
 *
 * #GArrowTime32ArrayBuilder is the class to create a new
 * #GArrowTime32Array.
 *
 * #GArrowTime64ArrayBuilder is the class to create a new
 * #GArrowTime64Array.
 *
 * #GArrowListArrayBuilder is the class to create a new
 * #GArrowListArray.
 *
 * #GArrowLargeListArrayBuilder is the class to create a new
 * #GArrowLargeListArray.
 *
 * #GArrowStructArrayBuilder is the class to create a new
 * #GArrowStructArray.
 *
 * #GArrowMapArrayBuilder is the class to create a new
 * #GArrowMapArray.
 *
 * #GArrowDecimal128ArrayBuilder is the class to create a new
 * #GArrowDecimal128Array.
 */

typedef struct GArrowArrayBuilderPrivate_ {
  arrow::ArrayBuilder *array_builder;
  gboolean have_ownership;
} GArrowArrayBuilderPrivate;

enum {
  PROP_0,
  PROP_ARRAY_BUILDER
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GArrowArrayBuilder,
                                    garrow_array_builder,
                                    G_TYPE_OBJECT)

#define GARROW_ARRAY_BUILDER_GET_PRIVATE(obj)         \
  static_cast<GArrowArrayBuilderPrivate *>(           \
     garrow_array_builder_get_instance_private(       \
       GARROW_ARRAY_BUILDER(obj)))

static void
garrow_array_builder_finalize(GObject *object)
{
  auto priv = GARROW_ARRAY_BUILDER_GET_PRIVATE(object);

  if (priv->have_ownership) {
    delete priv->array_builder;
  }

  G_OBJECT_CLASS(garrow_array_builder_parent_class)->finalize(object);
}

static void
garrow_array_builder_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  auto priv = GARROW_ARRAY_BUILDER_GET_PRIVATE(object);

  switch (prop_id) {
  case PROP_ARRAY_BUILDER:
    priv->array_builder =
      static_cast<arrow::ArrayBuilder *>(g_value_get_pointer(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
garrow_array_builder_get_property(GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
garrow_array_builder_init(GArrowArrayBuilder *builder)
{
  auto priv = GARROW_ARRAY_BUILDER_GET_PRIVATE(builder);
  priv->have_ownership = TRUE;
}

static void
garrow_array_builder_class_init(GArrowArrayBuilderClass *klass)
{
  GObjectClass *gobject_class;
  GParamSpec *spec;

  gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize     = garrow_array_builder_finalize;
  gobject_class->set_property = garrow_array_builder_set_property;
  gobject_class->get_property = garrow_array_builder_get_property;

  spec = g_param_spec_pointer("array-builder",
                              "Array builder",
                              "The raw arrow::ArrayBuilder *",
                              static_cast<GParamFlags>(G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(gobject_class, PROP_ARRAY_BUILDER, spec);
}

static GArrowArrayBuilder *
garrow_array_builder_new(const std::shared_ptr<arrow::DataType> &type,
                         GError **error,
                         const char *context)
{
  auto memory_pool = arrow::default_memory_pool();
  std::unique_ptr<arrow::ArrayBuilder> arrow_builder;
  auto status = arrow::MakeBuilder(memory_pool, type, &arrow_builder);
  if (!garrow_error_check(error, status, context)) {
    return NULL;
  }
  return garrow_array_builder_new_raw(arrow_builder.release());
}

/**
 * garrow_array_builder_release_ownership: (skip)
 * @builder: A #GArrowArrayBuilder.
 *
 * Release ownership of `arrow::ArrayBuilder` in `builder`.
 *
 * Since: 0.8.0
 */
void
garrow_array_builder_release_ownership(GArrowArrayBuilder *builder)
{
  auto priv = GARROW_ARRAY_BUILDER_GET_PRIVATE(builder);
  priv->have_ownership = FALSE;
}

/**
 * garrow_array_builder_get_value_data_type:
 * @builder: A #GArrowArrayBuilder.
 *
 * Returns: (transfer full): The #GArrowDataType of the value of
 *   the array builder.
 *
 * Since: 0.9.0
 */
GArrowDataType *
garrow_array_builder_get_value_data_type(GArrowArrayBuilder *builder)
{
  auto arrow_builder = garrow_array_builder_get_raw(builder);
  auto arrow_type = arrow_builder->type();
  return garrow_data_type_new_raw(&arrow_type);
}

/**
 * garrow_array_builder_get_value_type:
 * @builder: A #GArrowArrayBuilder.
 *
 * Returns: The #GArrowType of the value of the array builder.
 *
 * Since: 0.9.0
 */
GArrowType
garrow_array_builder_get_value_type(GArrowArrayBuilder *builder)
{
  auto arrow_builder = garrow_array_builder_get_raw(builder);
  auto arrow_type = arrow_builder->type();
  return garrow_type_from_raw(arrow_type->id());
}

/**
 * garrow_array_builder_finish:
 * @builder: A #GArrowArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: (transfer full): The built #GArrowArray on success,
 *   %NULL on error.
 */
GArrowArray *
garrow_array_builder_finish(GArrowArrayBuilder *builder, GError **error)
{
  auto arrow_builder = garrow_array_builder_get_raw(builder);
  std::shared_ptr<arrow::Array> arrow_array;
  auto status = arrow_builder->Finish(&arrow_array);
  if (garrow_error_check(error, status, "[array-builder][finish]")) {
    return garrow_array_new_raw(&arrow_array);
  } else {
    return NULL;
  }
}


G_DEFINE_TYPE(GArrowNullArrayBuilder,
              garrow_null_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_null_array_builder_init(GArrowNullArrayBuilder *builder)
{
}

static void
garrow_null_array_builder_class_init(GArrowNullArrayBuilderClass *klass)
{
}

/**
 * garrow_null_array_builder_new:
 *
 * Returns: A newly created #GArrowNullArrayBuilder.
 *
 * Since: 0.13.0
 */
GArrowNullArrayBuilder *
garrow_null_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::null(),
                                          NULL,
                                          "[null-array-builder][new]");
  return GARROW_NULL_ARRAY_BUILDER(builder);
}

/**
 * garrow_null_array_builder_append_null:
 * @builder: A #GArrowNullArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.13.0
 */
gboolean
garrow_null_array_builder_append_null(GArrowNullArrayBuilder *builder,
                                      GError **error)
{
  return garrow_array_builder_append_null<arrow::NullBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[null-array-builder][append-null]");
}

/**
 * garrow_null_array_builder_append_nulls:
 * @builder: A #GArrowNullArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.13.0
 */
gboolean
garrow_null_array_builder_append_nulls(GArrowNullArrayBuilder *builder,
                                       gint64 n,
                                       GError **error)
{
  return garrow_array_builder_append_nulls<arrow::NullBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[null-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowBooleanArrayBuilder,
              garrow_boolean_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_boolean_array_builder_init(GArrowBooleanArrayBuilder *builder)
{
}

static void
garrow_boolean_array_builder_class_init(GArrowBooleanArrayBuilderClass *klass)
{
}

/**
 * garrow_boolean_array_builder_new:
 *
 * Returns: A newly created #GArrowBooleanArrayBuilder.
 */
GArrowBooleanArrayBuilder *
garrow_boolean_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::boolean(),
                                          NULL,
                                          "[boolean-array-builder][new]");
  return GARROW_BOOLEAN_ARRAY_BUILDER(builder);
}

/**
 * garrow_boolean_array_builder_append:
 * @builder: A #GArrowBooleanArrayBuilder.
 * @value: A boolean value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_boolean_array_builder_append_value() instead.
 */
gboolean
garrow_boolean_array_builder_append(GArrowBooleanArrayBuilder *builder,
                                    gboolean value,
                                    GError **error)
{
  return garrow_boolean_array_builder_append_value(builder, value, error);
}

/**
 * garrow_boolean_array_builder_append_value:
 * @builder: A #GArrowBooleanArrayBuilder.
 * @value: A boolean value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_boolean_array_builder_append_value(GArrowBooleanArrayBuilder *builder,
                                          gboolean value,
                                          GError **error)
{
  return garrow_array_builder_append_value<arrow::BooleanBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     static_cast<bool>(value),
     error,
     "[boolean-array-builder][append-value]");
}

/**
 * garrow_boolean_array_builder_append_values:
 * @builder: A #GArrowBooleanArrayBuilder.
 * @values: (array length=values_length): The array of boolean.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_boolean_array_builder_append_values(GArrowBooleanArrayBuilder *builder,
                                           const gboolean *values,
                                           gint64 values_length,
                                           const gboolean *is_valids,
                                           gint64 is_valids_length,
                                           GError **error)
{
  guint8 arrow_values[values_length];
  for (gint64 i = 0; i < values_length; ++i) {
    arrow_values[i] = values[i];
  }
  return garrow_array_builder_append_values<arrow::BooleanBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     arrow_values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[boolean-array-builder][append-values]");
}

/**
 * garrow_boolean_array_builder_append_null:
 * @builder: A #GArrowBooleanArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_boolean_array_builder_append_null(GArrowBooleanArrayBuilder *builder,
                                         GError **error)
{
  return garrow_array_builder_append_null<arrow::BooleanBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[boolean-array-builder][append-null]");
}

/**
 * garrow_boolean_array_builder_append_nulls:
 * @builder: A #GArrowBooleanArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_boolean_array_builder_append_nulls(GArrowBooleanArrayBuilder *builder,
                                          gint64 n,
                                          GError **error)
{
  return garrow_array_builder_append_nulls<arrow::BooleanBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[boolean-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowIntArrayBuilder,
              garrow_int_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_int_array_builder_init(GArrowIntArrayBuilder *builder)
{
}

static void
garrow_int_array_builder_class_init(GArrowIntArrayBuilderClass *klass)
{
}

/**
 * garrow_int_array_builder_new:
 *
 * Returns: A newly created #GArrowIntArrayBuilder.
 *
 * Since: 0.6.0
 */
GArrowIntArrayBuilder *
garrow_int_array_builder_new(void)
{
  auto memory_pool = arrow::default_memory_pool();
  auto arrow_builder = new arrow::AdaptiveIntBuilder(memory_pool);
  auto builder = garrow_array_builder_new_raw(arrow_builder,
                                              GARROW_TYPE_INT_ARRAY_BUILDER);
  return GARROW_INT_ARRAY_BUILDER(builder);
}

/**
 * garrow_int_array_builder_append:
 * @builder: A #GArrowIntArrayBuilder.
 * @value: A int value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.6.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_int_array_builder_append_value() instead.
 */
gboolean
garrow_int_array_builder_append(GArrowIntArrayBuilder *builder,
                                gint64 value,
                                GError **error)
{
  return garrow_int_array_builder_append_value(builder, value, error);
}

/**
 * garrow_int_array_builder_append_value:
 * @builder: A #GArrowIntArrayBuilder.
 * @value: A int value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_int_array_builder_append_value(GArrowIntArrayBuilder *builder,
                                      gint64 value,
                                      GError **error)
{
  return garrow_array_builder_append_value<arrow::AdaptiveIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[int-array-builder][append-value]");
}

/**
 * garrow_int_array_builder_append_values:
 * @builder: A #GArrowIntArrayBuilder.
 * @values: (array length=values_length): The array of int.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int_array_builder_append_values(GArrowIntArrayBuilder *builder,
                                       const gint64 *values,
                                       gint64 values_length,
                                       const gboolean *is_valids,
                                       gint64 is_valids_length,
                                       GError **error)
{
  return garrow_array_builder_append_values<arrow::AdaptiveIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const int64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[int-array-builder][append-values]");
}

/**
 * garrow_int_array_builder_append_null:
 * @builder: A #GArrowIntArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.6.0
 */
gboolean
garrow_int_array_builder_append_null(GArrowIntArrayBuilder *builder,
                                     GError **error)
{
  return garrow_array_builder_append_null<arrow::AdaptiveIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[int-array-builder][append-null]");
}

/**
 * garrow_int_array_builder_append_nulls:
 * @builder: A #GArrowIntArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int_array_builder_append_nulls(GArrowIntArrayBuilder *builder,
                                      gint64 n,
                                      GError **error)
{
  return garrow_array_builder_append_nulls<arrow::AdaptiveIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[int-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowUIntArrayBuilder,
              garrow_uint_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_uint_array_builder_init(GArrowUIntArrayBuilder *builder)
{
}

static void
garrow_uint_array_builder_class_init(GArrowUIntArrayBuilderClass *klass)
{
}

/**
 * garrow_uint_array_builder_new:
 *
 * Returns: A newly created #GArrowUIntArrayBuilder.
 *
 * Since: 0.8.0
 */
GArrowUIntArrayBuilder *
garrow_uint_array_builder_new(void)
{
  auto memory_pool = arrow::default_memory_pool();
  auto arrow_builder = new arrow::AdaptiveUIntBuilder(memory_pool);
  auto builder = garrow_array_builder_new_raw(arrow_builder,
                                              GARROW_TYPE_UINT_ARRAY_BUILDER);
  return GARROW_UINT_ARRAY_BUILDER(builder);
}

/**
 * garrow_uint_array_builder_append:
 * @builder: A #GArrowUIntArrayBuilder.
 * @value: A unsigned int value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_uint_array_builder_append_value() instead.
 */
gboolean
garrow_uint_array_builder_append(GArrowUIntArrayBuilder *builder,
                                 guint64 value,
                                 GError **error)
{
  return garrow_uint_array_builder_append_value(builder, value, error);
}

/**
 * garrow_uint_array_builder_append_value:
 * @builder: A #GArrowUIntArrayBuilder.
 * @value: A unsigned int value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_uint_array_builder_append_value(GArrowUIntArrayBuilder *builder,
                                       guint64 value,
                                       GError **error)
{
  return garrow_array_builder_append_value<arrow::AdaptiveUIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[uint-array-builder][append-value]");
}

/**
 * garrow_uint_array_builder_append_values:
 * @builder: A #GArrowUIntArrayBuilder.
 * @values: (array length=values_length): The array of unsigned int.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint_array_builder_append_values(GArrowUIntArrayBuilder *builder,
                                        const guint64 *values,
                                        gint64 values_length,
                                        const gboolean *is_valids,
                                        gint64 is_valids_length,
                                        GError **error)
{
  return garrow_array_builder_append_values<arrow::AdaptiveUIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const uint64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[uint-array-builder][append-values]");
}

/**
 * garrow_uint_array_builder_append_null:
 * @builder: A #GArrowUIntArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint_array_builder_append_null(GArrowUIntArrayBuilder *builder,
                                      GError **error)
{
  return garrow_array_builder_append_null<arrow::AdaptiveUIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[uint-array-builder][append-null]");
}

/**
 * garrow_uint_array_builder_append_nulls:
 * @builder: A #GArrowUIntArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint_array_builder_append_nulls(GArrowUIntArrayBuilder *builder,
                                       gint64 n,
                                       GError **error)
{
  return garrow_array_builder_append_nulls<arrow::AdaptiveUIntBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[uint-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowInt8ArrayBuilder,
              garrow_int8_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_int8_array_builder_init(GArrowInt8ArrayBuilder *builder)
{
}

static void
garrow_int8_array_builder_class_init(GArrowInt8ArrayBuilderClass *klass)
{
}

/**
 * garrow_int8_array_builder_new:
 *
 * Returns: A newly created #GArrowInt8ArrayBuilder.
 */
GArrowInt8ArrayBuilder *
garrow_int8_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::int8(),
                                          NULL,
                                          "[int8-array-builder][new]");
  return GARROW_INT8_ARRAY_BUILDER(builder);
}

/**
 * garrow_int8_array_builder_append:
 * @builder: A #GArrowInt8ArrayBuilder.
 * @value: A int8 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_int8_array_builder_append_value() instead.
 */
gboolean
garrow_int8_array_builder_append(GArrowInt8ArrayBuilder *builder,
                                 gint8 value,
                                 GError **error)
{
  return garrow_int8_array_builder_append_value(builder, value, error);
}

/**
 * garrow_int8_array_builder_append_value:
 * @builder: A #GArrowInt8ArrayBuilder.
 * @value: A int8 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_int8_array_builder_append_value(GArrowInt8ArrayBuilder *builder,
                                       gint8 value,
                                       GError **error)
{
  return garrow_array_builder_append_value<arrow::Int8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[int8-array-builder][append-value]");
}

/**
 * garrow_int8_array_builder_append_values:
 * @builder: A #GArrowInt8ArrayBuilder.
 * @values: (array length=values_length): The array of int8.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int8_array_builder_append_values(GArrowInt8ArrayBuilder *builder,
                                        const gint8 *values,
                                        gint64 values_length,
                                        const gboolean *is_valids,
                                        gint64 is_valids_length,
                                        GError **error)
{
  return garrow_array_builder_append_values<arrow::Int8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[int8-array-builder][append-values]");
}

/**
 * garrow_int8_array_builder_append_null:
 * @builder: A #GArrowInt8ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_int8_array_builder_append_null(GArrowInt8ArrayBuilder *builder,
                                      GError **error)
{
  return garrow_array_builder_append_null<arrow::Int8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[int8-array-builder][append-null]");
}

/**
 * garrow_int8_array_builder_append_nulls:
 * @builder: A #GArrowInt8ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int8_array_builder_append_nulls(GArrowInt8ArrayBuilder *builder,
                                       gint64 n,
                                       GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Int8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[int8-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowUInt8ArrayBuilder,
              garrow_uint8_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_uint8_array_builder_init(GArrowUInt8ArrayBuilder *builder)
{
}

static void
garrow_uint8_array_builder_class_init(GArrowUInt8ArrayBuilderClass *klass)
{
}

/**
 * garrow_uint8_array_builder_new:
 *
 * Returns: A newly created #GArrowUInt8ArrayBuilder.
 */
GArrowUInt8ArrayBuilder *
garrow_uint8_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::uint8(),
                                          NULL,
                                          "[uint8-array-builder][new]");
  return GARROW_UINT8_ARRAY_BUILDER(builder);
}

/**
 * garrow_uint8_array_builder_append:
 * @builder: A #GArrowUInt8ArrayBuilder.
 * @value: An uint8 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_uint8_array_builder_append_value() instead.
 */
gboolean
garrow_uint8_array_builder_append(GArrowUInt8ArrayBuilder *builder,
                                  guint8 value,
                                  GError **error)
{
  return garrow_uint8_array_builder_append_value(builder, value, error);
}

/**
 * garrow_uint8_array_builder_append_value:
 * @builder: A #GArrowUInt8ArrayBuilder.
 * @value: An uint8 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_uint8_array_builder_append_value(GArrowUInt8ArrayBuilder *builder,
                                  guint8 value,
                                  GError **error)
{
  return garrow_array_builder_append_value<arrow::UInt8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[uint8-array-builder][append-value]");
}

/**
 * garrow_uint8_array_builder_append_values:
 * @builder: A #GArrowUInt8ArrayBuilder.
 * @values: (array length=values_length): The array of uint8.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint8_array_builder_append_values(GArrowUInt8ArrayBuilder *builder,
                                         const guint8 *values,
                                         gint64 values_length,
                                         const gboolean *is_valids,
                                         gint64 is_valids_length,
                                         GError **error)
{
  return garrow_array_builder_append_values<arrow::UInt8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[uint8-array-builder][append-values]");
}

/**
 * garrow_uint8_array_builder_append_null:
 * @builder: A #GArrowUInt8ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_uint8_array_builder_append_null(GArrowUInt8ArrayBuilder *builder,
                                       GError **error)
{
  return garrow_array_builder_append_null<arrow::UInt8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[uint8-array-builder][append-null]");
}

/**
 * garrow_uint8_array_builder_append_nulls:
 * @builder: A #GArrowUInt8ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint8_array_builder_append_nulls(GArrowUInt8ArrayBuilder *builder,
                                        gint64 n,
                                        GError **error)
{
  return garrow_array_builder_append_nulls<arrow::UInt8Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[uint8-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowInt16ArrayBuilder,
              garrow_int16_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_int16_array_builder_init(GArrowInt16ArrayBuilder *builder)
{
}

static void
garrow_int16_array_builder_class_init(GArrowInt16ArrayBuilderClass *klass)
{
}

/**
 * garrow_int16_array_builder_new:
 *
 * Returns: A newly created #GArrowInt16ArrayBuilder.
 */
GArrowInt16ArrayBuilder *
garrow_int16_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::int16(),
                                          NULL,
                                          "[int16-array-builder][new]");
  return GARROW_INT16_ARRAY_BUILDER(builder);
}

/**
 * garrow_int16_array_builder_append:
 * @builder: A #GArrowInt16ArrayBuilder.
 * @value: A int16 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_int16_array_builder_append_value() instead.
 */
gboolean
garrow_int16_array_builder_append(GArrowInt16ArrayBuilder *builder,
                                  gint16 value,
                                  GError **error)
{
  return garrow_int16_array_builder_append_value(builder, value, error);
}

/**
 * garrow_int16_array_builder_append_value:
 * @builder: A #GArrowInt16ArrayBuilder.
 * @value: A int16 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_int16_array_builder_append_value(GArrowInt16ArrayBuilder *builder,
                                        gint16 value,
                                        GError **error)
{
  return garrow_array_builder_append_value<arrow::Int16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[int16-array-builder][append-value]");
}

/**
 * garrow_int16_array_builder_append_values:
 * @builder: A #GArrowInt16ArrayBuilder.
 * @values: (array length=values_length): The array of int16.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int16_array_builder_append_values(GArrowInt16ArrayBuilder *builder,
                                         const gint16 *values,
                                         gint64 values_length,
                                         const gboolean *is_valids,
                                         gint64 is_valids_length,
                                         GError **error)
{
  return garrow_array_builder_append_values<arrow::Int16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[int16-array-builder][append-values]");
}

/**
 * garrow_int16_array_builder_append_null:
 * @builder: A #GArrowInt16ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_int16_array_builder_append_null(GArrowInt16ArrayBuilder *builder,
                                       GError **error)
{
  return garrow_array_builder_append_null<arrow::Int16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[int16-array-builder][append-null]");
}

/**
 * garrow_int16_array_builder_append_nulls:
 * @builder: A #GArrowInt16ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int16_array_builder_append_nulls(GArrowInt16ArrayBuilder *builder,
                                        gint64 n,
                                        GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Int16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[int16-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowUInt16ArrayBuilder,
              garrow_uint16_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_uint16_array_builder_init(GArrowUInt16ArrayBuilder *builder)
{
}

static void
garrow_uint16_array_builder_class_init(GArrowUInt16ArrayBuilderClass *klass)
{
}

/**
 * garrow_uint16_array_builder_new:
 *
 * Returns: A newly created #GArrowUInt16ArrayBuilder.
 */
GArrowUInt16ArrayBuilder *
garrow_uint16_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::uint16(),
                                          NULL,
                                          "[uint16-array-builder][new]");
  return GARROW_UINT16_ARRAY_BUILDER(builder);
}

/**
 * garrow_uint16_array_builder_append:
 * @builder: A #GArrowUInt16ArrayBuilder.
 * @value: An uint16 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_uint16_array_builder_append_value() instead.
 */
gboolean
garrow_uint16_array_builder_append(GArrowUInt16ArrayBuilder *builder,
                                   guint16 value,
                                   GError **error)
{
  return garrow_uint16_array_builder_append_value(builder, value, error);
}

/**
 * garrow_uint16_array_builder_append_value:
 * @builder: A #GArrowUInt16ArrayBuilder.
 * @value: An uint16 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_uint16_array_builder_append_value(GArrowUInt16ArrayBuilder *builder,
                                         guint16 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::UInt16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[uint16-array-builder][append-value]");
}

/**
 * garrow_uint16_array_builder_append_values:
 * @builder: A #GArrowUInt16ArrayBuilder.
 * @values: (array length=values_length): The array of uint16.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint16_array_builder_append_values(GArrowUInt16ArrayBuilder *builder,
                                          const guint16 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::UInt16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[uint16-array-builder][append-values]");
}

/**
 * garrow_uint16_array_builder_append_null:
 * @builder: A #GArrowUInt16ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_uint16_array_builder_append_null(GArrowUInt16ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::UInt16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[uint16-array-builder][append-null]");
}

/**
 * garrow_uint16_array_builder_append_nulls:
 * @builder: A #GArrowUInt16ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint16_array_builder_append_nulls(GArrowUInt16ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::UInt16Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[uint16-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowInt32ArrayBuilder,
              garrow_int32_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_int32_array_builder_init(GArrowInt32ArrayBuilder *builder)
{
}

static void
garrow_int32_array_builder_class_init(GArrowInt32ArrayBuilderClass *klass)
{
}

/**
 * garrow_int32_array_builder_new:
 *
 * Returns: A newly created #GArrowInt32ArrayBuilder.
 */
GArrowInt32ArrayBuilder *
garrow_int32_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::int32(),
                                          NULL,
                                          "[int32-array-builder][new]");
  return GARROW_INT32_ARRAY_BUILDER(builder);
}

/**
 * garrow_int32_array_builder_append:
 * @builder: A #GArrowInt32ArrayBuilder.
 * @value: A int32 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_int32_array_builder_append_value() instead.
 */
gboolean
garrow_int32_array_builder_append(GArrowInt32ArrayBuilder *builder,
                                  gint32 value,
                                  GError **error)
{
  return garrow_int32_array_builder_append_value(builder, value, error);
}

/**
 * garrow_int32_array_builder_append_value:
 * @builder: A #GArrowInt32ArrayBuilder.
 * @value: A int32 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_int32_array_builder_append_value(GArrowInt32ArrayBuilder *builder,
                                        gint32 value,
                                        GError **error)
{
  return garrow_array_builder_append_value<arrow::Int32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[int32-array-builder][append-value]");
}

/**
 * garrow_int32_array_builder_append_values:
 * @builder: A #GArrowInt32ArrayBuilder.
 * @values: (array length=values_length): The array of int32.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int32_array_builder_append_values(GArrowInt32ArrayBuilder *builder,
                                         const gint32 *values,
                                         gint64 values_length,
                                         const gboolean *is_valids,
                                         gint64 is_valids_length,
                                         GError **error)
{
  return garrow_array_builder_append_values<arrow::Int32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[int32-array-builder][append-values]");
}

/**
 * garrow_int32_array_builder_append_null:
 * @builder: A #GArrowInt32ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_int32_array_builder_append_null(GArrowInt32ArrayBuilder *builder,
                                       GError **error)
{
  return garrow_array_builder_append_null<arrow::Int32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[int32-array-builder][append-null]");
}

/**
 * garrow_int32_array_builder_append_nulls:
 * @builder: A #GArrowInt32ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int32_array_builder_append_nulls(GArrowInt32ArrayBuilder *builder,
                                        gint64 n,
                                        GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Int32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[int32-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowUInt32ArrayBuilder,
              garrow_uint32_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_uint32_array_builder_init(GArrowUInt32ArrayBuilder *builder)
{
}

static void
garrow_uint32_array_builder_class_init(GArrowUInt32ArrayBuilderClass *klass)
{
}

/**
 * garrow_uint32_array_builder_new:
 *
 * Returns: A newly created #GArrowUInt32ArrayBuilder.
 */
GArrowUInt32ArrayBuilder *
garrow_uint32_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::uint32(),
                                          NULL,
                                          "[uint32-array-builder][new]");
  return GARROW_UINT32_ARRAY_BUILDER(builder);
}

/**
 * garrow_uint32_array_builder_append:
 * @builder: A #GArrowUInt32ArrayBuilder.
 * @value: An uint32 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_uint32_array_builder_append_value() instead.
 */
gboolean
garrow_uint32_array_builder_append(GArrowUInt32ArrayBuilder *builder,
                                   guint32 value,
                                   GError **error)
{
  return garrow_uint32_array_builder_append_value(builder, value, error);
}

/**
 * garrow_uint32_array_builder_append_value:
 * @builder: A #GArrowUInt32ArrayBuilder.
 * @value: An uint32 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_uint32_array_builder_append_value(GArrowUInt32ArrayBuilder *builder,
                                         guint32 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::UInt32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[uint32-array-builder][append-value]");
}

/**
 * garrow_uint32_array_builder_append_values:
 * @builder: A #GArrowUInt32ArrayBuilder.
 * @values: (array length=values_length): The array of uint32.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint32_array_builder_append_values(GArrowUInt32ArrayBuilder *builder,
                                          const guint32 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::UInt32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[uint32-array-builder][append-values]");
}

/**
 * garrow_uint32_array_builder_append_null:
 * @builder: A #GArrowUInt32ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_uint32_array_builder_append_null(GArrowUInt32ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::UInt32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[uint32-array-builder][append-null]");
}

/**
 * garrow_uint32_array_builder_append_nulls:
 * @builder: A #GArrowUInt32ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint32_array_builder_append_nulls(GArrowUInt32ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::UInt32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[uint32-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowInt64ArrayBuilder,
              garrow_int64_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_int64_array_builder_init(GArrowInt64ArrayBuilder *builder)
{
}

static void
garrow_int64_array_builder_class_init(GArrowInt64ArrayBuilderClass *klass)
{
}

/**
 * garrow_int64_array_builder_new:
 *
 * Returns: A newly created #GArrowInt64ArrayBuilder.
 */
GArrowInt64ArrayBuilder *
garrow_int64_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::int64(),
                                          NULL,
                                          "[int64-array-builder][new]");
  return GARROW_INT64_ARRAY_BUILDER(builder);
}

/**
 * garrow_int64_array_builder_append:
 * @builder: A #GArrowInt64ArrayBuilder.
 * @value: A int64 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_int64_array_builder_append_value() instead.
 */
gboolean
garrow_int64_array_builder_append(GArrowInt64ArrayBuilder *builder,
                                  gint64 value,
                                  GError **error)
{
  return garrow_int64_array_builder_append_value(builder, value, error);
}

/**
 * garrow_int64_array_builder_append_value:
 * @builder: A #GArrowInt64ArrayBuilder.
 * @value: A int64 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_int64_array_builder_append_value(GArrowInt64ArrayBuilder *builder,
                                        gint64 value,
                                        GError **error)
{
  return garrow_array_builder_append_value<arrow::Int64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[int64-array-builder][append-value]");
}

/**
 * garrow_int64_array_builder_append_values:
 * @builder: A #GArrowInt64ArrayBuilder.
 * @values: (array length=values_length): The array of int64.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int64_array_builder_append_values(GArrowInt64ArrayBuilder *builder,
                                         const gint64 *values,
                                         gint64 values_length,
                                         const gboolean *is_valids,
                                         gint64 is_valids_length,
                                         GError **error)
{
  return garrow_array_builder_append_values<arrow::Int64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const int64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[int64-array-builder][append-values]");
}

/**
 * garrow_int64_array_builder_append_null:
 * @builder: A #GArrowInt64ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_int64_array_builder_append_null(GArrowInt64ArrayBuilder *builder,
                                       GError **error)
{
  return garrow_array_builder_append_null<arrow::Int64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[int64-array-builder][append-null]");
}

/**
 * garrow_int64_array_builder_append_nulls:
 * @builder: A #GArrowInt64ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_int64_array_builder_append_nulls(GArrowInt64ArrayBuilder *builder,
                                        gint64 n,
                                        GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Int64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[int64-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowUInt64ArrayBuilder,
              garrow_uint64_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_uint64_array_builder_init(GArrowUInt64ArrayBuilder *builder)
{
}

static void
garrow_uint64_array_builder_class_init(GArrowUInt64ArrayBuilderClass *klass)
{
}

/**
 * garrow_uint64_array_builder_new:
 *
 * Returns: A newly created #GArrowUInt64ArrayBuilder.
 */
GArrowUInt64ArrayBuilder *
garrow_uint64_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::uint64(),
                                          NULL,
                                          "[uint64-array-builder][new]");
  return GARROW_UINT64_ARRAY_BUILDER(builder);
}

/**
 * garrow_uint64_array_builder_append:
 * @builder: A #GArrowUInt64ArrayBuilder.
 * @value: An uint64 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_uint64_array_builder_append_value() instead.
 */
gboolean
garrow_uint64_array_builder_append(GArrowUInt64ArrayBuilder *builder,
                                  guint64 value,
                                  GError **error)
{
  return garrow_uint64_array_builder_append_value(builder, value, error);
}

/**
 * garrow_uint64_array_builder_append_value:
 * @builder: A #GArrowUInt64ArrayBuilder.
 * @value: An uint64 value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_uint64_array_builder_append_value(GArrowUInt64ArrayBuilder *builder,
                                         guint64 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::UInt64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[uint64-array-builder][append-value]");
}

/**
 * garrow_uint64_array_builder_append_values:
 * @builder: A #GArrowUInt64ArrayBuilder.
 * @values: (array length=values_length): The array of uint64.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint64_array_builder_append_values(GArrowUInt64ArrayBuilder *builder,
                                          const guint64 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::UInt64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const uint64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[uint64-array-builder][append-values]");
}

/**
 * garrow_uint64_array_builder_append_null:
 * @builder: A #GArrowUInt64ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_uint64_array_builder_append_null(GArrowUInt64ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::UInt64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[uint64-array-builder][append-null]");
}

/**
 * garrow_uint64_array_builder_append_nulls:
 * @builder: A #GArrowUInt64ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_uint64_array_builder_append_nulls(GArrowUInt64ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::UInt64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[uint64-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowFloatArrayBuilder,
              garrow_float_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_float_array_builder_init(GArrowFloatArrayBuilder *builder)
{
}

static void
garrow_float_array_builder_class_init(GArrowFloatArrayBuilderClass *klass)
{
}

/**
 * garrow_float_array_builder_new:
 *
 * Returns: A newly created #GArrowFloatArrayBuilder.
 */
GArrowFloatArrayBuilder *
garrow_float_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::float32(),
                                          NULL,
                                          "[float-array-builder][new]");
  return GARROW_FLOAT_ARRAY_BUILDER(builder);
}

/**
 * garrow_float_array_builder_append:
 * @builder: A #GArrowFloatArrayBuilder.
 * @value: A float value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_float_array_builder_append_value() instead.
 */
gboolean
garrow_float_array_builder_append(GArrowFloatArrayBuilder *builder,
                                  gfloat value,
                                  GError **error)
{
  return garrow_float_array_builder_append_value(builder, value, error);
}

/**
 * garrow_float_array_builder_append_value:
 * @builder: A #GArrowFloatArrayBuilder.
 * @value: A float value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_float_array_builder_append_value(GArrowFloatArrayBuilder *builder,
                                        gfloat value,
                                        GError **error)
{
  return garrow_array_builder_append_value<arrow::FloatBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[float-array-builder][append-value]");
}

/**
 * garrow_float_array_builder_append_values:
 * @builder: A #GArrowFloatArrayBuilder.
 * @values: (array length=values_length): The array of float.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_float_array_builder_append_values(GArrowFloatArrayBuilder *builder,
                                         const gfloat *values,
                                         gint64 values_length,
                                         const gboolean *is_valids,
                                         gint64 is_valids_length,
                                         GError **error)
{
  return garrow_array_builder_append_values<arrow::FloatBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[float-array-builder][append-values]");
}

/**
 * garrow_float_array_builder_append_null:
 * @builder: A #GArrowFloatArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_float_array_builder_append_null(GArrowFloatArrayBuilder *builder,
                                       GError **error)
{
  return garrow_array_builder_append_null<arrow::FloatBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[float-array-builder][append-null]");
}

/**
 * garrow_float_array_builder_append_nulls:
 * @builder: A #GArrowFloatArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_float_array_builder_append_nulls(GArrowFloatArrayBuilder *builder,
                                        gint64 n,
                                        GError **error)
{
  return garrow_array_builder_append_nulls<arrow::FloatBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[float-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowDoubleArrayBuilder,
              garrow_double_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_double_array_builder_init(GArrowDoubleArrayBuilder *builder)
{
}

static void
garrow_double_array_builder_class_init(GArrowDoubleArrayBuilderClass *klass)
{
}

/**
 * garrow_double_array_builder_new:
 *
 * Returns: A newly created #GArrowDoubleArrayBuilder.
 */
GArrowDoubleArrayBuilder *
garrow_double_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::float64(),
                                          NULL,
                                          "[double-array-builder][new]");
  return GARROW_DOUBLE_ARRAY_BUILDER(builder);
}

/**
 * garrow_double_array_builder_append:
 * @builder: A #GArrowDoubleArrayBuilder.
 * @value: A double value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_double_array_builder_append_value() instead.
 */
gboolean
garrow_double_array_builder_append(GArrowDoubleArrayBuilder *builder,
                                   gdouble value,
                                   GError **error)
{
  return garrow_double_array_builder_append_value(builder, value, error);
}

/**
 * garrow_double_array_builder_append_value:
 * @builder: A #GArrowDoubleArrayBuilder.
 * @value: A double value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_double_array_builder_append_value(GArrowDoubleArrayBuilder *builder,
                                         gdouble value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::DoubleBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[double-array-builder][append-value]");
}

/**
 * garrow_double_array_builder_append_values:
 * @builder: A #GArrowDoubleArrayBuilder.
 * @values: (array length=values_length): The array of double.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_double_array_builder_append_values(GArrowDoubleArrayBuilder *builder,
                                          const gdouble *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::DoubleBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[double-array-builder][append-values]");
}

/**
 * garrow_double_array_builder_append_null:
 * @builder: A #GArrowDoubleArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_double_array_builder_append_null(GArrowDoubleArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::DoubleBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[double-array-builder][append-null]");
}

/**
 * garrow_double_array_builder_append_nulls:
 * @builder: A #GArrowDoubleArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_double_array_builder_append_nulls(GArrowDoubleArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::DoubleBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[double-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowBinaryArrayBuilder,
              garrow_binary_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_binary_array_builder_init(GArrowBinaryArrayBuilder *builder)
{
}

static void
garrow_binary_array_builder_class_init(GArrowBinaryArrayBuilderClass *klass)
{
}

/**
 * garrow_binary_array_builder_new:
 *
 * Returns: A newly created #GArrowBinaryArrayBuilder.
 */
GArrowBinaryArrayBuilder *
garrow_binary_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::binary(),
                                          NULL,
                                          "[binary-array-builder][new]");
  return GARROW_BINARY_ARRAY_BUILDER(builder);
}

/**
 * garrow_binary_array_builder_append:
 * @builder: A #GArrowBinaryArrayBuilder.
 * @value: (array length=length): A binary value.
 * @length: A value length.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_binary_array_builder_append_value() instead.
 */
gboolean
garrow_binary_array_builder_append(GArrowBinaryArrayBuilder *builder,
                                   const guint8 *value,
                                   gint32 length,
                                   GError **error)
{
  return garrow_binary_array_builder_append_value(builder, value, length, error);
}

/**
 * garrow_binary_array_builder_append_value:
 * @builder: A #GArrowBinaryArrayBuilder.
 * @value: (array length=length): A binary value.
 * @length: A value length.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_binary_array_builder_append_value(GArrowBinaryArrayBuilder *builder,
                                         const guint8 *value,
                                         gint32 length,
                                         GError **error)
{
  auto arrow_builder =
    static_cast<arrow::BinaryBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append(value, length);
  return garrow_error_check(error,
                            status,
                            "[binary-array-builder][append-value]");
}

/**
 * garrow_binary_array_builder_append_value_bytes:
 * @builder: A #GArrowBinaryArrayBuilder.
 * @value: A binary value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_binary_array_builder_append_value_bytes(GArrowBinaryArrayBuilder *builder,
                                               GBytes *value,
                                               GError **error)
{
  auto arrow_builder =
    static_cast<arrow::BinaryBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  gsize size;
  gconstpointer data = g_bytes_get_data(value, &size);
  auto status = arrow_builder->Append(static_cast<const uint8_t *>(data),
                                      size);
  return garrow_error_check(error,
                            status,
                            "[binary-array-builder][append-value-bytes]");
}

/**
 * garrow_binary_array_builder_append_values:
 * @builder: A #GArrowLargeBinaryArrayBuilder.
 * @values: (array length=values_length): The array of #GBytes.
 * @values_length: The length of @values.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth @is_valids is %TRUE, the Nth @values is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of @is_valids.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_binary_array_builder_append_values(GArrowBinaryArrayBuilder *builder,
                                          GBytes **values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::BinaryBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[binary-array-builder][append-values]");
}

/**
 * garrow_binary_array_builder_append_null:
 * @builder: A #GArrowBinaryArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
garrow_binary_array_builder_append_null(GArrowBinaryArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::BinaryBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[binary-array-builder][append-null]");
}

/**
 * garrow_binary_array_builder_append_nulls:
 * @builder: A #GArrowBinaryArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_binary_array_builder_append_nulls(GArrowBinaryArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::BinaryBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[binary-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowLargeBinaryArrayBuilder,
              garrow_large_binary_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_large_binary_array_builder_init(GArrowLargeBinaryArrayBuilder *builder)
{
}

static void
garrow_large_binary_array_builder_class_init(GArrowLargeBinaryArrayBuilderClass *klass)
{
}

/**
 * garrow_large_binary_array_builder_new:
 *
 * Returns: A newly created #GArrowLargeBinaryArrayBuilder.
 *
 * Since: 0.16.0
 */
GArrowLargeBinaryArrayBuilder *
garrow_large_binary_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::large_binary(),
                                          NULL,
                                          "[large-binary-array-builder][new]");
  return GARROW_LARGE_BINARY_ARRAY_BUILDER(builder);
}

/**
 * garrow_large_binary_array_builder_append_value:
 * @builder: A #GArrowLargeBinaryArrayBuilder.
 * @value: (array length=length): A binary value.
 * @length: A value length.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_binary_array_builder_append_value(GArrowLargeBinaryArrayBuilder *builder,
                                               const guint8 *value,
                                               gint64 length,
                                               GError **error)
{
  auto arrow_builder =
    static_cast<arrow::LargeBinaryBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append(value, length);
  return garrow_error_check(error,
                            status,
                            "[large-binary-array-builder][append-value]");
}

/**
 * garrow_large_binary_array_builder_append_value_bytes:
 * @builder: A #GArrowLargeBinaryArrayBuilder.
 * @value: A binary value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_binary_array_builder_append_value_bytes(GArrowLargeBinaryArrayBuilder *builder,
                                                     GBytes *value,
                                                     GError **error)
{
  auto arrow_builder =
    static_cast<arrow::LargeBinaryBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  gsize size;
  gconstpointer data = g_bytes_get_data(value, &size);
  auto status = arrow_builder->Append(static_cast<const uint8_t *>(data),
                                      size);
  return garrow_error_check(error,
                            status,
                            "[large-binary-array-builder][append-value-bytes]");
}

/**
 * garrow_large_binary_array_builder_append_values:
 * @builder: A #GArrowLargeBinaryArrayBuilder.
 * @values: (array length=values_length): The array of #GBytes.
 * @values_length: The length of @values.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth @is_valids is %TRUE, the Nth @values is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of @is_valids.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_binary_array_builder_append_values(GArrowLargeBinaryArrayBuilder *builder,
                                                GBytes **values,
                                                gint64 values_length,
                                                const gboolean *is_valids,
                                                gint64 is_valids_length,
                                                GError **error)
{
  return garrow_array_builder_append_values<arrow::LargeBinaryBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[large-binary-array-builder][append-values]");
}

/**
 * garrow_large_binary_array_builder_append_null:
 * @builder: A #GArrowLargeBinaryArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_binary_array_builder_append_null(GArrowLargeBinaryArrayBuilder *builder,
                                              GError **error)
{
  return garrow_array_builder_append_null<arrow::LargeBinaryBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[large-binary-array-builder][append-null]");
}

/**
 * garrow_large_binary_array_builder_append_nulls:
 * @builder: A #GArrowLargeBinaryArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_binary_array_builder_append_nulls(GArrowLargeBinaryArrayBuilder *builder,
                                               gint64 n,
                                               GError **error)
{
  return garrow_array_builder_append_nulls<arrow::LargeBinaryBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[large-binary-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowStringArrayBuilder,
              garrow_string_array_builder,
              GARROW_TYPE_BINARY_ARRAY_BUILDER)

static void
garrow_string_array_builder_init(GArrowStringArrayBuilder *builder)
{
}

static void
garrow_string_array_builder_class_init(GArrowStringArrayBuilderClass *klass)
{
}

/**
 * garrow_string_array_builder_new:
 *
 * Returns: A newly created #GArrowStringArrayBuilder.
 */
GArrowStringArrayBuilder *
garrow_string_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::utf8(),
                                          NULL,
                                          "[string-array-builder][new]");
  return GARROW_STRING_ARRAY_BUILDER(builder);
}

/**
 * garrow_string_array_builder_append:
 * @builder: A #GArrowStringArrayBuilder.
 * @value: A string value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Deprecated: 0.12.0:
 *   Use garrow_string_array_builder_append_value() instead.
 */
gboolean
garrow_string_array_builder_append(GArrowStringArrayBuilder *builder,
                                   const gchar *value,
                                   GError **error)
{
  return garrow_string_array_builder_append_string(builder, value, error);
}

/**
 * garrow_string_array_builder_append_value: (skip)
 * @builder: A #GArrowStringArrayBuilder.
 * @value: A string value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 *
 * Deprecated: 1.0.0:
 *   Use garrow_string_array_builder_append_string() instead.
 */
gboolean
garrow_string_array_builder_append_value(GArrowStringArrayBuilder *builder,
                                         const gchar *value,
                                         GError **error)
{
  return garrow_string_array_builder_append_string(builder, value, error);
}

/**
 * garrow_string_array_builder_append_string:
 * @builder: A #GArrowStringArrayBuilder.
 * @value: A string value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_string_array_builder_append_string(GArrowStringArrayBuilder *builder,
                                          const gchar *value,
                                          GError **error)
{
  auto arrow_builder =
    static_cast<arrow::StringBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append(value,
                                      static_cast<gint32>(strlen(value)));
  return garrow_error_check(error,
                            status,
                            "[string-array-builder][append-string]");
}

/**
 * garrow_string_array_builder_append_values: (skip)
 * @builder: A #GArrowStringArrayBuilder.
 * @values: (array length=values_length): The array of strings.
 * @values_length: The length of @values.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth @is_valids is %TRUE, the Nth @values is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of @is_valids.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.10.0
 *
 * Deprecated: 1.0.0:
 *   Use garrow_string_array_builder_append_strings() instead.
 */
gboolean
garrow_string_array_builder_append_values(GArrowStringArrayBuilder *builder,
                                          const gchar **values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_string_array_builder_append_strings(builder,
                                                    values,
                                                    values_length,
                                                    is_valids,
                                                    is_valids_length,
                                                    error);
}

/**
 * garrow_string_array_builder_append_strings:
 * @builder: A #GArrowStringArrayBuilder.
 * @values: (array length=values_length): The array of strings.
 * @values_length: The length of @values.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth @is_valids is %TRUE, the Nth @values is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of @is_valids.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_string_array_builder_append_strings(GArrowStringArrayBuilder *builder,
                                           const gchar **values,
                                           gint64 values_length,
                                           const gboolean *is_valids,
                                           gint64 is_valids_length,
                                           GError **error)
{
  return garrow_array_builder_append_values<arrow::StringBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[string-array-builder][append-strings]");
}


G_DEFINE_TYPE(GArrowLargeStringArrayBuilder,
              garrow_large_string_array_builder,
              GARROW_TYPE_LARGE_BINARY_ARRAY_BUILDER)

static void
garrow_large_string_array_builder_init(GArrowLargeStringArrayBuilder *builder)
{
}

static void
garrow_large_string_array_builder_class_init(GArrowLargeStringArrayBuilderClass *klass)
{
}

/**
 * garrow_large_string_array_builder_new:
 *
 * Returns: A newly created #GArrowLargeStringArrayBuilder.
 *
 * Since: 0.16.0
 */
GArrowLargeStringArrayBuilder *
garrow_large_string_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::large_utf8(),
                                          NULL,
                                          "[large-string-array-builder][new]");
  return GARROW_LARGE_STRING_ARRAY_BUILDER(builder);
}

/**
 * garrow_large_string_array_builder_append_string:
 * @builder: A #GArrowLargeStringArrayBuilder.
 * @value: A string value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_string_array_builder_append_string(GArrowLargeStringArrayBuilder *builder,
                                                const gchar *value,
                                                GError **error)
{
  auto arrow_builder =
    static_cast<arrow::LargeStringBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));
  auto status = arrow_builder->Append(value);
  return garrow_error_check(error,
                            status,
                            "[large-string-array-builder][append-string]");
}

/**
 * garrow_large_string_array_builder_append_strings:
 * @builder: A #GArrowLargeStringArrayBuilder.
 * @values: (array length=values_length): The array of strings.
 * @values_length: The length of @values.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth @is_valids is %TRUE, the Nth @values is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of @is_valids.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_string_array_builder_append_strings(GArrowLargeStringArrayBuilder *builder,
                                                 const gchar **values,
                                                 gint64 values_length,
                                                 const gboolean *is_valids,
                                                 gint64 is_valids_length,
                                                 GError **error)
{
  return garrow_array_builder_append_values<arrow::LargeStringBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[large-string-array-builder][append-strings]");
}


G_DEFINE_TYPE(GArrowDate32ArrayBuilder,
              garrow_date32_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_date32_array_builder_init(GArrowDate32ArrayBuilder *builder)
{
}

static void
garrow_date32_array_builder_class_init(GArrowDate32ArrayBuilderClass *klass)
{
}

/**
 * garrow_date32_array_builder_new:
 *
 * Returns: A newly created #GArrowDate32ArrayBuilder.
 *
 * Since: 0.7.0
 */
GArrowDate32ArrayBuilder *
garrow_date32_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::date32(),
                                          NULL,
                                          "[date32-array-builder][new]");
  return GARROW_DATE32_ARRAY_BUILDER(builder);
}

/**
 * garrow_date32_array_builder_append:
 * @builder: A #GArrowDate32ArrayBuilder.
 * @value: The number of days since UNIX epoch in signed 32bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_date32_array_builder_append_value() instead.
 */
gboolean
garrow_date32_array_builder_append(GArrowDate32ArrayBuilder *builder,
                                   gint32 value,
                                   GError **error)
{
  return garrow_date32_array_builder_append_value(builder, value, error);
}

/**
 * garrow_date32_array_builder_append_value:
 * @builder: A #GArrowDate32ArrayBuilder.
 * @value: The number of days since UNIX epoch in signed 32bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_date32_array_builder_append_value(GArrowDate32ArrayBuilder *builder,
                                         gint32 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::Date32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[date32-array-builder][append-value]");
}

/**
 * garrow_date32_array_builder_append_values:
 * @builder: A #GArrowDate32ArrayBuilder.
 * @values: (array length=values_length): The array of
 *   the number of days since UNIX epoch in signed 32bit integer.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_date32_array_builder_append_values(GArrowDate32ArrayBuilder *builder,
                                          const gint32 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::Date32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[date32-array-builder][append-values]");
}

/**
 * garrow_date32_array_builder_append_null:
 * @builder: A #GArrowDate32ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 */
gboolean
garrow_date32_array_builder_append_null(GArrowDate32ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::Date32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[date32-array-builder][append-null]");
}

/**
 * garrow_date32_array_builder_append_nulls:
 * @builder: A #GArrowDate32ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_date32_array_builder_append_nulls(GArrowDate32ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Date32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[date32-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowDate64ArrayBuilder,
              garrow_date64_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_date64_array_builder_init(GArrowDate64ArrayBuilder *builder)
{
}

static void
garrow_date64_array_builder_class_init(GArrowDate64ArrayBuilderClass *klass)
{
}

/**
 * garrow_date64_array_builder_new:
 *
 * Returns: A newly created #GArrowDate64ArrayBuilder.
 *
 * Since: 0.7.0
 */
GArrowDate64ArrayBuilder *
garrow_date64_array_builder_new(void)
{
  auto builder = garrow_array_builder_new(arrow::date64(),
                                          NULL,
                                          "[date64-array-builder][new]");
  return GARROW_DATE64_ARRAY_BUILDER(builder);
}

/**
 * garrow_date64_array_builder_append:
 * @builder: A #GArrowDate64ArrayBuilder.
 * @value: The number of milliseconds since UNIX epoch in signed 64bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_date64_array_builder_append_value() instead.
 */
gboolean
garrow_date64_array_builder_append(GArrowDate64ArrayBuilder *builder,
                                   gint64 value,
                                   GError **error)
{
  return garrow_date64_array_builder_append_value(builder, value, error);
}

/**
 * garrow_date64_array_builder_append_value:
 * @builder: A #GArrowDate64ArrayBuilder.
 * @value: The number of milliseconds since UNIX epoch in signed 64bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_date64_array_builder_append_value(GArrowDate64ArrayBuilder *builder,
                                         gint64 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::Date64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[date64-array-builder][append-value]");
}

/**
 * garrow_date64_array_builder_append_values:
 * @builder: A #GArrowDate64ArrayBuilder.
 * @values: (array length=values_length): The array of
 *   the number of milliseconds since UNIX epoch in signed 64bit integer.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_date64_array_builder_append_values(GArrowDate64ArrayBuilder *builder,
                                          const gint64 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::Date64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const int64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[date64-array-builder][append-values]");
}

/**
 * garrow_date64_array_builder_append_null:
 * @builder: A #GArrowDate64ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 */
gboolean
garrow_date64_array_builder_append_null(GArrowDate64ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::Date64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[date64-array-builder][append-null]");
}

/**
 * garrow_date64_array_builder_append_nulls:
 * @builder: A #GArrowDate64ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_date64_array_builder_append_nulls(GArrowDate64ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Date64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[date64-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowTimestampArrayBuilder,
              garrow_timestamp_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_timestamp_array_builder_init(GArrowTimestampArrayBuilder *builder)
{
}

static void
garrow_timestamp_array_builder_class_init(GArrowTimestampArrayBuilderClass *klass)
{
}

/**
 * garrow_timestamp_array_builder_new:
 * @data_type: A #GArrowTimestampDataType.
 *
 * Returns: A newly created #GArrowTimestampArrayBuilder.
 *
 * Since: 0.7.0
 */
GArrowTimestampArrayBuilder *
garrow_timestamp_array_builder_new(GArrowTimestampDataType *data_type)
{
  auto arrow_data_type = garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          NULL,
                                          "[timestamp-array-builder][new]");
  return GARROW_TIMESTAMP_ARRAY_BUILDER(builder);
}

/**
 * garrow_timestamp_array_builder_append:
 * @builder: A #GArrowTimestampArrayBuilder.
 * @value: The number of milliseconds since UNIX epoch in signed 64bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_timestamp_array_builder_append_value() instead.
 */
gboolean
garrow_timestamp_array_builder_append(GArrowTimestampArrayBuilder *builder,
                                      gint64 value,
                                      GError **error)
{
  return garrow_timestamp_array_builder_append_value(builder, value, error);
}

/**
 * garrow_timestamp_array_builder_append_value:
 * @builder: A #GArrowTimestampArrayBuilder.
 * @value: The number of milliseconds since UNIX epoch in signed 64bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_timestamp_array_builder_append_value(GArrowTimestampArrayBuilder *builder,
                                            gint64 value,
                                            GError **error)
{
  return garrow_array_builder_append_value<arrow::TimestampBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[timestamp-array-builder][append-value]");
}

/**
 * garrow_timestamp_array_builder_append_values:
 * @builder: A #GArrowTimestampArrayBuilder.
 * @values: (array length=values_length): The array of
 *   the number of milliseconds since UNIX epoch in signed 64bit integer.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_timestamp_array_builder_append_values(GArrowTimestampArrayBuilder *builder,
                                             const gint64 *values,
                                             gint64 values_length,
                                             const gboolean *is_valids,
                                             gint64 is_valids_length,
                                             GError **error)
{
  return garrow_array_builder_append_values<arrow::TimestampBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const int64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[timestamp-array-builder][append-values]");
}

/**
 * garrow_timestamp_array_builder_append_null:
 * @builder: A #GArrowTimestampArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 */
gboolean
garrow_timestamp_array_builder_append_null(GArrowTimestampArrayBuilder *builder,
                                           GError **error)
{
  return garrow_array_builder_append_null<arrow::TimestampBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[timestamp-array-builder][append-null]");
}

/**
 * garrow_timestamp_array_builder_append_nulls:
 * @builder: A #GArrowTimestampArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_timestamp_array_builder_append_nulls(GArrowTimestampArrayBuilder *builder,
                                            gint64 n,
                                            GError **error)
{
  return garrow_array_builder_append_nulls<arrow::TimestampBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[timestamp-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowTime32ArrayBuilder,
              garrow_time32_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_time32_array_builder_init(GArrowTime32ArrayBuilder *builder)
{
}

static void
garrow_time32_array_builder_class_init(GArrowTime32ArrayBuilderClass *klass)
{
}

/**
 * garrow_time32_array_builder_new:
 * @data_type: A #GArrowTime32DataType.
 *
 * Returns: A newly created #GArrowTime32ArrayBuilder.
 *
 * Since: 0.7.0
 */
GArrowTime32ArrayBuilder *
garrow_time32_array_builder_new(GArrowTime32DataType *data_type)
{
  auto arrow_data_type = garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          NULL,
                                          "[time32-array-builder][new]");
  return GARROW_TIME32_ARRAY_BUILDER(builder);
}

/**
 * garrow_time32_array_builder_append:
 * @builder: A #GArrowTime32ArrayBuilder.
 * @value: The number of days since UNIX epoch in signed 32bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_time32_array_builder_append_value() instead.
 */
gboolean
garrow_time32_array_builder_append(GArrowTime32ArrayBuilder *builder,
                                   gint32 value,
                                   GError **error)
{
  return garrow_time32_array_builder_append_value(builder, value, error);
}

/**
 * garrow_time32_array_builder_append_value:
 * @builder: A #GArrowTime32ArrayBuilder.
 * @value: The number of days since UNIX epoch in signed 32bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_time32_array_builder_append_value(GArrowTime32ArrayBuilder *builder,
                                         gint32 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::Time32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[time32-array-builder][append-value]");
}

/**
 * garrow_time32_array_builder_append_values:
 * @builder: A #GArrowTime32ArrayBuilder.
 * @values: (array length=values_length): The array of
 *   the number of days since UNIX epoch in signed 32bit integer.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_time32_array_builder_append_values(GArrowTime32ArrayBuilder *builder,
                                          const gint32 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::Time32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     values,
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[time32-array-builder][append-values]");
}

/**
 * garrow_time32_array_builder_append_null:
 * @builder: A #GArrowTime32ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 */
gboolean
garrow_time32_array_builder_append_null(GArrowTime32ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::Time32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[time32-array-builder][append-null]");
}

/**
 * garrow_time32_array_builder_append_nulls:
 * @builder: A #GArrowTime32ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_time32_array_builder_append_nulls(GArrowTime32ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Time32Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[time32-array-builder][append-nulls]");
}


G_DEFINE_TYPE(GArrowTime64ArrayBuilder,
              garrow_time64_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_time64_array_builder_init(GArrowTime64ArrayBuilder *builder)
{
}

static void
garrow_time64_array_builder_class_init(GArrowTime64ArrayBuilderClass *klass)
{
}

/**
 * garrow_time64_array_builder_new:
 * @data_type: A #GArrowTime64DataType.
 *
 * Returns: A newly created #GArrowTime64ArrayBuilder.
 *
 * Since: 0.7.0
 */
GArrowTime64ArrayBuilder *
garrow_time64_array_builder_new(GArrowTime64DataType *data_type)
{
  auto arrow_data_type = garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          NULL,
                                          "[time64-array-builder][new]");
  return GARROW_TIME64_ARRAY_BUILDER(builder);
}

/**
 * garrow_time64_array_builder_append:
 * @builder: A #GArrowTime64ArrayBuilder.
 * @value: The number of milliseconds since UNIX epoch in signed 64bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_time64_array_builder_append_value() instead.
 */
gboolean
garrow_time64_array_builder_append(GArrowTime64ArrayBuilder *builder,
                                   gint64 value,
                                   GError **error)
{
  return garrow_time64_array_builder_append_value(builder, value, error);
}

/**
 * garrow_time64_array_builder_append_value:
 * @builder: A #GArrowTime64ArrayBuilder.
 * @value: The number of milliseconds since UNIX epoch in signed 64bit integer.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_time64_array_builder_append_value(GArrowTime64ArrayBuilder *builder,
                                         gint64 value,
                                         GError **error)
{
  return garrow_array_builder_append_value<arrow::Time64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     value,
     error,
     "[time64-array-builder][append-value]");
}

/**
 * garrow_time64_array_builder_append_values:
 * @builder: A #GArrowTime64ArrayBuilder.
 * @values: (array length=values_length): The array of
 *   the number of milliseconds since UNIX epoch in signed 64bit integer.
 * @values_length: The length of `values`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_time64_array_builder_append_values(GArrowTime64ArrayBuilder *builder,
                                          const gint64 *values,
                                          gint64 values_length,
                                          const gboolean *is_valids,
                                          gint64 is_valids_length,
                                          GError **error)
{
  return garrow_array_builder_append_values<arrow::Time64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const int64_t *>(values),
     values_length,
     is_valids,
     is_valids_length,
     error,
     "[time64-array-builder][append-values]");
}

/**
 * garrow_time64_array_builder_append_null:
 * @builder: A #GArrowTime64ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.7.0
 */
gboolean
garrow_time64_array_builder_append_null(GArrowTime64ArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::Time64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[time64-array-builder][append-null]");
}

/**
 * garrow_time64_array_builder_append_nulls:
 * @builder: A #GArrowTime64ArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.8.0
 */
gboolean
garrow_time64_array_builder_append_nulls(GArrowTime64ArrayBuilder *builder,
                                         gint64 n,
                                         GError **error)
{
  return garrow_array_builder_append_nulls<arrow::Time64Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[time64-array-builder][append-nulls]");
}


typedef struct GArrowListArrayBuilderPrivate_ {
  GArrowArrayBuilder *value_builder;
} GArrowListArrayBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GArrowListArrayBuilder,
                           garrow_list_array_builder,
                           GARROW_TYPE_ARRAY_BUILDER)

#define GARROW_LIST_ARRAY_BUILDER_GET_PRIVATE(obj)         \
  static_cast<GArrowListArrayBuilderPrivate *>(            \
     garrow_list_array_builder_get_instance_private(       \
       GARROW_LIST_ARRAY_BUILDER(obj)))

static void
garrow_list_array_builder_dispose(GObject *object)
{
  auto priv = GARROW_LIST_ARRAY_BUILDER_GET_PRIVATE(object);

  if (priv->value_builder) {
    g_object_unref(priv->value_builder);
    priv->value_builder = NULL;
  }

  G_OBJECT_CLASS(garrow_list_array_builder_parent_class)->dispose(object);
}

static void
garrow_list_array_builder_init(GArrowListArrayBuilder *builder)
{
}

static void
garrow_list_array_builder_class_init(GArrowListArrayBuilderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->dispose = garrow_list_array_builder_dispose;
}

/**
 * garrow_list_array_builder_new:
 * @data_type: A #GArrowListDataType for value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: A newly created #GArrowListArrayBuilder.
 */
GArrowListArrayBuilder *
garrow_list_array_builder_new(GArrowListDataType *data_type,
                              GError **error)
{
  if (!GARROW_IS_LIST_DATA_TYPE(data_type)) {
    g_set_error(error,
                GARROW_ERROR,
                GARROW_ERROR_INVALID,
                "[list-array-builder][new] data type must be list data type");
    return NULL;
  }

  auto arrow_data_type =
    garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          error,
                                          "[list-array-builder][new]");
  return GARROW_LIST_ARRAY_BUILDER(builder);
}

/**
 * garrow_list_array_builder_append:
 * @builder: A #GArrowListArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new list element. To append a new list element, you
 * need to call this function then append list element values to
 * `value_builder`. `value_builder` is the #GArrowArrayBuilder
 * specified to constructor. You can get `value_builder` by
 * garrow_list_array_builder_get_value_builder().
 *
 * |[<!-- language="C" -->
 * GArrowInt8ArrayBuilder *value_builder;
 * GArrowListArrayBuilder *builder;
 *
 * value_builder = garrow_int8_array_builder_new();
 * builder = garrow_list_array_builder_new(value_builder, NULL);
 *
 * // Start 0th list element: [1, 0, -1]
 * garrow_list_array_builder_append(builder, NULL);
 * garrow_int8_array_builder_append(value_builder, 1);
 * garrow_int8_array_builder_append(value_builder, 0);
 * garrow_int8_array_builder_append(value_builder, -1);
 *
 * // Start 1st list element: [-29, 29]
 * garrow_list_array_builder_append(builder, NULL);
 * garrow_int8_array_builder_append(value_builder, -29);
 * garrow_int8_array_builder_append(value_builder, 29);
 *
 * {
 *   // [[1, 0, -1], [-29, 29]]
 *   GArrowArray *array = garrow_array_builder_finish(builder);
 *   // Now, builder is needless.
 *   g_object_unref(builder);
 *   g_object_unref(value_builder);
 *
 *   // Use array...
 *   g_object_unref(array);
 * }
 * ]|
 *
 * Deprecated: 0.12.0:
 *   Use garrow_list_array_builder_append_value() instead.
 */
gboolean
garrow_list_array_builder_append(GArrowListArrayBuilder *builder,
                                 GError **error)
{
  return garrow_list_array_builder_append_value(builder, error);
}

/**
 * garrow_list_array_builder_append_value:
 * @builder: A #GArrowListArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new list element. To append a new list element, you
 * need to call this function then append list element values to
 * `value_builder`. `value_builder` is the #GArrowArrayBuilder
 * specified to constructor. You can get `value_builder` by
 * garrow_list_array_builder_get_value_builder().
 *
 * |[<!-- language="C" -->
 * GArrowInt8ArrayBuilder *value_builder;
 * GArrowListArrayBuilder *builder;
 *
 * value_builder = garrow_int8_array_builder_new();
 * builder = garrow_list_array_builder_new(value_builder, NULL);
 *
 * // Start 0th list element: [1, 0, -1]
 * garrow_list_array_builder_append(builder, NULL);
 * garrow_int8_array_builder_append(value_builder, 1);
 * garrow_int8_array_builder_append(value_builder, 0);
 * garrow_int8_array_builder_append(value_builder, -1);
 *
 * // Start 1st list element: [-29, 29]
 * garrow_list_array_builder_append(builder, NULL);
 * garrow_int8_array_builder_append(value_builder, -29);
 * garrow_int8_array_builder_append(value_builder, 29);
 *
 * {
 *   // [[1, 0, -1], [-29, 29]]
 *   GArrowArray *array = garrow_array_builder_finish(builder);
 *   // Now, builder is needless.
 *   g_object_unref(builder);
 *   g_object_unref(value_builder);
 *
 *   // Use array...
 *   g_object_unref(array);
 * }
 * ]|
 *
 * Since: 0.12.0
 */
gboolean
garrow_list_array_builder_append_value(GArrowListArrayBuilder *builder,
                                       GError **error)
{
  auto arrow_builder =
    static_cast<arrow::ListBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append();
  return garrow_error_check(error, status, "[list-array-builder][append-value]");
}

/**
 * garrow_list_array_builder_append_null:
 * @builder: A #GArrowListArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new NULL element.
 */
gboolean
garrow_list_array_builder_append_null(GArrowListArrayBuilder *builder,
                                      GError **error)
{
  return garrow_array_builder_append_null<arrow::ListBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[list-array-builder][append-null]");
}

/**
 * garrow_list_array_builder_get_value_builder:
 * @builder: A #GArrowListArrayBuilder.
 *
 * Returns: (transfer none): The #GArrowArrayBuilder for values.
 */
GArrowArrayBuilder *
garrow_list_array_builder_get_value_builder(GArrowListArrayBuilder *builder)
{
  auto priv = GARROW_LIST_ARRAY_BUILDER_GET_PRIVATE(builder);
  if (!priv->value_builder) {
    auto arrow_builder =
      static_cast<arrow::ListBuilder *>(
        garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));
    auto arrow_value_builder = arrow_builder->value_builder();
    priv->value_builder = garrow_array_builder_new_raw(arrow_value_builder);
    garrow_array_builder_release_ownership(priv->value_builder);
  }
  return priv->value_builder;
}


typedef struct GArrowLargeListArrayBuilderPrivate_ {
  GArrowArrayBuilder *value_builder;
} GArrowLargeListArrayBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GArrowLargeListArrayBuilder,
                           garrow_large_list_array_builder,
                           GARROW_TYPE_ARRAY_BUILDER)

#define GARROW_LARGE_LIST_ARRAY_BUILDER_GET_PRIVATE(obj)        \
  static_cast<GArrowLargeListArrayBuilderPrivate *>(            \
     garrow_large_list_array_builder_get_instance_private(      \
       GARROW_LARGE_LIST_ARRAY_BUILDER(obj)))

static void
garrow_large_list_array_builder_dispose(GObject *object)
{
  auto priv = GARROW_LARGE_LIST_ARRAY_BUILDER_GET_PRIVATE(object);

  if (priv->value_builder) {
    g_object_unref(priv->value_builder);
    priv->value_builder = NULL;
  }

  G_OBJECT_CLASS(garrow_large_list_array_builder_parent_class)->dispose(object);
}

static void
garrow_large_list_array_builder_init(GArrowLargeListArrayBuilder *builder)
{
}

static void
garrow_large_list_array_builder_class_init(GArrowLargeListArrayBuilderClass *klass)
{
  auto gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->dispose = garrow_large_list_array_builder_dispose;
}

/**
 * garrow_large_list_array_builder_new:
 * @data_type: A #GArrowLargeListDataType for value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: A newly created #GArrowLargeListArrayBuilder.
 *
 * Since: 0.16.0
 */
GArrowLargeListArrayBuilder *
garrow_large_list_array_builder_new(GArrowLargeListDataType *data_type,
                                    GError **error)
{
  if (!GARROW_IS_LARGE_LIST_DATA_TYPE(data_type)) {
    g_set_error(error,
                GARROW_ERROR,
                GARROW_ERROR_INVALID,
                "[large-list-array-builder][new] data type must be large list data type");
    return NULL;
  }

  auto arrow_data_type =
    garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          error,
                                          "[large-list-array-builder][new]");
  return GARROW_LARGE_LIST_ARRAY_BUILDER(builder);
}

/**
 * garrow_large_list_array_builder_append_value:
 * @builder: A #GArrowLargeListArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new list element. To append a new list element, you
 * need to call this function then append list element values to
 * `value_builder`. `value_builder` is the #GArrowArrayBuilder
 * specified to constructor. You can get `value_builder` by
 * garrow_large_list_array_builder_get_value_builder().
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_list_array_builder_append_value(GArrowLargeListArrayBuilder *builder,
                                             GError **error)
{
  auto arrow_builder =
    static_cast<arrow::LargeListBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append();
  return garrow_error_check(error, status, "[large-list-array-builder][append-value]");
}

/**
 * garrow_large_list_array_builder_append_null:
 * @builder: A #GArrowLargeListArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new NULL element.
 *
 * Since: 0.16.0
 */
gboolean
garrow_large_list_array_builder_append_null(GArrowLargeListArrayBuilder *builder,
                                            GError **error)
{
  return garrow_array_builder_append_null<arrow::LargeListBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[large-list-array-builder][append-null]");
}

/**
 * garrow_large_list_array_builder_get_value_builder:
 * @builder: A #GArrowLargeListArrayBuilder.
 *
 * Returns: (transfer none): The #GArrowArrayBuilder for values.
 *
 * Since: 0.16.0
 */
GArrowArrayBuilder *
garrow_large_list_array_builder_get_value_builder(GArrowLargeListArrayBuilder *builder)
{
  auto priv = GARROW_LARGE_LIST_ARRAY_BUILDER_GET_PRIVATE(builder);
  if (!priv->value_builder) {
    auto arrow_builder =
      static_cast<arrow::LargeListBuilder *>(
        garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));
    auto arrow_value_builder = arrow_builder->value_builder();
    priv->value_builder = garrow_array_builder_new_raw(arrow_value_builder);
    garrow_array_builder_release_ownership(priv->value_builder);
  }
  return priv->value_builder;
}


typedef struct GArrowStructArrayBuilderPrivate_ {
  GList *field_builders;
} GArrowStructArrayBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GArrowStructArrayBuilder,
                           garrow_struct_array_builder,
                           GARROW_TYPE_ARRAY_BUILDER)

#define GARROW_STRUCT_ARRAY_BUILDER_GET_PRIVATE(obj)         \
  static_cast<GArrowStructArrayBuilderPrivate *>(            \
     garrow_struct_array_builder_get_instance_private(       \
       GARROW_STRUCT_ARRAY_BUILDER(obj)))

static void
garrow_struct_array_builder_dispose(GObject *object)
{
  auto priv = GARROW_STRUCT_ARRAY_BUILDER_GET_PRIVATE(object);

  for (auto node = priv->field_builders; node; node = g_list_next(node)) {
    auto field_builder = static_cast<GArrowArrayBuilder *>(node->data);
    GArrowArrayBuilderPrivate *field_builder_priv;

    field_builder_priv = GARROW_ARRAY_BUILDER_GET_PRIVATE(field_builder);
    field_builder_priv->array_builder = nullptr;
    g_object_unref(field_builder);
  }
  g_list_free(priv->field_builders);
  priv->field_builders = NULL;

  G_OBJECT_CLASS(garrow_struct_array_builder_parent_class)->dispose(object);
}

static void
garrow_struct_array_builder_init(GArrowStructArrayBuilder *builder)
{
}

static void
garrow_struct_array_builder_class_init(GArrowStructArrayBuilderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->dispose = garrow_struct_array_builder_dispose;
}

/**
 * garrow_struct_array_builder_new:
 * @data_type: #GArrowStructDataType for the struct.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: A newly created #GArrowStructArrayBuilder.
 */
GArrowStructArrayBuilder *
garrow_struct_array_builder_new(GArrowStructDataType *data_type,
                                GError **error)
{
  if (!GARROW_IS_STRUCT_DATA_TYPE(data_type)) {
    g_set_error(error,
                GARROW_ERROR,
                GARROW_ERROR_INVALID,
                "[struct-array-builder][new] data type must be struct data type");
    return NULL;
  }

  auto arrow_data_type = garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          error,
                                          "[struct-array-builder][new]");
  return GARROW_STRUCT_ARRAY_BUILDER(builder);
}

/**
 * garrow_struct_array_builder_append:
 * @builder: A #GArrowStructArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new struct element. To append a new struct element,
 * you need to call this function then append struct element field
 * values to all `field_builder`s. `field_value`s are the
 * #GArrowArrayBuilder specified to constructor. You can get
 * `field_builder` by garrow_struct_array_builder_get_field_builder()
 * or garrow_struct_array_builder_get_field_builders().
 *
 * |[<!-- language="C" -->
 * // TODO
 * ]|
 *
 * Deprecated: 0.12.0:
 *   Use garrow_struct_array_builder_append_value() instead.
 */
gboolean
garrow_struct_array_builder_append(GArrowStructArrayBuilder *builder,
                                   GError **error)
{
  return garrow_struct_array_builder_append_value(builder, error);
}

/**
 * garrow_struct_array_builder_append_value:
 * @builder: A #GArrowStructArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new struct element. To append a new struct element,
 * you need to call this function then append struct element field
 * values to all `field_builder`s. `field_value`s are the
 * #GArrowArrayBuilder specified to constructor. You can get
 * `field_builder` by garrow_struct_array_builder_get_field_builder()
 * or garrow_struct_array_builder_get_field_builders().
 *
 * |[<!-- language="C" -->
 * // TODO
 * ]|
 *
 * Since: 0.12.0
 */
gboolean
garrow_struct_array_builder_append_value(GArrowStructArrayBuilder *builder,
                                         GError **error)
{
  auto arrow_builder =
    static_cast<arrow::StructBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append();
  return garrow_error_check(error,
                            status,
                            "[struct-array-builder][append-value]");
}

/**
 * garrow_struct_array_builder_append_null:
 * @builder: A #GArrowStructArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new NULL element.
 */
gboolean
garrow_struct_array_builder_append_null(GArrowStructArrayBuilder *builder,
                                        GError **error)
{
  return garrow_array_builder_append_null<arrow::StructBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[struct-array-builder][append-null]");
}

/**
 * garrow_struct_array_builder_get_field_builder:
 * @builder: A #GArrowStructArrayBuilder.
 * @i: The index of the field in the struct.
 *
 * Returns: (transfer none): The #GArrowArrayBuilder for the i-th field.
 */
GArrowArrayBuilder *
garrow_struct_array_builder_get_field_builder(GArrowStructArrayBuilder *builder,
                                              gint i)
{
  auto field_builders = garrow_struct_array_builder_get_field_builders(builder);
  auto field_builder = g_list_nth_data(field_builders, i);
  return static_cast<GArrowArrayBuilder *>(field_builder);
}

/**
 * garrow_struct_array_builder_get_field_builders:
 * @builder: A #GArrowStructArrayBuilder.
 *
 * Returns: (element-type GArrowArray) (transfer none):
 *   The #GArrowArrayBuilder for all fields.
 */
GList *
garrow_struct_array_builder_get_field_builders(GArrowStructArrayBuilder *builder)
{
  auto priv = GARROW_STRUCT_ARRAY_BUILDER_GET_PRIVATE(builder);
  if (!priv->field_builders) {
    auto arrow_struct_builder =
      static_cast<arrow::StructBuilder *>(
        garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

    GList *field_builders = NULL;
    for (int i = 0; i < arrow_struct_builder->num_fields(); ++i) {
      auto arrow_field_builder = arrow_struct_builder->field_builder(i);
      auto field_builder = garrow_array_builder_new_raw(arrow_field_builder);
      field_builders = g_list_prepend(field_builders, field_builder);
    }
    priv->field_builders = g_list_reverse(field_builders);
  }

  return priv->field_builders;
}


typedef struct GArrowMapArrayBuilderPrivate_ {
  GArrowArrayBuilder *key_builder;
  GArrowArrayBuilder *item_builder;
  GArrowArrayBuilder *value_builder;
} GArrowMapArrayBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GArrowMapArrayBuilder,
                           garrow_map_array_builder,
                           GARROW_TYPE_ARRAY_BUILDER)

#define GARROW_MAP_ARRAY_BUILDER_GET_PRIVATE(object)      \
  static_cast<GArrowMapArrayBuilderPrivate *>(            \
     garrow_map_array_builder_get_instance_private(       \
       GARROW_MAP_ARRAY_BUILDER(object)))

static void
garrow_map_array_builder_dispose(GObject *object)
{
  auto priv = GARROW_MAP_ARRAY_BUILDER_GET_PRIVATE(object);

  if (priv->key_builder) {
    g_object_unref(priv->key_builder);
    priv->key_builder = NULL;
  }

  if (priv->item_builder) {
    g_object_unref(priv->item_builder);
    priv->item_builder = NULL;
  }

  if (priv->value_builder) {
    g_object_unref(priv->value_builder);
    priv->value_builder = NULL;
  }

  G_OBJECT_CLASS(garrow_map_array_builder_parent_class)->dispose(object);
}

static void
garrow_map_array_builder_init(GArrowMapArrayBuilder *builder)
{
}

static void
garrow_map_array_builder_class_init(GArrowMapArrayBuilderClass *klass)
{
  auto gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->dispose = garrow_map_array_builder_dispose;
}

/**
 * garrow_map_array_builder_new:
 * @data_type: #GArrowMapDataType for the map.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: (nullable): A newly created #GArrowMapArrayBuilder on success,
 *   %NULL on error.
 *
 * Since: 0.17.0
 */
GArrowMapArrayBuilder *
garrow_map_array_builder_new(GArrowMapDataType *data_type,
                             GError **error)
{
  if (!GARROW_IS_MAP_DATA_TYPE(data_type)) {
    g_set_error(error,
                GARROW_ERROR,
                GARROW_ERROR_INVALID,
                "[map-array-builder][new] data type must be map data type");
    return NULL;
  }

  auto arrow_data_type = garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          error,
                                          "[map-array-builder][new]");
  if (builder) {
    return GARROW_MAP_ARRAY_BUILDER(builder);
  } else {
    return NULL;
  }
}

/**
 * garrow_map_array_builder_append_value:
 * @builder: A #GArrowMapArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.17.0
 */
gboolean
garrow_map_array_builder_append_value(GArrowMapArrayBuilder *builder,
                                      GError **error)
{
  auto arrow_builder =
    static_cast<arrow::MapBuilder *>(
      garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));

  auto status = arrow_builder->Append();
  return garrow::check(error,
                       status,
                       "[map-array-builder][append-value]");
}

/**
 * garrow_map_array_builder_append_values:
 * @builder: A #GArrowMapArrayBuilder.
 * @offsets: (array length=offsets_length): The array of signed int.
 * @offsets_length: The length of `offsets`.
 * @is_valids: (nullable) (array length=is_valids_length): The array of
 *   boolean that shows whether the Nth value is valid or not. If the
 *   Nth `is_valids` is %TRUE, the Nth `values` is valid value. Otherwise
 *   the Nth value is null value.
 * @is_valids_length: The length of `is_valids`.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple values at once. It's more efficient than multiple
 * `append` and `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.17.0
 */
gboolean
garrow_map_array_builder_append_values(GArrowMapArrayBuilder *builder,
                                       const gint32 *offsets,
                                       gint64 offsets_length,
                                       const gboolean *is_valids,
                                       gint64 is_valids_length,
                                       GError **error)
{
  return garrow_array_builder_append_values<arrow::MapBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     reinterpret_cast<const int32_t *>(offsets),
     offsets_length,
     is_valids,
     is_valids_length,
     error,
     "[map-array-builder][append-values]");
}

/**
 * garrow_map_array_builder_append_null:
 * @builder: A #GArrowMapArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.17.0
 */
gboolean
garrow_map_array_builder_append_null(GArrowMapArrayBuilder *builder,
                                     GError **error)
{
  return garrow_array_builder_append_null<arrow::MapBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[map-array-builder][append-null]");
}

/**
 * garrow_map_array_builder_append_nulls:
 * @builder: A #GArrowMapArrayBuilder.
 * @n: The number of null values to be appended.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Append multiple nulls at once. It's more efficient than multiple
 * `append_null` calls.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.17.0
 */
gboolean
garrow_map_array_builder_append_nulls(GArrowMapArrayBuilder *builder,
                                      gint64 n,
                                      GError **error)
{
  return garrow_array_builder_append_nulls<arrow::MapBuilder *>
    (GARROW_ARRAY_BUILDER(builder),
     n,
     error,
     "[map-array-builder][append-nulls]");
}

/**
 * garrow_map_array_builder_get_key_builder:
 * @builder: A #GArrowMapArrayBuilder.
 *
 * Returns: (transfer none): The #GArrowArrayBuilder for key values.
 *
 * Since: 0.17.0
 */
GArrowArrayBuilder *
garrow_map_array_builder_get_key_builder(GArrowMapArrayBuilder *builder)
{
  auto priv = GARROW_MAP_ARRAY_BUILDER_GET_PRIVATE(builder);
  if (!priv->key_builder) {
    auto arrow_builder =
      static_cast<arrow::MapBuilder *>(
        garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));
    auto arrow_key_builder = arrow_builder->key_builder();
    priv->key_builder = garrow_array_builder_new_raw(arrow_key_builder);
    garrow_array_builder_release_ownership(priv->key_builder);
  }
  return priv->key_builder;
}

/**
 * garrow_map_array_builder_get_item_builder:
 * @builder: A #GArrowMapArrayBuilder.
 *
 * Returns: (transfer none): The #GArrowArrayBuilder for item values.
 *
 * Since: 0.17.0
 */
GArrowArrayBuilder *
garrow_map_array_builder_get_item_builder(GArrowMapArrayBuilder *builder)
{
  auto priv = GARROW_MAP_ARRAY_BUILDER_GET_PRIVATE(builder);
  if (!priv->item_builder) {
    auto arrow_builder =
      static_cast<arrow::MapBuilder *>(
        garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));
    auto arrow_item_builder = arrow_builder->item_builder();
    priv->item_builder = garrow_array_builder_new_raw(arrow_item_builder);
    garrow_array_builder_release_ownership(priv->item_builder);
  }
  return priv->item_builder;
}

/**
 * garrow_map_array_builder_get_value_builder:
 * @builder: A #GArrowMapArrayBuilder.
 *
 * Returns: (transfer none): The #GArrowArrayBuilder to add map entries as struct values.
 *   This can be used instead of garrow_map_array_builder_get_key_builder() and
 *   garrow_map_array_builder_get_item_builder(). You can build map entries as a list of
 *   struct values with this builder.
 *
 * Since: 0.17.0
 */
GArrowArrayBuilder *
garrow_map_array_builder_get_value_builder(GArrowMapArrayBuilder *builder)
{
  auto priv = GARROW_MAP_ARRAY_BUILDER_GET_PRIVATE(builder);
  if (!priv->value_builder) {
    auto arrow_builder =
      static_cast<arrow::MapBuilder *>(
        garrow_array_builder_get_raw(GARROW_ARRAY_BUILDER(builder)));
    auto arrow_value_builder = arrow_builder->value_builder();
    priv->value_builder = garrow_array_builder_new_raw(arrow_value_builder);
    garrow_array_builder_release_ownership(priv->value_builder);
  }
  return priv->value_builder;
}


G_DEFINE_TYPE(GArrowDecimal128ArrayBuilder,
              garrow_decimal128_array_builder,
              GARROW_TYPE_ARRAY_BUILDER)

static void
garrow_decimal128_array_builder_init(GArrowDecimal128ArrayBuilder *builder)
{
}

static void
garrow_decimal128_array_builder_class_init(GArrowDecimal128ArrayBuilderClass *klass)
{
}

/**
 * garrow_decimal128_array_builder_new:
 * @data_type: #GArrowDecimal128DataType for the decimal.
 *
 * Returns: A newly created #GArrowDecimal128ArrayBuilder.
 *
 * Since: 0.10.0
 */
GArrowDecimal128ArrayBuilder *
garrow_decimal128_array_builder_new(GArrowDecimal128DataType *data_type)
{
  auto arrow_data_type = garrow_data_type_get_raw(GARROW_DATA_TYPE(data_type));
  auto builder = garrow_array_builder_new(arrow_data_type,
                                          NULL,
                                          "[decimal128-array-builder][new]");
  return GARROW_DECIMAL128_ARRAY_BUILDER(builder);
}

/**
 * garrow_decimal128_array_builder_append:
 * @builder: A #GArrowDecimal128ArrayBuilder.
 * @value: A decimal value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.10.0
 *
 * Deprecated: 0.12.0:
 *   Use garrow_decimal128_array_builder_append_value() instead.
 */
gboolean
garrow_decimal128_array_builder_append(GArrowDecimal128ArrayBuilder *builder,
                                       GArrowDecimal128 *value,
                                       GError **error)
{
  return garrow_decimal128_array_builder_append_value(builder, value, error);
}

/**
 * garrow_decimal128_array_builder_append_value:
 * @builder: A #GArrowDecimal128ArrayBuilder.
 * @value: A decimal value.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 0.12.0
 */
gboolean
garrow_decimal128_array_builder_append_value(GArrowDecimal128ArrayBuilder *builder,
                                             GArrowDecimal128 *value,
                                             GError **error)
{
  auto arrow_decimal = garrow_decimal128_get_raw(value);
  return garrow_array_builder_append_value<arrow::Decimal128Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     *arrow_decimal,
     error,
     "[decimal128-array-builder][append-value]");
}

/**
 * garrow_decimal128_array_builder_append_null:
 * @builder: A #GArrowDecimal128ArrayBuilder.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * It appends a new NULL element.
 *
 * Since: 0.12.0
 */
gboolean
garrow_decimal128_array_builder_append_null(GArrowDecimal128ArrayBuilder *builder,
                                            GError **error)
{
  return garrow_array_builder_append_null<arrow::Decimal128Builder *>
    (GARROW_ARRAY_BUILDER(builder),
     error,
     "[decimal128-array-builder][append-null]");
}

G_END_DECLS

GArrowArrayBuilder *
garrow_array_builder_new_raw(arrow::ArrayBuilder *arrow_builder,
                             GType type)
{
  if (type == G_TYPE_INVALID) {
    switch (arrow_builder->type()->id()) {
    case arrow::Type::type::NA:
      type = GARROW_TYPE_NULL_ARRAY_BUILDER;
      break;
    case arrow::Type::type::BOOL:
      type = GARROW_TYPE_BOOLEAN_ARRAY_BUILDER;
      break;
    case arrow::Type::type::UINT8:
      type = GARROW_TYPE_UINT8_ARRAY_BUILDER;
      break;
    case arrow::Type::type::INT8:
      type = GARROW_TYPE_INT8_ARRAY_BUILDER;
      break;
    case arrow::Type::type::UINT16:
      type = GARROW_TYPE_UINT16_ARRAY_BUILDER;
      break;
    case arrow::Type::type::INT16:
      type = GARROW_TYPE_INT16_ARRAY_BUILDER;
      break;
    case arrow::Type::type::UINT32:
      type = GARROW_TYPE_UINT32_ARRAY_BUILDER;
      break;
    case arrow::Type::type::INT32:
      type = GARROW_TYPE_INT32_ARRAY_BUILDER;
      break;
    case arrow::Type::type::UINT64:
      type = GARROW_TYPE_UINT64_ARRAY_BUILDER;
      break;
    case arrow::Type::type::INT64:
      type = GARROW_TYPE_INT64_ARRAY_BUILDER;
      break;
    case arrow::Type::type::FLOAT:
      type = GARROW_TYPE_FLOAT_ARRAY_BUILDER;
      break;
    case arrow::Type::type::DOUBLE:
      type = GARROW_TYPE_DOUBLE_ARRAY_BUILDER;
      break;
    case arrow::Type::type::BINARY:
      type = GARROW_TYPE_BINARY_ARRAY_BUILDER;
      break;
    case arrow::Type::type::LARGE_BINARY:
      type = GARROW_TYPE_LARGE_BINARY_ARRAY_BUILDER;
      break;
    case arrow::Type::type::STRING:
      type = GARROW_TYPE_STRING_ARRAY_BUILDER;
      break;
    case arrow::Type::type::LARGE_STRING:
      type = GARROW_TYPE_LARGE_STRING_ARRAY_BUILDER;
      break;
    case arrow::Type::type::DATE32:
      type = GARROW_TYPE_DATE32_ARRAY_BUILDER;
      break;
    case arrow::Type::type::DATE64:
      type = GARROW_TYPE_DATE64_ARRAY_BUILDER;
      break;
    case arrow::Type::type::TIMESTAMP:
      type = GARROW_TYPE_TIMESTAMP_ARRAY_BUILDER;
      break;
    case arrow::Type::type::TIME32:
      type = GARROW_TYPE_TIME32_ARRAY_BUILDER;
      break;
    case arrow::Type::type::TIME64:
      type = GARROW_TYPE_TIME64_ARRAY_BUILDER;
      break;
    case arrow::Type::type::LIST:
      type = GARROW_TYPE_LIST_ARRAY_BUILDER;
      break;
    case arrow::Type::type::LARGE_LIST:
      type = GARROW_TYPE_LARGE_LIST_ARRAY_BUILDER;
      break;
    case arrow::Type::type::STRUCT:
      type = GARROW_TYPE_STRUCT_ARRAY_BUILDER;
      break;
    case arrow::Type::type::MAP:
      type = GARROW_TYPE_MAP_ARRAY_BUILDER;
      break;
    case arrow::Type::type::DECIMAL:
      type = GARROW_TYPE_DECIMAL128_ARRAY_BUILDER;
      break;
    default:
      type = GARROW_TYPE_ARRAY_BUILDER;
      break;
    }
  }

  auto builder =
    GARROW_ARRAY_BUILDER(g_object_new(type,
                                      "array-builder", arrow_builder,
                                      NULL));
  return builder;
}

arrow::ArrayBuilder *
garrow_array_builder_get_raw(GArrowArrayBuilder *builder)
{
  auto priv = GARROW_ARRAY_BUILDER_GET_PRIVATE(builder);
  return priv->array_builder;
}

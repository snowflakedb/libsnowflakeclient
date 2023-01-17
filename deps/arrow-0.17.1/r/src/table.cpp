// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "./arrow_types.h"
#if defined(ARROW_R_WITH_ARROW)

#include <arrow/io/file.h>
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>

using Rcpp::DataFrame;

// [[arrow::export]]
std::shared_ptr<arrow::Table> Table__from_dataframe(DataFrame tbl) {
  auto rb = RecordBatch__from_dataframe(tbl);

  return VALUE_OR_STOP(arrow::Table::FromRecordBatches({std::move(rb)}));
}

// [[arrow::export]]
int Table__num_columns(const std::shared_ptr<arrow::Table>& x) {
  return x->num_columns();
}

// [[arrow::export]]
int Table__num_rows(const std::shared_ptr<arrow::Table>& x) { return x->num_rows(); }

// [[arrow::export]]
std::shared_ptr<arrow::Schema> Table__schema(const std::shared_ptr<arrow::Table>& x) {
  return x->schema();
}

// [[arrow::export]]
std::shared_ptr<arrow::ChunkedArray> Table__column(
    const std::shared_ptr<arrow::Table>& table, int i) {
  arrow::r::validate_index(i, table->num_columns());
  return table->column(i);
}

// [[arrow::export]]
std::shared_ptr<arrow::Field> Table__field(const std::shared_ptr<arrow::Table>& table,
                                           int i) {
  arrow::r::validate_index(i, table->num_columns());
  return table->field(i);
}

// [[arrow::export]]
std::vector<std::shared_ptr<arrow::ChunkedArray>> Table__columns(
    const std::shared_ptr<arrow::Table>& table) {
  auto nc = table->num_columns();
  std::vector<std::shared_ptr<arrow::ChunkedArray>> res(nc);
  for (int i = 0; i < nc; i++) {
    res[i] = table->column(i);
  }
  return res;
}

// [[arrow::export]]
std::vector<std::string> Table__ColumnNames(const std::shared_ptr<arrow::Table>& table) {
  return table->ColumnNames();
}

// [[arrow::export]]
std::shared_ptr<arrow::Table> Table__Slice1(const std::shared_ptr<arrow::Table>& table,
                                            int offset) {
  arrow::r::validate_slice_offset(offset, table->num_rows());
  return table->Slice(offset);
}

// [[arrow::export]]
std::shared_ptr<arrow::Table> Table__Slice2(const std::shared_ptr<arrow::Table>& table,
                                            int offset, int length) {
  arrow::r::validate_slice_offset(offset, table->num_rows());
  arrow::r::validate_slice_length(length, table->num_rows() - offset);
  return table->Slice(offset, length);
}

// [[arrow::export]]
bool Table__Equals(const std::shared_ptr<arrow::Table>& lhs,
                   const std::shared_ptr<arrow::Table>& rhs, bool check_metadata) {
  return lhs->Equals(*rhs.get(), check_metadata);
}

// [[arrow::export]]
bool Table__Validate(const std::shared_ptr<arrow::Table>& table) {
  STOP_IF_NOT_OK(table->Validate());
  return true;
}

// [[arrow::export]]
bool Table__ValidateFull(const std::shared_ptr<arrow::Table>& table) {
  STOP_IF_NOT_OK(table->ValidateFull());
  return true;
}

// [[arrow::export]]
std::shared_ptr<arrow::ChunkedArray> Table__GetColumnByName(
    const std::shared_ptr<arrow::Table>& table, const std::string& name) {
  return table->GetColumnByName(name);
}

// [[arrow::export]]
std::shared_ptr<arrow::Table> Table__select(const std::shared_ptr<arrow::Table>& table,
                                            const Rcpp::IntegerVector& indices) {
  R_xlen_t n = indices.size();

  std::vector<std::shared_ptr<arrow::Field>> fields(n);
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(n);

  for (R_xlen_t i = 0; i < n; i++) {
    int pos = indices[i] - 1;
    fields[i] = table->schema()->field(pos);
    columns[i] = table->column(pos);
  }

  auto schema = std::make_shared<arrow::Schema>(std::move(fields));
  return arrow::Table::Make(schema, columns);
}

bool all_record_batches(SEXP lst) {
  R_xlen_t n = XLENGTH(lst);
  for (R_xlen_t i = 0; i < n; i++) {
    if (!Rf_inherits(VECTOR_ELT(lst, i), "RecordBatch")) return false;
  }
  return true;
}

// [[arrow::export]]
std::shared_ptr<arrow::Table> Table__from_dots(SEXP lst, SEXP schema_sxp) {
  // lst can be either:
  // - a list of record batches, in which case we call Table::FromRecordBatches

  if (all_record_batches(lst)) {
    auto batches = arrow::r::List_to_shared_ptr_vector<arrow::RecordBatch>(lst);
    std::shared_ptr<arrow::Table> tab;

    if (Rf_inherits(schema_sxp, "Schema")) {
      auto schema = arrow::r::extract<arrow::Schema>(schema_sxp);
      tab = VALUE_OR_STOP(arrow::Table::FromRecordBatches(schema, std::move(batches)));
    } else {
      tab = VALUE_OR_STOP(arrow::Table::FromRecordBatches(std::move(batches)));
    }
    return tab;
  }

  int num_fields;
  STOP_IF_NOT_OK(arrow::r::count_fields(lst, &num_fields));

  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(num_fields);
  std::shared_ptr<arrow::Schema> schema;

  if (Rf_isNull(schema_sxp)) {
    // infer the schema from the ...
    std::vector<std::shared_ptr<arrow::Field>> fields(num_fields);
    SEXP names = Rf_getAttrib(lst, R_NamesSymbol);

    auto fill_one_column = [&columns, &fields](int j, SEXP x, SEXP name) {
      if (Rf_inherits(x, "ChunkedArray")) {
        auto chunked_array = arrow::r::extract<arrow::ChunkedArray>(x);
        fields[j] = arrow::field(CHAR(name), chunked_array->type());
        columns[j] = chunked_array;
      } else if (Rf_inherits(x, "Array")) {
        auto array = arrow::r::extract<arrow::Array>(x);
        fields[j] = arrow::field(CHAR(name), array->type());
        columns[j] = std::make_shared<arrow::ChunkedArray>(array);
      } else {
        auto array = Array__from_vector(x, R_NilValue);
        fields[j] = arrow::field(CHAR(name), array->type());
        columns[j] = std::make_shared<arrow::ChunkedArray>(array);
      }
    };

    for (R_xlen_t i = 0, j = 0; j < num_fields; i++) {
      SEXP name_i = STRING_ELT(names, i);
      SEXP x_i = VECTOR_ELT(lst, i);

      if (LENGTH(name_i) == 0) {
        SEXP names_x_i = Rf_getAttrib(x_i, R_NamesSymbol);
        for (R_xlen_t k = 0; k < XLENGTH(x_i); k++, j++) {
          fill_one_column(j, VECTOR_ELT(x_i, k), STRING_ELT(names_x_i, k));
        }
      } else {
        fill_one_column(j, x_i, name_i);
        j++;
      }
    }

    schema = std::make_shared<arrow::Schema>(std::move(fields));
  } else if (Rf_inherits(schema_sxp, "Schema")) {
    // use the schema that is given
    schema = arrow::r::extract<arrow::Schema>(schema_sxp);

    auto fill_one_column = [&columns, &schema](int j, SEXP x) {
      if (Rf_inherits(x, "ChunkedArray")) {
        auto chunked_array = arrow::r::extract<arrow::ChunkedArray>(x);
        columns[j] = chunked_array;
      } else if (Rf_inherits(x, "Array")) {
        auto array = arrow::r::extract<arrow::Array>(x);
        columns[j] = std::make_shared<arrow::ChunkedArray>(array);
      } else {
        auto type = schema->field(j)->type();
        auto array = arrow::r::Array__from_vector(x, type, false);
        columns[j] = std::make_shared<arrow::ChunkedArray>(array);
      }
    };

    SEXP names = Rf_getAttrib(lst, R_NamesSymbol);
    for (R_xlen_t i = 0, j = 0; j < num_fields; i++) {
      SEXP name_i = STRING_ELT(names, i);
      SEXP x_i = VECTOR_ELT(lst, i);

      if (LENGTH(name_i) == 0) {
        for (R_xlen_t k = 0; k < XLENGTH(x_i); k++, j++) {
          fill_one_column(j, VECTOR_ELT(x_i, k));
        }
      } else {
        fill_one_column(j, x_i);
        j++;
      }
    }
  } else {
    Rcpp::stop("schema must be an arrow::Schema or NULL");
  }

  return arrow::Table::Make(schema, columns);
}

#endif

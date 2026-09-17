<!--
  ~ Licensed to the Apache Software Foundation (ASF) under one
  ~ or more contributor license agreements.  See the NOTICE file
  ~ distributed with this work for additional information
  ~ regarding copyright ownership.  The ASF licenses this file
  ~ to you under the Apache License, Version 2.0 (the
  ~ "License"); you may not use this file except in compliance
  ~ with the License.  You may obtain a copy of the License at
  ~
  ~   http://www.apache.org/licenses/LICENSE-2.0
  ~
  ~ Unless required by applicable law or agreed to in writing,
  ~ software distributed under the License is distributed on an
  ~ "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  ~ KIND, either express or implied.  See the License for the
  ~ specific language governing permissions and limitations
  ~ under the License.
  -->

# Parquet Test Files

| File | Description |
| --- | --- |
| ARROW-17100.parquet | Parquet file written by PyArrow 2.0 with DataPageV2 and compressed columns. Prior to PyArrow 3.0, pages were compressed even if the is_compressed flag was 0. This was fixed in ARROW-10353, but for backwards compatibility readers may wish to support such a file. |
| alltypes-java.parquet | Parquet file written by using the Java DatasetWriter class in Arrow 14.0. Types supported do not include Map, Sparse and DenseUnion, Interval Day, Year, or MonthDayNano, and Float16. This file is used by https://github.com/apache/arrow/pull/38249 and was generated using the TestAllTypes#testAllTypesParquet() test case in the Java Dataset module. |

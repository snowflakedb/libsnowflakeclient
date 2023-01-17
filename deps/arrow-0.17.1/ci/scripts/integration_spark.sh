#!/usr/bin/env bash
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# exit on any error
set -eu

source_dir=${1}
spark_dir=${2}

pushd ${source_dir}/java
  arrow_version=`mvn org.apache.maven.plugins:maven-help-plugin:2.1.1:evaluate -Dexpression=project.version | sed -n -e '/^\[.*\]/ !{ /^[0-9]/ { p; q } }'`
popd

export MAVEN_OPTS="-Xmx2g -XX:ReservedCodeCacheSize=512m -Dorg.slf4j.simpleLogger.defaultLogLevel=warn"
export MAVEN_OPTS="${MAVEN_OPTS} -Dorg.slf4j.simpleLogger.log.org.apache.maven.cli.transfer.Slf4jMavenTransferListener=warn"

# build Spark with Arrow
pushd ${spark_dir}
  # update Spark pom with the Arrow version just installed and build Spark, need package phase for pyspark
  echo "Building Spark with Arrow ${arrow_version}"
  mvn versions:set-property -Dproperty=arrow.version -DnewVersion=${arrow_version}

  build/mvn -B -DskipTests package -pl sql/core -pl assembly -am

  spark_scala_tests=(
    "org.apache.spark.sql.execution.arrow"
    "org.apache.spark.sql.execution.vectorized.ColumnarBatchSuite"
    "org.apache.spark.sql.execution.vectorized.ArrowColumnVectorSuite")

  (echo "Testing Spark:"; IFS=$'\n'; echo "${spark_scala_tests[*]}")

  # TODO: should be able to only build spark-sql tests with adding "-pl sql/core" but not currently working
  build/mvn -B -Dtest=none -DwildcardSuites=$(IFS=,; echo "${spark_scala_tests[*]}") test

  # Run pyarrow related Python tests only
  spark_python_tests=(
    "pyspark.sql.tests.test_arrow"
    "pyspark.sql.tests.test_pandas_map"
    "pyspark.sql.tests.test_pandas_cogrouped_map"
    "pyspark.sql.tests.test_pandas_grouped_map"
    "pyspark.sql.tests.test_pandas_udf"
    "pyspark.sql.tests.test_pandas_udf_scalar"
    "pyspark.sql.tests.test_pandas_udf_grouped_agg"
    "pyspark.sql.tests.test_pandas_udf_window")

  (echo "Testing PySpark:"; IFS=$'\n'; echo "${spark_python_tests[*]}")
  python/run-tests --testnames "$(IFS=,; echo "${spark_python_tests[*]}")" --python-executables python
popd

/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.arrow.adapter.jdbc;

import java.io.IOException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;

import org.apache.arrow.vector.VectorSchemaRoot;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.dataformat.yaml.YAMLFactory;

/**
 * Class to abstract out some common test functionality for testing JDBC to Arrow.
 */
public abstract class AbstractJdbcToArrowTest {

  protected static final String BIGINT = "BIGINT_FIELD5";
  protected static final String BINARY = "BINARY_FIELD12";
  protected static final String BIT = "BIT_FIELD17";
  protected static final String BLOB = "BLOB_FIELD14";
  protected static final String BOOL = "BOOL_FIELD2";
  protected static final String CHAR = "CHAR_FIELD16";
  protected static final String CLOB = "CLOB_FIELD15";
  protected static final String DATE = "DATE_FIELD10";
  protected static final String DECIMAL = "DECIMAL_FIELD6";
  protected static final String DOUBLE = "DOUBLE_FIELD7";
  protected static final String INT = "INT_FIELD1";
  protected static final String REAL = "REAL_FIELD8";
  protected static final String SMALLINT = "SMALLINT_FIELD4";
  protected static final String TIME = "TIME_FIELD9";
  protected static final String TIMESTAMP = "TIMESTAMP_FIELD11";
  protected static final String TINYINT = "TINYINT_FIELD3";
  protected static final String VARCHAR = "VARCHAR_FIELD13";
  protected static final String NULL = "NULL_FIELD18";

  protected Connection conn = null;
  protected Table table;

  /**
   * This method creates Table object after reading YAML file.
   *
   * @param ymlFilePath path to file
   * @return Table object
   * @throws IOException on error
   */
  protected static Table getTable(String ymlFilePath, @SuppressWarnings("rawtypes") Class clss) throws IOException {
    return new ObjectMapper(new YAMLFactory()).readValue(
            clss.getClassLoader().getResourceAsStream(ymlFilePath), Table.class);
  }


  /**
   * This method creates Connection object and DB table and also populate data into table for test.
   *
   * @throws SQLException on error
   * @throws ClassNotFoundException on error
   */
  @Before
  public void setUp() throws SQLException, ClassNotFoundException {
    String url = "jdbc:h2:mem:JdbcToArrowTest";
    String driver = "org.h2.Driver";
    Class.forName(driver);
    conn = DriverManager.getConnection(url);
    try (Statement stmt = conn.createStatement();) {
      stmt.executeUpdate(table.getCreate());
      for (String insert : table.getData()) {
        stmt.executeUpdate(insert);
      }
    }
  }

  /**
   * Clean up method to close connection after test completes.
   *
   * @throws SQLException on error
   */
  @After
  public void destroy() throws SQLException {
    if (conn != null) {
      conn.close();
      conn = null;
    }
  }

  /**
   * Prepares test data and returns collection of Table object for each test iteration.
   *
   * @param testFiles files for test
   * @param clss Class type
   * @return Collection of Table objects
   * @throws SQLException on error
   * @throws ClassNotFoundException on error
   * @throws IOException on error
   */
  public static Object[][] prepareTestData(String[] testFiles, @SuppressWarnings("rawtypes") Class clss)
      throws SQLException, ClassNotFoundException, IOException {
    Object[][] tableArr = new Object[testFiles.length][];
    int i = 0;
    for (String testFile : testFiles) {
      tableArr[i++] = new Object[]{getTable(testFile, clss)};
    }
    return tableArr;
  }

  /**
   * Abstract method to implement test Functionality to test JdbcToArrow methods.
   *
   * @throws SQLException on error
   * @throws IOException on error
   */
  @Test
  public abstract void testJdbcToArrowValues() throws SQLException, IOException;

  /**
   * Abstract method to implement logic to assert test various datatype values.
   *
   * @param root VectorSchemaRoot for test
   */
  public abstract void testDataSets(VectorSchemaRoot root);

}

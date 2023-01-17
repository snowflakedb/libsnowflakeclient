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

package org.apache.arrow.vector.util;

import org.apache.arrow.util.Preconditions;
import org.apache.arrow.vector.ValueVector;

/**
 * Utility methods for {@link ValueVector}.
 */
public class ValueVectorUtility {

  /**
   * Get the toString() representation of vector suitable for debugging.
   * Note since vectors may have millions of values, this method only show max 20 values.
   * Examples as below (v represents value):
   * <li>
   *   vector with 0 value:
   *   []
   * </li>
   * <li>
   *   vector with 5 values (no more than 20 values):
   *   [v0, v1, v2, v3, v4]
   * </li>
   * <li>
   *  vector with 100 values (more than 20 values):
   *  [v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, ..., v90, v91, v92, v93, v94, v95, v96, v97, v98, v99]
   * </li>
   */
  public static String getToString(ValueVector vector, int start, int end) {
    Preconditions.checkNotNull(vector);
    final int length = end - start;
    Preconditions.checkArgument(length >= 0);
    Preconditions.checkArgument(start >= 0);
    Preconditions.checkArgument(end <= vector.getValueCount());

    if (length == 0) {
      return "[]";
    }

    final int window = 10;
    boolean skipComma = false;

    StringBuilder sb = new StringBuilder();
    sb.append('[');
    for (int i = start; i < end; i++) {
      if (skipComma) {
        skipComma = false;
      }
      if (i - start >= window && i < end - window) {
        sb.append("...");
        i = end - window - 1;
        skipComma = true;
      } else {
        sb.append(vector.getObject(i));
      }

      if (i == end - 1) {
        sb.append(']');
      } else {
        if (!skipComma) {
          sb.append(',');
        }
        sb.append(' ');
      }
    }

    return sb.toString();
  }
}

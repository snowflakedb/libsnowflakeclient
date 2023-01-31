/**
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

package org.apache.arrow.util;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/**
 * Utility methods for manipulating {@link java.util.Collections} and their subclasses/implementations.
 */
public class Collections2 {
  private Collections2() {}

  /**
   * Creates a {@link List} from the elements remaining in iterator.
   */
  public static <T> List<T> toList(Iterator<T> iterator) {
    List<T> target = new ArrayList<>();
    iterator.forEachRemaining(target::add);
    return target;
  }

  /**
   * Converts the iterable into a new {@link List}.
   */
  public static <T> List<T> toList(Iterable<T> iterable) {
    return StreamSupport.stream(iterable.spliterator(), false).collect(Collectors.toList());
  }

  /** Copies the elements of <code>map</code> to a new unmodifiable map. */
  public static <K,V> Map<K, V> immutableMapCopy(Map<K, V> map) {
    Map<K,V> newMap = new HashMap<>();
    newMap.putAll(map);
    return java.util.Collections.unmodifiableMap(newMap);
  }

  /** Copies the elements of list to a new unmodifiable list. */
  public static <V> List<V> immutableListCopy(List<V> list) {
    return Collections.unmodifiableList(list.stream().collect(Collectors.toList()));
  }

  /** Copies the values to a new unmodifiable list. */
  public static <V> List<V> asImmutableList(V...values) {
    return Collections.unmodifiableList(Arrays.asList(values));
  }
}

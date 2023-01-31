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

package org.apache.arrow.memory.rounding;

import java.lang.reflect.Field;

import org.apache.arrow.memory.BaseAllocator;
import org.apache.arrow.memory.NettyAllocationManager;

/**
 * The default rounding policy. That is, if the requested size is within the chunk size,
 * the rounded size will be the next power of two. Otherwise, the rounded size
 * will be identical to the requested size.
 */
public class DefaultRoundingPolicy implements RoundingPolicy {

  public final long chunkSize;

  /**
   * The singleton instance.
   */
  public static final DefaultRoundingPolicy INSTANCE = new DefaultRoundingPolicy();

  private DefaultRoundingPolicy() {
    try {
      Field field = NettyAllocationManager.class.getDeclaredField("CHUNK_SIZE");
      field.setAccessible(true);
      chunkSize = (Long) field.get(null);
    } catch (Exception e) {
      throw new RuntimeException("Failed to get chunk size from allocation manager");
    }
  }

  @Override
  public long getRoundedSize(long requestSize) {
    return requestSize < chunkSize ?
            BaseAllocator.nextPowerOfTwo(requestSize) : requestSize;
  }
}

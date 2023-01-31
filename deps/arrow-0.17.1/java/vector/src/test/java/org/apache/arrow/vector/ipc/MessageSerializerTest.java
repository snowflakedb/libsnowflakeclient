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

package org.apache.arrow.vector.ipc;

import static java.util.Arrays.asList;
import static org.apache.arrow.memory.util.LargeMemoryUtil.checkedCastToInt;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.Channels;
import java.util.Collections;
import java.util.List;

import org.apache.arrow.memory.BufferAllocator;
import org.apache.arrow.memory.RootAllocator;
import org.apache.arrow.vector.ipc.message.ArrowFieldNode;
import org.apache.arrow.vector.ipc.message.ArrowMessage;
import org.apache.arrow.vector.ipc.message.ArrowRecordBatch;
import org.apache.arrow.vector.ipc.message.MessageSerializer;
import org.apache.arrow.vector.types.pojo.ArrowType;
import org.apache.arrow.vector.types.pojo.DictionaryEncoding;
import org.apache.arrow.vector.types.pojo.Field;
import org.apache.arrow.vector.types.pojo.FieldType;
import org.apache.arrow.vector.types.pojo.Schema;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

import io.netty.buffer.ArrowBuf;

public class MessageSerializerTest {

  public static ArrowBuf buf(BufferAllocator alloc, byte[] bytes) {
    ArrowBuf buffer = alloc.buffer(bytes.length);
    buffer.writeBytes(bytes);
    return buffer;
  }

  public static byte[] array(ArrowBuf buf) {
    byte[] bytes = new byte[checkedCastToInt(buf.readableBytes())];
    buf.readBytes(bytes);
    return bytes;
  }

  private int intToByteRoundtrip(int v, byte[] bytes) {
    MessageSerializer.intToBytes(v, bytes);
    return MessageSerializer.bytesToInt(bytes);
  }

  @Test
  public void testIntToBytes() {
    byte[] bytes = new byte[4];
    int[] values = new int[] {1, 15, 1 << 8, 1 << 16, Integer.MAX_VALUE};
    for (int v: values) {
      assertEquals(intToByteRoundtrip(v, bytes), v);
    }
  }

  @Test
  public void testWriteMessageBufferAligned() throws IOException {
    ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
    WriteChannel out = new WriteChannel(Channels.newChannel(outputStream));

    // This is not a valid Arrow Message, only to test writing and alignment
    ByteBuffer buffer = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
    buffer.putInt(1);
    buffer.putInt(2);
    buffer.flip();

    int bytesWritten = MessageSerializer.writeMessageBuffer(out, 8, buffer);
    assertEquals(16, bytesWritten);

    buffer.rewind();
    buffer.putInt(3);
    buffer.flip();
    bytesWritten = MessageSerializer.writeMessageBuffer(out, 4, buffer);
    assertEquals(16, bytesWritten);

    ByteArrayInputStream inputStream = new ByteArrayInputStream(outputStream.toByteArray());
    ReadChannel in = new ReadChannel(Channels.newChannel(inputStream));
    ByteBuffer result = ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN);
    in.readFully(result);
    result.rewind();

    // First message continuation, size, and 2 int values
    assertEquals(MessageSerializer.IPC_CONTINUATION_TOKEN, result.getInt());
    assertEquals(8, result.getInt());
    assertEquals(1, result.getInt());
    assertEquals(2, result.getInt());

    // Second message continuation, size, 1 int value and 4 bytes padding
    assertEquals(MessageSerializer.IPC_CONTINUATION_TOKEN, result.getInt());
    assertEquals(8, result.getInt());
    assertEquals(3, result.getInt());
    assertEquals(0, result.getInt());
  }

  @Test
  public void testSchemaMessageSerialization() throws IOException {
    Schema schema = testSchema();
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    long size = MessageSerializer.serialize(
        new WriteChannel(Channels.newChannel(out)), schema);
    assertEquals(size, out.toByteArray().length);

    ByteArrayInputStream in = new ByteArrayInputStream(out.toByteArray());
    Schema deserialized = MessageSerializer.deserializeSchema(
        new ReadChannel(Channels.newChannel(in)));
    assertEquals(schema, deserialized);
    assertEquals(1, deserialized.getFields().size());
  }

  @Test
  public void testSchemaDictionaryMessageSerialization() throws IOException {
    DictionaryEncoding dictionary = new DictionaryEncoding(9L, false, new ArrowType.Int(8, true));
    Field field = new Field("test", new FieldType(true, ArrowType.Utf8.INSTANCE, dictionary, null), null);
    Schema schema = new Schema(Collections.singletonList(field));
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    long size = MessageSerializer.serialize(new WriteChannel(Channels.newChannel(out)), schema);
    assertEquals(size, out.toByteArray().length);

    ByteArrayInputStream in = new ByteArrayInputStream(out.toByteArray());
    Schema deserialized = MessageSerializer.deserializeSchema(new ReadChannel(Channels.newChannel(in)));
    assertEquals(schema, deserialized);
  }

  @Rule
  public ExpectedException expectedEx = ExpectedException.none();

  @Test
  public void testSerializeRecordBatch() throws IOException {
    byte[] validity = new byte[] {(byte) 255, 0};
    // second half is "undefined"
    byte[] values = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    BufferAllocator alloc = new RootAllocator(Long.MAX_VALUE);
    ArrowBuf validityb = buf(alloc, validity);
    ArrowBuf valuesb = buf(alloc, values);

    ArrowRecordBatch batch = new ArrowRecordBatch(
        16, asList(new ArrowFieldNode(16, 8)), asList(validityb, valuesb));

    ByteArrayOutputStream out = new ByteArrayOutputStream();
    MessageSerializer.serialize(new WriteChannel(Channels.newChannel(out)), batch);

    ByteArrayInputStream in = new ByteArrayInputStream(out.toByteArray());
    ReadChannel channel = new ReadChannel(Channels.newChannel(in));
    ArrowMessage deserialized = MessageSerializer.deserializeMessageBatch(channel, alloc);
    assertEquals(ArrowRecordBatch.class, deserialized.getClass());
    verifyBatch((ArrowRecordBatch) deserialized, validity, values);
  }

  public static Schema testSchema() {
    return new Schema(asList(new Field(
        "testField", FieldType.nullable(new ArrowType.Int(8, true)), Collections.<Field>emptyList())));
  }

  // Verifies batch contents matching test schema.
  public static void verifyBatch(ArrowRecordBatch batch, byte[] validity, byte[] values) {
    assertTrue(batch != null);
    List<ArrowFieldNode> nodes = batch.getNodes();
    assertEquals(1, nodes.size());
    ArrowFieldNode node = nodes.get(0);
    assertEquals(16, node.getLength());
    assertEquals(8, node.getNullCount());
    List<ArrowBuf> buffers = batch.getBuffers();
    assertEquals(2, buffers.size());
    assertArrayEquals(validity, MessageSerializerTest.array(buffers.get(0)));
    assertArrayEquals(values, MessageSerializerTest.array(buffers.get(1)));
  }

}

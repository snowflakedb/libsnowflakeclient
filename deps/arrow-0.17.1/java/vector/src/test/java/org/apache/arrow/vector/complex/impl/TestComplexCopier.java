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

package org.apache.arrow.vector.complex.impl;

import static org.junit.Assert.assertTrue;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.math.BigDecimal;

import org.apache.arrow.memory.BufferAllocator;
import org.apache.arrow.memory.RootAllocator;
import org.apache.arrow.vector.compare.VectorEqualsVisitor;
import org.apache.arrow.vector.complex.FixedSizeListVector;
import org.apache.arrow.vector.complex.ListVector;
import org.apache.arrow.vector.complex.MapVector;
import org.apache.arrow.vector.complex.StructVector;
import org.apache.arrow.vector.complex.reader.FieldReader;
import org.apache.arrow.vector.complex.writer.BaseWriter.StructWriter;
import org.apache.arrow.vector.complex.writer.FieldWriter;
import org.apache.arrow.vector.holders.DecimalHolder;
import org.apache.arrow.vector.types.Types;
import org.apache.arrow.vector.types.pojo.ArrowType;
import org.apache.arrow.vector.types.pojo.FieldType;
import org.apache.arrow.vector.util.DecimalUtility;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class TestComplexCopier {

  private BufferAllocator allocator;

  private static final int COUNT = 100;

  @Before
  public void init() {
    allocator = new RootAllocator(Long.MAX_VALUE);
  }

  @After
  public void terminate() throws Exception {
    allocator.close();
  }

  @Test
  public void testCopyFixedSizeListVector() {
    try (FixedSizeListVector from = FixedSizeListVector.empty("v", 3, allocator);
         FixedSizeListVector to = FixedSizeListVector.empty("v", 3, allocator)) {

      from.addOrGetVector(FieldType.nullable(Types.MinorType.INT.getType()));
      to.addOrGetVector(FieldType.nullable(Types.MinorType.INT.getType()));

      // populate from vector
      UnionFixedSizeListWriter writer = from.getWriter();
      for (int i = 0; i < COUNT; i++) {
        writer.startList();
        writer.integer().writeInt(i);
        writer.integer().writeInt(i * 2);
        writer.integer().writeInt(i * 3);
        writer.endList();
      }
      from.setValueCount(COUNT);
      to.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      for (int i = 0; i < COUNT; i++) {
        in.setPosition(i);
        out.setPosition(i);
        ComplexCopier.copy(in, out);
      }

      // validate equals
      assertTrue(VectorEqualsVisitor.vectorEquals(from, to));

    }
  }

  @Test
  public void testInvalidCopyFixedSizeListVector() {
    try (FixedSizeListVector from = FixedSizeListVector.empty("v", 3, allocator);
         FixedSizeListVector to = FixedSizeListVector.empty("v", 2, allocator)) {

      from.addOrGetVector(FieldType.nullable(Types.MinorType.INT.getType()));
      to.addOrGetVector(FieldType.nullable(Types.MinorType.INT.getType()));

      // populate from vector
      UnionFixedSizeListWriter writer = from.getWriter();
      for (int i = 0; i < COUNT; i++) {
        writer.startList();
        writer.integer().writeInt(i);
        writer.integer().writeInt(i * 2);
        writer.integer().writeInt(i * 3);
        writer.endList();
      }
      from.setValueCount(COUNT);
      to.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      IllegalStateException e = assertThrows(IllegalStateException.class,
          () -> ComplexCopier.copy(in, out));
      assertTrue(e.getMessage().contains("greater than listSize"));
    }
  }

  @Test
  public void testCopyMapVector() {
    try (final MapVector from = MapVector.empty("v", allocator, false);
         final MapVector to = MapVector.empty("v", allocator, false)) {

      from.allocateNew();

      UnionMapWriter mapWriter = from.getWriter();
      for (int i = 0; i < COUNT; i++) {
        mapWriter.setPosition(i);
        mapWriter.startMap();
        mapWriter.startEntry();
        mapWriter.key().integer().writeInt(i);
        mapWriter.value().integer().writeInt(i);
        mapWriter.endEntry();
        mapWriter.startEntry();
        mapWriter.key().decimal().writeDecimal(BigDecimal.valueOf(i * 2));
        mapWriter.value().decimal().writeDecimal(BigDecimal.valueOf(i * 2));
        mapWriter.endEntry();
        mapWriter.endMap();
      }

      from.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      for (int i = 0; i < COUNT; i++) {
        in.setPosition(i);
        out.setPosition(i);
        ComplexCopier.copy(in, out);
      }
      to.setValueCount(COUNT);

      // validate equals
      assertTrue(VectorEqualsVisitor.vectorEquals(from, to));
    }
  }

  @Test
  public void testCopyListVector() {
    try (ListVector from = ListVector.empty("v", allocator);
         ListVector to = ListVector.empty("v", allocator)) {

      UnionListWriter listWriter = from.getWriter();
      listWriter.allocate();

      for (int i = 0; i < COUNT; i++) {
        listWriter.setPosition(i);
        listWriter.startList();

        listWriter.integer().writeInt(i);
        listWriter.integer().writeInt(i * 2);

        listWriter.list().startList();
        listWriter.list().bigInt().writeBigInt(i);
        listWriter.list().bigInt().writeBigInt(i * 2);
        listWriter.list().bigInt().writeBigInt(i * 3);
        listWriter.list().endList();

        listWriter.list().startList();
        listWriter.list().decimal().writeDecimal(BigDecimal.valueOf(i * 4));
        listWriter.list().decimal().writeDecimal(BigDecimal.valueOf(i * 5));
        listWriter.list().endList();
        listWriter.endList();
      }
      from.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      for (int i = 0; i < COUNT; i++) {
        in.setPosition(i);
        out.setPosition(i);
        ComplexCopier.copy(in, out);
      }

      to.setValueCount(COUNT);

      // validate equals
      assertTrue(VectorEqualsVisitor.vectorEquals(from, to));

    }
  }

  @Test
  public void testCopyFixedSizedListOfDecimalsVector() {
    try (FixedSizeListVector from = FixedSizeListVector.empty("v", 4, allocator);
         FixedSizeListVector to = FixedSizeListVector.empty("v", 4, allocator)) {
      from.addOrGetVector(FieldType.nullable(new ArrowType.Decimal(3, 0)));
      to.addOrGetVector(FieldType.nullable(new ArrowType.Decimal(3, 0)));

      DecimalHolder holder = new DecimalHolder();
      holder.buffer = allocator.buffer(DecimalUtility.DECIMAL_BYTE_LENGTH);
      ArrowType arrowType = new ArrowType.Decimal(3, 0);

      // populate from vector
      UnionFixedSizeListWriter writer = from.getWriter();
      for (int i = 0; i < COUNT; i++) {
        writer.startList();
        writer.decimal().writeDecimal(BigDecimal.valueOf(i));

        DecimalUtility.writeBigDecimalToArrowBuf(new BigDecimal(i * 2), holder.buffer, 0);
        holder.start = 0;
        holder.scale = 0;
        holder.precision = 3;
        writer.decimal().write(holder);

        DecimalUtility.writeBigDecimalToArrowBuf(new BigDecimal(i * 3), holder.buffer, 0);
        writer.decimal().writeDecimal(0, holder.buffer, arrowType);

        writer.decimal().writeBigEndianBytesToDecimal(BigDecimal.valueOf(i * 4).unscaledValue().toByteArray(),
            arrowType);

        writer.endList();
      }
      from.setValueCount(COUNT);
      to.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      for (int i = 0; i < COUNT; i++) {
        in.setPosition(i);
        out.setPosition(i);
        ComplexCopier.copy(in, out);
      }

      // validate equals
      assertTrue(VectorEqualsVisitor.vectorEquals(from, to));
      holder.buffer.close();
    }
  }

  @Test
  public void testCopyUnionListWithDecimal() {
    try (ListVector from = ListVector.empty("v", allocator);
         ListVector to = ListVector.empty("v", allocator)) {

      UnionListWriter listWriter = from.getWriter();
      listWriter.allocate();

      for (int i = 0; i < COUNT; i++) {
        listWriter.setPosition(i);
        listWriter.startList();

        listWriter.decimal().writeDecimal(BigDecimal.valueOf(i * 2));
        listWriter.integer().writeInt(i);
        listWriter.decimal().writeBigEndianBytesToDecimal(BigDecimal.valueOf(i * 3).unscaledValue().toByteArray(),
            new ArrowType.Decimal(3, 0));

        listWriter.endList();
      }
      from.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      for (int i = 0; i < COUNT; i++) {
        in.setPosition(i);
        out.setPosition(i);
        ComplexCopier.copy(in, out);
      }

      to.setValueCount(COUNT);

      // validate equals
      assertTrue(VectorEqualsVisitor.vectorEquals(from, to));

    }
  }

  @Test
  public void testCopyStructVector() {
    try (final StructVector from = StructVector.empty("v", allocator);
         final StructVector to = StructVector.empty("v", allocator)) {

      from.allocateNewSafe();

      NullableStructWriter structWriter = from.getWriter();
      for (int i = 0; i < COUNT; i++) {
        structWriter.setPosition(i);
        structWriter.start();
        structWriter.integer("int").writeInt(i);
        structWriter.decimal("dec", 0, 38).writeDecimal(BigDecimal.valueOf(i * 2));
        StructWriter innerStructWriter = structWriter.struct("struc");
        innerStructWriter.start();
        innerStructWriter.integer("innerint").writeInt(i * 3);
        innerStructWriter.decimal("innerdec", 0, 38).writeDecimal(BigDecimal.valueOf(i * 4));
        innerStructWriter.decimal("innerdec", 0, 38).writeBigEndianBytesToDecimal(BigDecimal.valueOf(i * 4)
            .unscaledValue().toByteArray(), new ArrowType.Decimal(3, 0));
        innerStructWriter.end();
        structWriter.end();
      }

      from.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      for (int i = 0; i < COUNT; i++) {
        in.setPosition(i);
        out.setPosition(i);
        ComplexCopier.copy(in, out);
      }
      to.setValueCount(COUNT);

      // validate equals
      assertTrue(VectorEqualsVisitor.vectorEquals(from, to));
    }
  }

  @Test
  public void testCopyDecimalVectorWrongScale() {
    try (FixedSizeListVector from = FixedSizeListVector.empty("v", 3, allocator);
         FixedSizeListVector to = FixedSizeListVector.empty("v", 3, allocator)) {
      from.addOrGetVector(FieldType.nullable(new ArrowType.Decimal(3, 2)));
      to.addOrGetVector(FieldType.nullable(new ArrowType.Decimal(3, 1)));

      // populate from vector
      UnionFixedSizeListWriter writer = from.getWriter();
      for (int i = 0; i < COUNT; i++) {
        writer.startList();
        writer.decimal().writeDecimal(BigDecimal.valueOf(1.23));
        writer.decimal().writeDecimal(BigDecimal.valueOf(2.45));
        writer.endList();
      }
      from.setValueCount(COUNT);
      to.setValueCount(COUNT);

      // copy values
      FieldReader in = from.getReader();
      FieldWriter out = to.getWriter();
      UnsupportedOperationException e = assertThrows(UnsupportedOperationException.class,
          () -> ComplexCopier.copy(in, out));
      assertTrue(e.getMessage().contains("BigDecimal scale must equal that in the Arrow vector: 2 != 1"));
    }
  }
}

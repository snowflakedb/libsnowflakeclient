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

package org.apache.arrow.vector;

import static org.apache.arrow.util.Preconditions.checkArgument;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.apache.arrow.util.Collections2;
import org.apache.arrow.vector.ipc.message.ArrowFieldNode;
import org.apache.arrow.vector.ipc.message.ArrowRecordBatch;
import org.apache.arrow.vector.types.pojo.Field;

import io.netty.buffer.ArrowBuf;

/**
 * Loads buffers into vectors.
 */
public class VectorLoader {

  private final VectorSchemaRoot root;

  /**
   * Construct with a root to load and will create children in root based on schema.
   *
   * @param root the root to add vectors to based on schema
   */
  public VectorLoader(VectorSchemaRoot root) {
    this.root = root;
  }

  /**
   * Loads the record batch in the vectors.
   * will not close the record batch
   *
   * @param recordBatch the batch to load
   */
  public void load(ArrowRecordBatch recordBatch) {
    Iterator<ArrowBuf> buffers = recordBatch.getBuffers().iterator();
    Iterator<ArrowFieldNode> nodes = recordBatch.getNodes().iterator();
    for (FieldVector fieldVector : root.getFieldVectors()) {
      loadBuffers(fieldVector, fieldVector.getField(), buffers, nodes);
    }
    root.setRowCount(recordBatch.getLength());
    if (nodes.hasNext() || buffers.hasNext()) {
      throw new IllegalArgumentException("not all nodes and buffers were consumed. nodes: " +
          Collections2.toList(nodes).toString() + " buffers: " + Collections2.toList(buffers).toString());
    }
  }

  private void loadBuffers(
      FieldVector vector,
      Field field,
      Iterator<ArrowBuf> buffers,
      Iterator<ArrowFieldNode> nodes) {
    checkArgument(nodes.hasNext(), "no more field nodes for for field %s and vector %s", field, vector);
    ArrowFieldNode fieldNode = nodes.next();
    int bufferLayoutCount = TypeLayout.getTypeBufferCount(field.getType());
    List<ArrowBuf> ownBuffers = new ArrayList<>(bufferLayoutCount);
    for (int j = 0; j < bufferLayoutCount; j++) {
      ownBuffers.add(buffers.next());
    }
    try {
      vector.loadFieldBuffers(fieldNode, ownBuffers);
    } catch (RuntimeException e) {
      throw new IllegalArgumentException("Could not load buffers for field " +
          field + ". error message: " + e.getMessage(), e);
    }
    List<Field> children = field.getChildren();
    if (children.size() > 0) {
      List<FieldVector> childrenFromFields = vector.getChildrenFromFields();
      checkArgument(children.size() == childrenFromFields.size(),
          "should have as many children as in the schema: found %s expected %s",
          childrenFromFields.size(), children.size());
      for (int i = 0; i < childrenFromFields.size(); i++) {
        Field child = children.get(i);
        FieldVector fieldVector = childrenFromFields.get(i);
        loadBuffers(fieldVector, child, buffers, nodes);
      }
    }
  }

}

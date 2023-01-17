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

import org.apache.arrow.vector.complex.MapVector;
import org.apache.arrow.vector.complex.impl.UnionMapReader;
import org.apache.arrow.vector.complex.impl.UnionMapWriter;
import org.apache.arrow.vector.complex.reader.FieldReader;
import org.apache.arrow.vector.complex.writer.FieldWriter;

<@pp.dropOutputFile />
<@pp.changeOutputFile name="/org/apache/arrow/vector/complex/impl/ComplexCopier.java" />


<#include "/@includes/license.ftl" />

package org.apache.arrow.vector.complex.impl;

<#include "/@includes/vv_imports.ftl" />

/*
 * This class is generated using freemarker and the ${.template_name} template.
 */
@SuppressWarnings("unused")
public class ComplexCopier {

  /**
   * Do a deep copy of the value in input into output
   * @param input field to read from
   * @param output field to write to
   */
  public static void copy(FieldReader input, FieldWriter output) {
    writeValue(input, output);
  }

  private static void writeValue(FieldReader reader, FieldWriter writer) {
    final MinorType mt = reader.getMinorType();

      switch (mt) {

      case LIST:
      case FIXED_SIZE_LIST:
        if (reader.isSet()) {
          writer.startList();
          while (reader.next()) {
            writeValue(reader.reader(), getListWriterForReader(reader.reader(), writer));
          }
          writer.endList();
        }
        break;
      case MAP:
        if (reader.isSet()) {
          UnionMapWriter mapWriter = (UnionMapWriter) writer;
          UnionMapReader mapReader = (UnionMapReader) reader;

          mapWriter.startMap();
          while (mapReader.next()) {
            FieldReader structReader = reader.reader();
            UnionMapWriter structWriter = (UnionMapWriter) writer.struct();
            if (structReader.isSet()) {
              mapWriter.startEntry();
              writeValue(mapReader.key(), getStructWriterForReader(mapReader.key(), structWriter.key(), MapVector.KEY_NAME));
              writeValue(mapReader.value(), getStructWriterForReader(mapReader.value(), structWriter.value(), MapVector.VALUE_NAME));
              mapWriter.endEntry();
            }
          }
          mapWriter.endMap();
        }
        break;
      case STRUCT:
        if (reader.isSet()) {
          writer.start();
          for(String name : reader){
            FieldReader childReader = reader.reader(name);
            if(childReader.isSet()){
              writeValue(childReader, getStructWriterForReader(childReader, writer, name));
            }
          }
          writer.end();
        }
        break;
  <#list vv.types as type><#list type.minor as minor><#assign name = minor.class?cap_first />
  <#assign fields = minor.fields!type.fields />
  <#assign uncappedName = name?uncap_first/>

  <#if !minor.typeParams?? || minor.class?starts_with("Decimal") >

      case ${name?upper_case}:
        if (reader.isSet()) {
          Nullable${name}Holder ${uncappedName}Holder = new Nullable${name}Holder();
          reader.read(${uncappedName}Holder);
          if (${uncappedName}Holder.isSet == 1) {
            writer.write${name}(<#list fields as field>${uncappedName}Holder.${field.name}<#if field_has_next>, </#if></#list><#if minor.class == "Decimal">, new ArrowType.Decimal(decimalHolder.precision, decimalHolder.scale)</#if>);
          }
        }
        break;

  </#if>
  </#list></#list>
      }
 }

  private static FieldWriter getStructWriterForReader(FieldReader reader, StructWriter writer, String name) {
    switch (reader.getMinorType()) {
    <#list vv.types as type><#list type.minor as minor><#assign name = minor.class?cap_first />
    <#assign fields = minor.fields!type.fields />
    <#assign uncappedName = name?uncap_first/>
    <#if !minor.typeParams??>
    case ${name?upper_case}:
      return (FieldWriter) writer.<#if name == "Int">integer<#else>${uncappedName}</#if>(name);
    </#if>
    <#if minor.class == "Decimal">
    case ${name?upper_case}:
      if (reader.getField().getType() instanceof ArrowType.Decimal) {
        ArrowType.Decimal type = (ArrowType.Decimal) reader.getField().getType();
        return (FieldWriter) writer.${uncappedName}(name, type.getScale(), type.getPrecision());
      } else {
        return (FieldWriter) writer.${uncappedName}(name);
      }
    </#if>
    </#list></#list>
    case STRUCT:
      return (FieldWriter) writer.struct(name);
    case FIXED_SIZE_LIST:
    case LIST:
    case MAP:
      return (FieldWriter) writer.list(name);
    default:
      throw new UnsupportedOperationException(reader.getMinorType().toString());
    }
  }

  private static FieldWriter getListWriterForReader(FieldReader reader, ListWriter writer) {
    switch (reader.getMinorType()) {
    <#list vv.types as type><#list type.minor as minor><#assign name = minor.class?cap_first />
    <#assign fields = minor.fields!type.fields />
    <#assign uncappedName = name?uncap_first/>
    <#if !minor.typeParams?? || minor.class?starts_with("Decimal") >
    case ${name?upper_case}:
    return (FieldWriter) writer.<#if name == "Int">integer<#else>${uncappedName}</#if>();
    </#if>
    </#list></#list>
    case STRUCT:
      return (FieldWriter) writer.struct();
    case FIXED_SIZE_LIST:
    case LIST:
    case MAP:
      return (FieldWriter) writer.list();
    default:
      throw new UnsupportedOperationException(reader.getMinorType().toString());
    }
  }
}

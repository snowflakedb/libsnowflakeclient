#!/usr/bin/env python

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Generate boilerplate code for kernel instantiation and other tedious tasks


import io
import os

SIGNED_INTEGER_TYPES = ['Int8', 'Int16', 'Int32', 'Int64']
INTEGER_TYPES = SIGNED_INTEGER_TYPES + ['UInt8', 'UInt16', 'UInt32', 'UInt64']
FLOATING_TYPES = ['Float', 'Double']
NUMERIC_TYPES = ['Boolean'] + INTEGER_TYPES + FLOATING_TYPES
STRING_TYPES = ['String', 'LargeString']

DATE_TIME_TYPES = ['Date32', 'Date64', 'Time32', 'Time64', 'Timestamp',
                   'Duration']


def _format_type(name):
    return name + "Type"


class CastCodeGenerator(object):

    def __init__(self, type_name, out_types, parametric=False,
                 exclusions=None):
        self.type_name = type_name
        self.out_types = out_types
        self.parametric = parametric
        self.exclusions = exclusions

    def generate(self):
        buf = io.StringIO()
        print("#define {0}_CASES(TEMPLATE) \\"
              .format(self.type_name.upper()), file=buf)

        this_type = _format_type(self.type_name)

        templates = []
        for out_type in self.out_types:
            if not self.parametric and out_type == self.type_name:
                # Parametric types need T -> T cast generated
                continue
            templates.append("  TEMPLATE({0}, {1})"
                             .format(this_type, _format_type(out_type)))

        print(" \\\n".join(templates), file=buf)
        return buf.getvalue()


CAST_GENERATORS = [
    CastCodeGenerator('Boolean', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('UInt8', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Int8', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('UInt16', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Int16', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('UInt32', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('UInt64', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Int32', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Int64', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Float', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Double', NUMERIC_TYPES + STRING_TYPES),
    CastCodeGenerator('Decimal128', ['Decimal128'] + SIGNED_INTEGER_TYPES,
                      parametric=True),
    CastCodeGenerator('Date32', ['Date64']),
    CastCodeGenerator('Date64', ['Date32']),
    CastCodeGenerator('Time32', ['Time32', 'Time64'],
                      parametric=True),
    CastCodeGenerator('Time64', ['Time32', 'Time64'],
                      parametric=True),
    CastCodeGenerator('Timestamp', ['Date32', 'Date64', 'Timestamp'],
                      parametric=True),
    CastCodeGenerator('Duration', ['Duration'], parametric=True),
    CastCodeGenerator('Binary', ['String']),
    CastCodeGenerator('LargeBinary', ['LargeString']),
    CastCodeGenerator('String', NUMERIC_TYPES + ['Timestamp']),
    CastCodeGenerator('LargeString', NUMERIC_TYPES + ['Timestamp']),
    CastCodeGenerator('Dictionary',
                      INTEGER_TYPES + FLOATING_TYPES + DATE_TIME_TYPES +
                      ['Null', 'Binary', 'FixedSizeBinary', 'String',
                       'Decimal128'])
]


def generate_cast_code():
    blocks = [generator.generate() for generator in CAST_GENERATORS]
    return '\n'.join(blocks)


def write_file_with_preamble(path, code):
    preamble = """// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// THIS FILE IS AUTOMATICALLY GENERATED, DO NOT EDIT
// Generated by codegen.py script
"""

    with open(path, 'wb') as f:
        f.write(preamble.encode('utf-8'))
        f.write(code.encode('utf-8'))


def write_files():
    here = os.path.abspath(os.path.dirname(__file__))
    cast_code = generate_cast_code()
    write_file_with_preamble(os.path.join(here, 'cast_codegen_internal.h'),
                             cast_code)


if __name__ == '__main__':
    write_files()

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

class TestBufferOutputStream < Test::Unit::TestCase
  def test_new
    buffer = Arrow::ResizableBuffer.new(0)
    output_stream = Arrow::BufferOutputStream.new(buffer)
    output_stream.write("Hello")
    output_stream.close
    assert_equal("Hello", buffer.data.to_s)
  end

  def test_align
    buffer = Arrow::ResizableBuffer.new(0)
    output_stream = Arrow::BufferOutputStream.new(buffer)
    output_stream.write("Hello")
    output_stream.align(8)
    output_stream.close
    assert_equal("Hello\x00\x00\x00", buffer.data.to_s)
  end
end

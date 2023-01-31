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

class TestCast < Test::Unit::TestCase
  include Helper::Buildable
  include Helper::Omittable

  def test_safe
    data = [-1, 2, nil]
    assert_equal(build_int32_array(data),
                 build_int8_array(data).cast(Arrow::Int32DataType.new))
  end

  sub_test_case("allow-int-overflow") do
    def test_default
      assert_raise(Arrow::Error::Invalid) do
        build_int32_array([128]).cast(Arrow::Int8DataType.new)
      end
    end

    def test_true
      options = Arrow::CastOptions.new
      options.allow_int_overflow = true
      assert_equal(build_int8_array([-128]),
                   build_int32_array([128]).cast(Arrow::Int8DataType.new,
                                                 options))
    end
  end

  sub_test_case("allow-time-truncate") do
    def test_default
      after_epoch = 1504953190854 # 2017-09-09T10:33:10.854Z
      second_timestamp = Arrow::TimestampDataType.new(:second)
      assert_raise(Arrow::Error::Invalid) do
        build_timestamp_array(:milli, [after_epoch]).cast(second_timestamp)
      end
    end

    def test_true
      options = Arrow::CastOptions.new
      options.allow_time_truncate = true
      after_epoch_in_milli = 1504953190854 # 2017-09-09T10:33:10.854Z
      second_array = build_timestamp_array(:second,
                                           [after_epoch_in_milli / 1000])
      milli_array  = build_timestamp_array(:milli, [after_epoch_in_milli])
      second_timestamp = Arrow::TimestampDataType.new(:second)
      assert_equal(second_array,
                   milli_array.cast(second_timestamp, options))
    end
  end

  sub_test_case("allow-float-truncate") do
    def test_default
      assert_raise(Arrow::Error::Invalid) do
        build_float_array([1.1]).cast(Arrow::Int8DataType.new)
      end
    end

    def test_true
      options = Arrow::CastOptions.new
      options.allow_float_truncate = true
      int8_data_type = Arrow::Int8DataType.new
      assert_equal(build_int8_array([1]),
                   build_float_array([1.1]).cast(int8_data_type, options))
    end
  end

  sub_test_case("allow-invalid-utf8") do
    def test_default
      assert_raise(Arrow::Error::Invalid) do
        build_binary_array(["\xff"]).cast(Arrow::StringDataType.new)
      end
    end

    def test_true
      options = Arrow::CastOptions.new
      options.allow_invalid_utf8 = true
      string_data_type = Arrow::StringDataType.new
      assert_equal(build_string_array(["\xff"]),
                   build_binary_array(["\xff"]).cast(string_data_type, options))
    end
  end
end

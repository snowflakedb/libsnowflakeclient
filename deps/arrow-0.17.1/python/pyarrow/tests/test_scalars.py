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

import datetime
import pytest

import numpy as np

from pyarrow.compat import unittest
import pyarrow as pa


class TestScalars(unittest.TestCase):

    def test_null_singleton(self):
        with pytest.raises(Exception):
            pa.NAType()

    def test_nulls(self):
        arr = pa.array([None, None])
        for v in arr:
            assert v is pa.NA
            assert v.as_py() is None

    def test_null_equality(self):
        assert (pa.NA == pa.NA) is pa.NA
        assert (pa.NA == 1) is pa.NA

    def test_bool(self):
        arr = pa.array([True, None, False, None])

        v = arr[0]
        assert isinstance(v, pa.BooleanValue)
        assert repr(v) == "True"
        assert str(v) == "True"
        assert v.as_py() is True

        assert arr[1] is pa.NA

    def test_int64(self):
        arr = pa.array([1, 2, None])

        v = arr[0]
        assert isinstance(v, pa.Int64Value)
        assert repr(v) == "1"
        assert str(v) == "1"
        assert v.as_py() == 1
        assert v == 1

        assert arr[2] is pa.NA

    def test_double(self):
        arr = pa.array([1.5, None, 3])

        v = arr[0]
        assert isinstance(v, pa.DoubleValue)
        assert repr(v) == "1.5"
        assert str(v) == "1.5"
        assert v.as_py() == 1.5
        assert v == 1.5

        assert arr[1] is pa.NA

        v = arr[2]
        assert v.as_py() == 3.0

    def test_half_float(self):
        arr = pa.array([np.float16(1.5), None], type=pa.float16())
        v = arr[0]
        assert isinstance(v, pa.HalfFloatValue)
        assert repr(v) == "1.5"
        assert str(v) == "1.5"
        assert v.as_py() == 1.5
        assert v == 1.5

        assert arr[1] is pa.NA

    def test_string_unicode(self):
        arr = pa.array(['foo', None, 'mañana'])

        v = arr[0]
        assert isinstance(v, pa.StringValue)
        assert v.as_py() == 'foo'
        assert repr(v) == repr("foo")
        assert str(v) == "foo"
        assert v == 'foo'
        # Assert that newly created values are equal to the previously created
        # one.
        assert v == arr[0]

        assert arr[1] is pa.NA

        v = arr[2].as_py()
        assert v == 'mañana'
        assert isinstance(v, str)

    def test_large_string_unicode(self):
        arr = pa.array(['foo', None, 'mañana'], type=pa.large_string())

        v = arr[0]
        assert isinstance(v, pa.LargeStringValue)
        assert v.as_py() == 'foo'
        assert repr(v) == repr("foo")
        assert str(v) == "foo"
        assert v == 'foo'
        # Assert that newly created values are equal to the previously created
        # one.
        assert v == arr[0]

        assert arr[1] is pa.NA

        v = arr[2].as_py()
        assert v == 'mañana'
        assert isinstance(v, str)

    def test_bytes(self):
        arr = pa.array([b'foo', None, 'bar'])

        def check_value(v, expected):
            assert isinstance(v, pa.BinaryValue)
            assert v.as_py() == expected
            assert str(v) == str(expected)
            assert repr(v) == repr(expected)
            assert v == expected
            assert v != b'xxxxx'
            buf = v.as_buffer()
            assert isinstance(buf, pa.Buffer)
            assert buf.to_pybytes() == expected

        check_value(arr[0], b'foo')
        assert arr[1] is pa.NA
        check_value(arr[2], b'bar')

    def test_large_bytes(self):
        arr = pa.array([b'foo', None, 'bar'], type=pa.large_binary())

        def check_value(v, expected):
            assert isinstance(v, pa.LargeBinaryValue)
            assert v.as_py() == expected
            assert str(v) == str(expected)
            assert repr(v) == repr(expected)
            assert v == expected
            assert v != b'xxxxx'
            buf = v.as_buffer()
            assert isinstance(buf, pa.Buffer)
            assert buf.to_pybytes() == expected

        check_value(arr[0], b'foo')
        assert arr[1] is pa.NA
        check_value(arr[2], b'bar')

    def test_fixed_size_bytes(self):
        data = [b'foof', None, b'barb']
        arr = pa.array(data, type=pa.binary(4))

        v = arr[0]
        assert isinstance(v, pa.FixedSizeBinaryValue)
        assert v.as_py() == b'foof'

        assert arr[1] is pa.NA

        v = arr[2].as_py()
        assert v == b'barb'
        assert isinstance(v, bytes)

    def test_list(self):
        arr = pa.array([['foo', None], None, ['bar'], []])

        v = arr[0]
        assert len(v) == 2
        assert isinstance(v, pa.ListValue)
        assert repr(v) == "['foo', None]"
        assert v.as_py() == ['foo', None]
        assert v[0].as_py() == 'foo'
        assert v[1] is pa.NA
        assert v[-1] == v[1]
        assert v[-2] == v[0]
        with pytest.raises(IndexError):
            v[-3]
        with pytest.raises(IndexError):
            v[2]

        assert arr[1] is pa.NA

        v = arr[3]
        assert len(v) == 0

    def test_large_list(self):
        arr = pa.array([[123, None], None, [456], []],
                       type=pa.large_list(pa.int16()))

        v = arr[0]
        assert len(v) == 2
        assert isinstance(v, pa.LargeListValue)
        assert repr(v) == "[123, None]"
        assert v.as_py() == [123, None]
        assert v[0].as_py() == 123
        assert v[1] is pa.NA
        assert v[-1] == v[1]
        assert v[-2] == v[0]
        with pytest.raises(IndexError):
            v[-3]
        with pytest.raises(IndexError):
            v[2]

        assert arr[1] is pa.NA

        v = arr[3]
        assert len(v) == 0

    def test_map(self):
        arr = pa.array([[('a', 1), ('b', 2)], None, [], [('c', None)]],
                       pa.map_(pa.string(), pa.int8()))
        v = arr[0]
        assert len(v) == 2
        assert isinstance(v, pa.MapValue)
        assert repr(v) == repr([('a', 1), ('b', 2)])
        assert v.as_py() == [('a', 1), ('b', 2)]
        assert v[1] == ('b', 2)
        assert v[-1] == v[1]
        assert v[-2] == v[0]
        with pytest.raises(IndexError):
            v[-3]
        with pytest.raises(IndexError):
            v[2]

        assert arr[1] is pa.NA

        assert len(arr[2]) == 0

        v = arr[3]
        assert v == [('c', None)]
        assert len(v) == 1
        assert v[0][0] == 'c'
        assert v[0][1] is pa.NA

    def test_fixed_size_list(self):
        arr = pa.array([[1, None, 3], [4, 5, 6]], pa.list_(pa.int64(), 3))

        v = arr[0]
        assert len(v) == 3
        assert isinstance(v, pa.FixedSizeListValue)
        assert repr(v) == "[1, None, 3]"
        assert v.as_py() == [1, None, 3]
        assert v[0].as_py() == 1
        assert v[1] is pa.NA
        assert v[-1] == v[2]
        with pytest.raises(IndexError):
            v[-4]
        with pytest.raises(IndexError):
            v[3]

    def test_date(self):
        # ARROW-5125
        d1, d2 = datetime.date(3200, 1, 1), datetime.date(1960, 1, 1),
        extremes = pa.array([d1, d2], type=pa.date32())
        assert extremes[0] == d1
        assert extremes[1] == d2
        extremes = pa.array([d1, d2], type=pa.date64())
        assert extremes[0] == d1
        assert extremes[1] == d2

    @pytest.mark.pandas
    def test_timestamp(self):
        import pandas as pd
        arr = pd.date_range('2000-01-01 12:34:56', periods=10).values

        units = ['ns', 'us', 'ms', 's']

        for i, unit in enumerate(units):
            dtype = 'datetime64[{}]'.format(unit)
            arrow_arr = pa.Array.from_pandas(arr.astype(dtype))
            expected = pd.Timestamp('2000-01-01 12:34:56')

            assert arrow_arr[0].as_py() == expected
            assert arrow_arr[0].value * 1000**i == expected.value

            tz = 'America/New_York'
            arrow_type = pa.timestamp(unit, tz=tz)

            dtype = 'datetime64[{}]'.format(unit)
            arrow_arr = pa.Array.from_pandas(arr.astype(dtype),
                                             type=arrow_type)
            expected = (pd.Timestamp('2000-01-01 12:34:56')
                        .tz_localize('utc')
                        .tz_convert(tz))

            assert arrow_arr[0].as_py() == expected
            assert arrow_arr[0].value * 1000**i == expected.value

    @pytest.mark.nopandas
    def test_timestamp_nanos_nopandas(self):
        # ARROW-5450
        import pytz
        tz = 'America/New_York'
        ty = pa.timestamp('ns', tz=tz)
        arr = pa.array([
            946684800000000000,  # 2000-01-01 00:00:00
        ], type=ty)

        tzinfo = pytz.timezone(tz)
        expected = datetime.datetime(2000, 1, 1, tzinfo=tzinfo)
        expected = tzinfo.fromutc(expected)
        result = arr[0].as_py()
        assert result == expected
        assert result.year == 1999
        assert result.hour == 19

        # Non-zero nanos yields ValueError
        arr = pa.array([946684800000000001], type=ty)
        with pytest.raises(ValueError):
            arr[0].as_py()

    def test_timestamp_no_overflow(self):
        # ARROW-5450
        import pytz
        timestamp_rows = [
            datetime.datetime(1, 1, 1, 0, 0, 0, tzinfo=pytz.utc),
            None,
            datetime.datetime(9999, 12, 31, 23, 59, 59, 999999,
                              tzinfo=pytz.utc),
            datetime.datetime(1970, 1, 1, 0, 0, 0,
                              tzinfo=pytz.utc),
        ]
        arr = pa.array(timestamp_rows, pa.timestamp("us", tz="UTC"))
        result = arr.to_pylist()
        assert result == timestamp_rows

    def test_duration(self):
        arr = np.array([0, 3600000000000], dtype='timedelta64[ns]')

        units = ['us', 'ms', 's']

        for i, unit in enumerate(units):
            dtype = 'timedelta64[{}]'.format(unit)
            arrow_arr = pa.array(arr.astype(dtype))
            expected = datetime.timedelta(seconds=60*60)
            assert isinstance(arrow_arr[1].as_py(), datetime.timedelta)
            assert arrow_arr[1].as_py() == expected
            assert (arrow_arr[1].value * 1000**(i+1) ==
                    expected.total_seconds() * 1e9)

    @pytest.mark.pandas
    def test_duration_nanos_pandas(self):
        import pandas as pd
        arr = pa.array([0, 3600000000000], type=pa.duration('ns'))
        expected = pd.Timedelta('1 hour')
        assert isinstance(arr[1].as_py(), pd.Timedelta)
        assert arr[1].as_py() == expected
        assert arr[1].value == expected.value

        # Non-zero nanos work fine
        arr = pa.array([946684800000000001], type=pa.duration('ns'))
        assert arr[0].as_py() == pd.Timedelta(946684800000000001, unit='ns')

    @pytest.mark.nopandas
    def test_duration_nanos_nopandas(self):
        arr = pa.array([0, 3600000000000], pa.duration('ns'))
        expected = datetime.timedelta(seconds=60*60)
        assert isinstance(arr[1].as_py(), datetime.timedelta)
        assert arr[1].as_py() == expected
        assert arr[1].value == expected.total_seconds() * 1e9

        # Non-zero nanos yields ValueError
        arr = pa.array([946684800000000001], type=pa.duration('ns'))
        with pytest.raises(ValueError):
            arr[0].as_py()

    @pytest.mark.pandas
    def test_dictionary(self):
        import pandas as pd
        colors = ['red', 'green', 'blue']
        colors_dict = {'red': 0, 'green': 1, 'blue': 2}
        values = pd.Series(colors * 4)

        categorical = pd.Categorical(values, categories=colors)

        v = pa.DictionaryArray.from_arrays(categorical.codes,
                                           categorical.categories)
        for i, c in enumerate(values):
            assert v[i].as_py() == c
            assert v[i].dictionary_value == c
            assert v[i].index_value == colors_dict[c]

    def test_int_hash(self):
        # ARROW-640
        int_arr = pa.array([1, 1, 2, 1])
        assert hash(int_arr[0]) == hash(1)

    def test_float_hash(self):
        # ARROW-640
        float_arr = pa.array([1.4, 1.2, 2.5, 1.8])
        assert hash(float_arr[0]) == hash(1.4)

    def test_string_hash(self):
        # ARROW-640
        str_arr = pa.array(["foo", "bar"])
        assert hash(str_arr[1]) == hash("bar")

    def test_bytes_hash(self):
        # ARROW-640
        byte_arr = pa.array([b'foo', None, b'bar'])
        assert hash(byte_arr[2]) == hash(b"bar")

    def test_array_to_set(self):
        # ARROW-640
        arr = pa.array([1, 1, 2, 1])
        set_from_array = set(arr)
        assert isinstance(set_from_array, set)
        assert set_from_array == {1, 2}

    def test_struct_value_subscripting(self):
        ty = pa.struct([pa.field('x', pa.int16()),
                        pa.field('y', pa.float32())])
        arr = pa.array([(1, 2.5), (3, 4.5), (5, 6.5)], type=ty)

        assert arr[0]['x'] == 1
        assert arr[0]['y'] == 2.5
        assert arr[1]['x'] == 3
        assert arr[1]['y'] == 4.5
        assert arr[2]['x'] == 5
        assert arr[2]['y'] == 6.5

        with pytest.raises(IndexError):
            arr[4]['non-existent']

        with pytest.raises(KeyError):
            arr[0]['non-existent']

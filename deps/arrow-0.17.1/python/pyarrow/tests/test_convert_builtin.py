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

import pytest

from pyarrow.compat import unittest  # noqa
from pyarrow.pandas_compat import _pandas_api  # noqa
import pyarrow as pa

import collections
import datetime
import decimal
import itertools
import math
import traceback

import numpy as np
import pytz


int_type_pairs = [
    (np.int8, pa.int8()),
    (np.int16, pa.int16()),
    (np.int32, pa.int32()),
    (np.int64, pa.int64()),
    (np.uint8, pa.uint8()),
    (np.uint16, pa.uint16()),
    (np.uint32, pa.uint32()),
    (np.uint64, pa.uint64())]


np_int_types, _ = zip(*int_type_pairs)


class StrangeIterable:
    def __init__(self, lst):
        self.lst = lst

    def __iter__(self):
        return self.lst.__iter__()


class MyInt:
    def __init__(self, value):
        self.value = value

    def __int__(self):
        return self.value


class MyBrokenInt:
    def __int__(self):
        1/0  # MARKER


def check_struct_type(ty, expected):
    """
    Check a struct type is as expected, but not taking order into account.
    """
    assert pa.types.is_struct(ty)
    assert set(ty) == set(expected)


def test_iterable_types():
    arr1 = pa.array(StrangeIterable([0, 1, 2, 3]))
    arr2 = pa.array((0, 1, 2, 3))

    assert arr1.equals(arr2)


def test_empty_iterable():
    arr = pa.array(StrangeIterable([]))
    assert len(arr) == 0
    assert arr.null_count == 0
    assert arr.type == pa.null()
    assert arr.to_pylist() == []


def test_limited_iterator_types():
    arr1 = pa.array(iter(range(3)), type=pa.int64(), size=3)
    arr2 = pa.array((0, 1, 2))
    assert arr1.equals(arr2)


def test_limited_iterator_size_overflow():
    arr1 = pa.array(iter(range(3)), type=pa.int64(), size=2)
    arr2 = pa.array((0, 1))
    assert arr1.equals(arr2)


def test_limited_iterator_size_underflow():
    arr1 = pa.array(iter(range(3)), type=pa.int64(), size=10)
    arr2 = pa.array((0, 1, 2))
    assert arr1.equals(arr2)


def test_iterator_without_size():
    expected = pa.array((0, 1, 2))
    arr1 = pa.array(iter(range(3)))
    assert arr1.equals(expected)
    # Same with explicit type
    arr1 = pa.array(iter(range(3)), type=pa.int64())
    assert arr1.equals(expected)


def test_infinite_iterator():
    expected = pa.array((0, 1, 2))
    arr1 = pa.array(itertools.count(0), size=3)
    assert arr1.equals(expected)
    # Same with explicit type
    arr1 = pa.array(itertools.count(0), type=pa.int64(), size=3)
    assert arr1.equals(expected)


def _as_list(xs):
    return xs


def _as_tuple(xs):
    return tuple(xs)


def _as_deque(xs):
    # deque is a sequence while neither tuple nor list
    return collections.deque(xs)


def _as_dict_values(xs):
    # a dict values object is not a sequence, just a regular iterable
    dct = {k: v for k, v in enumerate(xs)}
    return dct.values()


def _as_numpy_array(xs):
    arr = np.empty(len(xs), dtype=object)
    arr[:] = xs
    return arr


parametrize_with_iterable_types = pytest.mark.parametrize(
    "seq", [_as_list, _as_tuple, _as_deque, _as_dict_values, _as_numpy_array])


@parametrize_with_iterable_types
def test_sequence_types(seq):
    arr1 = pa.array(seq([1, 2, 3]))
    arr2 = pa.array([1, 2, 3])

    assert arr1.equals(arr2)


@parametrize_with_iterable_types
def test_sequence_boolean(seq):
    expected = [True, None, False, None]
    arr = pa.array(seq(expected))
    assert len(arr) == 4
    assert arr.null_count == 2
    assert arr.type == pa.bool_()
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
def test_sequence_numpy_boolean(seq):
    expected = [np.bool(True), None, np.bool(False), None]
    arr = pa.array(seq(expected))
    assert len(arr) == 4
    assert arr.null_count == 2
    assert arr.type == pa.bool_()
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
def test_empty_list(seq):
    arr = pa.array(seq([]))
    assert len(arr) == 0
    assert arr.null_count == 0
    assert arr.type == pa.null()
    assert arr.to_pylist() == []


@parametrize_with_iterable_types
def test_nested_lists(seq):
    data = [[], [1, 2], None]
    arr = pa.array(seq(data))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.int64())
    assert arr.to_pylist() == data
    # With explicit type
    arr = pa.array(seq(data), type=pa.list_(pa.int32()))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.int32())
    assert arr.to_pylist() == data


@parametrize_with_iterable_types
def test_nested_large_lists(seq):
    data = [[], [1, 2], None]
    arr = pa.array(seq(data), type=pa.large_list(pa.int16()))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.large_list(pa.int16())
    assert arr.to_pylist() == data


@parametrize_with_iterable_types
def test_list_with_non_list(seq):
    # List types don't accept non-sequences
    with pytest.raises(TypeError):
        pa.array(seq([[], [1, 2], 3]), type=pa.list_(pa.int64()))
    with pytest.raises(TypeError):
        pa.array(seq([[], [1, 2], 3]), type=pa.large_list(pa.int64()))


@parametrize_with_iterable_types
def test_nested_arrays(seq):
    arr = pa.array(seq([np.array([], dtype=np.int64),
                        np.array([1, 2], dtype=np.int64), None]))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.int64())
    assert arr.to_pylist() == [[], [1, 2], None]


@parametrize_with_iterable_types
def test_nested_fixed_size_list(seq):
    # sequence of lists
    data = [[1, 2], [3, None], None]
    arr = pa.array(seq(data), type=pa.list_(pa.int64(), 2))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.int64(), 2)
    assert arr.to_pylist() == data

    # sequence of numpy arrays
    data = [np.array([1, 2], dtype='int64'), np.array([3, 4], dtype='int64'),
            None]
    arr = pa.array(seq(data), type=pa.list_(pa.int64(), 2))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.int64(), 2)
    assert arr.to_pylist() == [[1, 2], [3, 4], None]

    # incorrect length of the lists or arrays
    data = [[1, 2, 4], [3, None], None]
    for data in [[[1, 2, 3]], [np.array([1, 2, 4], dtype='int64')]]:
        with pytest.raises(
                ValueError, match="Length of item not correct: expected 2"):
            pa.array(seq(data), type=pa.list_(pa.int64(), 2))

    # with list size of 0
    data = [[], [], None]
    arr = pa.array(seq(data), type=pa.list_(pa.int64(), 0))
    assert len(arr) == 3
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.int64(), 0)
    assert arr.to_pylist() == [[], [], None]


@parametrize_with_iterable_types
def test_sequence_all_none(seq):
    arr = pa.array(seq([None, None]))
    assert len(arr) == 2
    assert arr.null_count == 2
    assert arr.type == pa.null()
    assert arr.to_pylist() == [None, None]


@parametrize_with_iterable_types
@pytest.mark.parametrize("np_scalar_pa_type", int_type_pairs)
def test_sequence_integer(seq, np_scalar_pa_type):
    np_scalar, pa_type = np_scalar_pa_type
    expected = [1, None, 3, None,
                np.iinfo(np_scalar).min, np.iinfo(np_scalar).max]
    arr = pa.array(seq(expected), type=pa_type)
    assert len(arr) == 6
    assert arr.null_count == 2
    assert arr.type == pa_type
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
@pytest.mark.parametrize("np_scalar_pa_type", int_type_pairs)
def test_sequence_integer_np_nan(seq, np_scalar_pa_type):
    # ARROW-2806: numpy.nan is a double value and thus should produce
    # a double array.
    _, pa_type = np_scalar_pa_type
    with pytest.raises(ValueError):
        pa.array(seq([np.nan]), type=pa_type, from_pandas=False)

    arr = pa.array(seq([np.nan]), type=pa_type, from_pandas=True)
    expected = [None]
    assert len(arr) == 1
    assert arr.null_count == 1
    assert arr.type == pa_type
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
@pytest.mark.parametrize("np_scalar_pa_type", int_type_pairs)
def test_sequence_integer_nested_np_nan(seq, np_scalar_pa_type):
    # ARROW-2806: numpy.nan is a double value and thus should produce
    # a double array.
    _, pa_type = np_scalar_pa_type
    with pytest.raises(ValueError):
        pa.array(seq([[np.nan]]), type=pa.list_(pa_type), from_pandas=False)

    arr = pa.array(seq([[np.nan]]), type=pa.list_(pa_type), from_pandas=True)
    expected = [[None]]
    assert len(arr) == 1
    assert arr.null_count == 0
    assert arr.type == pa.list_(pa_type)
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
def test_sequence_integer_inferred(seq):
    expected = [1, None, 3, None]
    arr = pa.array(seq(expected))
    assert len(arr) == 4
    assert arr.null_count == 2
    assert arr.type == pa.int64()
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
@pytest.mark.parametrize("np_scalar_pa_type", int_type_pairs)
def test_sequence_numpy_integer(seq, np_scalar_pa_type):
    np_scalar, pa_type = np_scalar_pa_type
    expected = [np_scalar(1), None, np_scalar(3), None,
                np_scalar(np.iinfo(np_scalar).min),
                np_scalar(np.iinfo(np_scalar).max)]
    arr = pa.array(seq(expected), type=pa_type)
    assert len(arr) == 6
    assert arr.null_count == 2
    assert arr.type == pa_type
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
@pytest.mark.parametrize("np_scalar_pa_type", int_type_pairs)
def test_sequence_numpy_integer_inferred(seq, np_scalar_pa_type):
    np_scalar, pa_type = np_scalar_pa_type
    expected = [np_scalar(1), None, np_scalar(3), None]
    expected += [np_scalar(np.iinfo(np_scalar).min),
                 np_scalar(np.iinfo(np_scalar).max)]
    arr = pa.array(seq(expected))
    assert len(arr) == 6
    assert arr.null_count == 2
    assert arr.type == pa_type
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
def test_sequence_custom_integers(seq):
    expected = [0, 42, 2**33 + 1, -2**63]
    data = list(map(MyInt, expected))
    arr = pa.array(seq(data), type=pa.int64())
    assert arr.to_pylist() == expected


@parametrize_with_iterable_types
def test_broken_integers(seq):
    data = [MyBrokenInt()]
    with pytest.raises(ZeroDivisionError) as exc_info:
        pa.array(seq(data), type=pa.int64())
    # Original traceback is kept
    tb_lines = traceback.format_tb(exc_info.tb)
    assert "# MARKER" in tb_lines[-1]


def test_numpy_scalars_mixed_type():
    # ARROW-4324
    data = [np.int32(10), np.float32(0.5)]
    arr = pa.array(data)
    expected = pa.array([10, 0.5], type='float64')
    assert arr.equals(expected)


@pytest.mark.xfail(reason="Type inference for uint64 not implemented",
                   raises=OverflowError)
def test_uint64_max_convert():
    data = [0, np.iinfo(np.uint64).max]

    arr = pa.array(data, type=pa.uint64())
    expected = pa.array(np.array(data, dtype='uint64'))
    assert arr.equals(expected)

    arr_inferred = pa.array(data)
    assert arr_inferred.equals(expected)


@pytest.mark.parametrize("bits", [8, 16, 32, 64])
def test_signed_integer_overflow(bits):
    ty = getattr(pa, "int%d" % bits)()
    # XXX ideally would always raise OverflowError
    with pytest.raises((OverflowError, pa.ArrowInvalid)):
        pa.array([2 ** (bits - 1)], ty)
    with pytest.raises((OverflowError, pa.ArrowInvalid)):
        pa.array([-2 ** (bits - 1) - 1], ty)


@pytest.mark.parametrize("bits", [8, 16, 32, 64])
def test_unsigned_integer_overflow(bits):
    ty = getattr(pa, "uint%d" % bits)()
    # XXX ideally would always raise OverflowError
    with pytest.raises((OverflowError, pa.ArrowInvalid)):
        pa.array([2 ** bits], ty)
    with pytest.raises((OverflowError, pa.ArrowInvalid)):
        pa.array([-1], ty)


def test_convert_with_mask():
    data = [1, 2, 3, 4, 5]
    mask = np.array([False, True, False, False, True])

    result = pa.array(data, mask=mask)
    expected = pa.array([1, None, 3, 4, None])

    assert result.equals(expected)

    # Mask wrong length
    with pytest.raises(ValueError):
        pa.array(data, mask=mask[1:])


def test_garbage_collection():
    import gc

    # Force the cyclic garbage collector to run
    gc.collect()

    bytes_before = pa.total_allocated_bytes()
    pa.array([1, None, 3, None])
    gc.collect()
    assert pa.total_allocated_bytes() == bytes_before


def test_sequence_double():
    data = [1.5, 1., None, 2.5, None, None]
    arr = pa.array(data)
    assert len(arr) == 6
    assert arr.null_count == 3
    assert arr.type == pa.float64()
    assert arr.to_pylist() == data


def test_double_auto_coerce_from_integer():
    # Done as part of ARROW-2814
    data = [1.5, 1., None, 2.5, None, None]
    arr = pa.array(data)

    data2 = [1.5, 1, None, 2.5, None, None]
    arr2 = pa.array(data2)

    assert arr.equals(arr2)

    data3 = [1, 1.5, None, 2.5, None, None]
    arr3 = pa.array(data3)

    data4 = [1., 1.5, None, 2.5, None, None]
    arr4 = pa.array(data4)

    assert arr3.equals(arr4)


def test_double_integer_coerce_representable_range():
    valid_values = [1.5, 1, 2, None, 1 << 53, -(1 << 53)]
    invalid_values = [1.5, 1, 2, None, (1 << 53) + 1]
    invalid_values2 = [1.5, 1, 2, None, -((1 << 53) + 1)]

    # it works
    pa.array(valid_values)

    # it fails
    with pytest.raises(ValueError):
        pa.array(invalid_values)

    with pytest.raises(ValueError):
        pa.array(invalid_values2)


def test_float32_integer_coerce_representable_range():
    f32 = np.float32
    valid_values = [f32(1.5), 1 << 24, -(1 << 24)]
    invalid_values = [f32(1.5), (1 << 24) + 1]
    invalid_values2 = [f32(1.5), -((1 << 24) + 1)]

    # it works
    pa.array(valid_values, type=pa.float32())

    # it fails
    with pytest.raises(ValueError):
        pa.array(invalid_values, type=pa.float32())

    with pytest.raises(ValueError):
        pa.array(invalid_values2, type=pa.float32())


def test_mixed_sequence_errors():
    with pytest.raises(ValueError, match="tried to convert to boolean"):
        pa.array([True, 'foo'], type=pa.bool_())

    with pytest.raises(ValueError, match="tried to convert to float32"):
        pa.array([1.5, 'foo'], type=pa.float32())

    with pytest.raises(ValueError, match="tried to convert to double"):
        pa.array([1.5, 'foo'])


@parametrize_with_iterable_types
@pytest.mark.parametrize("np_scalar,pa_type", [
    (np.float16, pa.float16()),
    (np.float32, pa.float32()),
    (np.float64, pa.float64())
])
@pytest.mark.parametrize("from_pandas", [True, False])
def test_sequence_numpy_double(seq, np_scalar, pa_type, from_pandas):
    data = [np_scalar(1.5), np_scalar(1), None, np_scalar(2.5), None, np.nan]
    arr = pa.array(seq(data), from_pandas=from_pandas)
    assert len(arr) == 6
    if from_pandas:
        assert arr.null_count == 3
    else:
        assert arr.null_count == 2
    if from_pandas:
        # The NaN is skipped in type inference, otherwise it forces a
        # float64 promotion
        assert arr.type == pa_type
    else:
        assert arr.type == pa.float64()

    assert arr.to_pylist()[:4] == data[:4]
    if from_pandas:
        assert arr.to_pylist()[5] is None
    else:
        assert np.isnan(arr.to_pylist()[5])


@pytest.mark.parametrize("from_pandas", [True, False])
@pytest.mark.parametrize("inner_seq", [np.array, list])
def test_ndarray_nested_numpy_double(from_pandas, inner_seq):
    # ARROW-2806
    data = np.array([
        inner_seq([1., 2.]),
        inner_seq([1., 2., 3.]),
        inner_seq([np.nan]),
        None
    ])
    arr = pa.array(data, from_pandas=from_pandas)
    assert len(arr) == 4
    assert arr.null_count == 1
    assert arr.type == pa.list_(pa.float64())
    if from_pandas:
        assert arr.to_pylist() == [[1.0, 2.0], [1.0, 2.0, 3.0], [None], None]
    else:
        np.testing.assert_equal(arr.to_pylist(),
                                [[1., 2.], [1., 2., 3.], [np.nan], None])


def test_nested_ndarray_in_object_array():
    # ARROW-4350
    arr = np.empty(2, dtype=object)
    arr[:] = [np.array([1, 2], dtype=np.int64),
              np.array([2, 3], dtype=np.int64)]

    arr2 = np.empty(2, dtype=object)
    arr2[0] = [3, 4]
    arr2[1] = [5, 6]

    expected_type = pa.list_(pa.list_(pa.int64()))
    assert pa.infer_type([arr]) == expected_type

    result = pa.array([arr, arr2])
    expected = pa.array([[[1, 2], [2, 3]], [[3, 4], [5, 6]]],
                        type=expected_type)

    assert result.equals(expected)

    # test case for len-1 arrays to ensure they are interpreted as
    # sublists and not scalars
    arr = np.empty(2, dtype=object)
    arr[:] = [np.array([1]), np.array([2])]
    result = pa.array([arr, arr])
    assert result.to_pylist() == [[[1], [2]], [[1], [2]]]


@pytest.mark.xfail(reason=("Type inference for multidimensional ndarray "
                           "not yet implemented"),
                   raises=AssertionError)
def test_multidimensional_ndarray_as_nested_list():
    # TODO(wesm): see ARROW-5645
    arr = np.array([[1, 2], [2, 3]], dtype=np.int64)
    arr2 = np.array([[3, 4], [5, 6]], dtype=np.int64)

    expected_type = pa.list_(pa.list_(pa.int64()))
    assert pa.infer_type([arr]) == expected_type

    result = pa.array([arr, arr2])
    expected = pa.array([[[1, 2], [2, 3]], [[3, 4], [5, 6]]],
                        type=expected_type)

    assert result.equals(expected)


def test_array_ignore_nan_from_pandas():
    # See ARROW-4324, this reverts logic that was introduced in
    # ARROW-2240
    with pytest.raises(ValueError):
        pa.array([np.nan, 'str'])

    arr = pa.array([np.nan, 'str'], from_pandas=True)
    expected = pa.array([None, 'str'])
    assert arr.equals(expected)


def test_nested_ndarray_different_dtypes():
    data = [
        np.array([1, 2, 3], dtype='int64'),
        None,
        np.array([4, 5, 6], dtype='uint32')
    ]

    arr = pa.array(data)
    expected = pa.array([[1, 2, 3], None, [4, 5, 6]],
                        type=pa.list_(pa.int64()))
    assert arr.equals(expected)

    t2 = pa.list_(pa.uint32())
    arr2 = pa.array(data, type=t2)
    expected2 = expected.cast(t2)
    assert arr2.equals(expected2)


def test_sequence_unicode():
    data = ['foo', 'bar', None, 'mañana']
    arr = pa.array(data)
    assert len(arr) == 4
    assert arr.null_count == 1
    assert arr.type == pa.string()
    assert arr.to_pylist() == data


def check_array_mixed_unicode_bytes(binary_type, string_type):
    values = ['qux', b'foo', bytearray(b'barz')]
    b_values = [b'qux', b'foo', b'barz']
    u_values = ['qux', 'foo', 'barz']

    arr = pa.array(values)
    expected = pa.array(b_values, type=pa.binary())
    assert arr.type == pa.binary()
    assert arr.equals(expected)

    arr = pa.array(values, type=binary_type)
    expected = pa.array(b_values, type=binary_type)
    assert arr.type == binary_type
    assert arr.equals(expected)

    arr = pa.array(values, type=string_type)
    expected = pa.array(u_values, type=string_type)
    assert arr.type == string_type
    assert arr.equals(expected)


def test_array_mixed_unicode_bytes():
    check_array_mixed_unicode_bytes(pa.binary(), pa.string())
    check_array_mixed_unicode_bytes(pa.large_binary(), pa.large_string())


@pytest.mark.large_memory
@pytest.mark.parametrize("ty", [pa.large_binary(), pa.large_string()])
def test_large_binary_array(ty):
    # Construct a large binary array with more than 4GB of data
    s = b"0123456789abcdefghijklmnopqrstuvwxyz" * 10
    nrepeats = math.ceil((2**32 + 5) / len(s))
    data = [s] * nrepeats
    arr = pa.array(data, type=ty)
    assert isinstance(arr, pa.Array)
    assert arr.type == ty
    assert len(arr) == nrepeats


@pytest.mark.large_memory
@pytest.mark.parametrize("ty", [pa.large_binary(), pa.large_string()])
def test_large_binary_value(ty):
    # Construct a large binary array with a single value larger than 4GB
    s = b"0123456789abcdefghijklmnopqrstuvwxyz"
    nrepeats = math.ceil((2**32 + 5) / len(s))
    arr = pa.array([b"foo", s * nrepeats, None, b"bar"], type=ty)
    assert isinstance(arr, pa.Array)
    assert arr.type == ty
    assert len(arr) == 4
    buf = arr[1].as_buffer()
    assert len(buf) == len(s) * nrepeats


def test_sequence_bytes():
    u1 = b'ma\xc3\xb1ana'

    data = [b'foo',
            memoryview(b'dada'),
            memoryview(b'd-a-t-a')[::2],  # non-contiguous is made contiguous
            u1.decode('utf-8'),  # unicode gets encoded,
            bytearray(b'bar'),
            None]
    for ty in [None, pa.binary(), pa.large_binary()]:
        arr = pa.array(data, type=ty)
        assert len(arr) == 6
        assert arr.null_count == 1
        assert arr.type == ty or pa.binary()
        assert arr.to_pylist() == [b'foo', b'dada', b'data', u1, b'bar', None]


@pytest.mark.parametrize("ty", [pa.string(), pa.large_string()])
def test_sequence_utf8_to_unicode(ty):
    # ARROW-1225
    data = [b'foo', None, b'bar']
    arr = pa.array(data, type=ty)
    assert arr.type == ty
    assert arr[0].as_py() == 'foo'

    # test a non-utf8 unicode string
    val = ('mañana').encode('utf-16-le')
    with pytest.raises(pa.ArrowInvalid):
        pa.array([val], type=ty)


def test_sequence_fixed_size_bytes():
    data = [b'foof', None, bytearray(b'barb'), b'2346']
    arr = pa.array(data, type=pa.binary(4))
    assert len(arr) == 4
    assert arr.null_count == 1
    assert arr.type == pa.binary(4)
    assert arr.to_pylist() == [b'foof', None, b'barb', b'2346']


def test_fixed_size_bytes_does_not_accept_varying_lengths():
    data = [b'foo', None, b'barb', b'2346']
    with pytest.raises(pa.ArrowInvalid):
        pa.array(data, type=pa.binary(4))


def test_sequence_date():
    data = [datetime.date(2000, 1, 1), None, datetime.date(1970, 1, 1),
            datetime.date(2040, 2, 26)]
    arr = pa.array(data)
    assert len(arr) == 4
    assert arr.type == pa.date32()
    assert arr.null_count == 1
    assert arr[0].as_py() == datetime.date(2000, 1, 1)
    assert arr[1].as_py() is None
    assert arr[2].as_py() == datetime.date(1970, 1, 1)
    assert arr[3].as_py() == datetime.date(2040, 2, 26)


@pytest.mark.parametrize('input',
                         [(pa.date32(), [10957, None]),
                          (pa.date64(), [10957 * 86400000, None])])
def test_sequence_explicit_types(input):
    t, ex_values = input
    data = [datetime.date(2000, 1, 1), None]
    arr = pa.array(data, type=t)
    arr2 = pa.array(ex_values, type=t)

    for x in [arr, arr2]:
        assert len(x) == 2
        assert x.type == t
        assert x.null_count == 1
        assert x[0].as_py() == datetime.date(2000, 1, 1)
        assert x[1] is pa.NA


def test_date32_overflow():
    # Overflow
    data3 = [2**32, None]
    with pytest.raises((OverflowError, pa.ArrowException)):
        pa.array(data3, type=pa.date32())


def test_sequence_timestamp():
    data = [
        datetime.datetime(2007, 7, 13, 1, 23, 34, 123456),
        None,
        datetime.datetime(2006, 1, 13, 12, 34, 56, 432539),
        datetime.datetime(2010, 8, 13, 5, 46, 57, 437699)
    ]
    arr = pa.array(data)
    assert len(arr) == 4
    assert arr.type == pa.timestamp('us')
    assert arr.null_count == 1
    assert arr[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                               23, 34, 123456)
    assert arr[1].as_py() is None
    assert arr[2].as_py() == datetime.datetime(2006, 1, 13, 12,
                                               34, 56, 432539)
    assert arr[3].as_py() == datetime.datetime(2010, 8, 13, 5,
                                               46, 57, 437699)


def test_sequence_numpy_timestamp():
    data = [
        np.datetime64(datetime.datetime(2007, 7, 13, 1, 23, 34, 123456)),
        None,
        np.datetime64(datetime.datetime(2006, 1, 13, 12, 34, 56, 432539)),
        np.datetime64(datetime.datetime(2010, 8, 13, 5, 46, 57, 437699))
    ]
    arr = pa.array(data)
    assert len(arr) == 4
    assert arr.type == pa.timestamp('us')
    assert arr.null_count == 1
    assert arr[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                               23, 34, 123456)
    assert arr[1].as_py() is None
    assert arr[2].as_py() == datetime.datetime(2006, 1, 13, 12,
                                               34, 56, 432539)
    assert arr[3].as_py() == datetime.datetime(2010, 8, 13, 5,
                                               46, 57, 437699)


def test_sequence_timestamp_with_unit():
    data = [
        datetime.datetime(2007, 7, 13, 1, 23, 34, 123456),
    ]

    s = pa.timestamp('s')
    ms = pa.timestamp('ms')
    us = pa.timestamp('us')

    arr_s = pa.array(data, type=s)
    assert len(arr_s) == 1
    assert arr_s.type == s
    assert arr_s[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                 23, 34, 0)

    arr_ms = pa.array(data, type=ms)
    assert len(arr_ms) == 1
    assert arr_ms.type == ms
    assert arr_ms[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                  23, 34, 123000)

    arr_us = pa.array(data, type=us)
    assert len(arr_us) == 1
    assert arr_us.type == us
    assert arr_us[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                  23, 34, 123456)


class MyDate(datetime.date):
    pass


class MyDatetime(datetime.datetime):
    pass


class MyTimedelta(datetime.timedelta):
    pass


def test_datetime_subclassing():
    data = [
        MyDate(2007, 7, 13),
    ]
    date_type = pa.date32()
    arr_date = pa.array(data, type=date_type)
    assert len(arr_date) == 1
    assert arr_date.type == date_type
    assert arr_date[0].as_py() == datetime.date(2007, 7, 13)

    data = [
        MyDatetime(2007, 7, 13, 1, 23, 34, 123456),
    ]

    s = pa.timestamp('s')
    ms = pa.timestamp('ms')
    us = pa.timestamp('us')

    arr_s = pa.array(data, type=s)
    assert len(arr_s) == 1
    assert arr_s.type == s
    assert arr_s[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                 23, 34, 0)

    arr_ms = pa.array(data, type=ms)
    assert len(arr_ms) == 1
    assert arr_ms.type == ms
    assert arr_ms[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                  23, 34, 123000)

    arr_us = pa.array(data, type=us)
    assert len(arr_us) == 1
    assert arr_us.type == us
    assert arr_us[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                  23, 34, 123456)

    data = [
        MyTimedelta(123, 456, 1002),
    ]

    s = pa.duration('s')
    ms = pa.duration('ms')
    us = pa.duration('us')

    arr_s = pa.array(data)
    assert len(arr_s) == 1
    assert arr_s.type == us
    assert arr_s[0].as_py() == datetime.timedelta(123, 456, 1002)

    arr_s = pa.array(data, type=s)
    assert len(arr_s) == 1
    assert arr_s.type == s
    assert arr_s[0].as_py() == datetime.timedelta(123, 456)

    arr_ms = pa.array(data, type=ms)
    assert len(arr_ms) == 1
    assert arr_ms.type == ms
    assert arr_ms[0].as_py() == datetime.timedelta(123, 456, 1000)

    arr_us = pa.array(data, type=us)
    assert len(arr_us) == 1
    assert arr_us.type == us
    assert arr_us[0].as_py() == datetime.timedelta(123, 456, 1002)


@pytest.mark.xfail(not _pandas_api.have_pandas,
                   reason="pandas required for nanosecond conversion")
def test_sequence_timestamp_nanoseconds():
    inputs = [
        [datetime.datetime(2007, 7, 13, 1, 23, 34, 123456)],
        [MyDatetime(2007, 7, 13, 1, 23, 34, 123456)]
    ]

    for data in inputs:
        ns = pa.timestamp('ns')
        arr_ns = pa.array(data, type=ns)
        assert len(arr_ns) == 1
        assert arr_ns.type == ns
        assert arr_ns[0].as_py() == datetime.datetime(2007, 7, 13, 1,
                                                      23, 34, 123456)


@pytest.mark.pandas
def test_sequence_timestamp_from_int_with_unit():
    # TODO(wesm): This test might be rewritten to assert the actual behavior
    # when pandas is not installed

    data = [1]

    s = pa.timestamp('s')
    ms = pa.timestamp('ms')
    us = pa.timestamp('us')
    ns = pa.timestamp('ns')

    arr_s = pa.array(data, type=s)
    assert len(arr_s) == 1
    assert arr_s.type == s
    assert repr(arr_s[0]) == "datetime.datetime(1970, 1, 1, 0, 0, 1)"
    assert str(arr_s[0]) == "1970-01-01 00:00:01"

    arr_ms = pa.array(data, type=ms)
    assert len(arr_ms) == 1
    assert arr_ms.type == ms
    assert repr(arr_ms[0]) == "datetime.datetime(1970, 1, 1, 0, 0, 0, 1000)"
    assert str(arr_ms[0]) == "1970-01-01 00:00:00.001000"

    arr_us = pa.array(data, type=us)
    assert len(arr_us) == 1
    assert arr_us.type == us
    assert repr(arr_us[0]) == "datetime.datetime(1970, 1, 1, 0, 0, 0, 1)"
    assert str(arr_us[0]) == "1970-01-01 00:00:00.000001"

    arr_ns = pa.array(data, type=ns)
    assert len(arr_ns) == 1
    assert arr_ns.type == ns
    assert repr(arr_ns[0]) == "Timestamp('1970-01-01 00:00:00.000000001')"
    assert str(arr_ns[0]) == "1970-01-01 00:00:00.000000001"

    expected_exc = TypeError

    class CustomClass():
        pass

    for ty in [ns, pa.date32(), pa.date64()]:
        with pytest.raises(expected_exc):
            pa.array([1, CustomClass()], type=ty)


@pytest.mark.parametrize('np_scalar', [True, False])
def test_sequence_duration(np_scalar):
    td1 = datetime.timedelta(2, 3601, 1)
    td2 = datetime.timedelta(1, 100, 1000)
    if np_scalar:
        data = [np.timedelta64(td1), None, np.timedelta64(td2)]
    else:
        data = [td1, None, td2]

    arr = pa.array(data)
    assert len(arr) == 3
    assert arr.type == pa.duration('us')
    assert arr.null_count == 1
    assert arr[0].as_py() == td1
    assert arr[1].as_py() is None
    assert arr[2].as_py() == td2


@pytest.mark.parametrize('unit', ['s', 'ms', 'us', 'ns'])
def test_sequence_duration_with_unit(unit):
    data = [
        datetime.timedelta(3, 22, 1001),
    ]
    expected = {'s': datetime.timedelta(3, 22),
                'ms': datetime.timedelta(3, 22, 1000),
                'us': datetime.timedelta(3, 22, 1001),
                'ns': datetime.timedelta(3, 22, 1001)}

    ty = pa.duration(unit)

    arr_s = pa.array(data, type=ty)
    assert len(arr_s) == 1
    assert arr_s.type == ty
    assert arr_s[0].as_py() == expected[unit]


@pytest.mark.parametrize('unit', ['s', 'ms', 'us', 'ns'])
def test_sequence_duration_from_int_with_unit(unit):
    data = [5]

    ty = pa.duration(unit)
    arr = pa.array(data, type=ty)
    assert len(arr) == 1
    assert arr.type == ty
    assert arr[0].value == 5


def test_sequence_duration_nested_lists():
    td1 = datetime.timedelta(1, 1, 1000)
    td2 = datetime.timedelta(1, 100)

    data = [[td1, None], [td1, td2]]

    arr = pa.array(data)
    assert len(arr) == 2
    assert arr.type == pa.list_(pa.duration('us'))
    assert arr.to_pylist() == data

    arr = pa.array(data, type=pa.list_(pa.duration('ms')))
    assert len(arr) == 2
    assert arr.type == pa.list_(pa.duration('ms'))
    assert arr.to_pylist() == data


def test_sequence_duration_nested_lists_numpy():
    td1 = datetime.timedelta(1, 1, 1000)
    td2 = datetime.timedelta(1, 100)

    data = [[np.timedelta64(td1), None],
            [np.timedelta64(td1), np.timedelta64(td2)]]

    arr = pa.array(data)
    assert len(arr) == 2
    assert arr.type == pa.list_(pa.duration('us'))
    assert arr.to_pylist() == [[td1, None], [td1, td2]]

    data = [np.array([np.timedelta64(td1), None], dtype='timedelta64[us]'),
            np.array([np.timedelta64(td1), np.timedelta64(td2)])]

    arr = pa.array(data)
    assert len(arr) == 2
    assert arr.type == pa.list_(pa.duration('us'))
    assert arr.to_pylist() == [[td1, None], [td1, td2]]


def test_sequence_nesting_levels():
    data = [1, 2, None]
    arr = pa.array(data)
    assert arr.type == pa.int64()
    assert arr.to_pylist() == data

    data = [[1], [2], None]
    arr = pa.array(data)
    assert arr.type == pa.list_(pa.int64())
    assert arr.to_pylist() == data

    data = [[1], [2, 3, 4], [None]]
    arr = pa.array(data)
    assert arr.type == pa.list_(pa.int64())
    assert arr.to_pylist() == data

    data = [None, [[None, 1]], [[2, 3, 4], None], [None]]
    arr = pa.array(data)
    assert arr.type == pa.list_(pa.list_(pa.int64()))
    assert arr.to_pylist() == data

    exceptions = (pa.ArrowInvalid, pa.ArrowTypeError)

    # Mixed nesting levels are rejected
    with pytest.raises(exceptions):
        pa.array([1, 2, [1]])

    with pytest.raises(exceptions):
        pa.array([1, 2, []])

    with pytest.raises(exceptions):
        pa.array([[1], [2], [None, [1]]])


def test_sequence_mixed_types_fails():
    data = ['a', 1, 2.0]
    with pytest.raises(pa.ArrowTypeError):
        pa.array(data)


def test_sequence_mixed_types_with_specified_type_fails():
    data = ['-10', '-5', {'a': 1}, '0', '5', '10']

    type = pa.string()
    with pytest.raises(TypeError):
        pa.array(data, type=type)


def test_sequence_decimal():
    data = [decimal.Decimal('1234.183'), decimal.Decimal('8094.234')]
    type = pa.decimal128(precision=7, scale=3)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == data


def test_sequence_decimal_different_precisions():
    data = [
        decimal.Decimal('1234234983.183'), decimal.Decimal('80943244.234')
    ]
    type = pa.decimal128(precision=13, scale=3)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == data


def test_sequence_decimal_no_scale():
    data = [decimal.Decimal('1234234983'), decimal.Decimal('8094324')]
    type = pa.decimal128(precision=10)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == data


def test_sequence_decimal_negative():
    data = [decimal.Decimal('-1234.234983'), decimal.Decimal('-8.094324')]
    type = pa.decimal128(precision=10, scale=6)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == data


def test_sequence_decimal_no_whole_part():
    data = [decimal.Decimal('-.4234983'), decimal.Decimal('.0103943')]
    type = pa.decimal128(precision=7, scale=7)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == data


def test_sequence_decimal_large_integer():
    data = [decimal.Decimal('-394029506937548693.42983'),
            decimal.Decimal('32358695912932.01033')]
    type = pa.decimal128(precision=23, scale=5)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == data


def test_sequence_decimal_from_integers():
    data = [0, 1, -39402950693754869342983]
    expected = [decimal.Decimal(x) for x in data]
    type = pa.decimal128(precision=28, scale=5)
    arr = pa.array(data, type=type)
    assert arr.to_pylist() == expected


def test_sequence_decimal_too_high_precision():
    # ARROW-6989 python decimal created from float has too high precision
    with pytest.raises(ValueError, match="precision out of range"):
        pa.array([decimal.Decimal(123.234)])


def test_range_types():
    arr1 = pa.array(range(3))
    arr2 = pa.array((0, 1, 2))
    assert arr1.equals(arr2)


def test_empty_range():
    arr = pa.array(range(0))
    assert len(arr) == 0
    assert arr.null_count == 0
    assert arr.type == pa.null()
    assert arr.to_pylist() == []


def test_structarray():
    arr = pa.StructArray.from_arrays([], names=[])
    assert arr.type == pa.struct([])
    assert len(arr) == 0
    assert arr.to_pylist() == []

    ints = pa.array([None, 2, 3], type=pa.int64())
    strs = pa.array(['a', None, 'c'], type=pa.string())
    bools = pa.array([True, False, None], type=pa.bool_())
    arr = pa.StructArray.from_arrays(
        [ints, strs, bools],
        ['ints', 'strs', 'bools'])

    expected = [
        {'ints': None, 'strs': 'a', 'bools': True},
        {'ints': 2, 'strs': None, 'bools': False},
        {'ints': 3, 'strs': 'c', 'bools': None},
    ]

    pylist = arr.to_pylist()
    assert pylist == expected, (pylist, expected)

    # len(names) != len(arrays)
    with pytest.raises(ValueError):
        pa.StructArray.from_arrays([ints], ['ints', 'strs'])


def test_struct_from_dicts():
    ty = pa.struct([pa.field('a', pa.int32()),
                    pa.field('b', pa.string()),
                    pa.field('c', pa.bool_())])
    arr = pa.array([], type=ty)
    assert arr.to_pylist() == []

    data = [{'a': 5, 'b': 'foo', 'c': True},
            {'a': 6, 'b': 'bar', 'c': False}]
    arr = pa.array(data, type=ty)
    assert arr.to_pylist() == data

    # With omitted values
    data = [{'a': 5, 'c': True},
            None,
            {},
            {'a': None, 'b': 'bar'}]
    arr = pa.array(data, type=ty)
    expected = [{'a': 5, 'b': None, 'c': True},
                None,
                {'a': None, 'b': None, 'c': None},
                {'a': None, 'b': 'bar', 'c': None}]
    assert arr.to_pylist() == expected


def test_struct_from_dicts_bytes_keys():
    # ARROW-6878
    ty = pa.struct([pa.field('a', pa.int32()),
                    pa.field('b', pa.string()),
                    pa.field('c', pa.bool_())])
    arr = pa.array([], type=ty)
    assert arr.to_pylist() == []

    data = [{b'a': 5, b'b': 'foo'},
            {b'a': 6, b'c': False}]
    arr = pa.array(data, type=ty)
    assert arr.to_pylist() == [
        {'a': 5, 'b': 'foo', 'c': None},
        {'a': 6, 'b': None, 'c': False},
    ]


def test_struct_from_tuples():
    ty = pa.struct([pa.field('a', pa.int32()),
                    pa.field('b', pa.string()),
                    pa.field('c', pa.bool_())])

    data = [(5, 'foo', True),
            (6, 'bar', False)]
    expected = [{'a': 5, 'b': 'foo', 'c': True},
                {'a': 6, 'b': 'bar', 'c': False}]
    arr = pa.array(data, type=ty)

    data_as_ndarray = np.empty(len(data), dtype=object)
    data_as_ndarray[:] = data
    arr2 = pa.array(data_as_ndarray, type=ty)
    assert arr.to_pylist() == expected

    assert arr.equals(arr2)

    # With omitted values
    data = [(5, 'foo', None),
            None,
            (6, None, False)]
    expected = [{'a': 5, 'b': 'foo', 'c': None},
                None,
                {'a': 6, 'b': None, 'c': False}]
    arr = pa.array(data, type=ty)
    assert arr.to_pylist() == expected

    # Invalid tuple size
    for tup in [(5, 'foo'), (), ('5', 'foo', True, None)]:
        with pytest.raises(ValueError, match="(?i)tuple size"):
            pa.array([tup], type=ty)


def test_struct_from_mixed_sequence():
    # It is forbidden to mix dicts and tuples when initializing a struct array
    ty = pa.struct([pa.field('a', pa.int32()),
                    pa.field('b', pa.string()),
                    pa.field('c', pa.bool_())])
    data = [(5, 'foo', True),
            {'a': 6, 'b': 'bar', 'c': False}]
    with pytest.raises(TypeError):
        pa.array(data, type=ty)


def test_struct_from_dicts_inference():
    expected_type = pa.struct([pa.field('a', pa.int64()),
                               pa.field('b', pa.string()),
                               pa.field('c', pa.bool_())])
    data = [{'a': 5, 'b': 'foo', 'c': True},
            {'a': 6, 'b': 'bar', 'c': False}]
    arr = pa.array(data)
    check_struct_type(arr.type, expected_type)
    assert arr.to_pylist() == data

    # With omitted values
    data = [{'a': 5, 'c': True},
            None,
            {},
            {'a': None, 'b': 'bar'}]
    expected = [{'a': 5, 'b': None, 'c': True},
                None,
                {'a': None, 'b': None, 'c': None},
                {'a': None, 'b': 'bar', 'c': None}]
    arr = pa.array(data)
    data_as_ndarray = np.empty(len(data), dtype=object)
    data_as_ndarray[:] = data
    arr2 = pa.array(data)

    check_struct_type(arr.type, expected_type)
    assert arr.to_pylist() == expected
    assert arr.equals(arr2)

    # Nested
    expected_type = pa.struct([
        pa.field('a', pa.struct([pa.field('aa', pa.list_(pa.int64())),
                                 pa.field('ab', pa.bool_())])),
        pa.field('b', pa.string())])
    data = [{'a': {'aa': [5, 6], 'ab': True}, 'b': 'foo'},
            {'a': {'aa': None, 'ab': False}, 'b': None},
            {'a': None, 'b': 'bar'}]
    arr = pa.array(data)
    assert arr.to_pylist() == data

    # Edge cases
    arr = pa.array([{}])
    assert arr.type == pa.struct([])
    assert arr.to_pylist() == [{}]

    # Mixing structs and scalars is rejected
    with pytest.raises((pa.ArrowInvalid, pa.ArrowTypeError)):
        pa.array([1, {'a': 2}])


def test_structarray_from_arrays_coerce():
    # ARROW-1706
    ints = [None, 2, 3]
    strs = ['a', None, 'c']
    bools = [True, False, None]
    ints_nonnull = [1, 2, 3]

    arrays = [ints, strs, bools, ints_nonnull]
    result = pa.StructArray.from_arrays(arrays,
                                        ['ints', 'strs', 'bools',
                                         'int_nonnull'])
    expected = pa.StructArray.from_arrays(
        [pa.array(ints, type='int64'),
         pa.array(strs, type='utf8'),
         pa.array(bools),
         pa.array(ints_nonnull, type='int64')],
        ['ints', 'strs', 'bools', 'int_nonnull'])

    with pytest.raises(ValueError):
        pa.StructArray.from_arrays(arrays)

    assert result.equals(expected)


def test_decimal_array_with_none_and_nan():
    values = [decimal.Decimal('1.234'), None, np.nan, decimal.Decimal('nan')]

    with pytest.raises(TypeError):
        # ARROW-6227: Without from_pandas=True, NaN is considered a float
        array = pa.array(values)

    array = pa.array(values, from_pandas=True)
    assert array.type == pa.decimal128(4, 3)
    assert array.to_pylist() == values[:2] + [None, None]

    array = pa.array(values, type=pa.decimal128(10, 4), from_pandas=True)
    assert array.to_pylist() == [decimal.Decimal('1.2340'), None, None, None]


@pytest.mark.parametrize('tz,name', [
    (pytz.FixedOffset(90), '+01:30'),
    (pytz.FixedOffset(-90), '-01:30'),
    (pytz.utc, 'UTC'),
    (pytz.timezone('America/New_York'), 'America/New_York')
])
def test_timezone_string(tz, name):
    assert pa.lib.tzinfo_to_string(tz) == name
    assert pa.lib.string_to_tzinfo(name) == tz


def test_map_from_dicts():
    data = [[{'key': b'a', 'value': 1}, {'key': b'b', 'value': 2}],
            [{'key': b'c', 'value': 3}],
            [{'key': b'd', 'value': 4}, {'key': b'e', 'value': 5},
             {'key': b'f', 'value': None}],
            [{'key': b'g', 'value': 7}]]
    expected = [[(d['key'], d['value']) for d in entry] for entry in data]

    arr = pa.array(expected, type=pa.map_(pa.binary(), pa.int32()))

    assert arr.to_pylist() == expected

    # With omitted values
    data[1] = None
    expected[1] = None

    arr = pa.array(expected, type=pa.map_(pa.binary(), pa.int32()))

    assert arr.to_pylist() == expected

    # Invalid dictionary
    for entry in [[{'value': 5}], [{}], [{'k': 1, 'v': 2}]]:
        with pytest.raises(ValueError, match="Invalid Map"):
            pa.array([entry], type=pa.map_('i4', 'i4'))

    # Invalid dictionary types
    for entry in [[{'key': '1', 'value': 5}], [{'key': {'value': 2}}]]:
        with pytest.raises(TypeError, match="integer is required"):
            pa.array([entry], type=pa.map_('i4', 'i4'))


def test_map_from_tuples():
    expected = [[(b'a', 1), (b'b', 2)],
                [(b'c', 3)],
                [(b'd', 4), (b'e', 5), (b'f', None)],
                [(b'g', 7)]]

    arr = pa.array(expected, type=pa.map_(pa.binary(), pa.int32()))

    assert arr.to_pylist() == expected

    # With omitted values
    expected[1] = None

    arr = pa.array(expected, type=pa.map_(pa.binary(), pa.int32()))

    assert arr.to_pylist() == expected

    # Invalid tuple size
    for entry in [[(5,)], [()], [('5', 'foo', True)]]:
        with pytest.raises(ValueError, match="(?i)tuple size"):
            pa.array([entry], type=pa.map_('i4', 'i4'))

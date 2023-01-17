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

import pickle
import weakref

import numpy as np
import pyarrow as pa

import pytest


class IntegerType(pa.PyExtensionType):

    def __init__(self):
        pa.PyExtensionType.__init__(self, pa.int64())

    def __reduce__(self):
        return IntegerType, ()


class UuidType(pa.PyExtensionType):

    def __init__(self):
        pa.PyExtensionType.__init__(self, pa.binary(16))

    def __reduce__(self):
        return UuidType, ()


class ParamExtType(pa.PyExtensionType):

    def __init__(self, width):
        self._width = width
        pa.PyExtensionType.__init__(self, pa.binary(width))

    @property
    def width(self):
        return self._width

    def __reduce__(self):
        return ParamExtType, (self.width,)


def ipc_write_batch(batch):
    stream = pa.BufferOutputStream()
    writer = pa.RecordBatchStreamWriter(stream, batch.schema)
    writer.write_batch(batch)
    writer.close()
    return stream.getvalue()


def ipc_read_batch(buf):
    reader = pa.RecordBatchStreamReader(buf)
    return reader.read_next_batch()


def test_ext_type_basics():
    ty = UuidType()
    assert ty.extension_name == "arrow.py_extension_type"


def test_ext_type__lifetime():
    ty = UuidType()
    wr = weakref.ref(ty)
    del ty
    assert wr() is None


def test_ext_type__storage_type():
    ty = UuidType()
    assert ty.storage_type == pa.binary(16)
    assert ty.__class__ is UuidType
    ty = ParamExtType(5)
    assert ty.storage_type == pa.binary(5)
    assert ty.__class__ is ParamExtType


def test_uuid_type_pickle():
    for proto in range(0, pickle.HIGHEST_PROTOCOL + 1):
        ty = UuidType()
        ser = pickle.dumps(ty, protocol=proto)
        del ty
        ty = pickle.loads(ser)
        wr = weakref.ref(ty)
        assert ty.extension_name == "arrow.py_extension_type"
        del ty
        assert wr() is None


def test_ext_type_equality():
    a = ParamExtType(5)
    b = ParamExtType(6)
    c = ParamExtType(6)
    assert a != b
    assert b == c
    d = UuidType()
    e = UuidType()
    assert a != d
    assert d == e


def test_ext_array_basics():
    ty = ParamExtType(3)
    storage = pa.array([b"foo", b"bar"], type=pa.binary(3))
    arr = pa.ExtensionArray.from_storage(ty, storage)
    arr.validate()
    assert arr.type is ty
    assert arr.storage.equals(storage)


def test_ext_array_lifetime():
    ty = ParamExtType(3)
    storage = pa.array([b"foo", b"bar"], type=pa.binary(3))
    arr = pa.ExtensionArray.from_storage(ty, storage)

    refs = [weakref.ref(ty), weakref.ref(arr), weakref.ref(storage)]
    del ty, storage, arr
    for ref in refs:
        assert ref() is None


def test_ext_array_errors():
    ty = ParamExtType(4)
    storage = pa.array([b"foo", b"bar"], type=pa.binary(3))
    with pytest.raises(TypeError, match="Incompatible storage type"):
        pa.ExtensionArray.from_storage(ty, storage)


def test_ext_array_equality():
    storage1 = pa.array([b"0123456789abcdef"], type=pa.binary(16))
    storage2 = pa.array([b"0123456789abcdef"], type=pa.binary(16))
    storage3 = pa.array([], type=pa.binary(16))
    ty1 = UuidType()
    ty2 = ParamExtType(16)

    a = pa.ExtensionArray.from_storage(ty1, storage1)
    b = pa.ExtensionArray.from_storage(ty1, storage2)
    assert a.equals(b)
    c = pa.ExtensionArray.from_storage(ty1, storage3)
    assert not a.equals(c)
    d = pa.ExtensionArray.from_storage(ty2, storage1)
    assert not a.equals(d)
    e = pa.ExtensionArray.from_storage(ty2, storage2)
    assert d.equals(e)
    f = pa.ExtensionArray.from_storage(ty2, storage3)
    assert not d.equals(f)


def test_ext_array_pickling():
    for proto in range(0, pickle.HIGHEST_PROTOCOL + 1):
        ty = ParamExtType(3)
        storage = pa.array([b"foo", b"bar"], type=pa.binary(3))
        arr = pa.ExtensionArray.from_storage(ty, storage)
        ser = pickle.dumps(arr, protocol=proto)
        del ty, storage, arr
        arr = pickle.loads(ser)
        arr.validate()
        assert isinstance(arr, pa.ExtensionArray)
        assert arr.type == ParamExtType(3)
        assert arr.type.storage_type == pa.binary(3)
        assert arr.storage.type == pa.binary(3)
        assert arr.storage.to_pylist() == [b"foo", b"bar"]


def test_cast_kernel_on_extension_arrays():
    # test array casting
    storage = pa.array([1, 2, 3, 4], pa.int64())
    arr = pa.ExtensionArray.from_storage(IntegerType(), storage)

    # test that no allocation happens during identity cast
    allocated_before_cast = pa.total_allocated_bytes()
    casted = arr.cast(pa.int64())
    assert pa.total_allocated_bytes() == allocated_before_cast

    cases = [
        (pa.int64(), pa.Int64Array),
        (pa.int32(), pa.Int32Array),
        (pa.int16(), pa.Int16Array),
        (pa.uint64(), pa.UInt64Array),
        (pa.uint32(), pa.UInt32Array),
        (pa.uint16(), pa.UInt16Array)
    ]
    for typ, klass in cases:
        casted = arr.cast(typ)
        assert casted.type == typ
        assert isinstance(casted, klass)

    # test chunked array casting
    arr = pa.chunked_array([arr, arr])
    casted = arr.cast(pa.int16())
    assert casted.type == pa.int16()
    assert isinstance(casted, pa.ChunkedArray)


def test_casting_to_extension_type_raises():
    arr = pa.array([1, 2, 3, 4], pa.int64())
    with pytest.raises(pa.ArrowNotImplementedError):
        arr.cast(IntegerType())


def example_batch():
    ty = ParamExtType(3)
    storage = pa.array([b"foo", b"bar"], type=pa.binary(3))
    arr = pa.ExtensionArray.from_storage(ty, storage)
    return pa.RecordBatch.from_arrays([arr], ["exts"])


def check_example_batch(batch):
    arr = batch.column(0)
    assert isinstance(arr, pa.ExtensionArray)
    assert arr.type.storage_type == pa.binary(3)
    assert arr.storage.to_pylist() == [b"foo", b"bar"]
    return arr


def test_ipc():
    batch = example_batch()
    buf = ipc_write_batch(batch)
    del batch

    batch = ipc_read_batch(buf)
    arr = check_example_batch(batch)
    assert arr.type == ParamExtType(3)


def test_ipc_unknown_type():
    batch = example_batch()
    buf = ipc_write_batch(batch)
    del batch

    orig_type = ParamExtType
    try:
        # Simulate the original Python type being unavailable.
        # Deserialization should not fail but return a placeholder type.
        del globals()['ParamExtType']

        batch = ipc_read_batch(buf)
        arr = check_example_batch(batch)
        assert isinstance(arr.type, pa.UnknownExtensionType)

        # Can be serialized again
        buf2 = ipc_write_batch(batch)
        del batch, arr

        batch = ipc_read_batch(buf2)
        arr = check_example_batch(batch)
        assert isinstance(arr.type, pa.UnknownExtensionType)
    finally:
        globals()['ParamExtType'] = orig_type

    # Deserialize again with the type restored
    batch = ipc_read_batch(buf2)
    arr = check_example_batch(batch)
    assert arr.type == ParamExtType(3)


class PeriodArray(pa.ExtensionArray):
    pass


class PeriodType(pa.ExtensionType):
    def __init__(self, freq):
        # attributes need to be set first before calling
        # super init (as that calls serialize)
        self._freq = freq
        pa.ExtensionType.__init__(self, pa.int64(), 'pandas.period')

    @property
    def freq(self):
        return self._freq

    def __arrow_ext_serialize__(self):
        return "freq={}".format(self.freq).encode()

    @classmethod
    def __arrow_ext_deserialize__(cls, storage_type, serialized):
        serialized = serialized.decode()
        assert serialized.startswith("freq=")
        freq = serialized.split('=')[1]
        return PeriodType(freq)

    def __eq__(self, other):
        if isinstance(other, pa.BaseExtensionType):
            return (type(self) == type(other) and
                    self.freq == other.freq)
        else:
            return NotImplemented


class PeriodTypeWithClass(PeriodType):
    def __init__(self, freq):
        PeriodType.__init__(self, freq)

    def __arrow_ext_class__(self):
        return PeriodArray

    @classmethod
    def __arrow_ext_deserialize__(cls, storage_type, serialized):
        freq = PeriodType.__arrow_ext_deserialize__(
            storage_type, serialized).freq
        return PeriodTypeWithClass(freq)


@pytest.fixture(params=[PeriodType('D'), PeriodTypeWithClass('D')])
def registered_period_type(request):
    # setup
    period_type = request.param
    period_class = period_type.__arrow_ext_class__()
    pa.register_extension_type(period_type)
    yield period_type, period_class
    # teardown
    try:
        pa.unregister_extension_type('pandas.period')
    except KeyError:
        pass


def test_generic_ext_type():
    period_type = PeriodType('D')
    assert period_type.extension_name == "pandas.period"
    assert period_type.storage_type == pa.int64()
    # default ext_class expected.
    assert period_type.__arrow_ext_class__() == pa.ExtensionArray


def test_generic_ext_type_ipc(registered_period_type):
    period_type, period_class = registered_period_type
    storage = pa.array([1, 2, 3, 4], pa.int64())
    arr = pa.ExtensionArray.from_storage(period_type, storage)
    batch = pa.RecordBatch.from_arrays([arr], ["ext"])
    # check the built array has exactly the expected clss
    assert type(arr) == period_class

    buf = ipc_write_batch(batch)
    del batch
    batch = ipc_read_batch(buf)

    result = batch.column(0)
    # check the deserialized array class is the expected one
    assert type(result) == period_class
    assert result.type.extension_name == "pandas.period"
    assert arr.storage.to_pylist() == [1, 2, 3, 4]

    # we get back an actual PeriodType
    assert isinstance(result.type, PeriodType)
    assert result.type.freq == 'D'
    assert result.type == period_type

    # using different parametrization as how it was registered
    period_type_H = period_type.__class__('H')
    assert period_type_H.extension_name == "pandas.period"
    assert period_type_H.freq == 'H'

    arr = pa.ExtensionArray.from_storage(period_type_H, storage)
    batch = pa.RecordBatch.from_arrays([arr], ["ext"])

    buf = ipc_write_batch(batch)
    del batch
    batch = ipc_read_batch(buf)
    result = batch.column(0)
    assert isinstance(result.type, PeriodType)
    assert result.type.freq == 'H'
    assert type(result) == period_class


def test_generic_ext_type_ipc_unknown(registered_period_type):
    period_type, _ = registered_period_type
    storage = pa.array([1, 2, 3, 4], pa.int64())
    arr = pa.ExtensionArray.from_storage(period_type, storage)
    batch = pa.RecordBatch.from_arrays([arr], ["ext"])

    buf = ipc_write_batch(batch)
    del batch

    # unregister type before loading again => reading unknown extension type
    # as plain array (but metadata in schema's field are preserved)
    pa.unregister_extension_type('pandas.period')

    batch = ipc_read_batch(buf)
    result = batch.column(0)

    assert isinstance(result, pa.Int64Array)
    ext_field = batch.schema.field('ext')
    assert ext_field.metadata == {
        b'ARROW:extension:metadata': b'freq=D',
        b'ARROW:extension:name': b'pandas.period'
    }


def test_generic_ext_type_equality():
    period_type = PeriodType('D')
    assert period_type.extension_name == "pandas.period"

    period_type2 = PeriodType('D')
    period_type3 = PeriodType('H')
    assert period_type == period_type2
    assert not period_type == period_type3


def test_generic_ext_type_register(registered_period_type):
    # test that trying to register other type does not segfault
    with pytest.raises(TypeError):
        pa.register_extension_type(pa.string())

    # register second time raises KeyError
    period_type = PeriodType('D')
    with pytest.raises(KeyError):
        pa.register_extension_type(period_type)


@pytest.mark.parquet
def test_parquet(tmpdir, registered_period_type):
    # parquet support for extension types
    period_type, period_class = registered_period_type
    storage = pa.array([1, 2, 3, 4], pa.int64())
    arr = pa.ExtensionArray.from_storage(period_type, storage)
    table = pa.table([arr], names=["ext"])

    import pyarrow.parquet as pq

    filename = tmpdir / 'extension_type.parquet'
    pq.write_table(table, filename)

    # stored in parquet as storage type but with extension metadata saved
    # in the serialized arrow schema
    meta = pq.read_metadata(filename)
    assert meta.schema.column(0).physical_type == "INT64"
    assert b"ARROW:schema" in meta.metadata

    import base64
    decoded_schema = base64.b64decode(meta.metadata[b"ARROW:schema"])
    schema = pa.ipc.read_schema(pa.BufferReader(decoded_schema))
    assert schema.field("ext").metadata == {
        b'ARROW:extension:metadata': b'freq=D',
        b'ARROW:extension:name': b'pandas.period'}

    # when reading in, properly create extension type if it is registered
    result = pq.read_table(filename)
    assert result.column("ext").type == period_type
    # get the exact array class defined by the registered type.
    result_array = result.column("ext").chunk(0)
    assert type(result_array) == period_class

    # when the type is not registered, read in as storage type
    pa.unregister_extension_type(period_type.extension_name)
    result = pq.read_table(filename)
    assert result.column("ext").type == pa.int64()


def test_to_numpy():
    period_type = PeriodType('D')
    storage = pa.array([1, 2, 3, 4], pa.int64())
    arr = pa.ExtensionArray.from_storage(period_type, storage)

    expected = storage.to_numpy()
    result = arr.to_numpy()
    np.testing.assert_array_equal(result, expected)

    result = np.asarray(arr)
    np.testing.assert_array_equal(result, expected)

    # chunked array
    charr = pa.chunked_array([arr])

    result = np.asarray(charr)
    np.testing.assert_array_equal(result, expected)

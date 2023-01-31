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


_NULL = NA = None


cdef class Scalar:
    """
    The base class for all array elements.
    """


cdef class NullType(Scalar):
    """
    Singleton for null array elements.
    """
    # TODO rename this NullValue?
    def __cinit__(self):
        global NA
        if NA is not None:
            raise Exception('Cannot create multiple NAType instances')

        self.type = null()

    def __repr__(self):
        return 'NULL'

    def as_py(self):
        """
        Return None
        """
        return None

    def __eq__(self, other):
        return NA


_NULL = NA = NullType()


cdef class ArrayValue(Scalar):
    """
    The base class for non-null array elements.
    """

    def __init__(self):
        raise TypeError("Do not call {}'s constructor directly, use array "
                        "subscription instead."
                        .format(self.__class__.__name__))

    cdef void init(self, DataType type, const shared_ptr[CArray]& sp_array,
                   int64_t index):
        self.type = type
        self.index = index
        self._set_array(sp_array)

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array

    def __repr__(self):
        if hasattr(self, 'as_py'):
            return repr(self.as_py())
        else:
            return super(Scalar, self).__repr__()

    def __str__(self):
        if hasattr(self, 'as_py'):
            return str(self.as_py())
        else:
            return super(Scalar, self).__str__()

    def __eq__(self, other):
        if hasattr(self, 'as_py'):
            if isinstance(other, ArrayValue):
                other = other.as_py()
            return self.as_py() == other
        else:
            raise NotImplementedError(
                "Cannot compare Arrow values that don't support as_py()")

    def __hash__(self):
        return hash(self.as_py())


cdef class BooleanValue(ArrayValue):
    """
    Concrete class for boolean array elements.
    """

    def as_py(self):
        """
        Return this value as a Python bool.
        """
        cdef CBooleanArray* ap = <CBooleanArray*> self.sp_array.get()
        return ap.Value(self.index)


cdef class Int8Value(ArrayValue):
    """
    Concrete class for int8 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt8Array* ap = <CInt8Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class UInt8Value(ArrayValue):
    """
    Concrete class for uint8 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt8Array* ap = <CUInt8Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class Int16Value(ArrayValue):
    """
    Concrete class for int16 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt16Array* ap = <CInt16Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class UInt16Value(ArrayValue):
    """
    Concrete class for uint16 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt16Array* ap = <CUInt16Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class Int32Value(ArrayValue):
    """
    Concrete class for int32 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt32Array* ap = <CInt32Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class UInt32Value(ArrayValue):
    """
    Concrete class for uint32 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt32Array* ap = <CUInt32Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class Int64Value(ArrayValue):
    """
    Concrete class for int64 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt64Array* ap = <CInt64Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class UInt64Value(ArrayValue):
    """
    Concrete class for uint64 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt64Array* ap = <CUInt64Array*> self.sp_array.get()
        return ap.Value(self.index)


cdef class Date32Value(ArrayValue):
    """
    Concrete class for date32 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python datetime.datetime instance.
        """
        cdef CDate32Array* ap = <CDate32Array*> self.sp_array.get()

        # Shift to seconds since epoch
        return (datetime.date(1970, 1, 1) +
                datetime.timedelta(days=ap.Value(self.index)))


cdef class Date64Value(ArrayValue):
    """
    Concrete class for date64 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python datetime.datetime instance.
        """
        cdef CDate64Array* ap = <CDate64Array*> self.sp_array.get()
        return (datetime.date(1970, 1, 1) +
                datetime.timedelta(
                    days=ap.Value(self.index) / 86400000))


cdef class Time32Value(ArrayValue):
    """
    Concrete class for time32 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python datetime.timedelta instance.
        """
        cdef:
            CTime32Array* ap = <CTime32Array*> self.sp_array.get()
            CTime32Type* dtype = <CTime32Type*> ap.type().get()

        if dtype.unit() == TimeUnit_SECOND:
            delta = datetime.timedelta(seconds=ap.Value(self.index))
            return (datetime.datetime(1970, 1, 1) + delta).time()
        else:
            return _box_time_milli(ap.Value(self.index))


cdef class Time64Value(ArrayValue):
    """
    Concrete class for time64 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python datetime.timedelta instance.
        """
        cdef:
            CTime64Array* ap = <CTime64Array*> self.sp_array.get()
            CTime64Type* dtype = <CTime64Type*> ap.type().get()

        cdef int64_t val = ap.Value(self.index)
        if dtype.unit() == TimeUnit_MICRO:
            return _box_time_micro(val)
        else:
            return (datetime.datetime(1970, 1, 1) +
                    datetime.timedelta(microseconds=val / 1000)).time()


cpdef _box_time_milli(int64_t val):
    delta = datetime.timedelta(milliseconds=val)
    return (datetime.datetime(1970, 1, 1) + delta).time()


cpdef _box_time_micro(int64_t val):
    return (datetime.datetime(1970, 1, 1) +
            datetime.timedelta(microseconds=val)).time()


cdef dict _DATETIME_CONVERSION_FUNCTIONS = {}
cdef c_bool _datetime_conversion_initialized = False


cdef _add_micros_maybe_localize(dt, micros, tzinfo):
    import pytz
    dt = dt.replace(microsecond=micros)
    if tzinfo is not None:
        if not isinstance(tzinfo, datetime.tzinfo):
            tzinfo = string_to_tzinfo(tzinfo)
        dt = tzinfo.fromutc(dt)
    return dt


cdef _datetime_from_seconds(int64_t v):
    return datetime.datetime(1970, 1, 1) + datetime.timedelta(seconds=v)


def _nanoseconds_to_datetime_safe(v, tzinfo):
    if v % 1000 != 0:
        raise ValueError("Nanosecond timestamp {} is not safely convertible "
                         " to microseconds to convert to datetime.datetime."
                         " Install pandas to return as Timestamp with "
                         " nanosecond support or access the .value attribute.")
    v = v // 1000
    micros = v % 1_000_000

    dt = _datetime_from_seconds(v // 1_000_000)
    return _add_micros_maybe_localize(dt, micros, tzinfo)


def _microseconds_to_datetime(v, tzinfo):
    micros = v % 1_000_000
    dt = _datetime_from_seconds(v // 1_000_000)
    return _add_micros_maybe_localize(dt, micros, tzinfo)


def _millis_to_datetime(v, tzinfo):
    millis = v % 1_000
    dt = _datetime_from_seconds(v // 1000)
    return _add_micros_maybe_localize(dt, millis * 1000, tzinfo)


def _seconds_to_datetime(v, tzinfo):
    dt = _datetime_from_seconds(v)
    return _add_micros_maybe_localize(dt, 0, tzinfo)


def _datetime_conversion_functions():
    global _datetime_conversion_initialized
    if _datetime_conversion_initialized:
        return _DATETIME_CONVERSION_FUNCTIONS

    _DATETIME_CONVERSION_FUNCTIONS.update({
        TimeUnit_SECOND: _seconds_to_datetime,
        TimeUnit_MILLI: _millis_to_datetime,
        TimeUnit_MICRO: _microseconds_to_datetime,
        TimeUnit_NANO: _nanoseconds_to_datetime_safe
    })

    try:
        import pandas as pd
        _DATETIME_CONVERSION_FUNCTIONS[TimeUnit_NANO] = (
            lambda x, tzinfo: pd.Timestamp(
                x, tz=tzinfo, unit='ns',
            )
        )
    except ImportError:
        pass

    _datetime_conversion_initialized = True
    return _DATETIME_CONVERSION_FUNCTIONS


cdef class TimestampValue(ArrayValue):
    """
    Concrete class for timestamp array elements.
    """

    @property
    def value(self):
        cdef CTimestampArray* ap = <CTimestampArray*> self.sp_array.get()
        cdef CTimestampType* dtype = <CTimestampType*> ap.type().get()
        return ap.Value(self.index)

    def as_py(self):
        """
        Return this value as a Pandas Timestamp instance (if available),
        otherwise as a Python datetime.timedelta instance.
        """
        cdef CTimestampArray* ap = <CTimestampArray*> self.sp_array.get()
        cdef CTimestampType* dtype = <CTimestampType*> ap.type().get()

        value = self.value

        if not dtype.timezone().empty():
            tzinfo = string_to_tzinfo(frombytes(dtype.timezone()))
        else:
            tzinfo = None

        try:
            converter = _datetime_conversion_functions()[dtype.unit()]
        except KeyError:
            raise ValueError(
                'Cannot convert nanosecond timestamps without pandas'
            )
        return converter(value, tzinfo=tzinfo)


cdef dict _TIMEDELTA_CONVERSION_FUNCTIONS = {}


def _nanoseconds_to_timedelta_safe(v):
    if v % 1000 != 0:
        raise ValueError(
            "Nanosecond duration {} is not safely convertible to microseconds "
            "to convert to datetime.timedelta. Install pandas to return as "
            "Timedelta with nanosecond support or access the .value "
            "attribute.".format(v))
    micros = v // 1000

    return datetime.timedelta(microseconds=micros)


def _timedelta_conversion_functions():
    if _TIMEDELTA_CONVERSION_FUNCTIONS:
        return _TIMEDELTA_CONVERSION_FUNCTIONS

    _TIMEDELTA_CONVERSION_FUNCTIONS.update({
        TimeUnit_SECOND: lambda v: datetime.timedelta(seconds=v),
        TimeUnit_MILLI: lambda v: datetime.timedelta(milliseconds=v),
        TimeUnit_MICRO: lambda v: datetime.timedelta(microseconds=v),
        TimeUnit_NANO: _nanoseconds_to_timedelta_safe
    })

    try:
        import pandas as pd
        _TIMEDELTA_CONVERSION_FUNCTIONS[TimeUnit_NANO] = (
            lambda v: pd.Timedelta(v, unit='ns')
        )
    except ImportError:
        pass

    return _TIMEDELTA_CONVERSION_FUNCTIONS


cdef class DurationValue(ArrayValue):
    """
    Concrete class for duration array elements.
    """

    @property
    def value(self):
        cdef CDurationArray* ap = <CDurationArray*> self.sp_array.get()
        return ap.Value(self.index)

    def as_py(self):
        """
        Return this value as a Pandas Timestamp instance (if available),
        otherwise as a Python datetime.timedelta instance.
        """
        cdef CDurationArray* ap = <CDurationArray*> self.sp_array.get()
        cdef CDurationType* dtype = <CDurationType*> ap.type().get()

        cdef int64_t value = ap.Value(self.index)
        converter = _timedelta_conversion_functions()[dtype.unit()]
        return converter(value)


cdef class HalfFloatValue(ArrayValue):
    """
    Concrete class for float16 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python float.
        """
        cdef CHalfFloatArray* ap = <CHalfFloatArray*> self.sp_array.get()
        return PyHalf_FromHalf(ap.Value(self.index))


cdef class FloatValue(ArrayValue):
    """
    Concrete class for float32 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python float.
        """
        cdef CFloatArray* ap = <CFloatArray*> self.sp_array.get()
        return ap.Value(self.index)


cdef class DoubleValue(ArrayValue):
    """
    Concrete class for float64 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python float.
        """
        cdef CDoubleArray* ap = <CDoubleArray*> self.sp_array.get()
        return ap.Value(self.index)


cdef class DecimalValue(ArrayValue):
    """
    Concrete class for decimal128 array elements.
    """

    def as_py(self):
        """
        Return this value as a Python Decimal.
        """
        cdef:
            CDecimal128Array* ap = <CDecimal128Array*> self.sp_array.get()
            c_string s = ap.FormatValue(self.index)
        return _pydecimal.Decimal(s.decode('utf8'))


cdef class StringValue(ArrayValue):
    """
    Concrete class for string (utf8) array elements.
    """

    def as_py(self):
        """
        Return this value as a Python unicode string.
        """
        cdef CStringArray* ap = <CStringArray*> self.sp_array.get()
        return ap.GetString(self.index).decode('utf-8')

    def as_buffer(self):
        """
        Return a view over this value as a Buffer object.
        """
        cdef:
            CStringArray* ap = <CStringArray*> self.sp_array.get()
            shared_ptr[CBuffer] buf

        buf = SliceBuffer(ap.value_data(), ap.value_offset(self.index),
                          ap.value_length(self.index))
        return pyarrow_wrap_buffer(buf)


cdef class LargeStringValue(ArrayValue):
    """
    Concrete class for large string (utf8) array elements.
    """

    def as_py(self):
        """
        Return this value as a Python unicode string.
        """
        cdef CLargeStringArray* ap = <CLargeStringArray*> self.sp_array.get()
        return ap.GetString(self.index).decode('utf-8')

    def as_buffer(self):
        """
        Return a view over this value as a Buffer object.
        """
        cdef:
            CLargeStringArray* ap = <CLargeStringArray*> self.sp_array.get()
            shared_ptr[CBuffer] buf

        buf = SliceBuffer(ap.value_data(), ap.value_offset(self.index),
                          ap.value_length(self.index))
        return pyarrow_wrap_buffer(buf)


cdef class BinaryValue(ArrayValue):
    """
    Concrete class for variable-sized binary array elements.
    """

    def as_py(self):
        """
        Return this value as a Python bytes object.
        """
        cdef:
            const uint8_t* ptr
            int32_t length
            CBinaryArray* ap = <CBinaryArray*> self.sp_array.get()

        ptr = ap.GetValue(self.index, &length)
        return cp.PyBytes_FromStringAndSize(<const char*>(ptr), length)

    def as_buffer(self):
        """
        Return a view over this value as a Buffer object.
        """
        cdef:
            CBinaryArray* ap = <CBinaryArray*> self.sp_array.get()
            shared_ptr[CBuffer] buf

        buf = SliceBuffer(ap.value_data(), ap.value_offset(self.index),
                          ap.value_length(self.index))
        return pyarrow_wrap_buffer(buf)


cdef class LargeBinaryValue(ArrayValue):
    """
    Concrete class for large variable-sized binary array elements.
    """

    def as_py(self):
        """
        Return this value as a Python bytes object.
        """
        cdef:
            const uint8_t* ptr
            int64_t length
            CLargeBinaryArray* ap = <CLargeBinaryArray*> self.sp_array.get()

        ptr = ap.GetValue(self.index, &length)
        return cp.PyBytes_FromStringAndSize(<const char*>(ptr), length)

    def as_buffer(self):
        """
        Return a view over this value as a Buffer object.
        """
        cdef:
            CLargeBinaryArray* ap = <CLargeBinaryArray*> self.sp_array.get()
            shared_ptr[CBuffer] buf

        buf = SliceBuffer(ap.value_data(), ap.value_offset(self.index),
                          ap.value_length(self.index))
        return pyarrow_wrap_buffer(buf)


cdef class ListValue(ArrayValue):
    """
    Concrete class for list array elements.
    """

    def __len__(self):
        """
        Return the number of values.
        """
        return self.length()

    def __getitem__(self, i):
        """
        Return the value at the given index.
        """
        return self.getitem(_normalize_index(i, self.length()))

    def __iter__(self):
        """
        Iterate over this element's values.
        """
        for i in range(len(self)):
            yield self.getitem(i)
        raise StopIteration

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array
        self.ap = <CListArray*> sp_array.get()
        self.value_type = pyarrow_wrap_data_type(self.ap.value_type())

    cdef getitem(self, int64_t i):
        cdef int64_t j = self.ap.value_offset(self.index) + i
        return box_scalar(self.value_type, self.ap.values(), j)

    cdef int64_t length(self):
        return self.ap.value_length(self.index)

    def as_py(self):
        """
        Return this value as a Python list.
        """
        cdef:
            int64_t j
            list result = []

        for j in range(len(self)):
            result.append(self.getitem(j).as_py())

        return result


cdef class LargeListValue(ArrayValue):
    """
    Concrete class for large list array elements.
    """

    def __len__(self):
        """
        Return the number of values.
        """
        return self.length()

    def __getitem__(self, i):
        """
        Return the value at the given index.
        """
        return self.getitem(_normalize_index(i, self.length()))

    def __iter__(self):
        """
        Iterate over this element's values.
        """
        for i in range(len(self)):
            yield self.getitem(i)
        raise StopIteration

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array
        self.ap = <CLargeListArray*> sp_array.get()
        self.value_type = pyarrow_wrap_data_type(self.ap.value_type())

    cdef getitem(self, int64_t i):
        cdef int64_t j = self.ap.value_offset(self.index) + i
        return box_scalar(self.value_type, self.ap.values(), j)

    cdef int64_t length(self):
        return self.ap.value_length(self.index)

    def as_py(self):
        """
        Return this value as a Python list.
        """
        cdef:
            int64_t j
            list result = []

        for j in range(len(self)):
            result.append(self.getitem(j).as_py())

        return result


cdef class MapValue(ArrayValue):
    """
    Concrete class for map array elements.
    """

    def __len__(self):
        """
        Return the number of values.
        """
        return self.length()

    def __getitem__(self, i):
        """
        Return the value at the given index.
        """
        return self.getitem(_normalize_index(i, self.length()))

    def __iter__(self):
        """
        Iterate over this element's values.
        """
        for i in range(len(self)):
            yield self.getitem(i)
        raise StopIteration

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array
        self.ap = <CMapArray*> sp_array.get()
        self.key_type = pyarrow_wrap_data_type(self.ap.map_type().key_type())
        self.item_type = pyarrow_wrap_data_type(self.ap.map_type().item_type())

    cdef getitem(self, int64_t i):
        cdef int64_t j = self.ap.value_offset(self.index) + i
        return (box_scalar(self.key_type, self.ap.keys(), j),
                box_scalar(self.item_type, self.ap.items(), j))

    cdef int64_t length(self):
        return self.ap.value_length(self.index)

    def as_py(self):
        """
        Return this value as a Python list of tuples, each containing a
        key and item.
        """
        cdef:
            int64_t j
            list result = []

        for j in range(len(self)):
            key, item = self.getitem(j)
            result.append((key.as_py(), item.as_py()))

        return result


cdef class FixedSizeListValue(ArrayValue):
    """
    Concrete class for fixed size list array elements.
    """

    def __len__(self):
        """
        Return the number of values.
        """
        return self.length()

    def __getitem__(self, i):
        """
        Return the value at the given index.
        """
        return self.getitem(_normalize_index(i, self.length()))

    def __iter__(self):
        """
        Iterate over this element's values.
        """
        for i in range(len(self)):
            yield self.getitem(i)
        raise StopIteration

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array
        self.ap = <CFixedSizeListArray*> sp_array.get()
        self.value_type = pyarrow_wrap_data_type(self.ap.value_type())

    cdef getitem(self, int64_t i):
        cdef int64_t j = self.ap.value_offset(self.index) + i
        return box_scalar(self.value_type, self.ap.values(), j)

    cdef int64_t length(self):
        return self.ap.value_length(self.index)

    def as_py(self):
        """
        Return this value as a Python list.
        """
        cdef:
            int64_t j
            list result = []

        for j in range(len(self)):
            result.append(self.getitem(j).as_py())

        return result


cdef class UnionValue(ArrayValue):
    """
    Concrete class for union array elements.
    """

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array
        self.ap = <CUnionArray*> sp_array.get()

    cdef getitem(self, int64_t i):
        cdef int child_id = self.ap.child_id(i)
        cdef shared_ptr[CArray] child = self.ap.child(child_id)
        if self.ap.mode() == _UnionMode_SPARSE:
            return box_scalar(self.type[child_id].type, child, i)
        else:
            return box_scalar(self.type[child_id].type, child,
                              self.ap.value_offset(i))

    def as_py(self):
        """
        Return this value as a Python object.

        The exact type depends on the underlying union member.
        """
        return self.getitem(self.index).as_py()


cdef class FixedSizeBinaryValue(ArrayValue):
    """
    Concrete class for fixed-size binary array elements.
    """

    def as_py(self):
        """
        Return this value as a Python bytes object.
        """
        cdef:
            CFixedSizeBinaryArray* ap
            CFixedSizeBinaryType* ap_type
            int32_t length
            const char* data
        ap = <CFixedSizeBinaryArray*> self.sp_array.get()
        ap_type = <CFixedSizeBinaryType*> ap.type().get()
        length = ap_type.byte_width()
        data = <const char*> ap.GetValue(self.index)
        return cp.PyBytes_FromStringAndSize(data, length)


cdef class StructValue(ArrayValue):
    """
    Concrete class for struct array elements.
    """

    cdef void _set_array(self, const shared_ptr[CArray]& sp_array):
        self.sp_array = sp_array
        self.ap = <CStructArray*> sp_array.get()

    def __getitem__(self, key):
        """
        Return the child value for the given field name.
        """
        cdef:
            CStructType* type
            int index

        type = <CStructType*> self.type.type
        index = type.GetFieldIndex(tobytes(key))

        if index < 0:
            raise KeyError(key)

        return pyarrow_wrap_array(self.ap.field(index))[self.index]

    def as_py(self):
        """
        Return this value as a Python dict.
        """
        cdef:
            vector[shared_ptr[CField]] child_fields = self.type.type.children()

        wrapped_arrays = [pyarrow_wrap_array(self.ap.field(i))
                          for i in range(self.ap.num_fields())]
        child_names = [child.get().name() for child in child_fields]
        # Return the struct as a dict
        return {
            frombytes(name): child_array[self.index].as_py()
            for name, child_array in
            zip(child_names, wrapped_arrays)
        }


cdef class DictionaryValue(ArrayValue):
    """
    Concrete class for dictionary-encoded array elements.
    """

    def as_py(self):
        """
        Return this value as a Python object.

        The exact type depends on the dictionary value type.
        """
        return self.dictionary_value.as_py()

    @property
    def index_value(self):
        """
        Return this value's underlying index as a ArrayValue of the right
        signed integer type.
        """
        cdef CDictionaryArray* darr = <CDictionaryArray*>(self.sp_array.get())
        indices = pyarrow_wrap_array(darr.indices())
        return indices[self.index]

    @property
    def dictionary_value(self):
        """
        Return this value's underlying dictionary value as a ArrayValue.
        """
        cdef CDictionaryArray* darr = <CDictionaryArray*>(self.sp_array.get())
        dictionary = pyarrow_wrap_array(darr.dictionary())
        return dictionary[self.index_value.as_py()]


cdef dict _array_value_classes = {
    _Type_BOOL: BooleanValue,
    _Type_UINT8: UInt8Value,
    _Type_UINT16: UInt16Value,
    _Type_UINT32: UInt32Value,
    _Type_UINT64: UInt64Value,
    _Type_INT8: Int8Value,
    _Type_INT16: Int16Value,
    _Type_INT32: Int32Value,
    _Type_INT64: Int64Value,
    _Type_DATE32: Date32Value,
    _Type_DATE64: Date64Value,
    _Type_TIME32: Time32Value,
    _Type_TIME64: Time64Value,
    _Type_TIMESTAMP: TimestampValue,
    _Type_DURATION: DurationValue,
    _Type_HALF_FLOAT: HalfFloatValue,
    _Type_FLOAT: FloatValue,
    _Type_DOUBLE: DoubleValue,
    _Type_LIST: ListValue,
    _Type_LARGE_LIST: LargeListValue,
    _Type_MAP: MapValue,
    _Type_FIXED_SIZE_LIST: FixedSizeListValue,
    _Type_UNION: UnionValue,
    _Type_BINARY: BinaryValue,
    _Type_STRING: StringValue,
    _Type_LARGE_BINARY: LargeBinaryValue,
    _Type_LARGE_STRING: LargeStringValue,
    _Type_FIXED_SIZE_BINARY: FixedSizeBinaryValue,
    _Type_DECIMAL: DecimalValue,
    _Type_STRUCT: StructValue,
    _Type_DICTIONARY: DictionaryValue,
}

cdef class ScalarValue(Scalar):
    """
    The base class for scalars.
    """

    def __init__(self):
        raise TypeError("Do not call {}'s constructor directly."
                        .format(self.__class__.__name__))

    cdef void init(self, const shared_ptr[CScalar]& sp_scalar):
        self.sp_scalar = sp_scalar

    def __repr__(self):
        if hasattr(self, 'as_py'):
            return repr(self.as_py())
        else:
            return super(Scalar, self).__repr__()

    def __str__(self):
        if hasattr(self, 'as_py'):
            return str(self.as_py())
        else:
            return super(Scalar, self).__str__()

    def __eq__(self, other):
        if hasattr(self, 'as_py'):
            if isinstance(other, ScalarValue):
                other = other.as_py()
            return self.as_py() == other
        else:
            raise NotImplemented(
                "Cannot compare scalars that don't support as_py()")

    def __hash__(self):
        return hash(self.as_py())


cdef class NullScalar(ScalarValue):
    """
    Concrete class for null scalars.
    """

    def as_py(self):
        """
        Return this value as a Python None.
        """
        return None


cdef class BooleanScalar(ScalarValue):
    """
    Concrete class for boolean scalars.
    """

    def as_py(self):
        """
        Return this value as a Python bool.
        """
        cdef CBooleanScalar* sp = <CBooleanScalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class UInt8Scalar(ScalarValue):
    """
    Concrete class for uint8 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt8Scalar* sp = <CUInt8Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class Int8Scalar(ScalarValue):
    """
    Concrete class for int8 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt8Scalar* sp = <CInt8Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class UInt16Scalar(ScalarValue):
    """
    Concrete class for uint16 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt16Scalar* sp = <CUInt16Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class Int16Scalar(ScalarValue):
    """
    Concrete class for int16 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt16Scalar* sp = <CInt16Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class UInt32Scalar(ScalarValue):
    """
    Concrete class for uint32 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt32Scalar* sp = <CUInt32Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class Int32Scalar(ScalarValue):
    """
    Concrete class for int32 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt32Scalar* sp = <CInt32Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class UInt64Scalar(ScalarValue):
    """
    Concrete class for uint64 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CUInt64Scalar* sp = <CUInt64Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class Int64Scalar(ScalarValue):
    """
    Concrete class for int64 scalars.
    """

    def as_py(self):
        """
        Return this value as a Python int.
        """
        cdef CInt64Scalar* sp = <CInt64Scalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class FloatScalar(ScalarValue):
    """
    Concrete class for float scalars.
    """

    def as_py(self):
        """
        Return this value as a Python float.
        """
        cdef CFloatScalar* sp = <CFloatScalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class DoubleScalar(ScalarValue):
    """
    Concrete class for double scalars.
    """

    def as_py(self):
        """
        Return this value as a Python float.
        """
        cdef CDoubleScalar* sp = <CDoubleScalar*> self.sp_scalar.get()
        return sp.value if sp.is_valid else None


cdef class StringScalar(ScalarValue):
    """
    Concrete class for string scalars.
    """

    def as_py(self):
        """
        Return this value as a Python string.
        """
        cdef CStringScalar* sp = <CStringScalar*> self.sp_scalar.get()
        if sp.is_valid:
            return frombytes(pyarrow_wrap_buffer(sp.value).to_pybytes())
        else:
            return None


cdef dict _scalar_classes = {
    _Type_NA: NullScalar,
    _Type_BOOL: BooleanScalar,
    _Type_UINT8: UInt8Scalar,
    _Type_UINT16: UInt16Scalar,
    _Type_UINT32: UInt32Scalar,
    _Type_UINT64: UInt64Scalar,
    _Type_INT8: Int8Scalar,
    _Type_INT16: Int16Scalar,
    _Type_INT32: Int32Scalar,
    _Type_INT64: Int64Scalar,
    _Type_FLOAT: FloatScalar,
    _Type_DOUBLE: DoubleScalar,
    _Type_STRING: StringScalar,
}

cdef object box_scalar(DataType type, const shared_ptr[CArray]& sp_array,
                       int64_t index):
    cdef ArrayValue value

    if type.type.id() == _Type_NA:
        return _NULL
    elif sp_array.get().IsNull(index):
        return _NULL
    else:
        klass = _array_value_classes[type.type.id()]
        value = klass.__new__(klass)
        value.init(type, sp_array, index)
        return value

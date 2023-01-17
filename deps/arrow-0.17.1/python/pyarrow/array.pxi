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

import warnings


cdef _sequence_to_array(object sequence, object mask, object size,
                        DataType type, CMemoryPool* pool, c_bool from_pandas):
    cdef int64_t c_size
    cdef PyConversionOptions options

    if type is not None:
        options.type = type.sp_type

    if size is not None:
        options.size = size

    options.pool = pool
    options.from_pandas = from_pandas

    cdef shared_ptr[CChunkedArray] out

    with nogil:
        check_status(ConvertPySequence(sequence, mask, options, &out))

    if out.get().num_chunks() == 1:
        return pyarrow_wrap_array(out.get().chunk(0))
    else:
        return pyarrow_wrap_chunked_array(out)


cdef inline _is_array_like(obj):
    if isinstance(obj, np.ndarray):
        return True
    return pandas_api._have_pandas_internal() and pandas_api.is_array_like(obj)


def _ndarray_to_arrow_type(object values, DataType type):
    return pyarrow_wrap_data_type(_ndarray_to_type(values, type))


cdef shared_ptr[CDataType] _ndarray_to_type(object values,
                                            DataType type) except *:
    cdef shared_ptr[CDataType] c_type

    dtype = values.dtype

    if type is None and dtype != object:
        with nogil:
            check_status(NumPyDtypeToArrow(dtype, &c_type))

    if type is not None:
        c_type = type.sp_type

    return c_type


cdef _ndarray_to_array(object values, object mask, DataType type,
                       c_bool from_pandas, c_bool safe, CMemoryPool* pool):
    cdef:
        shared_ptr[CChunkedArray] chunked_out
        shared_ptr[CDataType] c_type = _ndarray_to_type(values, type)
        CCastOptions cast_options = CCastOptions(safe)

    with nogil:
        check_status(NdarrayToArrow(pool, values, mask, from_pandas,
                                    c_type, cast_options, &chunked_out))

    if chunked_out.get().num_chunks() > 1:
        return pyarrow_wrap_chunked_array(chunked_out)
    else:
        return pyarrow_wrap_array(chunked_out.get().chunk(0))


cdef _codes_to_indices(object codes, object mask, DataType type,
                       MemoryPool memory_pool):
    """
    Convert the codes of a pandas Categorical to indices for a pyarrow
    DictionaryArray, taking into account missing values + mask
    """
    if mask is None:
        mask = codes == -1
    else:
        mask = mask | (codes == -1)
    return array(codes, mask=mask, type=type, memory_pool=memory_pool)


def _handle_arrow_array_protocol(obj, type, mask, size):
    if mask is not None or size is not None:
        raise ValueError(
            "Cannot specify a mask or a size when passing an object that is "
            "converted with the __arrow_array__ protocol.")
    res = obj.__arrow_array__(type=type)
    if not isinstance(res, (Array, ChunkedArray)):
        raise TypeError("The object's __arrow_array__ method does not "
                        "return a pyarrow Array or ChunkedArray.")
    return res


def array(object obj, type=None, mask=None, size=None, from_pandas=None,
          bint safe=True, MemoryPool memory_pool=None):
    """
    Create pyarrow.Array instance from a Python object.

    Parameters
    ----------
    obj : sequence, iterable, ndarray or Series
        If both type and size are specified may be a single use iterable. If
        not strongly-typed, Arrow type will be inferred for resulting array.
    type : pyarrow.DataType
        Explicit type to attempt to coerce to, otherwise will be inferred from
        the data.
    mask : array[bool], optional
        Indicate which values are null (True) or not null (False).
    size : int64, optional
        Size of the elements. If the input is larger than size bail at this
        length. For iterators, if size is larger than the input iterator this
        will be treated as a "max size", but will involve an initial allocation
        of size followed by a resize to the actual size (so if you know the
        exact size specifying it correctly will give you better performance).
    from_pandas : bool, default None
        Use pandas's semantics for inferring nulls from values in
        ndarray-like data. If passed, the mask tasks precedence, but
        if a value is unmasked (not-null), but still null according to
        pandas semantics, then it is null. Defaults to False if not
        passed explicitly by user, or True if a pandas object is
        passed in.
    safe : bool, default True
        Check for overflows or other unsafe conversions.
    memory_pool : pyarrow.MemoryPool, optional
        If not passed, will allocate memory from the currently-set default
        memory pool.

    Returns
    -------
    array : pyarrow.Array or pyarrow.ChunkedArray
        A ChunkedArray instead of an Array is returned if:
        - the object data overflowed binary storage.
        - the object's ``__arrow_array__`` protocol method returned a chunked
          array.

    Notes
    -----
    Localized timestamps will currently be returned as UTC (pandas's native
    representation).  Timezone-naive data will be implicitly interpreted as
    UTC.

    Examples
    --------
    >>> import pandas as pd
    >>> import pyarrow as pa
    >>> pa.array(pd.Series([1, 2]))
    <pyarrow.array.Int64Array object at 0x7f674e4c0e10>
    [
      1,
      2
    ]

    >>> import numpy as np
    >>> pa.array(pd.Series([1, 2]), np.array([0, 1],
    ... dtype=bool))
    <pyarrow.array.Int64Array object at 0x7f9019e11208>
    [
      1,
      null
    ]
    """
    cdef:
        CMemoryPool* pool = maybe_unbox_memory_pool(memory_pool)
        bint is_pandas_object = False
        bint c_from_pandas

    type = ensure_type(type, allow_none=True)

    if from_pandas is None:
        c_from_pandas = False
    else:
        c_from_pandas = from_pandas

    if hasattr(obj, '__arrow_array__'):
        return _handle_arrow_array_protocol(obj, type, mask, size)
    elif _is_array_like(obj):
        if mask is not None:
            # out argument unused
            mask = get_values(mask, &is_pandas_object)

        values = get_values(obj, &is_pandas_object)
        if is_pandas_object and from_pandas is None:
            c_from_pandas = True

        if isinstance(values, np.ma.MaskedArray):
            if mask is not None:
                raise ValueError("Cannot pass a numpy masked array and "
                                 "specify a mask at the same time")
            else:
                # don't use shrunken masks
                mask = None if values.mask is np.ma.nomask else values.mask
                values = values.data

        if hasattr(values, '__arrow_array__'):
            return _handle_arrow_array_protocol(values, type, mask, size)
        elif pandas_api.is_categorical(values):
            if type is not None:
                if type.id != Type_DICTIONARY:
                    return _ndarray_to_array(
                        np.asarray(values), mask, type, c_from_pandas, safe,
                        pool)
                index_type = type.index_type
                value_type = type.value_type
                if values.ordered != type.ordered:
                    warnings.warn(
                        "The 'ordered' flag of the passed categorical values "
                        "does not match the 'ordered' of the specified type. "
                        "Using the flag of the values, but in the future this "
                        "mismatch will raise a ValueError.",
                        FutureWarning, stacklevel=2)
            else:
                index_type = None
                value_type = None

            indices = _codes_to_indices(
                values.codes, mask, index_type, memory_pool)
            try:
                dictionary = array(
                    values.categories.values, type=value_type,
                    memory_pool=memory_pool)
            except TypeError:
                # TODO when removing the deprecation warning, this whole
                # try/except can be removed (to bubble the TypeError of
                # the first array(..) call)
                if value_type is not None:
                    warnings.warn(
                        "The dtype of the 'categories' of the passed "
                        "categorical values ({0}) does not match the "
                        "specified type ({1}). For now ignoring the specified "
                        "type, but in the future this mismatch will raise a "
                        "TypeError".format(
                            values.categories.dtype, value_type),
                        FutureWarning, stacklevel=2)
                    dictionary = array(
                        values.categories.values, memory_pool=memory_pool)
                else:
                    raise

            return DictionaryArray.from_arrays(
                indices, dictionary, ordered=values.ordered, safe=safe)
        else:
            if pandas_api.have_pandas:
                values, type = pandas_api.compat.get_datetimetz_type(
                    values, obj.dtype, type)
            return _ndarray_to_array(values, mask, type, c_from_pandas, safe,
                                     pool)
    else:
        # ConvertPySequence does strict conversion if type is explicitly passed
        return _sequence_to_array(obj, mask, size, type, pool, c_from_pandas)


def asarray(values, type=None):
    """
    Convert to pyarrow.Array, inferring type if not provided.

    Parameters
    ----------
    values : array-like
        This can be a sequence, numpy.ndarray, pyarrow.Array or
        pyarrow.ChunkedArray. If a ChunkedArray is passed, the output will be
        a ChunkedArray, otherwise the output will be a Array.
    type : string or DataType
        Explicitly construct the array with this type. Attempt to cast if
        indicated type is different.

    Returns
    -------
    arr : Array or ChunkedArray
    """
    if isinstance(values, (Array, ChunkedArray)):
        if type is not None and not values.type.equals(type):
            values = values.cast(type)
        return values
    else:
        return array(values, type=type)


def infer_type(values, mask=None, from_pandas=False):
    """
    Attempt to infer Arrow data type that can hold the passed Python
    sequence type in an Array object

    Parameters
    ----------
    values : array-like
        Sequence to infer type from.
    mask : ndarray (bool type), optional
        Optional exclusion mask where True marks null, False non-null.
    from_pandas : bool, default False
        Use pandas's NA/null sentinel values for type inference.

    Returns
    -------
    type : DataType
    """
    cdef:
        shared_ptr[CDataType] out
        c_bool use_pandas_sentinels = from_pandas

    if mask is not None and not isinstance(mask, np.ndarray):
        mask = np.array(mask, dtype=bool)

    check_status(InferArrowType(values, mask, use_pandas_sentinels, &out))
    return pyarrow_wrap_data_type(out)


def _normalize_slice(object arrow_obj, slice key):
    cdef:
        Py_ssize_t start, stop, step
        Py_ssize_t n = len(arrow_obj)

    start = key.start or 0
    if start < 0:
        start += n
        if start < 0:
            start = 0
    elif start >= n:
        start = n

    stop = key.stop if key.stop is not None else n
    if stop < 0:
        stop += n
        if stop < 0:
            stop = 0
    elif stop >= n:
        stop = n

    step = key.step or 1
    if step != 1:
        raise IndexError('only slices with step 1 supported')
    else:
        return arrow_obj.slice(start, stop - start)


cdef Py_ssize_t _normalize_index(Py_ssize_t index,
                                 Py_ssize_t length) except -1:
    if index < 0:
        index += length
        if index < 0:
            raise IndexError("index out of bounds")
    elif index >= length:
        raise IndexError("index out of bounds")
    return index


cdef class _FunctionContext:
    cdef:
        unique_ptr[CFunctionContext] ctx

    def __cinit__(self):
        self.ctx.reset(new CFunctionContext(c_default_memory_pool()))

cdef _FunctionContext _global_ctx = _FunctionContext()

cdef CFunctionContext* _context() nogil:
    return _global_ctx.ctx.get()


cdef wrap_datum(const CDatum& datum):
    if datum.kind() == DatumType_ARRAY:
        return pyarrow_wrap_array(MakeArray(datum.array()))
    elif datum.kind() == DatumType_CHUNKED_ARRAY:
        return pyarrow_wrap_chunked_array(datum.chunked_array())
    elif datum.kind() == DatumType_RECORD_BATCH:
        return pyarrow_wrap_batch(datum.record_batch())
    elif datum.kind() == DatumType_TABLE:
        return pyarrow_wrap_table(datum.table())
    elif datum.kind() == DatumType_SCALAR:
        return pyarrow_wrap_scalar(datum.scalar())
    else:
        raise ValueError("Unable to wrap Datum in a Python object")


cdef _append_array_buffers(const CArrayData* ad, list res):
    """
    Recursively append Buffer wrappers from *ad* and its children.
    """
    cdef size_t i, n
    assert ad != NULL
    n = ad.buffers.size()
    for i in range(n):
        buf = ad.buffers[i]
        res.append(pyarrow_wrap_buffer(buf)
                   if buf.get() != NULL else None)
    n = ad.child_data.size()
    for i in range(n):
        _append_array_buffers(ad.child_data[i].get(), res)


cdef _reduce_array_data(const CArrayData* ad):
    """
    Recursively dissect ArrayData to (pickable) tuples.
    """
    cdef size_t i, n
    assert ad != NULL

    n = ad.buffers.size()
    buffers = []
    for i in range(n):
        buf = ad.buffers[i]
        buffers.append(pyarrow_wrap_buffer(buf)
                       if buf.get() != NULL else None)

    children = []
    n = ad.child_data.size()
    for i in range(n):
        children.append(_reduce_array_data(ad.child_data[i].get()))

    if ad.dictionary.get() != NULL:
        dictionary = _reduce_array_data(ad.dictionary.get().data().get())
    else:
        dictionary = None

    return pyarrow_wrap_data_type(ad.type), ad.length, ad.null_count, \
        ad.offset, buffers, children, dictionary


cdef shared_ptr[CArrayData] _reconstruct_array_data(data):
    """
    Reconstruct CArrayData objects from the tuple structure generated
    by _reduce_array_data.
    """
    cdef:
        int64_t length, null_count, offset, i
        DataType dtype
        Buffer buf
        vector[shared_ptr[CBuffer]] c_buffers
        vector[shared_ptr[CArrayData]] c_children
        shared_ptr[CArray] c_dictionary

    dtype, length, null_count, offset, buffers, children, dictionary = data

    for i in range(len(buffers)):
        buf = buffers[i]
        if buf is None:
            c_buffers.push_back(shared_ptr[CBuffer]())
        else:
            c_buffers.push_back(buf.buffer)

    for i in range(len(children)):
        c_children.push_back(_reconstruct_array_data(children[i]))

    if dictionary is not None:
        c_dictionary = MakeArray(_reconstruct_array_data(dictionary))

    return CArrayData.MakeWithChildrenAndDictionary(
        dtype.sp_type,
        length,
        c_buffers,
        c_children,
        c_dictionary,
        null_count,
        offset)


def _restore_array(data):
    """
    Reconstruct an Array from pickled ArrayData.
    """
    cdef shared_ptr[CArrayData] ad = _reconstruct_array_data(data)
    return pyarrow_wrap_array(MakeArray(ad))


cdef CFilterOptions _convert_filter_option(object null_selection_behavior):
    cdef CFilterOptions options

    if null_selection_behavior == 'drop':
        options.null_selection_behavior = \
            CFilterNullSelectionBehavior_DROP
    elif null_selection_behavior == 'emit_null':
        options.null_selection_behavior = \
            CFilterNullSelectionBehavior_EMIT_NULL
    else:
        raise ValueError(
            '"{}" is not a valid null_selection_behavior'.format(
                null_selection_behavior)
        )
    return options


cdef class _PandasConvertible:

    def to_pandas(
            self,
            memory_pool=None,
            categories=None,
            bint strings_to_categorical=False,
            bint zero_copy_only=False,
            bint integer_object_nulls=False,
            bint date_as_object=True,
            bint use_threads=True,
            bint deduplicate_objects=True,
            bint ignore_metadata=False,
            bint safe=True,
            bint split_blocks=False,
            bint self_destruct=False,
            types_mapper=None
    ):
        """
        Convert to a pandas-compatible NumPy array or DataFrame, as appropriate

        Parameters
        ----------
        memory_pool : MemoryPool, default None
            Arrow MemoryPool to use for allocations. Uses the default memory
            pool is not passed.
        strings_to_categorical : bool, default False
            Encode string (UTF8) and binary types to pandas.Categorical.
        categories: list, default empty
            List of fields that should be returned as pandas.Categorical. Only
            applies to table-like data structures.
        zero_copy_only : bool, default False
            Raise an ArrowException if this function call would require copying
            the underlying data.
        integer_object_nulls : bool, default False
            Cast integers with nulls to objects
        date_as_object : bool, default True
            Cast dates to objects. If False, convert to datetime64[ns] dtype.
        use_threads: bool, default True
            Whether to parallelize the conversion using multiple threads.
        deduplicate_objects : bool, default False
            Do not create multiple copies Python objects when created, to save
            on memory use. Conversion will be slower.
        ignore_metadata : bool, default False
            If True, do not use the 'pandas' metadata to reconstruct the
            DataFrame index, if present
        safe : bool, default True
            For certain data types, a cast is needed in order to store the
            data in a pandas DataFrame or Series (e.g. timestamps are always
            stored as nanoseconds in pandas). This option controls whether it
            is a safe cast or not.
        split_blocks : bool, default False
            If True, generate one internal "block" for each column when
            creating a pandas.DataFrame from a RecordBatch or Table. While this
            can temporarily reduce memory note that various pandas operations
            can trigger "consolidation" which may balloon memory use.
        self_destruct : bool, default False
            EXPERIMENTAL: If True, attempt to deallocate the originating Arrow
            memory while converting the Arrow object to pandas. If you use the
            object after calling to_pandas with this option it will crash your
            program.
        types_mapper : function, default None
            A function mapping a pyarrow DataType to a pandas ExtensionDtype.
            This can be used to override the default pandas type for conversion
            of built-in pyarrow types or in absence of pandas_metadata in the
            Table schema. The function receives a pyarrow DataType and is
            expected to return a pandas ExtensionDtype or ``None`` if the
            default conversion should be used for that type. If you have
            a dictionary mapping, you can pass ``dict.get`` as function.

        Returns
        -------
        pandas.Series or pandas.DataFrame depending on type of object
        """
        options = dict(
            pool=memory_pool,
            strings_to_categorical=strings_to_categorical,
            zero_copy_only=zero_copy_only,
            integer_object_nulls=integer_object_nulls,
            date_as_object=date_as_object,
            use_threads=use_threads,
            deduplicate_objects=deduplicate_objects,
            safe=safe,
            split_blocks=split_blocks,
            self_destruct=self_destruct
        )
        return self._to_pandas(options, categories=categories,
                               ignore_metadata=ignore_metadata,
                               types_mapper=types_mapper)


cdef PandasOptions _convert_pandas_options(dict options):
    cdef PandasOptions result
    result.pool = maybe_unbox_memory_pool(options['pool'])
    result.strings_to_categorical = options['strings_to_categorical']
    result.zero_copy_only = options['zero_copy_only']
    result.integer_object_nulls = options['integer_object_nulls']
    result.date_as_object = options['date_as_object']
    result.use_threads = options['use_threads']
    result.deduplicate_objects = options['deduplicate_objects']
    result.safe_cast = options['safe']
    result.split_blocks = options['split_blocks']
    result.self_destruct = options['self_destruct']
    return result


cdef class Array(_PandasConvertible):
    """
    The base class for all Arrow arrays.
    """

    def __init__(self):
        raise TypeError("Do not call {}'s constructor directly, use one of "
                        "the `pyarrow.Array.from_*` functions instead."
                        .format(self.__class__.__name__))

    cdef void init(self, const shared_ptr[CArray]& sp_array) except *:
        self.sp_array = sp_array
        self.ap = sp_array.get()
        self.type = pyarrow_wrap_data_type(self.sp_array.get().type())

    def __eq__(self, other):
        raise NotImplementedError('Comparisons with pyarrow.Array are not '
                                  'implemented')

    def _debug_print(self):
        with nogil:
            check_status(DebugPrint(deref(self.ap), 0))

    def diff(self, Array other):
        """
        Compare contents of this array against another one.

        Return string containing the result of arrow::Diff comparing contents
        of this array against the other array.
        """
        cdef c_string result
        with nogil:
            result = self.ap.Diff(deref(other.ap))
        return frombytes(result)

    def cast(self, object target_type, bint safe=True):
        """
        Cast array values to another data type.

        Example
        -------

        >>> from datetime import datetime
        >>> import pyarrow as pa
        >>> arr = pa.array([datetime(2010, 1, 1), datetime(2015, 1, 1)])
        >>> arr.type
        TimestampType(timestamp[us])

        You can use ``pyarrow.DataType`` objects to specify the target type:

        >>> arr.cast(pa.timestamp('ms'))
        <pyarrow.lib.TimestampArray object at 0x10420eb88>
        [
          1262304000000,
          1420070400000
        ]
        >>> arr.cast(pa.timestamp('ms')).type
        TimestampType(timestamp[ms])

        Alternatively, it is also supported to use the string aliases for these
        types:

        >>> arr.cast('timestamp[ms]')
        <pyarrow.lib.TimestampArray object at 0x10420eb88>
        [
          1262304000000,
          1420070400000
        ]
        >>> arr.cast('timestamp[ms]').type
        TimestampType(timestamp[ms])

        Parameters
        ----------
        target_type : DataType
            Type to cast to
        safe : bool, default True
            Check for overflows or other unsafe conversions

        Returns
        -------
        casted : Array
        """
        cdef:
            CCastOptions options = CCastOptions(safe)
            DataType type = ensure_type(target_type)
            shared_ptr[CArray] result

        with nogil:
            check_status(Cast(_context(), self.ap[0], type.sp_type,
                              options, &result))

        return pyarrow_wrap_array(result)

    def view(self, object target_type):
        """
        Return zero-copy "view" of array as another data type.

        The data types must have compatible columnar buffer layouts

        Parameters
        ----------
        target_type : DataType
            Type to construct view as.

        Returns
        -------
        view : Array
        """
        cdef DataType type = ensure_type(target_type)
        cdef shared_ptr[CArray] result
        with nogil:
            result = GetResultValue(self.ap.View(type.sp_type))
        return pyarrow_wrap_array(result)

    def sum(self):
        """
        Sum the values in a numerical array.
        """
        cdef CDatum out

        with nogil:
            check_status(Sum(_context(), CDatum(self.sp_array), &out))

        return wrap_datum(out)

    def unique(self):
        """
        Compute distinct elements in array.
        """
        cdef shared_ptr[CArray] result

        with nogil:
            check_status(Unique(_context(), CDatum(self.sp_array), &result))

        return pyarrow_wrap_array(result)

    def dictionary_encode(self):
        """
        Compute dictionary-encoded representation of array.
        """
        cdef CDatum out

        with nogil:
            check_status(DictionaryEncode(_context(), CDatum(self.sp_array),
                                          &out))

        return wrap_datum(out)

    def value_counts(self):
        """
        Compute counts of unique elements in array.

        Returns
        -------
        An array of  <input type "Values", int64_t "Counts"> structs
        """
        cdef shared_ptr[CArray] result

        with nogil:
            check_status(ValueCounts(_context(), CDatum(self.sp_array),
                                     &result))
        return pyarrow_wrap_array(result)

    @staticmethod
    def from_pandas(obj, mask=None, type=None, bint safe=True,
                    MemoryPool memory_pool=None):
        """
        Convert pandas.Series to an Arrow Array.

        This method uses Pandas semantics about what values indicate
        nulls. See pyarrow.array for more general conversion from arrays or
        sequences to Arrow arrays.

        Parameters
        ----------
        sequence : ndarray, pandas.Series, array-like
        mask : array (boolean), optional
            Indicate which values are null (True) or not null (False).
        type : pyarrow.DataType
            Explicit type to attempt to coerce to, otherwise will be inferred
            from the data.
        safe : bool, default True
            Check for overflows or other unsafe conversions.
        memory_pool : pyarrow.MemoryPool, optional
            If not passed, will allocate memory from the currently-set default
            memory pool.

        Notes
        -----
        Localized timestamps will currently be returned as UTC (pandas's native
        representation). Timezone-naive data will be implicitly interpreted as
        UTC.

        Returns
        -------
        array : pyarrow.Array or pyarrow.ChunkedArray
            ChunkedArray is returned if object data overflows binary buffer.
        """
        return array(obj, mask=mask, type=type, safe=safe, from_pandas=True,
                     memory_pool=memory_pool)

    def __reduce__(self):
        return _restore_array, \
            (_reduce_array_data(self.sp_array.get().data().get()),)

    @staticmethod
    def from_buffers(DataType type, length, buffers, null_count=-1, offset=0,
                     children=None):
        """
        Construct an Array from a sequence of buffers.

        The concrete type returned depends on the datatype.

        Parameters
        ----------
        type : DataType
            The value type of the array.
        length : int
            The number of values in the array.
        buffers : List[Buffer]
            The buffers backing this array.
        null_count : int, default -1
            The number of null entries in the array. Negative value means that
            the null count is not known.
        offset : int, default 0
            The array's logical offset (in values, not in bytes) from the
            start of each buffer.
        children : List[Array], default None
            Nested type children with length matching type.num_children.

        Returns
        -------
        array : Array
        """
        cdef:
            Buffer buf
            Array child
            vector[shared_ptr[CBuffer]] c_buffers
            vector[shared_ptr[CArrayData]] c_child_data
            shared_ptr[CArrayData] array_data

        children = children or []

        if type.num_children != len(children):
            raise ValueError("Type's expected number of children "
                             "({0}) did not match the passed number "
                             "({1}).".format(type.num_children, len(children)))

        if type.num_buffers != len(buffers):
            raise ValueError("Type's expected number of buffers "
                             "({0}) did not match the passed number "
                             "({1}).".format(type.num_buffers, len(buffers)))

        for buf in buffers:
            # None will produce a null buffer pointer
            c_buffers.push_back(pyarrow_unwrap_buffer(buf))

        for child in children:
            c_child_data.push_back(child.ap.data())

        array_data = CArrayData.MakeWithChildren(type.sp_type, length,
                                                 c_buffers, c_child_data,
                                                 null_count, offset)
        cdef Array result = pyarrow_wrap_array(MakeArray(array_data))
        result.validate()
        return result

    @property
    def null_count(self):
        return self.sp_array.get().null_count()

    @property
    def nbytes(self):
        """
        Total number of bytes consumed by the elements of the array.
        """
        size = 0
        for buf in self.buffers():
            if buf is not None:
                size += buf.size
        return size

    def __sizeof__(self):
        return super(Array, self).__sizeof__() + self.nbytes

    def __iter__(self):
        for i in range(len(self)):
            yield self.getitem(i)

    def __repr__(self):
        type_format = object.__repr__(self)
        return '{0}\n{1}'.format(type_format, str(self))

    def to_string(self, int indent=0, int window=10):
        cdef:
            c_string result

        with nogil:
            check_status(
                PrettyPrint(
                    deref(self.ap),
                    PrettyPrintOptions(indent, window),
                    &result
                )
            )

        return frombytes(result)

    def format(self, **kwargs):
        import warnings
        warnings.warn('Array.format is deprecated, use Array.to_string')
        return self.to_string(**kwargs)

    def __str__(self):
        return self.to_string()

    def equals(Array self, Array other):
        return self.ap.Equals(deref(other.ap))

    def __len__(self):
        return self.length()

    cdef int64_t length(self):
        if self.sp_array.get():
            return self.sp_array.get().length()
        else:
            return 0

    def isnull(self):
        raise NotImplemented

    def __getitem__(self, index):
        """
        Return the value at the given index.

        Returns
        -------
        value : Scalar
        """
        if PySlice_Check(index):
            return _normalize_slice(self, index)

        return self.getitem(_normalize_index(index, self.length()))

    cdef getitem(self, int64_t i):
        return box_scalar(self.type, self.sp_array, i)

    def slice(self, offset=0, length=None):
        """
        Compute zero-copy slice of this array.

        Parameters
        ----------
        offset : int, default 0
            Offset from start of array to slice.
        length : int, default None
            Length of slice (default is until end of Array starting from
            offset).

        Returns
        -------
        sliced : RecordBatch
        """
        cdef:
            shared_ptr[CArray] result

        if offset < 0:
            raise IndexError('Offset must be non-negative')

        if length is None:
            result = self.ap.Slice(offset)
        else:
            result = self.ap.Slice(offset, length)

        return pyarrow_wrap_array(result)

    def take(self, Array indices):
        """
        Take elements from an array.

        The resulting array will be of the same type as the input array, with
        elements taken from the input array at the given indices. If an index
        is null then the taken element will be null.

        Parameters
        ----------
        indices : Array
            The indices of the values to extract. Array needs to be of
            integer type.

        Returns
        -------
        Array

        Examples
        --------

        >>> import pyarrow as pa
        >>> arr = pa.array(["a", "b", "c", None, "e", "f"])
        >>> indices = pa.array([0, None, 4, 3])
        >>> arr.take(indices)
        <pyarrow.lib.StringArray object at 0x7ffa4fc7d368>
        [
          "a",
          null,
          "e",
          null
        ]
        """
        cdef:
            cdef CTakeOptions options
            cdef CDatum out

        with nogil:
            check_status(Take(_context(), CDatum(self.sp_array),
                              CDatum(indices.sp_array), options, &out))

        return wrap_datum(out)

    def filter(self, Array mask, null_selection_behavior='drop'):
        """
        Filter the array with a boolean mask.

        Parameters
        ----------
        mask : Array
            The boolean mask indicating which values to extract.
        null_selection_behavior : str, default 'drop'
            Configure the behavior on encountering a null slot in the mask.
            Allowed values are 'drop' and 'emit_null'.

            - 'drop': nulls will be treated as equivalent to False.
            - 'emit_null': nulls will result in a null in the output.

        Returns
        -------
        Array

        Examples
        --------

        >>> import pyarrow as pa
        >>> arr = pa.array(["a", "b", "c", None, "e"])
        >>> mask = pa.array([True, False, None, False, True])
        >>> arr.filter(mask)
        <pyarrow.lib.StringArray object at 0x7fa826df9200>
        [
          "a",
          "e"
        ]
        >>> arr.filter(mask, null_selection_behavior='emit_null')
        <pyarrow.lib.StringArray object at 0x7fa826df9200>
        [
          "a",
          null,
          "e"
        ]
        """
        cdef:
            CDatum out
            CFilterOptions options

        options = _convert_filter_option(null_selection_behavior)

        with nogil:
            check_status(FilterKernel(_context(), CDatum(self.sp_array),
                                      CDatum(mask.sp_array), options, &out))

        return wrap_datum(out)

    def _to_pandas(self, options, **kwargs):
        return _array_like_to_pandas(self, options)

    def __array__(self, dtype=None):
        values = self.to_numpy(zero_copy_only=False)
        if dtype is None:
            return values
        return values.astype(dtype)

    def to_numpy(self, zero_copy_only=True, writable=False):
        """
        Return a NumPy view or copy of this array (experimental).

        By default, tries to return a view of this array. This is only
        supported for primitive arrays with the same memory layout as NumPy
        (i.e. integers, floating point, ..) and without any nulls.

        Parameters
        ----------
        zero_copy_only : bool, default True
            If True, an exception will be raised if the conversion to a numpy
            array would require copying the underlying data (e.g. in presence
            of nulls, or for non-primitive types).
        writable : bool, default False
            For numpy arrays created with zero copy (view on the Arrow data),
            the resulting array is not writable (Arrow data is immutable).
            By setting this to True, a copy of the array is made to ensure
            it is writable.

        Returns
        -------
        array : numpy.ndarray
        """
        cdef:
            PyObject* out
            PandasOptions c_options
            object values

        if zero_copy_only and writable:
            raise ValueError(
                "Cannot return a writable array if asking for zero-copy")

        c_options.zero_copy_only = zero_copy_only

        with nogil:
            check_status(ConvertArrayToPandas(c_options, self.sp_array,
                                              self, &out))

        # wrap_array_output uses pandas to convert to Categorical, here
        # always convert to numpy array without pandas dependency
        array = PyObject_to_object(out)

        if isinstance(array, dict):
            array = np.take(array['dictionary'], array['indices'])

        if writable and not array.flags.writeable:
            # if the conversion already needed to a copy, writeable is True
            array = array.copy()
        return array

    def to_pylist(self):
        """
        Convert to a list of native Python objects.

        Returns
        -------
        lst : list
        """
        return [x.as_py() for x in self]

    def validate(self, *, full=False):
        """
        Perform validation checks.  An exception is raised if validation fails.

        By default only cheap validation checks are run.  Pass `full=True`
        for thorough validation checks (potentially O(n)).

        Parameters
        ----------
        full: bool, default False
            If True, run expensive checks, otherwise cheap checks only.

        Raises
        ------
        ArrowInvalid
        """
        if full:
            with nogil:
                check_status(self.ap.ValidateFull())
        else:
            with nogil:
                check_status(self.ap.Validate())

    @property
    def offset(self):
        """
        A relative position into another array's data.

        The purpose is to enable zero-copy slicing. This value defaults to zero
        but must be applied on all operations with the physical storage
        buffers.
        """
        return self.sp_array.get().offset()

    def buffers(self):
        """
        Return a list of Buffer objects pointing to this array's physical
        storage.

        To correctly interpret these buffers, you need to also apply the offset
        multiplied with the size of the stored data type.
        """
        res = []
        _append_array_buffers(self.sp_array.get().data().get(), res)
        return res

    def _export_to_c(self, uintptr_t out_ptr, uintptr_t out_schema_ptr=0):
        """
        Export to a C ArrowArray struct, given its pointer.

        If a C ArrowSchema struct pointer is also given, the array type
        is exported to it at the same time.

        Parameters
        ----------
        out_ptr: int
            The raw pointer to a C ArrowArray struct.
        out_schema_ptr: int (optional)
            The raw pointer to a C ArrowSchema struct.

        Be careful: if you don't pass the ArrowArray struct to a consumer,
        array memory will leak.  This is a low-level function intended for
        expert users.
        """
        with nogil:
            check_status(ExportArray(deref(self.sp_array),
                                     <ArrowArray*> out_ptr,
                                     <ArrowSchema*> out_schema_ptr))

    @staticmethod
    def _import_from_c(uintptr_t in_ptr, type):
        """
        Import Array from a C ArrowArray struct, given its pointer
        and the imported array type.

        Parameters
        ----------
        in_ptr: int
            The raw pointer to a C ArrowArray struct.
        type: DataType or int
            Either a DataType object, or the raw pointer to a C ArrowSchema
            struct.

        This is a low-level function intended for expert users.
        """
        cdef:
            shared_ptr[CArray] c_array

        c_type = pyarrow_unwrap_data_type(type)
        if c_type == nullptr:
            # Not a DataType object, perhaps a raw ArrowSchema pointer
            type_ptr = <uintptr_t> type
            with nogil:
                c_array = GetResultValue(ImportArray(<ArrowArray*> in_ptr,
                                                     <ArrowSchema*> type_ptr))
        else:
            with nogil:
                c_array = GetResultValue(ImportArray(<ArrowArray*> in_ptr,
                                                     c_type))
        return pyarrow_wrap_array(c_array)


cdef _array_like_to_pandas(obj, options):
    cdef:
        PyObject* out
        PandasOptions c_options = _convert_pandas_options(options)

    original_type = obj.type
    name = obj._name

    # ARROW-3789(wesm): Convert date/timestamp types to datetime64[ns]
    c_options.coerce_temporal_nanoseconds = True

    if isinstance(obj, Array):
        with nogil:
            check_status(ConvertArrayToPandas(c_options,
                                              (<Array> obj).sp_array,
                                              obj, &out))
    elif isinstance(obj, ChunkedArray):
        with nogil:
            check_status(libarrow.ConvertChunkedArrayToPandas(
                c_options,
                (<ChunkedArray> obj).sp_chunked_array,
                obj, &out))

    result = pandas_api.series(wrap_array_output(out), name=name)

    if (isinstance(original_type, TimestampType) and
            original_type.tz is not None):
        from pyarrow.pandas_compat import make_tz_aware
        result = make_tz_aware(result, original_type.tz)

    return result


cdef wrap_array_output(PyObject* output):
    cdef object obj = PyObject_to_object(output)

    if isinstance(obj, dict):
        return pandas_api.categorical_type(obj['indices'],
                                           categories=obj['dictionary'],
                                           ordered=obj['ordered'],
                                           fastpath=True)
    else:
        return obj


cdef class NullArray(Array):
    """
    Concrete class for Arrow arrays of null data type.
    """


cdef class BooleanArray(Array):
    """
    Concrete class for Arrow arrays of boolean data type.
    """


cdef class NumericArray(Array):
    """
    A base class for Arrow numeric arrays.
    """


cdef class IntegerArray(NumericArray):
    """
    A base class for Arrow integer arrays.
    """


cdef class FloatingPointArray(NumericArray):
    """
    A base class for Arrow floating-point arrays.
    """


cdef class Int8Array(IntegerArray):
    """
    Concrete class for Arrow arrays of int8 data type.
    """


cdef class UInt8Array(IntegerArray):
    """
    Concrete class for Arrow arrays of uint8 data type.
    """


cdef class Int16Array(IntegerArray):
    """
    Concrete class for Arrow arrays of int16 data type.
    """


cdef class UInt16Array(IntegerArray):
    """
    Concrete class for Arrow arrays of uint16 data type.
    """


cdef class Int32Array(IntegerArray):
    """
    Concrete class for Arrow arrays of int32 data type.
    """


cdef class UInt32Array(IntegerArray):
    """
    Concrete class for Arrow arrays of uint32 data type.
    """


cdef class Int64Array(IntegerArray):
    """
    Concrete class for Arrow arrays of int64 data type.
    """


cdef class UInt64Array(IntegerArray):
    """
    Concrete class for Arrow arrays of uint64 data type.
    """


cdef class Date32Array(NumericArray):
    """
    Concrete class for Arrow arrays of date32 data type.
    """


cdef class Date64Array(NumericArray):
    """
    Concrete class for Arrow arrays of date64 data type.
    """


cdef class TimestampArray(NumericArray):
    """
    Concrete class for Arrow arrays of timestamp data type.
    """


cdef class Time32Array(NumericArray):
    """
    Concrete class for Arrow arrays of time32 data type.
    """


cdef class Time64Array(NumericArray):
    """
    Concrete class for Arrow arrays of time64 data type.
    """


cdef class DurationArray(NumericArray):
    """
    Concrete class for Arrow arrays of duration data type.
    """

cdef class HalfFloatArray(FloatingPointArray):
    """
    Concrete class for Arrow arrays of float16 data type.
    """


cdef class FloatArray(FloatingPointArray):
    """
    Concrete class for Arrow arrays of float32 data type.
    """


cdef class DoubleArray(FloatingPointArray):
    """
    Concrete class for Arrow arrays of float64 data type.
    """


cdef class FixedSizeBinaryArray(Array):
    """
    Concrete class for Arrow arrays of a fixed-size binary data type.
    """


cdef class Decimal128Array(FixedSizeBinaryArray):
    """
    Concrete class for Arrow arrays of decimal128 data type.
    """


cdef class ListArray(Array):
    """
    Concrete class for Arrow arrays of a list data type.
    """

    @staticmethod
    def from_arrays(offsets, values, MemoryPool pool=None):
        """
        Construct ListArray from arrays of int32 offsets and values.

        Parameters
        ----------
        offsets : Array (int32 type)
        values : Array (any type)

        Returns
        -------
        list_array : ListArray
        """
        cdef:
            Array _offsets, _values
            shared_ptr[CArray] out
        cdef CMemoryPool* cpool = maybe_unbox_memory_pool(pool)

        _offsets = asarray(offsets, type='int32')
        _values = asarray(values)

        with nogil:
            out = GetResultValue(
                CListArray.FromArrays(_offsets.ap[0], _values.ap[0], cpool))
        cdef Array result = pyarrow_wrap_array(out)
        result.validate()
        return result

    @property
    def values(self):
        cdef CListArray* arr = <CListArray*> self.ap
        return pyarrow_wrap_array(arr.values())

    @property
    def offsets(self):
        """
        Return the offsets as an int32 array.
        """
        return Array.from_buffers(
            int32(), len(self) + 1, [None, self.buffers()[1]],
            offset=self.offset)

    def flatten(self, MemoryPool memory_pool=None):
        """
        Unnest this ListArray by one level.

        The returned Array is logically a concatenation of all the sub-lists
        in this Array.

        Note that this method is different from ``self.values()`` in that
        it takes care of the slicing offset as well as null elements backed
        by non-empty sub-lists.

        Parameters
        ----------
        memory_pool : pyarrow.MemoryPool, optional
            If not passed, will allocate memory from the currently-set default
            memory pool

        Returns
        -------
        result : Array
        """
        cdef:
            shared_ptr[CArray] c_result_array
            CMemoryPool* cpool = maybe_unbox_memory_pool(memory_pool)
            CListArray* arr = <CListArray*> self.ap

        with nogil:
            c_result_array = GetResultValue(arr.Flatten(cpool))

        return pyarrow_wrap_array(c_result_array)


cdef class LargeListArray(Array):
    """
    Concrete class for Arrow arrays of a large list data type.

    Identical to ListArray, but 64-bit offsets.
    """

    @staticmethod
    def from_arrays(offsets, values, MemoryPool pool=None):
        """
        Construct LargeListArray from arrays of int64 offsets and values.

        Parameters
        ----------
        offsets : Array (int64 type)
        values : Array (any type)

        Returns
        -------
        list_array : LargeListArray
        """
        cdef:
            Array _offsets, _values
            shared_ptr[CArray] out
        cdef CMemoryPool* cpool = maybe_unbox_memory_pool(pool)

        _offsets = asarray(offsets, type='int64')
        _values = asarray(values)

        with nogil:
            out = GetResultValue(
                CLargeListArray.FromArrays(_offsets.ap[0], _values.ap[0],
                                           cpool))
        cdef Array result = pyarrow_wrap_array(out)
        result.validate()
        return result

    @property
    def values(self):
        cdef CLargeListArray* arr = <CLargeListArray*> self.ap
        return pyarrow_wrap_array(arr.values())

    @property
    def offsets(self):
        """
        Return the offsets as an int64 array.
        """
        return Array.from_buffers(
            int64(), len(self) + 1, [None, self.buffers()[1]],
            offset=self.offset)

    def flatten(self, MemoryPool memory_pool=None):
        """
        Unnest this LargeListArray by one level.

        The returned Array is logically a concatenation of all the sub-lists
        in this Array.

        Note that this method is different from ``self.values()`` in that
        it takes care of the slicing offset as well as null elements backed
        by non-empty sub-lists.

        Parameters
        ----------
        memory_pool : pyarrow.MemoryPool, optional
            If not passed, will allocate memory from the currently-set default
            memory pool.

        Returns
        -------
        result : Array
        """
        cdef:
            shared_ptr[CArray] c_result_array
            CMemoryPool* cpool = maybe_unbox_memory_pool(memory_pool)
            CLargeListArray* arr = <CLargeListArray*> self.ap

        with nogil:
            c_result_array = GetResultValue(arr.Flatten(cpool))

        return pyarrow_wrap_array(c_result_array)


cdef class MapArray(Array):
    """
    Concrete class for Arrow arrays of a map data type.
    """

    @staticmethod
    def from_arrays(offsets, keys, items, MemoryPool pool=None):
        """
        Construct MapArray from arrays of int32 offsets and key, item arrays.

        Parameters
        ----------
        offsets : array-like or sequence (int32 type)
        keys : array-like or sequence (any type)
        items : array-like or sequence (any type)

        Returns
        -------
        map_array : MapArray
        """
        cdef:
            Array _offsets, _keys, _items
            shared_ptr[CArray] out
        cdef CMemoryPool* cpool = maybe_unbox_memory_pool(pool)

        _offsets = asarray(offsets, type='int32')
        _keys = asarray(keys)
        _items = asarray(items)

        with nogil:
            out = GetResultValue(
                CMapArray.FromArrays(_offsets.sp_array,
                                     _keys.sp_array,
                                     _items.sp_array, cpool))
        cdef Array result = pyarrow_wrap_array(out)
        result.validate()
        return result

    @property
    def keys(self):
        return pyarrow_wrap_array((<CMapArray*> self.ap).keys())

    @property
    def items(self):
        return pyarrow_wrap_array((<CMapArray*> self.ap).items())


cdef class FixedSizeListArray(Array):
    """
    Concrete class for Arrow arrays of a fixed size list data type.
    """

    @staticmethod
    def from_arrays(values, int32_t list_size):
        """
        Construct FixedSizeListArray from array of values and a list length.

        Parameters
        ----------
        values : Array (any type)
        list_size : int
            The fixed length of the lists.

        Returns
        -------
        FixedSizeListArray
        """
        cdef:
            Array _values
            CResult[shared_ptr[CArray]] c_result

        _values = asarray(values)

        with nogil:
            c_result = CFixedSizeListArray.FromArrays(
                _values.sp_array, list_size)
        cdef Array result = pyarrow_wrap_array(GetResultValue(c_result))
        result.validate()
        return result

    @property
    def values(self):
        return self.flatten()

    def flatten(self):
        """
        Unnest this FixedSizeListArray by one level.

        Returns
        -------
        result : Array
        """
        cdef CFixedSizeListArray* arr = <CFixedSizeListArray*> self.ap
        return pyarrow_wrap_array(arr.values())


cdef class UnionArray(Array):
    """
    Concrete class for Arrow arrays of a Union data type.
    """

    @staticmethod
    def from_dense(Array types, Array value_offsets, list children,
                   list field_names=None, list type_codes=None):
        """
        Construct dense UnionArray from arrays of int8 types, int32 offsets and
        children arrays

        Parameters
        ----------
        types : Array (int8 type)
        value_offsets : Array (int32 type)
        children : list
        field_names : list
        type_codes : list

        Returns
        -------
        union_array : UnionArray
        """
        cdef shared_ptr[CArray] out
        cdef vector[shared_ptr[CArray]] c
        cdef Array child
        cdef vector[c_string] c_field_names
        cdef vector[int8_t] c_type_codes
        for child in children:
            c.push_back(child.sp_array)
        if field_names is not None:
            for x in field_names:
                c_field_names.push_back(tobytes(x))
        if type_codes is not None:
            for x in type_codes:
                c_type_codes.push_back(x)
        with nogil:
            out = GetResultValue(CUnionArray.MakeDense(
                deref(types.ap), deref(value_offsets.ap), c, c_field_names,
                c_type_codes))
        cdef Array result = pyarrow_wrap_array(out)
        result.validate()
        return result

    @staticmethod
    def from_sparse(Array types, list children, list field_names=None,
                    list type_codes=None):
        """
        Construct sparse UnionArray from arrays of int8 types and children
        arrays

        Parameters
        ----------
        types : Array (int8 type)
        children : list
        field_names : list
        type_codes : list

        Returns
        -------
        union_array : UnionArray
        """
        cdef shared_ptr[CArray] out
        cdef vector[shared_ptr[CArray]] c
        cdef Array child
        cdef vector[c_string] c_field_names
        cdef vector[int8_t] c_type_codes
        for child in children:
            c.push_back(child.sp_array)
        if field_names is not None:
            for x in field_names:
                c_field_names.push_back(tobytes(x))
        if type_codes is not None:
            for x in type_codes:
                c_type_codes.push_back(x)
        with nogil:
            out = GetResultValue(CUnionArray.MakeSparse(
                deref(types.ap), c, c_field_names, c_type_codes))
        cdef Array result = pyarrow_wrap_array(out)
        result.validate()
        return result


cdef class StringArray(Array):
    """
    Concrete class for Arrow arrays of string (or utf8) data type.
    """

    @staticmethod
    def from_buffers(int length, Buffer value_offsets, Buffer data,
                     Buffer null_bitmap=None, int null_count=-1,
                     int offset=0):
        """
        Construct a StringArray from value_offsets and data buffers.
        If there are nulls in the data, also a null_bitmap and the matching
        null_count must be passed.

        Parameters
        ----------
        length : int
        value_offsets : Buffer
        data : Buffer
        null_bitmap : Buffer, optional
        null_count : int, default 0
        offset : int, default 0

        Returns
        -------
        string_array : StringArray
        """
        return Array.from_buffers(utf8(), length,
                                  [null_bitmap, value_offsets, data],
                                  null_count, offset)


cdef class LargeStringArray(Array):
    """
    Concrete class for Arrow arrays of large string (or utf8) data type.
    """

    @staticmethod
    def from_buffers(int length, Buffer value_offsets, Buffer data,
                     Buffer null_bitmap=None, int null_count=-1,
                     int offset=0):
        """
        Construct a LargeStringArray from value_offsets and data buffers.
        If there are nulls in the data, also a null_bitmap and the matching
        null_count must be passed.

        Parameters
        ----------
        length : int
        value_offsets : Buffer
        data : Buffer
        null_bitmap : Buffer, optional
        null_count : int, default 0
        offset : int, default 0

        Returns
        -------
        string_array : StringArray
        """
        return Array.from_buffers(large_utf8(), length,
                                  [null_bitmap, value_offsets, data],
                                  null_count, offset)


cdef class BinaryArray(Array):
    """
    Concrete class for Arrow arrays of variable-sized binary data type.
    """


cdef class LargeBinaryArray(Array):
    """
    Concrete class for Arrow arrays of large variable-sized binary data type.
    """


cdef class DictionaryArray(Array):
    """
    Concrete class for dictionary-encoded Arrow arrays.
    """

    def dictionary_encode(self):
        return self

    @property
    def dictionary(self):
        cdef CDictionaryArray* darr = <CDictionaryArray*>(self.ap)

        if self._dictionary is None:
            self._dictionary = pyarrow_wrap_array(darr.dictionary())

        return self._dictionary

    @property
    def indices(self):
        cdef CDictionaryArray* darr = <CDictionaryArray*>(self.ap)

        if self._indices is None:
            self._indices = pyarrow_wrap_array(darr.indices())

        return self._indices

    @staticmethod
    def from_arrays(indices, dictionary, mask=None, bint ordered=False,
                    bint from_pandas=False, bint safe=True,
                    MemoryPool memory_pool=None):
        """
        Construct a DictionaryArray from indices and values.

        Parameters
        ----------
        indices : pyarrow.Array, numpy.ndarray or pandas.Series, int type
            Non-negative integers referencing the dictionary values by zero
            based index.
        dictionary : pyarrow.Array, ndarray or pandas.Series
            The array of values referenced by the indices.
        mask : ndarray or pandas.Series, bool type
            True values indicate that indices are actually null.
        from_pandas : bool, default False
            If True, the indices should be treated as though they originated in
            a pandas.Categorical (null encoded as -1).
        ordered : bool, default False
            Set to True if the category values are ordered.
        safe : bool, default True
            If True, check that the dictionary indices are in range.
        memory_pool : MemoryPool, default None
            For memory allocations, if required, otherwise uses default pool.

        Returns
        -------
        dict_array : DictionaryArray
        """
        cdef:
            Array _indices, _dictionary
            shared_ptr[CDataType] c_type
            shared_ptr[CArray] c_result

        if isinstance(indices, Array):
            if mask is not None:
                raise NotImplementedError(
                    "mask not implemented with Arrow array inputs yet")
            _indices = indices
        else:
            if from_pandas:
                _indices = _codes_to_indices(indices, mask, None, memory_pool)
            else:
                _indices = array(indices, mask=mask, memory_pool=memory_pool)

        if isinstance(dictionary, Array):
            _dictionary = dictionary
        else:
            _dictionary = array(dictionary, memory_pool=memory_pool)

        if not isinstance(_indices, IntegerArray):
            raise ValueError('Indices must be integer type')

        cdef c_bool c_ordered = ordered

        c_type.reset(new CDictionaryType(_indices.type.sp_type,
                                         _dictionary.sp_array.get().type(),
                                         c_ordered))

        if safe:
            with nogil:
                c_result = GetResultValue(
                    CDictionaryArray.FromArrays(c_type, _indices.sp_array,
                                                _dictionary.sp_array))
        else:
            c_result.reset(new CDictionaryArray(c_type, _indices.sp_array,
                                                _dictionary.sp_array))

        cdef Array result = pyarrow_wrap_array(c_result)
        result.validate()
        return result


cdef class StructArray(Array):
    """
    Concrete class for Arrow arrays of a struct data type.
    """

    def field(self, index):
        """
        Retrieves the child array belonging to field.

        Parameters
        ----------
        index : Union[int, str]
            Index / position or name of the field.

        Returns
        -------
        result : Array
        """
        cdef:
            CStructArray* arr = <CStructArray*> self.ap
            shared_ptr[CArray] child

        if isinstance(index, (bytes, str)):
            child = arr.GetFieldByName(tobytes(index))
            if child == nullptr:
                raise KeyError(index)
        elif isinstance(index, int):
            child = arr.field(
                <int>_normalize_index(index, self.ap.num_fields()))
        else:
            raise TypeError('Expected integer or string index')

        return pyarrow_wrap_array(child)

    def flatten(self, MemoryPool memory_pool=None):
        """
        Return one individual array for each field in the struct.

        Parameters
        ----------
        memory_pool : MemoryPool, default None
            For memory allocations, if required, otherwise use default pool.

        Returns
        -------
        result : List[Array]
        """
        cdef:
            vector[shared_ptr[CArray]] arrays
            CMemoryPool* pool = maybe_unbox_memory_pool(memory_pool)
            CStructArray* sarr = <CStructArray*> self.ap

        with nogil:
            arrays = GetResultValue(sarr.Flatten(pool))

        return [pyarrow_wrap_array(arr) for arr in arrays]

    @staticmethod
    def from_arrays(arrays, names=None, fields=None):
        """
        Construct StructArray from collection of arrays representing
        each field in the struct.

        Either field names or field instances must be passed.

        Parameters
        ----------
        arrays : sequence of Array
        names : List[str] (optional)
            Field names for each struct child.
        fields : List[Field] (optional)
            Field instances for each struct child.

        Returns
        -------
        result : StructArray
        """
        cdef:
            shared_ptr[CArray] c_array
            vector[shared_ptr[CArray]] c_arrays
            vector[c_string] c_names
            vector[shared_ptr[CField]] c_fields
            CResult[shared_ptr[CArray]] c_result
            ssize_t num_arrays
            ssize_t length
            ssize_t i
            Field py_field
            DataType struct_type

        if names is None and fields is None:
            raise ValueError('Must pass either names or fields')
        if names is not None and fields is not None:
            raise ValueError('Must pass either names or fields, not both')

        arrays = [asarray(x) for x in arrays]
        for arr in arrays:
            c_arrays.push_back(pyarrow_unwrap_array(arr))
        if names is not None:
            for name in names:
                c_names.push_back(tobytes(name))
        else:
            for item in fields:
                if isinstance(item, tuple):
                    py_field = field(*item)
                else:
                    py_field = item
                c_fields.push_back(py_field.sp_field)

        if (c_arrays.size() == 0 and c_names.size() == 0 and
                c_fields.size() == 0):
            # The C++ side doesn't allow this
            return array([], struct([]))

        if names is not None:
            # XXX Cannot pass "nullptr" for a shared_ptr<T> argument:
            # https://github.com/cython/cython/issues/3020
            c_result = CStructArray.MakeFromFieldNames(
                c_arrays, c_names, shared_ptr[CBuffer](), -1, 0)
        else:
            c_result = CStructArray.MakeFromFields(
                c_arrays, c_fields, shared_ptr[CBuffer](), -1, 0)
        cdef Array result = pyarrow_wrap_array(GetResultValue(c_result))
        result.validate()
        return result


cdef class ExtensionArray(Array):
    """
    Concrete class for Arrow extension arrays.
    """

    @property
    def storage(self):
        cdef:
            CExtensionArray* ext_array = <CExtensionArray*>(self.ap)

        return pyarrow_wrap_array(ext_array.storage())

    @staticmethod
    def from_storage(BaseExtensionType typ, Array storage):
        """
        Construct ExtensionArray from type and storage array.

        Parameters
        ----------
        typ: DataType
            The extension type for the result array.
        storage: Array
            The underlying storage for the result array.

        Returns
        -------
        ext_array : ExtensionArray
        """
        cdef:
            shared_ptr[CExtensionArray] ext_array

        if storage.type != typ.storage_type:
            raise TypeError("Incompatible storage type {0} "
                            "for extension type {1}".format(storage.type, typ))

        ext_array = make_shared[CExtensionArray](typ.sp_type, storage.sp_array)
        cdef Array result = pyarrow_wrap_array(<shared_ptr[CArray]> ext_array)
        result.validate()
        return result

    def _to_pandas(self, options, **kwargs):
        result = Array._to_pandas(self, options, **kwargs)
        # TODO(wesm): is passing through these parameters to the storage array
        # correct?
        return result.to_pandas(options, **kwargs)

    def to_numpy(self, **kwargs):
        """
        Convert extension array to a numpy ndarray.

        See Also
        --------
        Array.to_numpy
        """
        return self.storage.to_numpy(**kwargs)


cdef dict _array_classes = {
    _Type_NA: NullArray,
    _Type_BOOL: BooleanArray,
    _Type_UINT8: UInt8Array,
    _Type_UINT16: UInt16Array,
    _Type_UINT32: UInt32Array,
    _Type_UINT64: UInt64Array,
    _Type_INT8: Int8Array,
    _Type_INT16: Int16Array,
    _Type_INT32: Int32Array,
    _Type_INT64: Int64Array,
    _Type_DATE32: Date32Array,
    _Type_DATE64: Date64Array,
    _Type_TIMESTAMP: TimestampArray,
    _Type_TIME32: Time32Array,
    _Type_TIME64: Time64Array,
    _Type_DURATION: DurationArray,
    _Type_HALF_FLOAT: HalfFloatArray,
    _Type_FLOAT: FloatArray,
    _Type_DOUBLE: DoubleArray,
    _Type_LIST: ListArray,
    _Type_LARGE_LIST: LargeListArray,
    _Type_MAP: MapArray,
    _Type_FIXED_SIZE_LIST: FixedSizeListArray,
    _Type_UNION: UnionArray,
    _Type_BINARY: BinaryArray,
    _Type_STRING: StringArray,
    _Type_LARGE_BINARY: LargeBinaryArray,
    _Type_LARGE_STRING: LargeStringArray,
    _Type_DICTIONARY: DictionaryArray,
    _Type_FIXED_SIZE_BINARY: FixedSizeBinaryArray,
    _Type_DECIMAL: Decimal128Array,
    _Type_STRUCT: StructArray,
    _Type_EXTENSION: ExtensionArray,
}


cdef object get_array_class_from_type(
        const shared_ptr[CDataType]& sp_data_type):
    cdef CDataType* data_type = sp_data_type.get()
    if data_type == NULL:
        raise ValueError('Array data type was NULL')

    if data_type.id() == _Type_EXTENSION:
        py_ext_data_type = pyarrow_wrap_data_type(sp_data_type)
        return py_ext_data_type.__arrow_ext_class__()
    else:
        return _array_classes[data_type.id()]


cdef object get_values(object obj, bint* is_series):
    if pandas_api.is_series(obj) or pandas_api.is_index(obj):
        result = pandas_api.get_values(obj)
        is_series[0] = True
    elif isinstance(obj, np.ndarray):
        result = obj
        is_series[0] = False
    else:
        result = pandas_api.series(obj).values
        is_series[0] = False

    return result


def concat_arrays(arrays, MemoryPool memory_pool=None):
    """
    Concatenate the given arrays.

    The contents of the input arrays are copied into the returned array.

    Raises
    ------
    ArrowInvalid : if not all of the arrays have the same type.

    Parameters
    ----------
    arrays : iterable of pyarrow.Array
        Arrays to concatenate, must be identically typed.
    memory_pool : MemoryPool, default None
        For memory allocations. If None, the default pool is used.
    """
    cdef:
        vector[shared_ptr[CArray]] c_arrays
        shared_ptr[CArray] c_result
        Array array
        CMemoryPool* pool = maybe_unbox_memory_pool(memory_pool)

    for array in arrays:
        c_arrays.push_back(array.sp_array)

    with nogil:
        check_status(Concatenate(c_arrays, pool, &c_result))

    return pyarrow_wrap_array(c_result)


def _empty_array(DataType type):
    """
    Create empty array of the given type.
    """
    if type.id == Type_DICTIONARY:
        arr = DictionaryArray.from_arrays(
            _empty_array(type.index_type), _empty_array(type.value_type),
            ordered=type.ordered)
    else:
        arr = array([], type=type)
    return arr

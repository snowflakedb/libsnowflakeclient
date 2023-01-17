﻿// Licensed to the Apache Software Foundation (ASF) under one or more
// contributor license agreements. See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// The ASF licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Apache.Arrow.Ipc
{
    public class ArrowFileWriter: ArrowStreamWriter
    {
        private long _currentRecordBatchOffset = -1;

        private bool HasWrittenHeader { get; set; }

        private List<Block> RecordBatchBlocks { get; }

        public ArrowFileWriter(Stream stream, Schema schema)
            : this(stream, schema, leaveOpen: false)
        {
        }

        public ArrowFileWriter(Stream stream, Schema schema, bool leaveOpen)
            : this(stream, schema, leaveOpen, options: null)
        {
        }

        public ArrowFileWriter(Stream stream, Schema schema, bool leaveOpen, IpcOptions options)
            : base(stream, schema, leaveOpen, options)
        {
            if (!stream.CanWrite)
            {
                throw new ArgumentException("stream must be writable", nameof(stream));
            }

            // TODO: Remove seek requirement

            if (!stream.CanSeek)
            {
                throw new ArgumentException("stream must be seekable", nameof(stream));
            }

            HasWrittenHeader = false;

            RecordBatchBlocks = new List<Block>();
        }

        public override async Task WriteRecordBatchAsync(RecordBatch recordBatch, CancellationToken cancellationToken = default)
        {
            // TODO: Compare record batch schema

            if (!HasWrittenHeader)
            {
                await WriteHeaderAsync(cancellationToken).ConfigureAwait(false);
                HasWrittenHeader = true;
            }

            cancellationToken.ThrowIfCancellationRequested();

            await WriteRecordBatchInternalAsync(recordBatch, cancellationToken)
                .ConfigureAwait(false);
        }

        private protected override void StartingWritingRecordBatch()
        {
            _currentRecordBatchOffset = BaseStream.Position;
        }

        private protected override void FinishedWritingRecordBatch(long bodyLength, long metadataLength)
        {
            // Record batches only appear after a Schema is written, so the record batch offsets must
            // always be greater than 0.
            Debug.Assert(_currentRecordBatchOffset > 0, "_currentRecordBatchOffset must be positive.");

            int metadataLengthInt = checked((int)metadataLength);

            Debug.Assert(BitUtility.IsMultipleOf8(_currentRecordBatchOffset));
            Debug.Assert(BitUtility.IsMultipleOf8(metadataLengthInt));
            Debug.Assert(BitUtility.IsMultipleOf8(bodyLength));

            var block = new Block(
                offset: _currentRecordBatchOffset,
                length: bodyLength,
                metadataLength: metadataLengthInt);

            RecordBatchBlocks.Add(block);

            _currentRecordBatchOffset = -1;
        }

        private protected override async ValueTask WriteEndInternalAsync(CancellationToken cancellationToken)
        {
            await base.WriteEndInternalAsync(cancellationToken);

            await WriteFooterAsync(Schema, cancellationToken);
        }

        private async Task WriteHeaderAsync(CancellationToken cancellationToken)
        {
            // Write magic number and empty padding up to the 8-byte boundary

            await WriteMagicAsync(cancellationToken).ConfigureAwait(false);
            await WritePaddingAsync(CalculatePadding(ArrowFileConstants.Magic.Length))
                .ConfigureAwait(false);
        }

        private async Task WriteFooterAsync(Schema schema, CancellationToken cancellationToken)
        {
            Builder.Clear();

            var offset = BaseStream.Position;

            // Serialize the schema

            var schemaOffset = SerializeSchema(schema);

            // Serialize all record batches

            Flatbuf.Footer.StartRecordBatchesVector(Builder, RecordBatchBlocks.Count);

            foreach (var recordBatch in RecordBatchBlocks)
            {
                Flatbuf.Block.CreateBlock(
                    Builder, recordBatch.Offset, recordBatch.MetadataLength, recordBatch.BodyLength);
            }

            var recordBatchesVectorOffset = Builder.EndVector();

            // Serialize all dictionaries
            // NOTE: Currently unsupported.

            Flatbuf.Footer.StartDictionariesVector(Builder, 0);

            var dictionaryBatchesOffset = Builder.EndVector();

            // Serialize and write the footer flatbuffer

            var footerOffset = Flatbuf.Footer.CreateFooter(Builder, CurrentMetadataVersion,
                schemaOffset, dictionaryBatchesOffset, recordBatchesVectorOffset);

            Builder.Finish(footerOffset.Value);

            cancellationToken.ThrowIfCancellationRequested();

            await WriteFlatBufferAsync(cancellationToken).ConfigureAwait(false);

            // Write footer length

            cancellationToken.ThrowIfCancellationRequested();

            await Buffers.RentReturnAsync(4, async (buffer) =>
            {
                int footerLength;
                checked
                {
                    footerLength = (int)(BaseStream.Position - offset);
                }

                BinaryPrimitives.WriteInt32LittleEndian(buffer.Span, footerLength);

                await BaseStream.WriteAsync(buffer, cancellationToken).ConfigureAwait(false);
            }).ConfigureAwait(false);

            // Write magic

            await WriteMagicAsync(cancellationToken).ConfigureAwait(false);
        }

        private ValueTask WriteMagicAsync(CancellationToken cancellationToken)
        {
            return BaseStream.WriteAsync(ArrowFileConstants.Magic, cancellationToken);
        }
    }
}

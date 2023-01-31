// Licensed to the Apache Software Foundation (ASF) under one
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

//! Execution plan for reading Parquet files

use std::fs::File;
use std::rc::Rc;
use std::sync::{Arc, Mutex};
use std::thread;

use crate::error::{ExecutionError, Result};
use crate::execution::physical_plan::common;
use crate::execution::physical_plan::{BatchIterator, ExecutionPlan, Partition};
use arrow::datatypes::Schema;
use arrow::record_batch::{RecordBatch, RecordBatchReader};
use parquet::file::reader::SerializedFileReader;

use crossbeam::channel::{unbounded, Receiver, Sender};
use parquet::arrow::{ArrowReader, ParquetFileArrowReader};

/// Execution plan for scanning a Parquet file
pub struct ParquetExec {
    /// Path to directory containing partitioned Parquet files with the same schema
    filenames: Vec<String>,
    /// Schema after projection is applied
    schema: Arc<Schema>,
    /// Projection for which columns to load
    projection: Vec<usize>,
    /// Batch size
    batch_size: usize,
}

impl ParquetExec {
    /// Create a new Parquet reader execution plan
    pub fn try_new(
        path: &str,
        projection: Option<Vec<usize>>,
        batch_size: usize,
    ) -> Result<Self> {
        let mut filenames: Vec<String> = vec![];
        common::build_file_list(path, &mut filenames, ".parquet")?;
        if filenames.is_empty() {
            Err(ExecutionError::General("No files found".to_string()))
        } else {
            let file = File::open(&filenames[0])?;
            let file_reader = Rc::new(SerializedFileReader::new(file)?);
            let mut arrow_reader = ParquetFileArrowReader::new(file_reader);
            let schema = arrow_reader.get_schema()?;

            let projection = match projection {
                Some(p) => p,
                None => (0..schema.fields().len()).collect(),
            };

            let projected_schema = Schema::new(
                projection
                    .iter()
                    .map(|i| schema.field(*i).clone())
                    .collect(),
            );

            Ok(Self {
                filenames,
                schema: Arc::new(projected_schema),
                projection,
                batch_size,
            })
        }
    }
}

impl ExecutionPlan for ParquetExec {
    fn schema(&self) -> Arc<Schema> {
        self.schema.clone()
    }

    fn partitions(&self) -> Result<Vec<Arc<dyn Partition>>> {
        let partitions = self
            .filenames
            .iter()
            .map(|filename| {
                Arc::new(ParquetPartition::new(
                    &filename,
                    self.projection.clone(),
                    self.schema.clone(),
                    self.batch_size,
                )) as Arc<dyn Partition>
            })
            .collect();
        Ok(partitions)
    }
}

struct ParquetPartition {
    iterator: Arc<Mutex<dyn BatchIterator>>,
}

impl ParquetPartition {
    /// Create a new Parquet partition
    pub fn new(
        filename: &str,
        projection: Vec<usize>,
        schema: Arc<Schema>,
        batch_size: usize,
    ) -> Self {
        // because the parquet implementation is not thread-safe, it is necessary to execute
        // on a thread and communicate with channels
        let (request_tx, request_rx): (Sender<()>, Receiver<()>) = unbounded();
        let (response_tx, response_rx): (
            Sender<Result<Option<RecordBatch>>>,
            Receiver<Result<Option<RecordBatch>>>,
        ) = unbounded();

        let filename = filename.to_string();

        thread::spawn(move || {
            //TODO error handling, remove unwraps

            // open file
            let file = File::open(&filename).unwrap();
            match SerializedFileReader::new(file) {
                Ok(file_reader) => {
                    let file_reader = Rc::new(file_reader);

                    let mut arrow_reader = ParquetFileArrowReader::new(file_reader);

                    match arrow_reader
                        .get_record_reader_by_columns(projection, batch_size)
                    {
                        Ok(mut batch_reader) => {
                            while let Ok(_) = request_rx.recv() {
                                match batch_reader.next_batch() {
                                    Ok(Some(batch)) => {
                                        response_tx.send(Ok(Some(batch))).unwrap();
                                    }
                                    Ok(None) => {
                                        response_tx.send(Ok(None)).unwrap();
                                        break;
                                    }
                                    Err(e) => {
                                        response_tx
                                            .send(Err(ExecutionError::General(format!(
                                                "{:?}",
                                                e
                                            ))))
                                            .unwrap();
                                        break;
                                    }
                                }
                            }
                        }

                        Err(e) => {
                            response_tx
                                .send(Err(ExecutionError::General(format!("{:?}", e))))
                                .unwrap();
                        }
                    }
                }

                Err(e) => {
                    response_tx
                        .send(Err(ExecutionError::General(format!("{:?}", e))))
                        .unwrap();
                }
            }
        });

        let iterator = Arc::new(Mutex::new(ParquetIterator {
            schema,
            request_tx,
            response_rx,
        }));

        Self { iterator }
    }
}

impl Partition for ParquetPartition {
    fn execute(&self) -> Result<Arc<Mutex<dyn BatchIterator>>> {
        Ok(self.iterator.clone())
    }
}

struct ParquetIterator {
    schema: Arc<Schema>,
    request_tx: Sender<()>,
    response_rx: Receiver<Result<Option<RecordBatch>>>,
}

impl BatchIterator for ParquetIterator {
    fn schema(&self) -> Arc<Schema> {
        self.schema.clone()
    }

    fn next(&mut self) -> Result<Option<RecordBatch>> {
        match self.request_tx.send(()) {
            Ok(_) => match self.response_rx.recv() {
                Ok(batch) => batch,
                Err(e) => Err(ExecutionError::General(format!(
                    "Error receiving batch: {:?}",
                    e
                ))),
            },
            _ => Err(ExecutionError::General(
                "Error sending request for next batch".to_string(),
            )),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test() -> Result<()> {
        let testdata =
            env::var("PARQUET_TEST_DATA").expect("PARQUET_TEST_DATA not defined");
        let filename = format!("{}/alltypes_plain.parquet", testdata);
        let parquet_exec = ParquetExec::try_new(&filename, Some(vec![0, 1, 2]), 1024)?;
        let partitions = parquet_exec.partitions()?;
        assert_eq!(partitions.len(), 1);

        let results = partitions[0].execute()?;
        let mut results = results.lock().unwrap();
        let batch = results.next()?.unwrap();

        assert_eq!(8, batch.num_rows());
        assert_eq!(3, batch.num_columns());

        let field_names: Vec<&str> = batch
            .schema()
            .fields()
            .iter()
            .map(|f| f.name().as_str())
            .collect();
        assert_eq!(vec!["id", "bool_col", "tinyint_col"], field_names);

        let batch = results.next()?;
        assert!(batch.is_none());

        Ok(())
    }
}

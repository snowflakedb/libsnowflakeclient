This directory contains AVRO files corresponding to the parquet testing files at https://github.com/apache/parquet-testing/blob/master/data/

These files were created by using spark using the commands from https://gist.github.com/Igosuki/324b011f40185269d3fc552350d21744

Roughly:
```scala
import com.github.mrpowers.spark.daria.sql.DariaWriters
import org.apache.hadoop.fs.FileSystem
import org.apache.hadoop.fs.Path
import org.apache.hadoop.conf.Configuration 
import org.apache.commons.io.FilenameUtils

val fileGlobs = sc.getConf.get("spark.driver.globs")
val dest = sc.getConf.get("spark.driver.out")

val fs = FileSystem.get(new Configuration(true));
val status = fs.globStatus(new Path(fileGlobs))
for (fileStatus <- status) {
    val path = fileStatus.getPath().toString()
    try {
        val dfin = spark.read.format("parquet").load(path)
        val fileName = fileStatus.getPath().getName();
        val fileNameWithOutExt = FilenameUtils.removeExtension(fileName);
        val destination = s"${dest}/${fileNameWithOutExt}.avro"
        println(s"Converting $path to avro at $destination")
        DariaWriters.writeSingleFile(
            df = dfin,
            format = "avro",
            sc = spark.sparkContext,
            tmpFolder = s"/tmp/dw/${fileName}",
            filename = destination
        )
    } catch {
        case e: Throwable => println(s"failed to convert $path : ${e.getMessage}")
    }
}
```

Additional notes:

| File                      | Description                                                                                                                                                                                                                                                                                   |
|:------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| alltypes_nulls_plain.avro           | Contains a single row with null values for each scalar data type, i.e, `{"string_col":null,"int_col":null,"bool_col":null,"bigint_col":null,"float_col":null,"double_col":null,"bytes_col":null}`. Generated from https://gist.github.com/nenorbot/5a92e24f8f3615488f75e2a18a105c76 |
| nested_records.avro                 | Contains two rows of nested record types. Generated from https://github.com/sarutak/avro-data-generator/blob/master/src/bin/nested-records.rs                                                                                                                                       |
| simple_enum.avro                    | Contains four rows of enum types. Generated from https://github.com/sarutak/avro-data-generator/blob/master/src/bin/simple-enum.rs                                                                                                                                                  |
| simple_fixed                        | Contains two rows of fixed types. Generated from https://github.com/sarutak/avro-data-generator/blob/master/src/bin/simple-fixed.rs                                                                                                                                                 |
| zero_byte.avro                      | Contains two rows of zero bytes followed by a non-zero byte row. Generated from https://gist.github.com/jecsand838/e57647d0d12853f3cf07c350a6a40395                                                                                                                                 |
| duration_uuid.avro                  | Contains four rows of uuid and duration logic types. Generated from https://gist.github.com/jecsand838/cbdaaf581af78f357778bf87d2f3cf15                                                                                                                                             |
| fixed256_decimal.avro               | Generated from https://gist.github.com/jecsand838/3890349bdb33082a3e8fdcae3257eef7                                                                                                                                                                                                  |
| fixed_length_decimal_legacy_32.avro | Generated from https://gist.github.com/jecsand838/3890349bdb33082a3e8fdcae3257eef7                                                                                                                                                                                                  |
| int128_decimal.avro                 | Generated from https://gist.github.com/jecsand838/3890349bdb33082a3e8fdcae3257eef7                                                                                                                                                                                                  |
| int256_decimal.avro                 | Generated from https://gist.github.com/jecsand838/3890349bdb33082a3e8fdcae3257eef7                                                                                                                                                                                                  |
| timestamp_logical_types.avro        | Contains two rows with timestamp-* and local-timestamp-* logical types for millis/micros/nanos.                                                                                                                                                                                     |

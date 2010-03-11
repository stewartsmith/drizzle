#
# Tests tons of different CREATE TABLE
# variations and the replication stream
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

# Test the field generation

CREATE TABLE t1 (
  autoinc_int_field INT NOT NULL AUTO_INCREMENT
, null_int_field INT NULL
, not_null_bigint_field BIGINT NOT NULL
, null_bigint_field BIGINT NULL
, not_null_int_field INT NOT NULL
, null_varchar_field VARCHAR(100) NULL
, not_null_varchar_field VARCHAR(100) NOT NULL
, null_enum_field ENUM ('val1', 'val2') NULL
, not_null_enum_field ENUM ('val1', 'val2') NOT NULL
, null_date_field DATE NULL
, not_null_date_field DATE NOT NULL
, null_datetime_field DATETIME NULL
, not_null_datetime_field DATETIME NOT NULL
, null_blob_field BLOB NULL
, not_null_blob_field BLOB NOT NULL
, null_text_field TEXT NULL
, not_null_text_field TEXT NOT NULL
, null_timestamp_field TIMESTAMP NULL
, not_null_timestamp_field TIMESTAMP NOT NULL
, null_double_field DOUBLE NULL
, not_null_double_field DOUBLE NOT NULL
, null_decimal_field DECIMAL(10,2) NULL
, not_null_decimal_field DECIMAL(10,2) NOT NULL
, PRIMARY KEY (autoinc_int_field)
);

DROP TABLE t1;

# Test the index generation

CREATE TABLE t1 (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
, key1 VARCHAR(10) NOT NULL
, key2 DOUBLE NOT NULL
, key3 BLOB NOT NULL
, UNIQUE KEY (key1)
, KEY named_key (key2)
, KEY partial_key (key3(30))
);

DROP TABLE t1;

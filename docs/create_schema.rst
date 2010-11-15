CREATE SCHEMA
=============

CREATE SCHEMA [IF NOT EXISTS] schema_name
  [engine_options] ...

engine_options:
    engine_option [[,] engine_option] ...

engine_option:
  [DEFAULT] COLLATE = collation_name
  { engine_specific }

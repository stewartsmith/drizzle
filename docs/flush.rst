FLUSH
=====

FLUSH 
    flush_option [, flush_option] ...

flush_option:
    | TABLES table_name [, table_name] ...
    | TABLES WITH READ LOCK
    | LOGS
    | STATUS
    
To release a FLUSH TABLES WITH READ LOCK, you must issue an UNLOCK TABLES.

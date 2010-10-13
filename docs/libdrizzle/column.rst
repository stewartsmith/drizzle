
.. highlightlang:: c

Column Object
-------------

.. index:: object: drizzle_column_st

General Functions
^^^^^^^^^^^^^^^^^

These functions are used to get detailed column information. This
information is usually sent as the first part of a result set. There are
multiple ways for column information to be buffered depending on the
functions being used.


.. c:function:: drizzle_column_st *   drizzle_column_create (drizzle_result_st *result, drizzle_column_st *column)

.. c:function:: void  drizzle_column_free (drizzle_column_st *column)

.. c:function:: drizzle_result_st *   drizzle_column_drizzle_result (drizzle_column_st *column)

.. c:function:: const char *  drizzle_column_catalog (drizzle_column_st *column)

.. c:function:: const char *  drizzle_column_db (drizzle_column_st *column)

.. c:function:: const char *  drizzle_column_table (drizzle_column_st *column)

.. c:function:: const char *  drizzle_column_orig_table (drizzle_column_st *column)

.. c:function:: const char *  drizzle_column_name (drizzle_column_st *column)

.. c:function:: const char *  drizzle_column_orig_name (drizzle_column_st *column)

.. c:function:: drizzle_charset_t   drizzle_column_charset (drizzle_column_st *column)

.. c:function:: uint32_t  drizzle_column_size (drizzle_column_st *column)

.. c:function:: size_t  drizzle_column_max_size (drizzle_column_st *column)

.. c:function:: void  drizzle_column_set_max_size (drizzle_column_st *column, size_t size)

.. c:function:: drizzle_column_type_t   drizzle_column_type (drizzle_column_st *column)

.. c:function:: drizzle_column_type_drizzle_t   drizzle_column_type_drizzle (drizzle_column_st *column)

.. c:function:: drizzle_column_flags_t  drizzle_column_flags (drizzle_column_st *column)

.. c:function:: uint8_t   drizzle_column_decimals (drizzle_column_st *column)

.. c:function:: const uint8_t *   drizzle_column_default_value (drizzle_column_st *column, size_t *size)

Client Functions
^^^^^^^^^^^^^^^^

These functions are used to get detailed column information. This
information is usually sent as the first part of a result set. There are
both buffered and unbuffered functions provided.


.. c:function:: drizzle_return_t  drizzle_column_skip (drizzle_result_st *result)

.. c:function:: drizzle_column_st *   drizzle_column_read (drizzle_result_st *result, drizzle_column_st *column, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_return_t  drizzle_column_buffer (drizzle_result_st *result)

.. c:function:: drizzle_column_st *   drizzle_column_next (drizzle_result_st *result)

.. c:function:: drizzle_column_st *   drizzle_column_prev (drizzle_result_st *result)

.. c:function:: void  drizzle_column_seek (drizzle_result_st *result, uint16_t column)

.. c:function:: drizzle_column_st *   drizzle_column_index (drizzle_result_st *result, uint16_t column)

.. c:function:: uint16_t  drizzle_column_current (drizzle_result_st *result)

Server Functions
^^^^^^^^^^^^^^^^

These functions allow you to send column information over a connection.


.. c:function:: drizzle_return_t  drizzle_column_write (drizzle_result_st *result, drizzle_column_st *column)

.. c:function:: void  drizzle_column_set_catalog (drizzle_column_st *column, const char *catalog)

.. c:function:: void  drizzle_column_set_db (drizzle_column_st *column, const char *db)

.. c:function:: void  drizzle_column_set_table (drizzle_column_st *column, const char *table)

.. c:function:: void  drizzle_column_set_orig_table (drizzle_column_st *column, const char *orig_table)

.. c:function:: void  drizzle_column_set_name (drizzle_column_st *column, const char *name)

.. c:function:: void  drizzle_column_set_orig_name (drizzle_column_st *column, const char *orig_name)

.. c:function:: void  drizzle_column_set_charset (drizzle_column_st *column, drizzle_charset_t charset)

.. c:function:: void  drizzle_column_set_size (drizzle_column_st *column, uint32_t size)

.. c:function:: void  drizzle_column_set_type (drizzle_column_st *column, drizzle_column_type_t type)

.. c:function:: void  drizzle_column_set_flags (drizzle_column_st *column, drizzle_column_flags_t flags)

.. c:function:: void  drizzle_column_set_decimals (drizzle_column_st *column, uint8_t decimals)

.. c:function:: void  drizzle_column_set_default_value (drizzle_column_st *column, const uint8_t *default_value, size_t size)


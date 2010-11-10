
.. highlightlang:: c

Result Object
-------------

.. index:: object: drizzle_result_st

General Functions
^^^^^^^^^^^^^^^^^

These are core result functions used by both clients and servers.


.. c:function:: drizzle_result_st *   drizzle_result_create (drizzle_con_st *con, drizzle_result_st *result)

.. c:function:: drizzle_result_st *   drizzle_result_clone (drizzle_con_st *con, drizzle_result_st *result, drizzle_result_st *from)

.. c:function:: void  drizzle_result_free (drizzle_result_st *result)

.. c:function:: void  drizzle_result_free_all (drizzle_con_st *con)

.. c:function:: drizzle_con_st *  drizzle_result_drizzle_con (drizzle_result_st *result)

.. c:function:: bool  drizzle_result_eof (drizzle_result_st *result)

.. c:function:: const char *  drizzle_result_info (drizzle_result_st *result)

.. c:function:: const char *  drizzle_result_error (drizzle_result_st *result)

.. c:function:: uint16_t  drizzle_result_error_code (drizzle_result_st *result)

.. c:function:: const char *  drizzle_result_sqlstate (drizzle_result_st *result)

.. c:function:: uint16_t  drizzle_result_warning_count (drizzle_result_st *result)

.. c:function:: uint64_t  drizzle_result_insert_id (drizzle_result_st *result)

.. c:function:: uint64_t  drizzle_result_affected_rows (drizzle_result_st *result)

.. c:function:: uint16_t  drizzle_result_column_count (drizzle_result_st *result)

.. c:function:: uint64_t  drizzle_result_row_count (drizzle_result_st *result)

Client Functions
^^^^^^^^^^^^^^^^

These functions read or buffer the result for a client command.

.. c:function:: drizzle_result_st *   drizzle_result_read (drizzle_con_st *con, drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_return_t  drizzle_result_buffer (drizzle_result_st *result)

.. c:function:: size_t  drizzle_result_row_size (drizzle_result_st *result)

Server Functions
^^^^^^^^^^^^^^^^

These functions allow you to send result packets over a connection.


.. c:function:: drizzle_return_t  drizzle_result_write (drizzle_con_st *con, drizzle_result_st *result, bool flush)

.. c:function:: void  drizzle_result_set_row_size (drizzle_result_st *result, size_t size)

.. c:function:: void  drizzle_result_calc_row_size (drizzle_result_st *result, const

.. c:function:: drizzle_field_t *field, const size_t *size)

.. c:function:: void  drizzle_result_set_eof (drizzle_result_st *result, bool eof)

.. c:function:: void  drizzle_result_set_info (drizzle_result_st *result, const char *info)

.. c:function:: void  drizzle_result_set_error (drizzle_result_st *result, const char *error)

.. c:function:: void  drizzle_result_set_error_code (drizzle_result_st *result, uint16_t error_code)

.. c:function:: void  drizzle_result_set_sqlstate (drizzle_result_st *result, const char *sqlstate)

.. c:function:: void  drizzle_result_set_warning_count (drizzle_result_st *result, uint16_t warning_count)

.. c:function:: void  drizzle_result_set_insert_id (drizzle_result_st *result, uint64_t insert_id)

.. c:function:: void  drizzle_result_set_affected_rows (drizzle_result_st *result, uint64_t affected_rows)

.. c:function:: void  drizzle_result_set_column_count (drizzle_result_st *result, uint16_t column_count)

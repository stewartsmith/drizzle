
.. highlightlang:: c

Row Object
----------

.. index:: object: drizzle_row_st

Client Functions
^^^^^^^^^^^^^^^^

These functions allow you to access rows in a result set. If the result is
unbuffered, you can read and buffer rows one at a time. If the rows are
buffered in the result, the drizzle_row_next() and related functions can be
used.


.. c:function:: uint64_t  drizzle_row_read (drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_row_t   drizzle_row_buffer (drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: void  drizzle_row_free (drizzle_result_st *result, drizzle_row_t row)

.. c:function:: size_t *  drizzle_row_field_sizes (drizzle_result_st *result)

.. c:function:: drizzle_row_t   drizzle_row_next (drizzle_result_st *result)

.. c:function:: drizzle_row_t   drizzle_row_prev (drizzle_result_st *result)

.. c:function:: void  drizzle_row_seek (drizzle_result_st *result, uint64_t row)

.. c:function:: drizzle_row_t   drizzle_row_index (drizzle_result_st *result, uint64_t row)

.. c:function:: uint64_t  drizzle_row_current (drizzle_result_st *result)

Server Functions
^^^^^^^^^^^^^^^^

These functions allow you to send row information over a connection.

.. c:function:: drizzle_return_t  drizzle_row_write (drizzle_result_st *result)

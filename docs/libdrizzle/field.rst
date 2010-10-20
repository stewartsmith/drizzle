

.. highlightlang:: c

Field Object
------------

.. index:: object: drizzle_field_st

Client Functions
^^^^^^^^^^^^^^^^

These functions allow you to access fields in a result set if the result is
unbuffered. If the result is buffered, you can access the fields through the
row functions.


.. c:function:: drizzle_field_t   drizzle_field_read (drizzle_result_st *result, size_t *offset, size_t *size, size_t *total, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_field_t   drizzle_field_buffer (drizzle_result_st *result, size_t *total, drizzle_return_t *ret_ptr)

.. c:function:: void  drizzle_field_free (drizzle_field_t field)

Server Functions
^^^^^^^^^^^^^^^^

These functions allow you to send a field over a connection.


.. c:function:: drizzle_return_t  drizzle_field_write (drizzle_result_st *result, const drizzle_field_t field, size_t size, size_t total)


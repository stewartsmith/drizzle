
.. highlightlang:: c

Query Object
------------

.. index:: object: drizzle_query_st

These functions are used to issue queries on a connection. Single queries
are made using the drizzle_query function, or you can queue multiple queries
and run them concurrently using the other query functions.


.. c:function:: drizzle_result_st *   drizzle_query (drizzle_con_st *con, drizzle_result_st *result, const char *query, size_t size, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_query_str (drizzle_con_st *con, drizzle_result_st *result, const char *query, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_query_inc (drizzle_con_st *con, drizzle_result_st *result, const char *query, size_t size, size_t total, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_query_st *  drizzle_query_add (drizzle_st *drizzle, drizzle_query_st *query, drizzle_con_st *con, drizzle_result_st *result, const char *query_string, size_t size, drizzle_query_options_t options, void *context)

.. c:function:: drizzle_query_st *  drizzle_query_create (drizzle_st *drizzle, drizzle_query_st *query)

.. c:function:: void  drizzle_query_free (drizzle_query_st *query)

.. c:function:: void  drizzle_query_free_all (drizzle_st *drizzle)

.. c:function:: drizzle_con_st *  drizzle_query_con (drizzle_query_st *query)

.. c:function:: void  drizzle_query_set_con (drizzle_query_st *query, drizzle_con_st *con)

.. c:function:: drizzle_result_st *   drizzle_query_result (drizzle_query_st *query)

.. c:function:: void  drizzle_query_set_result (drizzle_query_st *query, drizzle_result_st *result)

.. c:function:: char *  drizzle_query_string (drizzle_query_st *query, size_t *size)

.. c:function:: void  drizzle_query_set_string (drizzle_query_st *query, const char *string, size_t size)

.. c:function:: drizzle_query_options_t   drizzle_query_options (drizzle_query_st *query)

.. c:function:: void  drizzle_query_set_options (drizzle_query_st *query, drizzle_query_options_t options)

.. c:function:: void  drizzle_query_add_options (drizzle_query_st *query, drizzle_query_options_t options)

.. c:function:: void  drizzle_query_remove_options (drizzle_query_st *query, drizzle_query_options_t options)

.. c:function:: void *  drizzle_query_context (drizzle_query_st *query)

.. c:function:: void  drizzle_query_set_context (drizzle_query_st *query, void *context)

.. c:function:: void  drizzle_query_set_context_free_fn (drizzle_query_st *query,

.. c:function:: drizzle_query_context_free_fn *function)

.. c:function:: drizzle_query_st *  drizzle_query_run (drizzle_st *drizzle, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_return_t  drizzle_query_run_all (drizzle_st *drizzle)

.. c:function:: size_t  drizzle_escape_string (char *to, const char *from, size_t from_size)

.. c:function:: size_t  drizzle_hex_string (char *to, const char *from, size_t from_size)

.. c:function:: void  drizzle_mysql_password_hash (char *to, const char *from, size_t from_size)

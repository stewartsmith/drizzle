.. highlightlang:: c

Library Object
--------------

.. index:: object: drizzle_st

This is the core library structure that other structures (such as
connections) are created from.

There is no locking within a single drizzle_st structure, so for threaded
applications you must either ensure isolation in the application or use
multiple drizzle_st structures (for example, one for each thread).

General Functions
^^^^^^^^^^^^^^^^^

.. c:function:: const char *  drizzle_version (void)
.. c:function:: const char *  drizzle_bugreport (void)
.. c:function:: const char *  drizzle_verbose_name (drizzle_verbose_t verbose)
.. c:function:: drizzle_st *  drizzle_create (drizzle_st *drizzle)
.. c:function:: drizzle_st *  drizzle_clone (drizzle_st *drizzle, const drizzle_st *from)
.. c:function:: void  drizzle_free (drizzle_st *drizzle)
.. c:function:: const char *  drizzle_error (const drizzle_st *drizzle)
.. c:function:: int   drizzle_errno (const drizzle_st *drizzle)

.. c:function:: uint16_t  drizzle_error_code (const drizzle_st *drizzle)

.. c:function:: const char *  drizzle_sqlstate (const drizzle_st *drizzle)

.. c:function:: drizzle_options_t   drizzle_options (const drizzle_st *drizzle)

.. c:function:: void  drizzle_set_options (drizzle_st *drizzle, drizzle_options_t options)

.. c:function:: void  drizzle_add_options (drizzle_st *drizzle, drizzle_options_t options)

.. c:function:: void  drizzle_remove_options (drizzle_st *drizzle, drizzle_options_t options)

.. c:function:: void *  drizzle_context (const drizzle_st *drizzle)

.. c:function:: void  drizzle_set_context (drizzle_st *drizzle, void *context)

.. c:function:: void  drizzle_set_context_free_fn (drizzle_st *drizzle,

.. c:function:: drizzle_context_free_fn *function)

.. c:function:: int   drizzle_timeout (const drizzle_st *drizzle)

.. c:function:: void  drizzle_set_timeout (drizzle_st *drizzle, int timeout)

.. c:function:: drizzle_verbose_t   drizzle_verbose (const drizzle_st *drizzle)

.. c:function:: void  drizzle_set_verbose (drizzle_st *drizzle, drizzle_verbose_t verbose)

.. c:function:: void  drizzle_set_log_fn (drizzle_st *drizzle, drizzle_log_fn *function, void *context)

.. c:function:: void  drizzle_set_event_watch_fn (drizzle_st *drizzle,

.. c:function:: drizzle_event_watch_fn *function, void *context)

.. c:function:: drizzle_con_st *  drizzle_con_create (drizzle_st *drizzle, drizzle_con_st *con)

.. c:function:: drizzle_con_st *  drizzle_con_clone (drizzle_st *drizzle, drizzle_con_st *con, const drizzle_con_st *from)

.. c:function:: void  drizzle_con_free (drizzle_con_st *con)

.. c:function:: void  drizzle_con_free_all (drizzle_st *drizzle)

.. c:function:: drizzle_return_t  drizzle_con_wait (drizzle_st *drizzle)

.. c:function:: drizzle_con_st *  drizzle_con_ready (drizzle_st *drizzle)

Functions for Client Only 
^^^^^^^^^^^^^^^^^^^^^^^^^

.. c:function:: drizzle_con_st *  drizzle_con_add_tcp (drizzle_st *drizzle, drizzle_con_st *con, const char *host, in_port_t port, const char *user, const char *password, const char *db, drizzle_con_options_t options)

.. c:function:: drizzle_con_st *  drizzle_con_add_uds (drizzle_st *drizzle, drizzle_con_st *con, const char *uds, const char *user, const char *password, const char *db, drizzle_con_options_t options)

Functions for Server Only
^^^^^^^^^^^^^^^^^^^^^^^^^

.. c:function:: drizzle_con_st *  drizzle_con_add_tcp_listen (drizzle_st *drizzle, drizzle_con_st *con, const char *host, in_port_t port, int backlog, drizzle_con_options_t options)

.. c:function:: drizzle_con_st *  drizzle_con_add_uds_listen (drizzle_st *drizzle, drizzle_con_st *con, const char *uds, int backlog, drizzle_con_options_t options)

.. c:function:: drizzle_con_st *  drizzle_con_ready_listen (drizzle_st *drizzle)

.. c:function:: drizzle_con_st *  drizzle_con_accept (drizzle_st *drizzle, drizzle_con_st *con, drizzle_return_t *ret_ptr)

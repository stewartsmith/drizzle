.. highlightlang:: c

Connection Object
-----------------

.. index:: object: drizzle_con_st

General Functions
^^^^^^^^^^^^^^^^^


.. c:function:: int   drizzle_con_fd (const drizzle_con_st *con)

.. c:function:: drizzle_return_t  drizzle_con_set_fd (drizzle_con_st *con, int fd)

.. c:function:: void  drizzle_con_close (drizzle_con_st *con)

.. c:function:: drizzle_return_t  drizzle_con_set_events (drizzle_con_st *con, short events)

.. c:function:: drizzle_return_t  drizzle_con_set_revents (drizzle_con_st *con, short revents)

.. c:function:: drizzle_st *  drizzle_con_drizzle (const drizzle_con_st *con)

.. c:function:: const char *  drizzle_con_error (const drizzle_con_st *con)

.. c:function:: int   drizzle_con_errno (const drizzle_con_st *con)

.. c:function:: uint16_t  drizzle_con_error_code (const drizzle_con_st *con)

.. c:function:: const char *  drizzle_con_sqlstate (const drizzle_con_st *con)

.. c:function:: drizzle_con_options_t   drizzle_con_options (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_options (drizzle_con_st *con, drizzle_con_options_t options)

.. c:function:: void  drizzle_con_add_options (drizzle_con_st *con, drizzle_con_options_t options)

.. c:function:: void  drizzle_con_remove_options (drizzle_con_st *con, drizzle_con_options_t options)

.. c:function:: const char *  drizzle_con_host (const drizzle_con_st *con)

.. c:function:: in_port_t   drizzle_con_port (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_tcp (drizzle_con_st *con, const char *host, in_port_t port)

.. c:function:: const char *  drizzle_con_uds (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_uds (drizzle_con_st *con, const char *uds)

.. c:function:: const char *  drizzle_con_user (const drizzle_con_st *con)

.. c:function:: const char *  drizzle_con_password (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_auth (drizzle_con_st *con, const char *user, const char *password)

.. c:function:: const char *  drizzle_con_db (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_db (drizzle_con_st *con, const char *db)

.. c:function:: void *  drizzle_con_context (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_context (drizzle_con_st *con, void *context)

.. c:function:: void  drizzle_con_set_context_free_fn (drizzle_con_st *con, drizzle_con_context_free_fn *function)

.. c:function:: uint8_t   drizzle_con_protocol_version (const drizzle_con_st *con)

.. c:function:: const char *  drizzle_con_server_version (const drizzle_con_st *con)

.. c:function:: uint32_t  drizzle_con_server_version_number (const drizzle_con_st *con)

.. c:function:: uint32_t  drizzle_con_thread_id (const drizzle_con_st *con)

.. c:function:: const uint8_t *   drizzle_con_scramble (const drizzle_con_st *con)

.. c:function:: drizzle_capabilities_t  drizzle_con_capabilities (const drizzle_con_st *con)

.. c:function:: drizzle_charset_t   drizzle_con_charset (const drizzle_con_st *con)

.. c:function:: drizzle_con_status_t  drizzle_con_status (const drizzle_con_st *con)

.. c:function:: uint32_t  drizzle_con_max_packet_size (const drizzle_con_st *con)

Functions for Clients
^^^^^^^^^^^^^^^^^^^^^

.. c:var DRIZZLE_SHUTDOWN_DEFAULT

.. c:function:: drizzle_return_t  drizzle_con_connect (drizzle_con_st *con)

.. c:function:: drizzle_result_st *   drizzle_con_quit (drizzle_con_st *con,

.. c:function:: drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_quit (drizzle_con_st *con, drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_con_select_db (drizzle_con_st *con,

.. c:function:: drizzle_result_st *result, const char *db, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_select_db (drizzle_con_st *con,

.. c:function:: drizzle_result_st *result, const char *db, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_con_shutdown (drizzle_con_st *con,

.. c:function:: drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_shutdown (drizzle_con_st *con,

.. c:function:: drizzle_result_st *result, uint32_t level, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_con_ping (drizzle_con_st *con,

.. c:function:: drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_ping (drizzle_con_st *con, drizzle_result_st *result, drizzle_return_t *ret_ptr)

.. c:function:: drizzle_result_st *   drizzle_con_command_write (drizzle_con_st *con, drizzle_result_st *result, drizzle_command_t command, const void *data, size_t size, size_t total, drizzle_return_t *ret_ptr)

Handshake Functions for Clients
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These functions are used to send and receive handshake packets for a client.
These are only used by low-level clients when the DRIZZLE_CON_RAW_PACKET
option is set, so most applications will never need to use these.


.. c:function:: drizzle_return_t  drizzle_handshake_server_read (drizzle_con_st *con)

Read handshake packet from the server in a client.

.. c:function:: drizzle_return_t  drizzle_handshake_client_write (drizzle_con_st *con)

Write client handshake packet to a server.

Functions for Servers
^^^^^^^^^^^^^^^^^^^^^

These functions extend the core connection functions with a set of functions for server application use. These functions allow you to set raw handshake information for use with the handshake write functions.


.. c:function:: drizzle_return_t  drizzle_con_listen (drizzle_con_st *con)

.. c:function:: int   drizzle_con_backlog (const drizzle_con_st *con)

.. c:function:: void  drizzle_con_set_backlog (drizzle_con_st *con, int backlog)

.. c:function:: void  drizzle_con_set_protocol_version (drizzle_con_st *con, uint8_t protocol_version)

.. c:function:: void  drizzle_con_set_server_version (drizzle_con_st *con, const char *server_version)

.. c:function:: void  drizzle_con_set_thread_id (drizzle_con_st *con, uint32_t thread_id)

.. c:function:: void  drizzle_con_set_scramble (drizzle_con_st *con, const uint8_t *scramble)

.. c:function:: void  drizzle_con_set_capabilities (drizzle_con_st *con, drizzle_capabilities_t capabilities)

.. c:function:: void  drizzle_con_set_charset (drizzle_con_st *con, drizzle_charset_t charset)

.. c:function:: void  drizzle_con_set_status (drizzle_con_st *con, drizzle_con_status_t status)

.. c:function:: void  drizzle_con_set_max_packet_size (drizzle_con_st *con, uint32_t max_packet_size)

.. c:function:: void  drizzle_con_copy_handshake (drizzle_con_st *con, drizzle_con_st *from)

.. c:function:: void *  drizzle_con_command_read (drizzle_con_st *con, drizzle_command_t *command, size_t *offset, size_t *size, size_t *total, drizzle_return_t *ret_ptr)

.. c:function:: void *  drizzle_con_command_buffer (drizzle_con_st *con, drizzle_command_t *command, size_t *total, drizzle_return_t *ret_ptr)

Handshake Functions for Server
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These functions are used to send and receive handshake packets in a server.


.. c:function:: drizzle_return_t  drizzle_handshake_server_write (drizzle_con_st *con)

Write server handshake packet to a client.


.. c:function:: drizzle_return_t  drizzle_handshake_client_read (drizzle_con_st *con)

Read handshake packet from the client in a server.


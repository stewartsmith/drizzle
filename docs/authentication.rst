AUTHENTICATION
==============

For Drizzle, authentication is handled by plugins.

Authentication is no longer based on the host/user and
schema/table/column model that was used in the MyISAM-based mysql.user
table.

The pluggable model capitalizes on existing systems such as PAM, LDAP
via PAM and HTTP authentication.


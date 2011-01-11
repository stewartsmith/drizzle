Storage Engines
===============

Drizzle's micro-kernel has been designed to allow for data to be stored and
provided for by different "engines". Engines differ in many ways, but the
three primary types are, temporary only, transactional, and non-transaction.

Drizzle runs by default with a transaction engine for both regular and
temporary tables. You can override this behavior by specifing a different
engine when creating a table.

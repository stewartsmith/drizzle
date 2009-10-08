# DROP FUNCTION IF EXISTS memc_servers_set;
# DROP FUNCTION IF EXISTS memc_set;
# DROP FUNCTION IF EXISTS memc_get;
# DROP FUNCTION IF EXISTS memc_delete;
# DROP FUNCTION IF EXISTS memc_append;
# DROP FUNCTION IF EXISTS memc_prepend;
# DROP FUNCTION IF EXISTS memc_behavior_set;
# DROP FUNCTION IF EXISTS memc_behavior_get;
# 
# CREATE FUNCTION memc_servers_set RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_set RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_get RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_delete RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_append RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_prepend RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_behavior_set RETURNS STRING SONAME "libmemcached_functions_mysql.so";
# CREATE FUNCTION memc_behavior_get RETURNS STRING SONAME "libmemcached_functions_mysql.so";

DROP TABLE IF EXISTS `t1`;
select memc_servers_set('127.0.0.1:11211');

CREATE TABLE `t1` (id integer NOT NULL auto_increment primary key, bcol text);
INSERT INTO `t1` (bcol) values ('This documentation is NOT distributed under a GPL license. Use of this documentation is subject to the following terms: You may create a printed copy of this documentation solely for your own personal use. Conversion to other formats is allowed as long as the actual content is not altered or edited in any way. You shall not publish or distribute this documentation in any form or on any media, except if you distribute the documentation in a manner similar to how MySQL disseminates it (that is, electronically for download on a Web site with the software) or on a CD-ROM or similar medium, provided however that the documentation is disseminated together with the software on the same medium. Any other use, such as any dissemination of printed copies or use of this documentation, in whole or in part, in another publication, requires the prior written consent from an authorized representative of MySQL AB. MySQL AB reserves any and all rights to this documentation not expressly granted above.');
select memc_set('mysql:doc1', bcol) from t1;
select memc_get('mysql:doc1');

select memc_delete('mysql:doc1');
select memc_get('mysql:doc1');

select memc_set('spot:test', ' Spot ');
select memc_get('spot:test');
select memc_prepend('spot:test', 'See');
select memc_get('spot:test');
select memc_append('spot:test', 'run.');
select memc_get('spot:test');
select memc_delete('spot:test');

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_NO_BLOCK');
select @behavior;
select memc_behavior_set('MEMCACHED_BEHAVIOR_NO_BLOCK', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_NO_BLOCK');
select memc_behavior_set('MEMCACHED_BEHAVIOR_NO_BLOCK', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_SUPPORT_CAS');
select @behavior;
select memc_behavior_set('MEMCACHED_BEHAVIOR_SUPPORT_CAS', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_SUPPORT_CAS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_SUPPORT_CAS', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_TCP_NODELAY');
select memc_behavior_set('MEMCACHED_BEHAVIOR_TCP_NODELAY', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_TCP_NODELAY');
select memc_behavior_set('MEMCACHED_BEHAVIOR_TCP_NODELAY', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_USER_DATA');
select memc_behavior_set('MEMCACHED_BEHAVIOR_USER_DATA', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_USER_DATA');
select memc_behavior_set('MEMCACHED_BEHAVIOR_USER_DATA', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_SORT_HOSTS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_SORT_HOSTS', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_SORT_HOSTS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_SORT_HOSTS', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_VERIFY_KEY');
select memc_behavior_set('MEMCACHED_BEHAVIOR_VERIFY_KEY', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_VERIFY_KEY');
select memc_behavior_set('MEMCACHED_BEHAVIOR_VERIFY_KEY', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_KETAMA');
select memc_behavior_set('MEMCACHED_BEHAVIOR_KETAMA', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_KETAMA');

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_CACHE_LOOKUPS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_CACHE_LOOKUPS', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_CACHE_LOOKUPS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_CACHE_LOOKUPS', @behavior);

set @behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS', '1');
select memc_behavior_get('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS');
select memc_behavior_set('MEMCACHED_BEHAVIOR_BUFFER_REQUESTS', @behavior);

set @preserve_behavior = memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');
select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_DEFAULT' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_MD5' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_CRC' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_FNV1_64' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_FNV1A_64' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_FNV1_32' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_FNV1A_32' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_JENKINS' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_HSIEH' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH','MEMCACHED_HASH_MURMUR' );
select memc_behavior_get('MEMCACHED_BEHAVIOR_HASH');

select memc_behavior_set('MEMCACHED_BEHAVIOR_HASH', @preserve_behavior);

set @preserve_dist= memc_behavior_get('MEMCACHED_BEHAVIOR_DISTRIBUTION');
select memc_behavior_set('MEMCACHED_BEHAVIOR_DISTRIBUTION', 'MEMCACHED_DISTRIBUTION_MODULA');
select memc_behavior_get('MEMCACHED_BEHAVIOR_DISTRIBUTION');

select memc_behavior_set('MEMCACHED_BEHAVIOR_DISTRIBUTION', 'MEMCACHED_DISTRIBUTION_CONSISTENT');
select memc_behavior_get('MEMCACHED_BEHAVIOR_DISTRIBUTION');

select memc_behavior_set('MEMCACHED_BEHAVIOR_DISTRIBUTION', 'MEMCACHED_DISTRIBUTION_CONSISTENT_KETAMA');
select memc_behavior_get('MEMCACHED_BEHAVIOR_DISTRIBUTION');
select memc_behavior_set('MEMCACHED_BEHAVIOR_DISTRIBUTION', @preserve_dist);

set @size = memc_behavior_get('MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE');
select @size;
select memc_behavior_set('MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE', 60000);
select memc_behavior_get('MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE');
select memc_behavior_set('MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE', @size);

set @size = memc_behavior_get('MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE');
select @size;
select memc_behavior_set('MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE', 120000);
select memc_behavior_get('MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE');
select memc_behavior_set('MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE', @size);

set @size = memc_behavior_get('MEMCACHED_BEHAVIOR_POLL_TIMEOUT');
select @size;
select memc_behavior_set('MEMCACHED_BEHAVIOR_POLL_TIMEOUT', 100);
select memc_behavior_get('MEMCACHED_BEHAVIOR_POLL_TIMEOUT');
select memc_behavior_set('MEMCACHED_BEHAVIOR_POLL_TIMEOUT', @size);


set @size = memc_behavior_get('MEMCACHED_BEHAVIOR_RETRY_TIMEOUT');
select @size;
select memc_behavior_set('MEMCACHED_BEHAVIOR_RETRY_TIMEOUT', 5);
select memc_behavior_get('MEMCACHED_BEHAVIOR_RETRY_TIMEOUT');
select memc_behavior_set('MEMCACHED_BEHAVIOR_RETRY_TIMEOUT', @size);

set @size = memc_behavior_get('MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK');
select @size;
select memc_behavior_set('MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK', 5);
select memc_behavior_get('MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK');
select memc_behavior_set('MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK', @size);

set @size = memc_behavior_get('MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK');
select @size;
select memc_behavior_set('MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK', 5);
select memc_behavior_get('MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK');
select memc_behavior_set('MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK', @size);

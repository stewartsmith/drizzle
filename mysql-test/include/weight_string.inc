select @@collation_connection;
select hex(weight_string('a'));
select hex(weight_string('A'));
select hex(weight_string('abc'));
select hex(weight_string('abc' as char(2)));
select hex(weight_string('abc' as char(3)));
select hex(weight_string('abc' as char(5)));

select @@collation_connection;
select collation(cast(_latin1 0xDF as char));
select hex(weight_string('s'));
select hex(weight_string(cast(_latin1 0xDF as char)));
select hex(weight_string(cast(_latin1 0xDF as char) as char(1)));
select hex(weight_string('c'));
select hex(weight_string('h'));
select hex(weight_string('ch'));
select hex(weight_string('i'));
# contraction 'ch' plus expansion 'sharp s'
select hex(weight_string(cast(_latin1 0x6368DF as char)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(1)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(2)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(3)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(4)));
# expansion 'shart s' plus contraction 'ch'
select hex(weight_string(cast(_latin1 0xDF6368 as char)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(1)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(2)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(3)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(4)));

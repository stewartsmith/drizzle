# Always creates valid CREATE TABLE statements. Quite complex and varied
# ones too.
#
# We then used gensql.pl in randgen to produce the test file.
#
# We're using this to test that the table proto doesn't change across
# releases (at least without intent).

query:
	create; show_proto; drop;

show_proto:
	SELECT SHOW_TABLE_MESSAGE('test', { "'$tablename'" } );

create:
	{ %fieldnames=(); $tablename=$prng->string(64); ''} CREATE TABLE IF NOT EXISTS { '`'.$tablename.'`' } create_fields table_comment;

table_comment:
	| | | | | | | | COMMENT=_english | COMMENT=_string(2048);

field_comment:
	| | | | | || | | | | | | COMMENT _english | COMMENT _string(1024);

create_fields:
	(field) |
	(field, field) |
	(field, field, field) |
	(field, field, field, field) |
	(field, field, field, field, field) |
	(field, field, field, field, field, field) |
	(field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field, field, field, field, field, field) |
	(field, field, field, field, field, field, field, field, field, field, field, field, field, field, field);

field:
	field_name field_type field_comment;

field_name:
	{ do { $f= '`'.$prng->string(10).'`'; $fieldnames{$f}++;} while($fieldnames{$f}>1); $f } ;

field_type:
	INT nullable unique|
	BIGINT nullable unique|
	DOUBLE nullable unique|
	DATETIME nullable unique|
	TIMESTAMP nullable unique|
	TEXT nullable |
	BLOB nullable |
	CHAR   ( { $collength= $prng->int(1,16383) }) nullable |
	VARCHAR( { $collength= $prng->int(1,16383) }) nullable |
	DECIMAL( { $m= $prng->int(1,65) } , { use List::Util qw(min); $prng->int(1,min($m,30)) } ) nullable
	;

nullable:
	| NOT NULL;

unique:
	| UNIQUE;

drop:
	DROP TABLE IF EXISTS { '`'.$tablename.'`' };


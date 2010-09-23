# Generate enough queries to exceed maximum Transaction message size

--disable_query_log

let $1=9300;
let $2=0;

while ($1)
{
  inc $2;
  eval INSERT INTO t (a, b) VALUES ($2, 'aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhhhhhhhhiiiiiiiiiijjjjjjjjjj');
  dec $1;
}

--enable_query_log

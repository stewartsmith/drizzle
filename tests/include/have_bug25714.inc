#
# Check if the variable DRIZZLE_BUG25714 is set
#
--require r/have_bug25714.require
disable_query_log;
eval select LENGTH("$DRIZZLE_BUG25714") > 0 as "have_bug25714_exe";
enable_query_log;

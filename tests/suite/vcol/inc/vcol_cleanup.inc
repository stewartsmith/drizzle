################################################################################
# inc/vcol_cleanup.inc                                                         #
#                                                                              #
# Purpose:                                                                     #
#   Removal of the objects created by the t/<test_name>.test                   #
#   scripts.                                                                   #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-08-31                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3;
set sql_warnings = 0;
--enable_warnings

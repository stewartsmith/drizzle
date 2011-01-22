# Copyright (C) 2010 Brian Aker
#
# This file is free software
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

AC_DEFUN([DRIZZLE_STACK_DIRECTION],[
  AC_CACHE_CHECK(stack direction for C alloca, ac_cv_c_stack_direction,
    [AC_RUN_IFELSE(
       [AC_LANG_PROGRAM(
          [[
	    #include <stdlib.h>

 /* Prevent compiler optimization by HP's compiler, see bug#42213 */
 #if defined(__HP_cc) || defined (__HP_aCC) || defined (__hpux)
 #pragma noinline
 #endif

 int find_stack_direction ()
 {
   static char *addr = 0;
   auto char dummy;
   if (addr == 0)
     {
       addr = &dummy;
       return find_stack_direction ();
     }
   else
     return (&dummy > addr) ? 1 : -1;
 }
	  ]],
	  [[
   exit (find_stack_direction() < 0);
	  ]]
       )],
       [ac_cv_c_stack_direction=1],
       [ac_cv_c_stack_direction=-1],
       [ac_cv_c_stack_direction=]
    )]
  )
 AC_DEFINE_UNQUOTED(STACK_DIRECTION, $ac_cv_c_stack_direction)
])

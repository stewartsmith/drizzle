AC_DEFUN([DRIZZLE_STACK_DIRECTION],
 [AC_CACHE_CHECK(stack direction for C alloca, ac_cv_c_stack_direction,
 [AC_TRY_RUN([#include <stdlib.h>
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
 int main ()
 {
   exit (find_stack_direction() < 0);
 }], ac_cv_c_stack_direction=1, ac_cv_c_stack_direction=-1,
   ac_cv_c_stack_direction=)])
 AC_DEFINE_UNQUOTED(STACK_DIRECTION, $ac_cv_c_stack_direction)
])dnl

/******************************************************
This file contains functions that need to be added to
MySQL code but have not been added yet.

Whenever you add a function here submit a MySQL bug
report (feature request) with the implementation. Then
write the bug number in the comment before the
function in this file.

When MySQL commits the function it can be deleted from
here. In a perfect world this file exists but is empty.

(c) 2007 Innobase Oy

Created November 07, 2007 Vasil Dimov
*******************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/***********************************************************************
Retrieve Session::thread_id
http://bugs.mysql.com/30930 */

#ifdef BUILD_DRIZZLE
unsigned long
ib_session_get_thread_id(
/*=================*/
				/* out: Session::thread_id */
	const void*	session);	/* in: Session */
#else
unsigned long
ib_thd_get_thread_id(
/*=================*/
				/* out: THD::thread_id */
	const void*	thd);	/* in: THD */
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-05-20
 *
 * CORE SYSTEM:
 * The definitions are used by all code, and is NOT required
 * by any header file!
 *
 */

#pragma once
#ifndef __CSGLOBAL_H__
#define __CSGLOBAL_H__

#include "CSDefs.h"
#include "CSObject.h"
#include "CSThread.h"


/* This is the call context: */
#define CS_CONTEXT			__FUNC__, __FILE__, __LINE__

#ifdef DEBUG
int cs_assert(const char *func, const char *file, int line, const char *message);
int cs_hope(const char *func, const char *file, int line, const char *message);

#define ASSERT(x)			((x) ? 1 : cs_assert(CS_CONTEXT, #x))
#define HOPE(x)				((x) ? 1 : cs_hope(CS_CONTEXT, #x))
#else
#define ASSERT(x)
#define ASSERT(x)
#endif

#ifdef DEBUG
#define retain()			retain(__FUNC__, __FILE__, __LINE__)
#define release()			release(__FUNC__, __FILE__, __LINE__)
#endif

#define new_(v, t)			do { v = new t; if (!v) CSException::throwOSError(CS_CONTEXT, ENOMEM); } while (0)

/*
 * -----------------------------------------------------------------------
 * Call stack
 */

/*
 * This macro must be placed at the start of every function.
 * It records the current context so that we can
 * dump a type of stack trace later if necessary.
 *
 * It also sets up the current thread pointer 'self'.
 */
#ifdef DEBUG
#define STACK_CHECK    CSReleasePtr self_reltop = self->relTop
#else
#define STACK_CHECK   
#endif

#define inner_()			int			cs_frame = self->callTop++; \
							STACK_CHECK ; \
							do { \
								if (cs_frame< CS_CALL_STACK_SIZE) { \
									self->callStack[cs_frame].cs_func = __FUNC__; \
									self->callStack[cs_frame].cs_file = __FILE__; \
									self->callStack[cs_frame].cs_line = __LINE__; \
								} \
							} while (0)

#define outer_()			self->callTop = cs_frame; \
							ASSERT(self->relTop == self_reltop); 

#define enter_()			CSThread	*self = CSThread::getSelf(); \
							inner_()

/*
 * On exit to a function, either exit_() or
 * return_() must be called.
 */
#define exit_()				do { \
								outer_(); \
								return; \
							} while (0)
					

#define return_(x)			do { \
								outer_(); \
								return(x); \
							} while (0)


/*
 * -----------------------------------------------------------------------
 * Throwing and catching (the jump stack)
 */

int prof_setjmp(void);

#define TX_CHK_JMP()		if ((self)->jumpDepth < 0 || (self)->jumpDepth >= CS_JUMP_STACK_SIZE) CSException::throwCoreError(__FUNC__, __FILE__, __LINE__, CS_ERR_JUMP_OVERFLOW)
#ifdef PROFILE
#define profile_setjmp		prof_setjmp()
#else
#define profile_setjmp			
#endif

#define throw_()			(self)->throwException()
#define try_(n)				int throw_##n; throw_##n = 0; TX_CHK_JMP(); \
							(self)->jumpEnv[(self)->jumpDepth].jb_res_top = (self)->relTop; \
							(self)->jumpEnv[(self)->jumpDepth].jb_call_top = (self)->callTop; \
							(self)->jumpDepth++; profile_setjmp; \
							if (setjmp((self)->jumpEnv[(self)->jumpDepth-1].jb_buffer)) goto catch_##n;
#define catch_(n)			(self)->jumpDepth--; goto cont_##n; catch_##n: (self)->jumpDepth--; self->caught();
#define cont_(n)			if (throw_##n) throw_(); cont_##n:
#define finally_(n)			(self)->jumpDepth--; goto final_##n; catch_##n: throw_##n = 1; (self)->jumpDepth--; self->caught(); final_##n: {
#define finally_end_block(n) } if (throw_##n) throw_();
#define finally_end_block_no_throw(n) }

/*
 * -----------------------------------------------------------------------
 * The release stack
 */

#define push_(r)			do { \
								if ((self)->relTop >= (self)->relStack + CS_RELEASE_STACK_SIZE) {\
									CSException::throwCoreError(CS_CONTEXT, CS_ERR_RELEASE_OVERFLOW); \
								} \
								(self)->relTop->r_type = CS_RELEASE_OBJECT; \
								(self)->relTop->x.r_object = (r); \
								(self)->relTop++; \
							} while (0)

#define push_ptr_(r)			do { \
								if ((self)->relTop >= (self)->relStack + CS_RELEASE_STACK_SIZE) {\
									CSException::throwCoreError(CS_CONTEXT, CS_ERR_RELEASE_OVERFLOW); \
								} \
								(self)->relTop->r_type = CS_RELEASE_MEM; \
								(self)->relTop->x.r_mem = (r); \
								(self)->relTop++; \
							} while (0)

#define push_ref_(r)			do { \
								if ((self)->relTop >= (self)->relStack + CS_RELEASE_STACK_SIZE) {\
									CSException::throwCoreError(CS_CONTEXT, CS_ERR_RELEASE_OVERFLOW); \
								} \
								(self)->relTop->r_type = CS_RELEASE_OBJECT_PTR; \
								(self)->relTop->x.r_objectPtr = (CSObject **)&(r); \
								(self)->relTop++; \
							} while (0)

#define pop_(r)				do { \
								ASSERT((self)->relTop > (self)->relStack); \
								if (((self)->relTop - 1)->r_type == CS_RELEASE_OBJECT) {\
									ASSERT(((self)->relTop - 1)->x.r_object == ((CSObject *) r)); \
								} else if (((self)->relTop - 1)->r_type == CS_RELEASE_MUTEX) {\
									ASSERT(((self)->relTop - 1)->x.r_mutex == ((CSMutex *) r)); \
								} else if (((self)->relTop - 1)->r_type == CS_RELEASE_POOLED) {\
									ASSERT(((self)->relTop - 1)->x.r_pooled == ((CSPooled *) r)); \
								} else if (((self)->relTop - 1)->r_type == CS_RELEASE_MEM) {\
									ASSERT(((self)->relTop - 1)->x.r_mem == ((void *) r)); \
								}  else if (((self)->relTop - 1)->r_type == CS_RELEASE_OBJECT_PTR) {\
									ASSERT(((self)->relTop - 1)->x.r_objectPtr == ((CSObject **) &(r))); \
								}  else {\
									ASSERT(false); \
								} \
								(self)->relTop--; \
							} while (0)

#define retain_(r)			do { \
								(r)->retain(); \
								push_(r); \
							} while (0)

#define release_(r)			do {  \
								ASSERT((self)->relTop > (self)->relStack); \
								if (((self)->relTop - 1)->r_type == CS_RELEASE_OBJECT) {\
									register CSObject *rp; \
									rp = ((self)->relTop - 1)->x.r_object; \
									ASSERT(rp == (CSObject *)(r)); \
									(self)->relTop--; \
									rp->release(); \
								} else if (((self)->relTop - 1)->r_type == CS_RELEASE_MEM) {\
									register void *mem; \
									mem = ((self)->relTop - 1)->x.r_mem; \
									ASSERT(mem == (void *)(r)); \
									(self)->relTop--; \
									cs_free(mem); \
								}  else if (((self)->relTop - 1)->r_type == CS_RELEASE_OBJECT_PTR) {\
									register CSObject **rp; \
									rp = ((self)->relTop - 1)->x.r_objectPtr; \
									ASSERT(rp == (CSObject **)&(r)); \
									(self)->relTop--; \
									if(*rp) (*rp)->release(); \
								} else {\
									ASSERT(false); \
								} \
							} while (0)

#define lock_(r)			do { \
								if ((self)->relTop >= (self)->relStack + CS_RELEASE_STACK_SIZE) {\
									CSException::throwCoreError(CS_CONTEXT, CS_ERR_RELEASE_OVERFLOW); \
								} \
								(r)->lock(); \
								(self)->relTop->r_type = CS_RELEASE_MUTEX; \
								(self)->relTop->x.r_mutex = (r); \
								(self)->relTop++; \
							} while (0)

#define locked_(r)			do { \
								if ((self)->relTop >= (self)->relStack + CS_RELEASE_STACK_SIZE) \
									CSException::throwCoreError(CS_CONTEXT, CS_ERR_RELEASE_OVERFLOW); \
								(self)->relTop->r_type = CS_RELEASE_MUTEX; \
								(self)->relTop->x.r_mutex = (r); \
								(self)->relTop++; \
							} while (0)

#define unlock_(r)			do {  \
								register CSMutex *rp; \
								ASSERT((self)->relTop > (self)->relStack); \
								ASSERT(((self)->relTop - 1)->r_type == CS_RELEASE_MUTEX); \
								rp = ((self)->relTop - 1)->x.r_mutex; \
								ASSERT(rp == (r)); \
								(self)->relTop--; \
								rp->unlock(); \
							} while (0)

#define frompool_(r)		do { \
								if ((self)->relTop >= (self)->relStack + CS_RELEASE_STACK_SIZE) {\
									CSException::throwCoreError(CS_CONTEXT, CS_ERR_RELEASE_OVERFLOW); \
								} \
								(self)->relTop->r_type = CS_RELEASE_POOLED; \
								(self)->relTop->x.r_pooled = (r); \
								(self)->relTop++; \
							} while (0)

#define backtopool_(r)		do {  \
								register CSPooled *rp; \
								ASSERT((self)->relTop > (self)->relStack); \
								ASSERT(((self)->relTop - 1)->r_type == CS_RELEASE_POOLED); \
								rp = ((self)->relTop - 1)->x.r_pooled; \
								ASSERT(rp == (r)); \
								(self)->relTop--; \
								rp->returnToPool(); \
							} while (0)

#endif

/******************************************************************************/
/*                                                                            */
/*                                    PORT.H                                  */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* This module contains macro definitions and types that are likely to        */
/* change between computers.                                                  */
/*                                                                            */
/******************************************************************************/

#ifndef DONE_PORT       /* Only do this if not previously done.                   */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <time.h>

#ifdef THINK_C
#	define UBYTE unsigned char      /* Unsigned byte                       */
#	define UWORD unsigned short     /* Unsigned word (2 bytes)             */
#	define ULONG unsigned long      /* Unsigned word (4 bytes)             */
#	define UCARD unsigned			/* Which ever is fastest, uns. or int  */
#	define BOOL  unsigned char      /* Boolean                             */
#	define FOPEN_BINARY_READ  "rb"  /* Mode string for binary reading.     */
#	define FOPEN_BINARY_WRITE "wb"  /* Mode string for binary writing.     */
#	define FOPEN_TEXT_APPEND  "a"   /* Mode string for text appending.     */
#	define REAL double              /* Used for floating point stuff.      */
#	define fast_copy(s,d,len) memcpy((d),(s),(ULONG) (len))
#	define rindex strrchr
#	define index strchr
#	define unlink remove
#else	/* put your stuff here */
#	define UBYTE unsigned char      /* Unsigned byte                       */
#	define UWORD unsigned short     /* Unsigned word (2 bytes)             */
#	define ULONG unsigned long      /* Unsigned word (4 bytes)             */
#	define UCARD unsigned			/* Which ever is fastest, uns. or int  */
#	define BOOL  unsigned char      /* Boolean                             */
#	define REAL double              /* Used for floating point stuff.      */
#	ifndef TRUE
#		define TRUE (1)
#		define FALSE (0)
#	endif
#endif

#ifdef unix
#	define FOPEN_BINARY_READ  "r"  /* Mode string for binary reading.     */
#	define FOPEN_BINARY_WRITE "w"  /* Mode string for binary writing.     */
#	define FOPEN_TEXT_APPEND  "a"  /* Mode string for text appending.     */
#	ifdef _SYSTYPE_SYSV
#		define fast_copy(s,d,len) memcpy((d),(s),(ULONG) (len))
#		define rindex strrchr
#		define index strchr
#		define unlink remove
#	else
#		define fast_copy(s,d,len) bcopy((s),(d),(ULONG) (len))	/* bsd */
#	endif	/* _SYSTYPE_SYSV */
#endif	/* unix */

#ifdef _FASTCOPY_
#	undef fast_copy
#endif

#define DONE_PORT                   /* Don't do all this again.            */
#define MALLOC_FAIL NULL            /* Failure status from malloc()        */
#define LOCAL static                /* For non-exported routines.          */
#define EXPORT                      /* Signals exported function.          */
#define then                        /* Useful for aligning ifs.            */

#endif

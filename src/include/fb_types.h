/*
 *	PROGRAM:		Firebird RDBMS definitions
 *	MODULE:			fb_types.h
 *	DESCRIPTION:	Firebird's platform independent data types header
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.ibphoenix.com/about/firebird/idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Mike Nordell and Mark O'Donohue
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2001
 *       Mike Nordel <tamlin@algonet.se>
 *       Mark O'Donohue <mark.odonohue@ludwig.edu.au>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "OS/2" port
 *
 */

#ifndef INCLUDE_FB_TYPES_H
#define INCLUDE_FB_TYPES_H

#include <cstddef>
#include <limits.h>

#if SIZEOF_LONG == 8
	/* EKU: Firebird requires (S)LONG to be 32 bit */
	typedef int SLONG;
	typedef unsigned int ULONG;
	inline constexpr SLONG SLONG_MIN = INT_MIN;
	inline constexpr SLONG SLONG_MAX = INT_MAX;
#elif SIZEOF_LONG == 4
	typedef long SLONG;
	typedef unsigned long ULONG;
	inline constexpr SLONG SLONG_MIN = LONG_MIN;
	inline constexpr SLONG SLONG_MAX = LONG_MAX;
#else
#error compile_time_failure: SIZEOF_LONG not specified
#endif

/* Basic data types */

typedef char SCHAR;

typedef unsigned char UCHAR;
typedef short SSHORT;
typedef unsigned short USHORT;

#ifdef WIN_NT
typedef __int64 SINT64;
typedef unsigned __int64 FB_UINT64;
#else
typedef long long int SINT64;
typedef unsigned long long int FB_UINT64;
#endif

/* Substitution of API data types */

typedef SCHAR ISC_SCHAR;
typedef UCHAR ISC_UCHAR;
typedef SSHORT ISC_SHORT;
typedef USHORT ISC_USHORT;
typedef SLONG ISC_LONG;
typedef ULONG ISC_ULONG;
typedef SINT64 ISC_INT64;
typedef FB_UINT64 ISC_UINT64;

#include "firebird/impl/types_pub.h"

typedef ISC_QUAD SQUAD;

inline constexpr SQUAD NULL_BLOB = { 0, 0 };

inline bool operator==(const SQUAD& s1, const SQUAD& s2) noexcept
{
	return s1.gds_quad_high == s2.gds_quad_high &&
		   s2.gds_quad_low == s1.gds_quad_low;
}

inline bool operator!=(const SQUAD& s1, const SQUAD& s2) noexcept
{
	return !(s1 == s2);
}

inline bool operator>(const SQUAD& s1, const SQUAD& s2) noexcept
{
	return (s1.gds_quad_high > s2.gds_quad_high) ||
		(s1.gds_quad_high == s2.gds_quad_high &&
		 s1.gds_quad_low > s2.gds_quad_low);
}


/*
 * TMN: some misc data types from all over the place
 */
struct vary
{
	USHORT vary_length;
	char   vary_string[1]; /* CVC: The original declaration used UCHAR. */
};

struct lstring
{
	ULONG	lstr_length;
	ULONG	lstr_allocated;
	UCHAR*	lstr_address;
};

typedef unsigned char BOOLEAN;
typedef char TEXT;				/* To be expunged over time */
typedef unsigned char BYTE;		/* Unsigned byte - common */
typedef intptr_t IPTR;
typedef uintptr_t U_IPTR;

typedef void (*FPTR_VOID) ();
typedef void (*FPTR_VOID_PTR) (void*);
typedef int (*FPTR_INT) ();
typedef int (*FPTR_INT_VOID_PTR) (void*);
typedef void (*FPTR_PRINT_CALLBACK) (void*, SSHORT, const char*);
/* Used for isc_version */
typedef void (*FPTR_VERSION_CALLBACK)(void*, const char*);
/* Used for isc_que_events and internal functions */
typedef void (*FPTR_EVENT_CALLBACK)(void*, USHORT, const UCHAR*);

/* The type of JRD's ERR_post, DSQL's ERRD_post & post_error,
 * REMOTE's move_error & GPRE's post_error.
 */
namespace Firebird {
	namespace Arg {
		class StatusVector;
	}
}
typedef void (*ErrorFunction) (const Firebird::Arg::StatusVector& v);
// kept for backward compatibility with old private API (CVT_move())
typedef void (*FPTR_ERROR) (ISC_STATUS, ...);

typedef ULONG RCRD_OFFSET;
typedef ULONG RCRD_LENGTH;
typedef USHORT FLD_LENGTH;
/* CVC: internal usage. I suspect the only reason to return int is that
vmslock.cpp:LOCK_convert() calls VMS' sys$enq that may require this signature,
but our code never uses the return value. */
typedef int (*lock_ast_t)(void*);

// Number of elements in an array
template <typename T, std::size_t N>
constexpr FB_SIZE_T FB_NELEM(const T (&)[N]) noexcept
{
	return static_cast<FB_SIZE_T>(N);
}

// Intl types
typedef SSHORT CHARSET_ID;
typedef SSHORT COLLATE_ID;
typedef USHORT TTYPE_ID;

// Stream type, had to move it from dsql/Nodes.h due to circular dependencies.
typedef ULONG StreamType;

// Alignment rule
template <typename T>
constexpr T FB_ALIGN(T n, uintptr_t b)
{
	return (T) ((((uintptr_t) n) + b - 1) & ~(b - 1));
}

// Various object IDs (longer-than-32-bit)

typedef FB_UINT64 AttNumber;
typedef FB_UINT64 TraNumber;
typedef FB_UINT64 StmtNumber;
typedef FB_UINT64 CommitNumber;
typedef ULONG SnapshotHandle;
typedef SINT64 SavNumber;

#endif /* INCLUDE_FB_TYPES_H */

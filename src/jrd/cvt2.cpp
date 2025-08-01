/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		cvt2.cpp
 *	DESCRIPTION:	Data mover and converter and comparator, etc.
 *			Routines used ONLY within engine.
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2001.6.18 Claudio Valderrama: Implement comparison on blobs and blobs against
 * other datatypes by request from Ann Harrison.
 */

#include "firebird.h"
#include <string.h>

#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "iberror.h"
#include "../jrd/intl.h"
#include "../common/TimeZoneUtil.h"
#include "../common/gdsassert.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/cvt2_proto.h"
#include "../common/cvt.h"
#include "../jrd/err_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/intl_classes.h"
#include "../jrd/Collation.h"
#include "../yvalve/gds_proto.h"
// CVC: I needed them here.
#include "../jrd/jrd.h"
#include "../jrd/blb_proto.h"
#include "../jrd/tra.h"
#include "../jrd/req.h"
#include "../jrd/constants.h"
#include "../common/utils_proto.h"
#include "../common/classes/Aligner.h"
#include "../common/classes/VaryStr.h"

using namespace Jrd;
using namespace Firebird;

/* The original order of dsc_type values corresponded to the priority
   of conversion (that is, always convert the lesser to the greater
   type.)  Introduction of dtype_int64 breaks that assumption: its
   position on the scale should be between dtype_long and dtype_real, but
   the types are integers, and dtype_quad occupies the only available
   place.  Renumbering all the higher-numbered types would be a major
   ODS change and a fundamental discomfort

   This table permits us to put the entries in the right order for
   comparison purpose, even though isc_int64 had to get number 19, which
   is otherwise too high.

   This table is indexed by dsc_dtype, and returns the relative priority
   of types for use when different types are compared.
   */
const BYTE CVT2_compare_priority[] =
{
	// dtype_unknown through dtype_varying have their natural values stored in the table.
	0,	// dtype_unknown
	1,	// dtype_text
	2,	// dtype_cstring
	3,	// dtype_varying
	// dtypes and 4, 5 are unused
	0, 0,
	// packed through long also have their natural values in the table
	6,	// dtype_packed
	7,	// dtype_byte,
	8,	// dtype_short
	9,	// dtype_long
	// Move quad up by one to make room for int64 at its proper place in the table
	11,	// dtype_quad
	// Leave space for int128
	13,	// dtype_real
	14,	// dtype_double
	15,	// dtype_d_float
	// Leave space for dec64 and dec128
	18,	// dtype_sql_date
	19,	// dtype_sql_time
	// Leave space for dtype_sql_time_tz
	21,	// dtype_timestamp
	// Leave space for dtype_timestamp_tz
	23,	// dtype_blob
	24,	// dtype_array
	10,	// dtype_int64 - goes right after long
	25,	// dtype_dbkey - compares with nothing except itself
	26,	// dtype_boolean - compares with nothing except itself
	16,	// dec64 - go after dtype_d_float
	17,	// dec128 - go after dec64 and before dtype_sql_date
	12,	// dtype_int128 - go after quad
	20,	// dtype_sql_time_tz - go after dtype_sql_time
	22,	// dtype_timestamp_tz - go after dtype_timestamp
	99, // dtype_ex_time_tz - should not be used here
	99  // dtype_ex_timestamp_tz - should not be used here
};

static inline int QUAD_COMPARE(const SQUAD* arg1, const SQUAD* arg2)
{
/**************************************
 *
 *      Q U A D _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *      Compare two descriptors.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/

	if (((SLONG*) arg1)[HIGH_WORD] > ((SLONG*) arg2)[HIGH_WORD])
		return 1;
	if (((SLONG*) arg1)[HIGH_WORD] < ((SLONG*) arg2)[HIGH_WORD])
		return -1;
	if (((ULONG*) arg1)[LOW_WORD] > ((ULONG*) arg2)[LOW_WORD])
		return 1;
	if (((ULONG*) arg1)[LOW_WORD] < ((ULONG*) arg2)[LOW_WORD])
		return -1;
	return 0;
}


bool CVT2_get_binary_comparable_desc(dsc* result, const dsc* arg1, const dsc* arg2)
{
/**************************************
 *
 *	C V T 2 _ g e t _ b i n a r y _ c o m p a r a b l e _ d e s c
 *
 **************************************
 *
 * Functional description
 *	Return descriptor of the data type to be used for direct (binary) comparison of the given arguments.
 *
 **************************************/

	if (arg1->dsc_dtype == dtype_blob || arg2->dsc_dtype == dtype_blob ||
		arg1->dsc_dtype == dtype_array || arg2->dsc_dtype == dtype_array)
	{
		// Any of the arguments is a blob or an array
		return false;
	}

	if (arg1->dsc_dtype == dtype_dbkey || arg2->dsc_dtype == dtype_dbkey)
	{
		// Any of the arguments is DBKEY
		result->makeText(MAX(arg1->getStringLength(), arg2->getStringLength()), ttype_binary);
	}
	else if (arg1->isText() && arg2->isText())
	{
		// Both arguments are strings
		if (arg1->getTextType() != arg2->getTextType())
		{
			// Charsets/collations are different
			return false;
		}

		if (arg1->dsc_dtype == arg2->dsc_dtype)
		{
			*result = *arg1;
			result->dsc_length = MAX(arg1->dsc_length, arg2->dsc_length);
		}
		else
		{
			result->makeText(MAX(arg1->getStringLength(), arg2->getStringLength()),
				arg1->getTextType());
		}
	}
	else if (arg1->dsc_dtype == arg2->dsc_dtype && arg1->dsc_scale == arg2->dsc_scale)
	{
		// Arguments can be compared directly
		*result = *arg1;
	}
	else if (arg1->dsc_dtype == dtype_boolean || arg2->dsc_dtype == dtype_boolean)
	{
		// boolean is not comparable to a non-boolean
		return false;
	}
	else
	{
		// Arguments are of different data types
		*result = (CVT2_compare_priority[arg1->dsc_dtype] > CVT2_compare_priority[arg2->dsc_dtype]) ? *arg1 : *arg2;

		if (arg1->isExact() && arg2->isExact())
			result->dsc_scale = MIN(arg1->dsc_scale, arg2->dsc_scale);
	}

	return true;
}


static int cmp_numeric_string(const dsc* arg1, const dsc* arg2, Firebird::DecimalStatus decSt)
{
/**************************************
 *
 *	c m p _ n u m e r i c _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Compare any numeric value with string.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/
	fb_assert(arg1->isNumeric());
	fb_assert(arg2->isText());

	Decimal128 buffer;		// enough to fit any required data
	SSHORT scale = 0;
	UCHAR* text;
	USHORT ttype;
	USHORT textLen = CVT_get_string_ptr(arg2, &ttype, &text, nullptr, 0, 0);

	dsc num2;
	num2.dsc_dtype = CVT_get_numeric(text, textLen, &scale, &buffer);
	num2.dsc_address = (UCHAR*)&buffer;
	num2.dsc_scale = scale;
	num2.dsc_length = type_lengths[num2.dsc_dtype];
	num2.dsc_sub_type = 0;
	num2.dsc_flags = 0;

	return CVT2_compare(arg1, &num2, decSt);
}


int CVT2_compare(const dsc* arg1, const dsc* arg2, Firebird::DecimalStatus decSt)
{
/**************************************
 *
 *	C V T 2 _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *	Compare two descriptors.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/
	thread_db* tdbb = NULL;

	// AB: Maybe we need a other error-message, but at least throw
	// a message when 1 or both input parameters are empty.
	if (!arg1 || !arg2) {
		BUGCHECK(189);	// msg 189 comparison not supported for specified data types.
	}

	// Handle the simple (matched) ones first

	if (arg1->dsc_dtype == arg2->dsc_dtype && arg1->dsc_scale == arg2->dsc_scale)
	{
		const UCHAR* p1 = arg1->dsc_address;
		const UCHAR* p2 = arg2->dsc_address;

		switch (arg1->dsc_dtype)
		{
		case dtype_short:
			if (*(SSHORT *) p1 == *(SSHORT *) p2)
				return 0;
			if (*(SSHORT *) p1 > *(SSHORT *) p2)
				return 1;
			return -1;

		case dtype_ex_time_tz:
		case dtype_sql_time_tz:
		case dtype_sql_time:
			if (*(ULONG *) p1 == *(ULONG *) p2)
				return 0;
			if (*(ULONG *) p1 > *(ULONG *) p2)
				return 1;
			return -1;

		case dtype_long:
		case dtype_sql_date:
			if (*(SLONG *) p1 == *(SLONG *) p2)
				return 0;
			if (*(SLONG *) p1 > *(SLONG *) p2)
				return 1;
			return -1;

		case dtype_quad:
			return QUAD_COMPARE((SQUAD *) p1, (SQUAD *) p2);

		case dtype_int64:
			if (*(SINT64 *) p1 == *(SINT64 *) p2)
				return 0;
			if (*(SINT64 *) p1 > *(SINT64 *) p2)
				return 1;
			return -1;

		case dtype_dbkey:
			// Compare canonical DBKEYs with respect to their
			// relation IDs and record numbers
			if (arg1->dsc_length == sizeof(RecordNumber::Packed) &&
				arg2->dsc_length == sizeof(RecordNumber::Packed))
			{
				const auto dbkey1 = (const RecordNumber::Packed*) arg1->dsc_address;
				const auto dbkey2 = (const RecordNumber::Packed*) arg2->dsc_address;

				if (dbkey1->bid_relation_id > dbkey2->bid_relation_id)
					return 1;
				if (dbkey1->bid_relation_id < dbkey2->bid_relation_id)
					return -1;

				RecordNumber recno1, recno2;
				recno1.bid_decode(dbkey1);
				recno2.bid_decode(dbkey2);

				if (recno1 > recno2)
					return 1;
				if (recno1 < recno2)
					return -1;

				return 0;
			}
			// Otherwise, use old ttype_binary compare rules
			{
				const auto l = MIN(arg1->dsc_length, arg2->dsc_length);
				const auto rc = memcmp(p1, p2, l);

				if (rc)
					return rc;
				if (arg1->dsc_length > l)
					return 1;
				if (arg2->dsc_length > l)
					return -1;

				return 0;
			}

		case dtype_ex_timestamp_tz:
		case dtype_timestamp_tz:
		case dtype_timestamp:
			if (((SLONG *) p1)[0] > ((SLONG *) p2)[0])
				return 1;
			if (((SLONG *) p1)[0] < ((SLONG *) p2)[0])
				return -1;
			if (((ULONG *) p1)[1] > ((ULONG *) p2)[1])
				return 1;
			if (((ULONG *) p1)[1] < ((ULONG *) p2)[1])
				return -1;
			return 0;

		case dtype_real:
			if (*(float *) p1 == *(float *) p2)
				return 0;
			if (*(float *) p1 > *(float *) p2)
				return 1;
			return -1;

		case DEFAULT_DOUBLE:
			if (*(double *) p1 == *(double *) p2)
				return 0;
			if (*(double *) p1 > *(double *) p2)
				return 1;
			return -1;

		case dtype_dec64:
			return ((Decimal64*) p1)->compare(decSt, *(Decimal64*) p2);

		case dtype_dec128:
			return ((Decimal128*) p1)->compare(decSt, *(Decimal128*) p2);

		case dtype_int128:
			return ((Int128*) p1)->compare(*(Int128*) p2);

		case dtype_boolean:
			return *p1 == *p2 ? 0 : *p1 < *p2 ? -1 : 1;

		case dtype_text:
		case dtype_varying:
		case dtype_cstring:
		case dtype_array:
		case dtype_blob:
			// Special processing below
			break;

		default:
			// the two arguments have identical dtype and scale, but the
			// dtype is not one of your defined types!
			fb_assert(FALSE);
			break;

		}						// switch on dtype
	}							// if dtypes and scales are equal

	// Handle mixed string comparisons

	if (arg1->dsc_dtype <= dtype_varying && arg2->dsc_dtype <= dtype_varying)
	{
		/*
		 * For the sake of optimization, we call INTL_compare
		 * only when we cannot just do byte-by-byte compare.
		 * We can do a local compare here, if
		 *    (a) one of the arguments is charset ttype_binary
		 * OR (b) both of the arguments are char set ttype_none
		 * OR (c) both of the arguments are char set ttype_ascii
		 * If any argument is ttype_dynamic, we must see the
		 * charset of the attachment.
		 */

		SET_TDBB(tdbb);
		CHARSET_ID charset1 = INTL_TTYPE(arg1);
		if (charset1 == ttype_dynamic)
			charset1 = INTL_charset(tdbb, charset1);

		CHARSET_ID charset2 = INTL_TTYPE(arg2);
		if (charset2 == ttype_dynamic)
			charset2 = INTL_charset(tdbb, charset2);

		if ((IS_INTL_DATA(arg1) || IS_INTL_DATA(arg2)) &&
			(charset1 != ttype_binary) &&
			(charset2 != ttype_binary) &&
			((charset1 != ttype_ascii) ||
			 (charset2 != ttype_ascii)) &&
			((charset1 != ttype_none) || (charset2 != ttype_none)))
		{
			return INTL_compare(tdbb, arg1, arg2, ERR_post);
		}

		UCHAR* p1 = NULL;
		UCHAR* p2 = NULL;
		USHORT t1, t2; // unused later
		USHORT length = CVT_get_string_ptr(arg1, &t1, &p1, NULL, 0, decSt);
		USHORT length2 = CVT_get_string_ptr(arg2, &t2, &p2, NULL, 0, decSt);

		int fill = length - length2;
		const UCHAR pad = charset1 == ttype_binary || charset2 == ttype_binary ? '\0' : ' ';

		if (length >= length2)
		{
			if (length2)
			{
				do
				{
					if (*p1++ != *p2++)
						return (p1[-1] > p2[-1]) ? 1 : -1;
				} while (--length2);
			}

			if (fill > 0)
			{
				do
				{
					if (*p1++ != pad)
						return (p1[-1] > pad) ? 1 : -1;
				} while (--fill);
			}

			return 0;
		}

		if (length)
		{
			do
			{
				if (*p1++ != *p2++)
					return (p1[-1] > p2[-1]) ? 1 : -1;
			} while (--length);
		}

		do
		{
			if (*p2++ != pad)
				return (pad > p2[-1]) ? 1 : -1;
		} while (++fill);

		return 0;
	}

	// Handle heterogeneous compares

	if (CVT2_compare_priority[arg1->dsc_dtype] < CVT2_compare_priority[arg2->dsc_dtype])
		return -CVT2_compare(arg2, arg1, decSt);

	// At this point, the type of arg1 is guaranteed to be "greater than" arg2,
	// in the sense that it is the preferred type for comparing the two.

	switch (arg1->dsc_dtype)
	{
	case dtype_ex_timestamp_tz:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_ex_timestamp_tz;
			ISC_TIMESTAMP_TZ_EX datetime;
			desc.dsc_length = sizeof(datetime);
			desc.dsc_address = (UCHAR*) &datetime;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_timestamp_tz:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_timestamp_tz;
			ISC_TIMESTAMP_TZ datetime;
			desc.dsc_length = sizeof(datetime);
			desc.dsc_address = (UCHAR*) &datetime;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_timestamp:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_timestamp;
			SLONG datetime[2];
			desc.dsc_length = sizeof(datetime);
			desc.dsc_address = (UCHAR*) datetime;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_ex_time_tz:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_ex_time_tz;
			ISC_TIME_TZ_EX atime;
			desc.dsc_length = sizeof(atime);
			desc.dsc_address = (UCHAR*) &atime;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_sql_time_tz:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_sql_time_tz;
			ISC_TIME_TZ atime;
			desc.dsc_length = sizeof(atime);
			desc.dsc_address = (UCHAR*) &atime;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_sql_time:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_sql_time;
			SLONG atime;
			desc.dsc_length = sizeof(atime);
			desc.dsc_address = (UCHAR*) &atime;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_sql_date:
		{
			DSC desc;
			MOVE_CLEAR(&desc, sizeof(desc));
			desc.dsc_dtype = dtype_sql_date;
			SLONG date;
			desc.dsc_length = sizeof(date);
			desc.dsc_address = (UCHAR*) &date;
			CVT_move(arg2, &desc, 0);
			return CVT2_compare(arg1, &desc, 0);
		}

	case dtype_short:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			SSHORT scale = MIN(arg1->dsc_scale, arg2->dsc_scale);
			const SLONG temp1 = CVT_get_long(arg1, scale, decSt, ERR_post);
			const SLONG temp2 = CVT_get_long(arg2, scale, decSt, ERR_post);

			if (temp1 == temp2)
				return 0;
			if (temp1 > temp2)
				return 1;
			return -1;
		}

	case dtype_long:
		// Since longs may overflow when scaled, use int64 instead
	case dtype_int64:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			SSHORT scale = MIN(arg1->dsc_scale, arg2->dsc_scale);
			const SINT64 temp1 = CVT_get_int64(arg1, scale, decSt, ERR_post);
			const SINT64 temp2 = CVT_get_int64(arg2, scale, decSt, ERR_post);

			if (temp1 == temp2)
				return 0;
			if (temp1 > temp2)
				return 1;
			return -1;
		}

	case dtype_quad:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			SSHORT scale = MIN(arg1->dsc_scale, arg2->dsc_scale);
			const SQUAD temp1 = CVT_get_quad(arg1, scale, decSt, ERR_post);
			const SQUAD temp2 = CVT_get_quad(arg2, scale, decSt, ERR_post);
			return QUAD_COMPARE(&temp1, &temp2);
		}

	case dtype_real:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			const float temp1 = (float) CVT_get_double(arg1, decSt, ERR_post);
			const float temp2 = (float) CVT_get_double(arg2, decSt, ERR_post);
			if (temp1 == temp2)
				return 0;
			if (temp1 > temp2)
				return 1;
			return -1;
		}

	case dtype_double:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			const double temp1 = CVT_get_double(arg1, decSt, ERR_post);
			const double temp2 = CVT_get_double(arg2, decSt, ERR_post);
			if (temp1 == temp2)
				return 0;
			if (temp1 > temp2)
				return 1;
			return -1;
		}

	case dtype_dec64:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			const Decimal64 temp1 = CVT_get_dec64(arg1, decSt, ERR_post);
			const Decimal64 temp2 = CVT_get_dec64(arg2, decSt, ERR_post);
			return temp1.compare(decSt, temp2);
		}

	case dtype_dec128:
		{
			const Decimal128 temp1 = CVT_get_dec128(arg1, decSt, ERR_post);
			const Decimal128 temp2 = CVT_get_dec128(arg2, decSt, ERR_post);
			return temp1.compare(decSt, temp2);
		}

	case dtype_int128:
		{
			if (arg2->isText())
				return cmp_numeric_string(arg1, arg2, decSt);

			SSHORT scale = MIN(arg1->dsc_scale, arg2->dsc_scale);
			const Int128 temp1 = CVT_get_int128(arg1, scale, decSt, ERR_post);
			const Int128 temp2 = CVT_get_int128(arg2, scale, decSt, ERR_post);
			return temp1.compare(temp2);
		}

	case dtype_blob:
		return CVT2_blob_compare(arg1, arg2, decSt);

	case dtype_array:
		ERR_post(Arg::Gds(isc_wish_list) << Arg::Gds(isc_blobnotsup) << "compare");
		break;

	case dtype_dbkey:
		if (arg2->isText())
		{
			UCHAR* p = NULL;
			USHORT ttype;
			const USHORT length = CVT_get_string_ptr(arg2, &ttype, &p, NULL, 0, decSt);

			// Compare DBKEY with a compatible binary string with respect to
			// relation IDs and record numbers
			if (arg1->dsc_length == sizeof(RecordNumber::Packed) &&
				ttype == ttype_binary && length == sizeof(RecordNumber::Packed))
			{
				Aligner<RecordNumber::Packed> alignedNumber(p, length);

				const auto dbkey1 = (const RecordNumber::Packed*) arg1->dsc_address;
				const auto dbkey2 = (const RecordNumber::Packed*) alignedNumber;

				if (dbkey1->bid_relation_id > dbkey2->bid_relation_id)
					return 1;
				if (dbkey1->bid_relation_id < dbkey2->bid_relation_id)
					return -1;

				RecordNumber recno1, recno2;
				recno1.bid_decode(dbkey1);
				recno2.bid_decode(dbkey2);

				if (recno1 > recno2)
					return 1;
				if (recno1 < recno2)
					return -1;

				return 0;
			}

			const auto l = MIN(arg1->dsc_length, length);
			const auto rc = memcmp(arg1->dsc_address, p, l);

			if (rc)
				return rc;
			if (arg1->dsc_length > l)
				return 1;
			if (length > l)
				return -1;

			return 0;
		}
		ERR_post(Arg::Gds(isc_wish_list) << Arg::Gds(isc_random) << "DB_KEY compare");
		break;

	case dtype_boolean:
		{
			const int temp1 = CVT_get_boolean(arg1, ERR_post) ? 1 : 0;
			const int temp2 = CVT_get_boolean(arg2, ERR_post) ? 1 : 0;

			if (temp1 == temp2)
				return 0;
			if (temp1 > temp2)
				return 1;
			return -1;
		}

	default:
		BUGCHECK(189);			// msg 189 comparison not supported for specified data types
		break;
	}
	return 0;
}


int CVT2_blob_compare(const dsc* arg1, const dsc* arg2, DecimalStatus decSt)
{
/**************************************
 *
 *	C V T 2 _ b l o b _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *	Compare two blobs.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *  Alternatively, it will try to compare a blob against a string;
 *	in this case, the string should be the second argument.
 * CVC: Ann Harrison asked for this function to make comparisons more
 * complete in the engine.
 *
 **************************************/

	SLONG l1, l2;
	USHORT ttype2;
	int ret_val = 0;

	thread_db* tdbb = NULL;
	SET_TDBB(tdbb);

	// DEV_BLKCHK (node, type_nod);

	if (arg1->dsc_dtype != dtype_blob)
		ERR_post(Arg::Gds(isc_wish_list) << Arg::Gds(isc_datnotsup));

	USHORT ttype1;
	if (arg1->dsc_sub_type == isc_blob_text)
		ttype1 = arg1->dsc_blob_ttype();       // Load blob character set and collation
	else
		ttype1 = ttype_binary;

	TextType* obj1 = INTL_texttype_lookup(tdbb, ttype1);
	ttype1 = obj1->getType();

	// Is arg2 a blob?
	if (arg2->dsc_dtype == dtype_blob)
	{
	    // Same blob id address?
		if (arg1->dsc_address == arg2->dsc_address)
			return 0;

		// Second test for blob id, checking relation and slot.
		const bid* bid1 = (bid*) arg1->dsc_address;
		const bid* bid2 = (bid*) arg2->dsc_address;
		if (*bid1 == *bid2)
		{
			return 0;
		}

		if (arg2->dsc_sub_type == isc_blob_text)
			ttype2 = arg2->dsc_blob_ttype();       // Load blob character set and collation
		else
			ttype2 = ttype_binary;

		TextType* obj2 = INTL_texttype_lookup(tdbb, ttype2);
		ttype2 = obj2->getType();

		if (ttype1 == ttype_binary || ttype2 == ttype_binary)
			ttype1 = ttype2 = ttype_binary;
		else if (ttype1 == ttype_none || ttype2 == ttype_none)
			ttype1 = ttype2 = ttype_none;

		obj1 = INTL_texttype_lookup(tdbb, ttype1);
		obj2 = INTL_texttype_lookup(tdbb, ttype2);

		CharSet* charSet1 = obj1->getCharSet();
		CharSet* charSet2 = obj2->getCharSet();

		Firebird::HalfStaticArray<UCHAR, BUFFER_LARGE> buffer1;
		Firebird::HalfStaticArray<UCHAR, BUFFER_LARGE> buffer2;
		fb_assert(BUFFER_LARGE % 4 == 0);	// 4 is our maximum character length

		UCHAR bpb[] = {isc_bpb_version1,
					   isc_bpb_source_type, 1, isc_blob_text, isc_bpb_source_interp, 1, 0,
					   isc_bpb_target_type, 1, isc_blob_text, isc_bpb_target_interp, 1, 0};
		USHORT bpbLength = 0;

		if (arg1->dsc_sub_type == isc_blob_text && arg2->dsc_sub_type == isc_blob_text)
		{
			bpb[6] = arg2->dsc_scale;	// source charset
			bpb[12] = arg1->dsc_scale;	// destination charset
			bpbLength = sizeof(bpb);
		}

	    blb* blob1 = blb::open(tdbb, tdbb->getRequest()->req_transaction,
			(bid*) arg1->dsc_address);
		blb* blob2 = blb::open2(tdbb, tdbb->getRequest()->req_transaction,
			(bid*) arg2->dsc_address, bpbLength, bpb);

		if (charSet1->isMultiByte())
		{
			buffer1.getBuffer(blob1->blb_length);
			buffer2.getBuffer(blob2->blb_length / charSet2->minBytesPerChar() * charSet1->maxBytesPerChar());
		}

		while (ret_val == 0 && !(blob1->blb_flags & BLB_eof) && !(blob2->blb_flags & BLB_eof))
		{
			l1 = blob1->BLB_get_data(tdbb, buffer1.begin(), buffer1.getCapacity(), false);
			l2 = blob2->BLB_get_data(tdbb, buffer2.begin(), buffer2.getCapacity(), false);

			ret_val = obj1->compare(l1, buffer1.begin(), l2, buffer2.begin());
		}

		if (ret_val == 0)
		{
			if ((blob1->blb_flags & BLB_eof) == BLB_eof)
				l1 = 0;

			if ((blob2->blb_flags & BLB_eof) == BLB_eof)
				l2 = 0;

			while (ret_val == 0 &&
				   !((blob1->blb_flags & BLB_eof) == BLB_eof &&
					 (blob2->blb_flags & BLB_eof) == BLB_eof))
			{
				if (!(blob1->blb_flags & BLB_eof))
					l1 = blob1->BLB_get_data(tdbb, buffer1.begin(), buffer1.getCapacity(), false);

				if (!(blob2->blb_flags & BLB_eof))
					l2 = blob2->BLB_get_data(tdbb, buffer2.begin(), buffer2.getCapacity(), false);

				ret_val = obj1->compare(l1, buffer1.begin(), l2, buffer2.begin());
			}
		}

		blob1->BLB_close(tdbb);
		blob2->BLB_close(tdbb);
	}
	else if (arg2->dsc_dtype == dtype_array)
	{
		// We do not accept arrays for now. Maybe InternalArrayDesc in the future.
		ERR_post(Arg::Gds(isc_wish_list) << Arg::Gds(isc_datnotsup));
	}
	else
	{
		// The second parameter should be a string.
		if (arg2->dsc_dtype <= dtype_varying)
		{
			if ((ttype2 = arg2->dsc_ttype()) != ttype_binary)
				ttype2 = ttype1;
		}
		else
			ttype2 = ttype1;

		if (ttype1 == ttype_binary || ttype2 == ttype_binary)
			ttype1 = ttype2 = ttype_binary;
		else if (ttype1 == ttype_none || ttype2 == ttype_none)
			ttype1 = ttype2 = ttype_none;

		obj1 = INTL_texttype_lookup(tdbb, ttype1);

		CharSet* charSet1 = obj1->getCharSet();

		Firebird::HalfStaticArray<UCHAR, BUFFER_LARGE> buffer1;
		UCHAR* p;
		MoveBuffer temp_str;

		l2 = CVT2_make_string2(arg2, ttype1, &p, temp_str, decSt);

		blb* blob1 = blb::open(tdbb, tdbb->getRequest()->req_transaction, (bid*) arg1->dsc_address);

		if (charSet1->isMultiByte())
			buffer1.getBuffer(blob1->blb_length);
		else
			buffer1.getBuffer(l2);

		l1 = blob1->BLB_get_data(tdbb, buffer1.begin(), buffer1.getCapacity(), false);
		ret_val = obj1->compare(l1, buffer1.begin(), l2, p);

		while (ret_val == 0 && (blob1->blb_flags & BLB_eof) != BLB_eof)
		{
			l1 = blob1->BLB_get_data(tdbb, buffer1.begin(), buffer1.getCapacity(), false);
			ret_val = obj1->compare(l1, buffer1.begin(), 0, p);
		}

		blob1->BLB_close(tdbb);
	}

	return ret_val;
}


void CVT2_make_metaname(const dsc* desc, MetaName& name, DecimalStatus decSt)
/**************************************
 *
 *	C V T 2 _ m a k e _ m e t a n a m e
 *
 **************************************
 *
 * Functional description
 *
 *     Convert the data from the desc to a string in the metadata charset.
 *     Then return the string as MetaName object.
 *
 **************************************/
{
	MoveBuffer buff;
	UCHAR* ptr = nullptr;

	const auto len = CVT2_make_string2(desc, CS_METADATA, &ptr, buff, decSt);
	name.assign(reinterpret_cast<const char*>(ptr), len);
}


USHORT CVT2_make_string2(const dsc* desc, USHORT to_interp, UCHAR** address, MoveBuffer& temp, DecimalStatus decSt)
{
/**************************************
 *
 *	C V T 2 _ m a k e _ s t r i n g 2
 *
 **************************************
 *
 * Functional description
 *
 *     Convert the data from the desc to a string in the specified interp.
 *     The pointer to this string is returned in address.
 *
 **************************************/
	UCHAR* from_buf;
	USHORT from_len;
	USHORT from_interp;

	fb_assert(desc != NULL);
	fb_assert(address != NULL);

	switch (desc->dsc_dtype)
	{
	case dtype_text:
		from_buf = desc->dsc_address;
		from_len = desc->dsc_length;
		from_interp = INTL_TTYPE(desc);
		break;

	case dtype_cstring:
		from_buf = desc->dsc_address;
		from_len = MIN(static_cast<USHORT>(strlen((char *) desc->dsc_address)), (unsigned) (desc->dsc_length - 1));
		from_interp = INTL_TTYPE(desc);
		break;

	case dtype_varying:
		{
			vary* varying = (vary*) desc->dsc_address;
			from_buf = reinterpret_cast<UCHAR*>(varying->vary_string);
			from_len = MIN(varying->vary_length, (USHORT) (desc->dsc_length - sizeof(SSHORT)));
			from_interp = INTL_TTYPE(desc);
		}
		break;
	}

	if (desc->isText())
	{
		if (from_interp == to_interp || to_interp == ttype_none || to_interp == ttype_binary)
		{
			*address = from_buf;
			return from_len;
		}

		thread_db* tdbb = JRD_get_thread_data();
		const USHORT cs1 = INTL_charset(tdbb, to_interp);
		const USHORT cs2 = INTL_charset(tdbb, from_interp);
		if (cs1 == cs2)
		{
			*address = from_buf;
			return from_len;
		}

		USHORT length = INTL_convert_bytes(tdbb, cs1, NULL, 0, cs2, from_buf, from_len, ERR_post);
		UCHAR* tempptr = temp.getBuffer(length);
		length = INTL_convert_bytes(tdbb, cs1, tempptr, length, cs2, from_buf, from_len, ERR_post);
		*address = tempptr;
		temp.resize(length);
		return length;
	}

	// Not string data, then  -- convert value to varying string.

	dsc temp_desc;
	MOVE_CLEAR(&temp_desc, sizeof(temp_desc));
	temp_desc.dsc_length = temp.getCapacity();
	temp_desc.dsc_address = temp.getBuffer(temp_desc.dsc_length);
	vary* vtmp = reinterpret_cast<vary*>(temp_desc.dsc_address);
	temp_desc.dsc_dtype = dtype_varying;
	temp_desc.setTextType(to_interp);
	CVT_move(desc, &temp_desc, decSt);
	*address = reinterpret_cast<UCHAR*>(vtmp->vary_string);

	return vtmp->vary_length;
}

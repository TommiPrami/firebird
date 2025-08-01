/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Claudio Valderrama on 16-Mar-2007
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Claudio Valderrama
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "UserBlob.h"
#include "ibase.h"
#include "../yvalve/gds_proto.h"

static constexpr USHORT SEGMENT_LIMIT = 65535;


bool UserBlob::open(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid)
{
	if (m_direction != dir_none)
		return false;

	if (!blobIsNull(blobid) && !isc_open_blob(m_status, &db, &trans, &m_blob, &blobid))
	{
		m_direction = dir_read;
		return true;
	}
	return false;
}

bool UserBlob::open(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid,
					USHORT bpb_len, const UCHAR* bpb)
{
	if (m_direction != dir_none)
		return false;

	if ((bpb_len > 0 && !bpb) || blobIsNull(blobid))
		return false;

	if (!isc_open_blob2(m_status, &db, &trans, &m_blob, &blobid, bpb_len, bpb))
	{
		m_direction = dir_read;
		return true;
	}
	return false;
}

bool UserBlob::create(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid)
{
	if (m_direction != dir_none)
		return false;

	blobid.gds_quad_high = blobid.gds_quad_low = 0;
	if (!isc_create_blob(m_status, &db, &trans, &m_blob, &blobid))
	{
		m_direction = dir_write;
		return true;
	}
	return false;
}

bool UserBlob::create(FB_API_HANDLE& db, FB_API_HANDLE& trans, ISC_QUAD& blobid,
			USHORT bpb_len, const UCHAR* bpb)
{
	if (m_direction != dir_none)
		return false;

	if (bpb_len > 0 && !bpb)
		return false;

	const char* bpb2 = reinterpret_cast<const char*>(bpb);
	blobid.gds_quad_high = blobid.gds_quad_low = 0;
	if (!isc_create_blob2(m_status, &db, &trans, &m_blob, &blobid, bpb_len, bpb2))
	{
		m_direction = dir_write;
		return true;
	}
	return false;
}

bool UserBlob::close(bool force_internal_SV)
{
	bool rc = false;
	if (m_blob)
	{
		rc = !isc_close_blob(force_internal_SV ? m_default_status : m_status, &m_blob);
		m_blob = 0;
		m_direction = dir_none;
	}
	return rc;
}

bool UserBlob::getSegment(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len)
{
	real_len = 0;

#ifdef DEV_BUILD
	if (!m_blob || m_direction == dir_write)
		return false;

	if (!len || !buffer)
		return false;
#endif

	USHORT olen = 0;
	const USHORT ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<USHORT>(len);
	char* buf2 = static_cast<char*>(buffer);
	if (!isc_get_segment(m_status, &m_blob, &olen, ilen, buf2) || m_status[1] == isc_segment)
	{
		real_len = olen;
		return true;
	}
	return false;
}

bool UserBlob::getData(FB_SIZE_T len, void* buffer, FB_SIZE_T& real_len,
						bool use_sep, const UCHAR separator)
{
	if (!m_blob || m_direction == dir_write)
		return false;

	if (!len || !buffer)
		return false;

	bool rc = false;
	real_len = 0;
	char* buf2 = static_cast<char*>(buffer);
	while (len)
	{
		USHORT olen = 0;
		const USHORT ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<USHORT>(len);
		if (!isc_get_segment(m_status, &m_blob, &olen, ilen, buf2) || m_status[1] == isc_segment)
		{
			len -= olen;
			buf2 += olen;
			real_len += olen;
			if (len && use_sep) // Append the segment separator.
			{
				--len;
				*buf2++ = separator;
				++real_len;
			}
			rc = true;
		}
		else
			break;
	}
	return rc;
}

bool UserBlob::putSegment(FB_SIZE_T len, const void* buffer)
{
#ifdef DEV_BUILD
	if (!m_blob || m_direction == dir_read)
		return false;

	if (len > 0 && !buffer)
		return false;
#endif

	const USHORT ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<USHORT>(len);
	const char* buf2 = static_cast<const char*>(buffer);
	return !isc_put_segment(m_status, &m_blob, ilen, buf2);
}

bool UserBlob::putSegment(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len)
{
#ifdef DEV_BUILD
	if (!m_blob || m_direction == dir_read)
		return false;

	if (len > 0 && !buffer)
		return false;
#endif

	real_len = 0;
	USHORT ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<USHORT>(len);
	const char* buf2 = static_cast<const char*>(buffer);
	if (isc_put_segment(m_status, &m_blob, ilen, buf2))
		return false;

	real_len = ilen;
	return true;
}

bool UserBlob::putData(FB_SIZE_T len, const void* buffer, FB_SIZE_T& real_len)
{
	if (!m_blob || m_direction == dir_read)
		return false;

	if (len > 0 && !buffer)
		return false;

	real_len = 0;
	const char* buf2 = static_cast<const char*>(buffer);
	while (len)
	{
		USHORT ilen = len > SEGMENT_LIMIT ? SEGMENT_LIMIT : static_cast<USHORT>(len);
		if (isc_put_segment(m_status, &m_blob, ilen, buf2))
			return false;

		len -= ilen;
		buf2 += ilen;
		real_len += ilen;
	}
	return true;
}

bool UserBlob::getInfo(FB_SIZE_T items_size, const UCHAR* items,
						FB_SIZE_T info_size, UCHAR* blob_info) const
{
	if (!m_blob || m_direction != dir_read)
		return false;

	// We have to cater for the API limitations.
	const SSHORT in_len = static_cast<SSHORT>(MIN(items_size, MAX_SSHORT));
	const SSHORT out_len = static_cast<SSHORT>(MIN(info_size, MAX_SSHORT));
	// That the API declares the second param as non const is a bug.
	FB_API_HANDLE blob = m_blob;
	return !isc_blob_info(m_status, &blob,
							in_len, reinterpret_cast<const char*>(items),
							out_len, reinterpret_cast<char*>(blob_info));
}


bool getBlobSize(const UserBlob& b, FB_UINT64* size, ULONG* seg_count, USHORT* max_seg)
{
/**************************************
 *
 *	g e t B l o b S i z e
 *
 **************************************
 *
 * Functional description
 *	Get the size, number of segments, and max
 *	segment length of a blob.  Return true
 *	if it happens to succeed.
 *	This is a clone of gds__blob_size.
 *
 **************************************/
	static const UCHAR blob_items[] =
	{
		isc_info_blob_max_segment,
		isc_info_blob_num_segments,
		isc_info_blob_total_length
	};

	UCHAR buffer[64];

	if (!b.getInfo(sizeof(blob_items), blob_items, sizeof(buffer), buffer))
		return false;

	const UCHAR* p = buffer;
	const UCHAR* const end = buffer + sizeof(buffer);
	for (UCHAR item = *p++; item != isc_info_end && p < end; item = *p++)
	{
		const auto l = gds__vax_integer(p, 2);
		p += 2;
		const auto n = isc_portable_integer(p, l);
		p += l;

		switch (item)
		{
		case isc_info_blob_max_segment:
			if (max_seg)
				*max_seg = (USHORT) n;
			break;

		case isc_info_blob_num_segments:
			if (seg_count)
				*seg_count = (ULONG) n;
			break;

		case isc_info_blob_total_length:
			if (size)
				*size = (FB_UINT64) n;
			break;

		default:
			return false;
		}
	}

	return true;
}

/* If someone sees the need to not depend on gds.cpp...
static SLONG fb_vax_integer(const UCHAR* ptr, int length)
{
// **************************************
// *
// *	f b _ v a x _ i n t e g e r
// *
// **************************************
// *
// * Functional description
// *	Pick up (and convert) a VAX style integer of length 1, 2, or 4 bytes.
// *
// **************************************
	SLONG value = 0;
	int shift = 0;

	while (--length >= 0) {
		value += ((SLONG) *ptr++) << shift;
		shift += 8;
	}

	return value;
}
*/


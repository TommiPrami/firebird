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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2006 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_FILE_H
#define CLASSES_FILE_H

#include "../common/classes/array.h"

#if !defined(SOLARIS) && !defined(AIX)
typedef FB_UINT64 offset_t;
#endif

namespace Firebird {

class File
{
public:
	virtual ~File() {}

	virtual FB_SIZE_T read(offset_t, void*, FB_SIZE_T) = 0;
	virtual FB_SIZE_T write(offset_t, const void*, FB_SIZE_T) = 0;

	virtual void unlink() noexcept = 0;

	virtual offset_t getSize() const noexcept = 0;
};

class ZeroBuffer
{
	static constexpr FB_SIZE_T DEFAULT_SIZE = 1024 * 256;
	static constexpr FB_SIZE_T SYS_PAGE_SIZE = 1024 * 4;

public:
	explicit ZeroBuffer(MemoryPool& p, FB_SIZE_T size = DEFAULT_SIZE)
		: buffer(p)
	{
		bufSize = size;
		bufAligned = buffer.getBuffer(bufSize + SYS_PAGE_SIZE);
		bufAligned = FB_ALIGN(bufAligned, SYS_PAGE_SIZE);
		memset(bufAligned, 0, size);
	}

	const char* getBuffer() const noexcept { return bufAligned; }
	FB_SIZE_T getSize() const noexcept { return bufSize; }

private:
	Firebird::Array<char> buffer;
	char* bufAligned;
	FB_SIZE_T bufSize;
};

} // namespace

#endif // CLASSES_FILE_H

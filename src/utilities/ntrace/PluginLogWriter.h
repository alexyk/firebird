/*
 *	PROGRAM:	SQL Trace plugin
 *	MODULE:		PluginLogWriter.h
 *	DESCRIPTION:	Plugin log writer implementation
 *
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
*/

#ifndef PLUGINLOGWRITER_H
#define PLUGINLOGWRITER_H


#include "firebird.h"
#include "../../jrd/ntrace.h"
#include "../../common/classes/timestamp.h"
#include "../../common/isc_s_proto.h"
#include "../../common/os/path_utils.h"
#include "../../common/classes/ImplementHelper.h"
#include "../../common/classes/TimerImpl.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


class PluginLogWriter FB_FINAL :
	public Firebird::RefCntIface<Firebird::ITraceLogWriterImpl<PluginLogWriter, Firebird::CheckStatusWrapper> >
{
public:
	PluginLogWriter(const char* fileName, size_t maxSize);
	~PluginLogWriter();

	// TraceLogWriter implementation
	virtual FB_SIZE_T write(const void* buf, FB_SIZE_T size);
	virtual FB_SIZE_T write_s(Firebird::CheckStatusWrapper* status, const void* buf, unsigned size);

	virtual int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

private:
	SINT64 seekToEnd();
	void reopen();
	void checkErrno(const char* operation);

	// evaluate new value or clear idle timer
	void setupIdleTimer(bool clear);

	void onIdleTimer(Firebird::TimerImpl*);

	// Windows requires explicit syncronization when few processes appends to the
	// same file simultaneously, therefore we used our fastMutex for this
	// purposes. On Posix's platforms we honour O_APPEND flag which works
	// better as in this case syncronization is performed by OS kernel itself.
#ifdef WIN_NT
	static void checkMutex(const TEXT*, int);
	void lock();
	void unlock();

	struct Firebird::mtx m_mutex;

	class Guard
	{
	public:
		explicit Guard(PluginLogWriter* log) : m_log(*log)
		{
			m_log.lock();
		}

		~Guard()
		{
			m_log.unlock();
		}

	private:
		PluginLogWriter& m_log;
	};
#else
	class Guard : public Firebird::MutexLockGuard
	{
	public:
		explicit Guard(PluginLogWriter* log) :
			Firebird::MutexLockGuard(log->m_mutex, FB_FUNCTION)
		{}
	};

	Firebird::Mutex m_mutex;
#endif

	Firebird::PathName m_fileName;
	int		 m_fileHandle;
	size_t	 m_maxSize;

	typedef Firebird::TimerImpl IdleTimer;
	Firebird::RefPtr<IdleTimer> m_idleTimer;
};

#endif // PLUGINLOGWRITER_H

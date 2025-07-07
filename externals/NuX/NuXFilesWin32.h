#ifndef NuXFilesWin32_h
#define NuXFilesWin32_h

#if (_WIN32_WINNT < 0x0500)
	#undef _WIN32_WINNT
	#define _WIN32_WINNT 0x0500
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include "NuXFiles.h"

namespace NuXFiles {

class ReadOnlyFile::Impl {
	friend class ReadOnlyFile;
	friend class ReadWriteFile;
	friend class ExchangingFile;
	public:		Impl(const Path& path, ::HANDLE handle) : path(path), handle(handle) { } // Inherit handle.
	public:		::HANDLE getWin32Handle() const { return handle; }
	public:		virtual ~Impl();
	protected:	Path path;
	protected:	::HANDLE handle;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

}

#endif

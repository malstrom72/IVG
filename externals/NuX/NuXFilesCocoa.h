#ifndef NuXFilesCocoa_h
#define NuXFilesCocoa_h

#if (__OBJC__)
#import <Cocoa/Cocoa.h>
#else
class NSURL;
class NSFileHandle;
#endif

#include "NuXFiles.h"

#define NUXFILES_COCOA_USE_POSIX_FOR_IO 1

namespace NuXFiles {

class Path::Impl {
	friend class Path;
	
	public:
		Impl(NSURL* url);
		Impl(const Impl& that);
		Impl& operator=(const Impl& that);
		NSURL* getNSURL();
		~Impl();
			
	protected:
		NSURL* url;
};

class ReadOnlyFile::Impl {
	friend class ReadOnlyFile;
	friend class ReadWriteFile;
	friend class ExchangingFile;
	
	public:
		Impl(const Path& path, NSFileHandle* fileHandle);
		NSFileHandle* getNSFileHandle() const;
		~Impl();
		
	protected:
		Path path;
		NSFileHandle* fileHandle;
		
	private:
		Impl(const Impl& copy); // N/A
		Impl& operator=(const Impl& copy); // N/A
};

}

#endif

/*
BSD 2-Clause License

Copyright (c) 2005-2025, Magnus Lidström

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
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

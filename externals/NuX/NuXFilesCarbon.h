/*
 *  NuXFilesCarbon.h
 *  NuXTest
 *
 *  Created by Magnus Lidström on 1/10/06.
 *  Copyright 2006 NuEdge Development. All rights reserved.
 *
 */

#ifndef NuXFilesCarbon_h
#define NuXFilesCarbon_h

#include <Carbon/Carbon.h>

#include "NuXFiles.h"

namespace NuXFiles {

class Path::Impl {
	friend class Path;
	public:			Impl(::CFURLRef urlRef) : urlRef(urlRef) { assert(urlRef != 0); assert(::CFURLCanBeDecomposed(urlRef)); ::CFRetain(urlRef); } // Creates a path implementation from an existing url-ref, the url-ref is retained so you are still responsible for releasing it after this construction. Use like this: { NuXFiles::Path urlPath(new NuXFiles::Path::Impl(urlRef)); }
	public:			Impl(const ::FSRef& fsRef); // Creates a path implementation from an existing FSRef, use like this: { NuXFiles::Path fsRefPath(new NuXFiles::Path::Impl(fsRef)); }
	public:			::CFURLRef getCarbonURLRef() const { return urlRef; }
	public:			bool tryToGetCarbonFSRef(::FSRef& fsRef) const { return ::CFURLGetFSRef(urlRef, &fsRef); }
	public:			virtual ~Impl();
	protected:		Impl(::CFURLRef baseURL, const std::wstring& pathString);
	protected:		Impl(const Impl& copy) : urlRef(copy.urlRef) { assert(copy.urlRef != 0); ::CFRetain(copy.urlRef); }
	protected:		Impl& operator=(const Impl& copy);
	protected:		::CFURLRef urlRef;
};

class ReadOnlyFile::Impl {
	friend class ReadOnlyFile;
	friend class ReadWriteFile;
	friend class ExchangingFile;
	public:		Impl(::SInt16 forkRefNum) : forkRefNum(forkRefNum) { } // Inherit forkRefNum.
	public:		::SInt16 getCarbonForkRefNum() const { return forkRefNum; }
	public:		virtual ~Impl();
	protected:  ::SInt16 forkRefNum;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

}

#endif

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

#include "NuXFiles.h"

namespace NuXFiles {

class Path::Impl {
	friend class Path;
	public:			Impl(const std::string& path) : path(path) { }
	public:			const std::string& getPosixPath() const { return path; }
	protected:		const std::string path;
};

class ReadOnlyFile::Impl {
	friend class ReadOnlyFile;
	friend class ReadWriteFile;
	friend class ExchangingFile;
	public:		Impl(int fileDescriptor) : fileDescriptor(fileDescriptor) { assert(fileDescriptor >= 0); }
	public:		int getPosixFileDescriptor() const { return fileDescriptor; }
	protected:  const int fileDescriptor;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

}

#endif

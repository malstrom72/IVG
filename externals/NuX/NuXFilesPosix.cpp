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
#include <vector>
#include <string>
#include <sstream>
#include <locale>
#include <codecvt>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glob.h>
#include <cstring>
#include <cassert>
#include "NuXFilesPosix.h"

namespace NuXFiles {

static std::wstring fromUTF8(const std::string& s) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	return conv.from_bytes(s);
}

static std::string toUTF8(const std::wstring& s) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	return conv.to_bytes(s);
}

static bool gotTrailingSlash(const std::wstring& p) {
	return (!p.empty() && p.back() == L'/');
}

static std::wstring appendSlash(const std::wstring& p) {
	return gotTrailingSlash(p) ? p : p + L'/';
}

static std::wstring removeSlash(const std::wstring& p) {
	return gotTrailingSlash(p) ? std::wstring(p.begin(), p.end() - 1) : p;
}

/* --- PathAttributes --- */

PathAttributes::PathAttributes()
	: isReadOnly(false), isHidden(false), win32Attributes(0), macFileType(0), macFileCreator(0) {}

/* --- PathTime --- */

PathTime::PathTime(time_t cTime) {
	long long t = static_cast<long long>(cTime);
	high = static_cast<int>(t >> 32);
	low = static_cast<unsigned int>(t);
}

time_t PathTime::convertToCTime() const {
	long long t = (static_cast<long long>(high) << 32) | low;
	return static_cast<time_t>(t);
}

static std::wstring canonicalize(const std::wstring& in) {
	std::wstring result;

	bool isAbs = (!in.empty() && in[0] == L'/');
	if (isAbs) {
		result = L"/";
	} else {
		char buf[PATH_MAX];
		if (!::getcwd(buf, sizeof (buf))) {
			throw Exception("Error getting cwd");
		}
		result = fromUTF8(buf);
		if (result.empty() || result.back() != L'/') {
			result += L'/';
		}
	}

	std::wstring path = in;
	bool endsSlash = gotTrailingSlash(path);
	if (!endsSlash && path.size() >= 2) {
		if (path.substr(path.size() - 2) == L"/." ||
				(path.size() >= 3 && path.substr(path.size() - 3) == L"/..")) {
			endsSlash = true;
		}
	}
	if (isAbs && !path.empty()) {
		path = path.substr(1);
	}

	size_t pos = 0;
	std::vector<std::wstring> components;
	while (pos <= path.size()) {
		size_t slash = path.find(L'/', pos);
		std::wstring part = path.substr(pos, slash == std::wstring::npos
				? std::wstring::npos : slash - pos);
		if (!part.empty() && part != L".") {
			if (part == L"..") {
				if (!components.empty()) {
					components.pop_back();
				}
			} else {
				components.push_back(part);
			}
		}
		if (slash == std::wstring::npos) {
			break;
		}
		pos = slash + 1;
	}

	for (size_t i = 0; i < components.size(); ++i) {
		result += components[i];
		if (i + 1 < components.size()) {
			result += L'/';
		}
	}

	if (result.empty() || (endsSlash && result.back() != L'/')) {
		result += L'/';
	}
	return result;
}

std::string Exception::describe() const {
	if (descriptionUTF8.empty()) {
		std::ostringstream ss;
		ss << errorStringUTF8;
		if (!path.isNull()) {
			ss << " : " << toUTF8(path.getFullPath());
		}
		if (errorCode != 0) {
			ss << " [" << errorCode << ']';
		}
		descriptionUTF8 = ss.str();
	}
	return descriptionUTF8;
}

/* --- Path --- */

Path::Path(const std::wstring& pathString)
	: impl(new Impl(toUTF8(canonicalize(pathString)))) {
}

Path::~Path() {
	delete impl;
}

Path::Path(const Path& copy)
	: impl(copy.impl ? new Impl(*copy.impl) : 0) {
}

Path& Path::operator=(const Path& copy) {
	if (this != &copy) {
		delete impl;
		impl = copy.impl ? new Impl(*copy.impl) : 0;
	}
	return *this;
}

wchar_t Path::getSeparator() { return L'/'; }

std::wstring Path::appendSeparator(const std::wstring& path) { return appendSlash(path); }

std::wstring Path::removeSeparator(const std::wstring& path) { return removeSlash(path); }

bool Path::isValidChar(wchar_t c) { return (c >= 32); }

Path Path::getCurrentDirectoryPath() { return Path(L"./"); }

void Path::listRoots(std::vector<Path>& roots) { roots.push_back(Path(L"/")); }

bool Path::isRoot() const { return (!isNull() && impl->path == "/"); }

bool Path::isDirectoryPath() const { return (!isNull() && gotTrailingSlash(fromUTF8(impl->path))); }

int Path::compare(const Path& other) const {
	if (this == &other) {
		return 0;
	}
	if (impl == 0 || other.impl == 0) {
		return (impl ? 1 : 0) - (other.impl ? 1 : 0);
	}
	if (impl->path == other.impl->path) {
		return 0;
	}
	return (impl->path < other.impl->path) ? -1 : 1;
}

bool Path::equals(const Path& other) const { return compare(other) == 0; }

bool Path::operator==(const Path& other) const { return compare(other) == 0; }

Path Path::getParent() const {
	assert(!isNull());
	assert(!isRoot());
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t pos = p.find_last_of(L'/');
	if (pos == std::wstring::npos) return Path(L"/");
	return Path(p.substr(0, pos + 1));
}

Path Path::getRelative(const std::wstring& pathString) const {
	assert(!isNull());
	if (!pathString.empty() && pathString[0] == L'/') {
		return Path(pathString);
	} else {
		return Path(appendSlash(fromUTF8(impl->path)) + pathString);
	}
}

Path Path::withoutExtension() const {
	assert(!isNull());
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t slash = p.find_last_of(L'/');
	size_t dot = p.find_last_of(L'.');
	if (dot != std::wstring::npos && dot > slash) {
		p = p.substr(0, dot);
	}
	if (isDirectoryPath()) {
		p += L'/';
	}
	return Path(p);
}

Path Path::withExtension(const std::wstring& extensionString) const {
	assert(!isNull());
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t slash = p.find_last_of(L'/');
	size_t dot = p.find_last_of(L'.');
	if (dot != std::wstring::npos && dot > slash) {
		p = p.substr(0, dot);
	}
	if (!extensionString.empty() && extensionString[0] != L'.') {
		p += L'.';
	}
	p += extensionString;
	if (isDirectoryPath()) {
		p += L'/';
	}
	return Path(p);
}

bool Path::hasExtension() const {
	assert(!isNull());
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t slash = p.find_last_of(L'/');
	size_t dot = p.find_last_of(L'.');
	return (dot != std::wstring::npos && dot > slash + 1 && dot < p.length() - 1);
}

std::wstring Path::getName() const {
	assert(!isNull());
	if (isRoot()) {
		return std::wstring();
	}
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t slash = p.find_last_of(L'/');
	size_t dot = p.find_last_of(L'.');
	std::wstring name = p.substr(slash == std::wstring::npos ? 0 : slash + 1);
	if (dot != std::wstring::npos && dot > slash + 1 && dot < p.length() - 1) {
		name = name.substr(0, dot - (slash == std::wstring::npos ? 0 : slash + 1));
	}
	return name;
}

std::wstring Path::getExtension() const {
	assert(!isNull());
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t slash = p.find_last_of(L'/');
	size_t dot = p.find_last_of(L'.');
	if (dot != std::wstring::npos && dot > slash + 1 && dot < p.length() - 1) {
		return p.substr(dot + 1);
	}
	return std::wstring();
}

std::wstring Path::getNameWithExtension() const {
	assert(!isNull());
	if (isRoot()) {
		return std::wstring();
	}
	std::wstring p = removeSlash(fromUTF8(impl->path));
	size_t slash = p.find_last_of(L'/');
	std::wstring name = p.substr(slash == std::wstring::npos ? 0 : slash + 1);
	return name;
}

std::wstring Path::getFullPath() const { assert(!isNull()); return fromUTF8(impl->path); }

bool Path::exists() const {
	assert(!isNull());
	struct stat st;
	return (::stat(impl->path.c_str(), &st) == 0);
}

bool Path::isFile() const {
	assert(!isNull());
	struct stat st;
	return (::stat(impl->path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

bool Path::isDirectory() const {
	assert(!isNull());
	struct stat st;
	return (::stat(impl->path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

PathInfo Path::getInfo() const {
	assert(!isNull());
	struct stat st;
	if (::stat(impl->path.c_str(), &st) != 0) {
		throw Exception("Error stat", *this, errno);
	}
	PathInfo info;
	info.isDirectory = S_ISDIR(st.st_mode);
	info.fileSize = Int64(static_cast<int>(st.st_size >> 32), static_cast<unsigned int>(st.st_size));
	info.creationTime = PathTime(st.st_ctime);
	info.modificationTime = PathTime(st.st_mtime);
	info.lastAccessTime = PathTime(st.st_atime);
	info.attributes.isReadOnly = ((st.st_mode & S_IWUSR) == 0);
	return info;
}

void Path::create() const {
	assert(!isNull());
	if (::mkdir(impl->path.c_str(), 0777) != 0) {
		throw Exception("Error creating directory", *this, errno);
	}
}

bool Path::tryToCreate() const {
	assert(!isNull());
	return (::mkdir(impl->path.c_str(), 0777) == 0);
}

void Path::erase() const {
	assert(!isNull());
	if (isDirectory()) {
		if (::rmdir(impl->path.c_str()) != 0) {
			throw Exception("Error deleting directory", *this, errno);
		}
	} else {
		if (::unlink(impl->path.c_str()) != 0) {
			throw Exception("Error deleting file", *this, errno);
		}
	}
}

bool Path::tryToErase() const {
	assert(!isNull());
	if (isDirectory()) {
		return (::rmdir(impl->path.c_str()) == 0);
	} else {
		return (::unlink(impl->path.c_str()) == 0);
	}
}

void Path::moveRename(const Path& dst) const {
	assert(!isNull());
	if (::rename(impl->path.c_str(), dst.impl->path.c_str()) != 0) {
		throw Exception("Error renaming", *this, errno);
	}
}

void Path::copy(const Path& dst) const {
	assert(!isNull());
	int infd = ::open(impl->path.c_str(), O_RDONLY);
	if (infd < 0) {
		throw Exception("Error opening source", *this, errno);
	}
	int outfd = ::open(dst.impl->path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (outfd < 0) {
		int err = errno;
		::close(infd);
		throw Exception("Error creating dest", dst, err);
	}
	char buf[8192];
	ssize_t r;
	while ((r = ::read(infd, buf, sizeof (buf))) > 0) {
		ssize_t w = ::write(outfd, buf, r);
		if (w != r) {
			int err = errno;
			::close(infd);
			::close(outfd);
			throw Exception("Error copying", dst, err);
		}
	}
	if (r < 0) {
		int err = errno;
		::close(infd);
		::close(outfd);
		throw Exception("Error copying", *this, err);
	}
	::close(infd);
	::close(outfd);
}

Path Path::createTempFile() const {
	assert(!isNull());
	std::wstring dir = isDirectoryPath() ? fromUTF8(impl->path) : fromUTF8(getParent().impl->path);
	std::string utf8dir = toUTF8(dir);
	std::string tmpl = utf8dir + "tmpXXXXXX";
	std::vector<char> buf(tmpl.begin(), tmpl.end());
	buf.push_back('\0');
	int fd = ::mkstemp(&buf[0]);
	if (fd < 0) {
		throw Exception("Error creating temp", *this, errno);
	}
	::close(fd);
	return Path(fromUTF8(&buf[0]));
}

bool Path::matchesFilter(const PathListFilter& filter) const {
	assert(!isNull());
	if (filter.excludeHidden) {
		std::wstring n = getNameWithExtension();
		if (!n.empty() && n[0] == L'.') return false;
	}
	bool isDir = isDirectory();
	if (filter.excludeFiles && !isDir) return false;
	if (filter.excludeDirectories && isDir) return false;
	if (!filter.includeExtension.empty() && !isDir) {
		if (getExtension() != filter.includeExtension) return false;
	}
	return true;
}

void Path::listSubPaths(std::vector<Path>& subPaths, const PathListFilter& filter) const {
	assert(!isNull());
	DIR* dir = ::opendir(impl->path.c_str());
	if (!dir) throw Exception("Error listing file directory", *this, errno);
	struct dirent* ent;
	while ((ent = ::readdir(dir)) != 0) {
		std::string name(ent->d_name);
		if (name == "." || name == "..") continue;
		Path child = getRelative(fromUTF8(name));
		if (child.matchesFilter(filter)) subPaths.push_back(child);
	}
	::closedir(dir);
}

void Path::findPaths(std::vector<Path>& paths, const std::wstring& pattern, const PathListFilter& filter) {
	std::string utf8 = toUTF8(pattern);
	glob_t g;
	int r = ::glob(utf8.c_str(), 0, 0, &g);
	if (r == 0) {
		for (size_t i = 0; i < g.gl_pathc; ++i) {
			std::string p(g.gl_pathv[i]);
			struct stat st;
			if (::stat(p.c_str(), &st) != 0) continue;
			std::wstring w = fromUTF8(p);
			if (S_ISDIR(st.st_mode) && !w.empty() && w.back() != L'/') w += L'/';
			Path path(w);
			if (path.matchesFilter(filter)) paths.push_back(path);
		}
	}
	globfree(&g);
}

/* --- ReadOnlyFile --- */

ReadOnlyFile::Impl::~Impl() { ::close(fileDescriptor); }

ReadOnlyFile::ReadOnlyFile(const Path& path, bool allowConcurrentWrites)
	   : impl(0)
{
	(void)allowConcurrentWrites;
	const int fd = ::open(path.getImpl()->getPosixPath().c_str(), O_RDONLY);
	if (fd < 0) {
		throw Exception("Error opening file", path, errno);
	}
	impl = new Impl(path, fd);
}

Int64 ReadOnlyFile::getSize() const {
	struct stat st;
	if (::fstat(impl->fileDescriptor, &st) != 0) {
		throw Exception("Error stat", getPath(), errno);
	}
	return Int64(static_cast<int>(st.st_size >> 32), static_cast<unsigned int>(st.st_size));
}

int ReadOnlyFile::tryToRead(Int64 pos, int count, unsigned char* bytes) const {
	off_t offset = (static_cast<off_t>(pos.getHigh()) << 32) | pos.getLow();
	ssize_t r = ::pread(impl->fileDescriptor, bytes, count, offset);
	if (r < 0) {
		throw Exception("Error reading", getPath(), errno);
	}
	return static_cast<int>(r);
}

void ReadOnlyFile::read(Int64 pos, int count, unsigned char* bytes) const {
	if (tryToRead(pos, count, bytes) != count) {
		throw Exception("Error reading", getPath(), EOVERFLOW);
	}
}

Path ReadOnlyFile::getPath() const { return impl->path; }

ReadOnlyFile::~ReadOnlyFile() { delete impl; }

/* --- ReadWriteFile --- */

ReadWriteFile::ReadWriteFile(const Path& path, bool allowConcurrentReads, bool allowConcurrentWrites)
	   : ReadOnlyFile(static_cast<Impl*>(0))
{
	(void)allowConcurrentReads;
	(void)allowConcurrentWrites;
	const int fd = ::open(path.getImpl()->getPosixPath().c_str(), O_RDWR);
	if (fd < 0) {
		throw Exception("Error opening file", path, errno);
	}
	impl = new Impl(path, fd);
}

ReadWriteFile::ReadWriteFile(const Path& path, const PathAttributes&, bool replaceExisting, bool allowReads, bool allowWrites)
	   : ReadOnlyFile(static_cast<Impl*>(0))
{
	(void)allowReads;
	(void)allowWrites;
	const int fd = ::open(path.getImpl()->getPosixPath().c_str(), O_RDWR | O_CREAT | (replaceExisting ? O_TRUNC : O_EXCL), 0666);
	if (fd < 0) {
		throw Exception("Error creating file", path, errno);
	}
	impl = new Impl(path, fd);
}

void ReadWriteFile::write(Int64 pos, int count, const unsigned char* bytes) {
	off_t offset = (static_cast<off_t>(pos.getHigh()) << 32) | pos.getLow();
	ssize_t w = ::pwrite(impl->fileDescriptor, bytes, count, offset);
	if (w != count) {
		throw Exception("Error writing", getPath(), errno);
	}
}

void ReadWriteFile::flush() { ::fsync(impl->fileDescriptor); }

/* --- ExchangingFile --- */

ExchangingFile::ExchangingFile(const Path& path, const PathAttributes& attrs)
		: ReadWriteFile(path.createTempFile(), attrs, true, false, false), originalPath(path) { }

void ExchangingFile::commit() {
	if (!originalPath.isNull()) {
		flush();
		Path temp = getPath();
		delete impl;
		impl = 0;
		if (::rename(temp.getImpl()->getPosixPath().c_str(), originalPath.getImpl()->getPosixPath().c_str()) != 0) {
			throw Exception("Error committing", originalPath, errno);
		}
		originalPath = Path();
	}
}

ExchangingFile::~ExchangingFile() {
	if (!originalPath.isNull()) {
		Path temp = getPath();
		delete impl;
		impl = 0;
		::unlink(temp.getImpl()->getPosixPath().c_str());
	}
}

} // namespace NuXFiles


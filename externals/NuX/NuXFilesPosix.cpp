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
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glob.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "NuXFilesPosix.h"

namespace NuXFiles {

/*
	UTF-8 <-> UTF-32 conversion, hand-rolled because `std::wstring_convert` and `std::codecvt_utf8` are C++11
	only and are removed in C++26, and this file is also compiled as C++03. Both directions accept only Unicode scalar
	values, so overlong forms, surrogates and code points above U+10FFFF are rejected. `wchar_t` is UTF-32 here, unlike
	the UTF-16 Win32 backend.
*/
typedef unsigned int UniChar;
typedef char WCharIsUTF32Assertion[sizeof (wchar_t) >= 4 ? 1 : -1];

// Paths from `getcwd`, `readdir` and `glob` are not guaranteed well-formed, so this returns false instead of assuming.
static bool tryFromUTF8(const std::string& s, std::wstring& result) {
	result.clear();
	result.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		const UniChar lead = static_cast<unsigned char>(s[i]);
		// The number of continuation bytes, and the smallest code point that needs them (anything less is overlong).
		size_t count;
		UniChar smallest;
		UniChar c;
		if (lead < 0x80) {
			count = 0; smallest = 0; c = lead;
		} else if ((lead & 0xE0) == 0xC0) {
			count = 1; smallest = 0x80; c = lead & 0x1F;
		} else if ((lead & 0xF0) == 0xE0) {
			count = 2; smallest = 0x800; c = lead & 0x0F;
		} else if ((lead & 0xF8) == 0xF0) {
			count = 3; smallest = 0x10000; c = lead & 0x07;
		} else {
			return false;
		}
		if (s.size() - i <= count) {
			return false;
		}
		for (size_t j = 1; j <= count; ++j) {
			const UniChar b = static_cast<unsigned char>(s[i + j]);
			if ((b & 0xC0) != 0x80) {
				return false;
			}
			c = (c << 6) | (b & 0x3F);
		}
		if (c < smallest || (c >= 0xD800 && c < 0xE000) || c > 0x10FFFF) {
			return false;
		}
		result += static_cast<wchar_t>(c);
		i += count + 1;
	}
	return true;
}

static std::wstring fromUTF8(const std::string& s) {
	std::wstring result;
	if (!tryFromUTF8(s, result)) {
		throw Exception("Invalid UTF-8 in path");
	}
	return result;
}

// Wide strings may come straight from a caller, so the code points are validated here too.
static std::string toUTF8(const std::wstring& s) {
	static const unsigned char LEAD_BITS[4] = { 0x00, 0xC0, 0xE0, 0xF0 };
	std::string result;
	result.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		const UniChar c = static_cast<UniChar>(s[i]);
		if ((c >= 0xD800 && c < 0xE000) || c > 0x10FFFF) {
			throw Exception("Invalid Unicode code point in path");
		}
		const int count = (c < 0x80 ? 0 : (c < 0x800 ? 1 : (c < 0x10000 ? 2 : 3)));
		result += static_cast<char>(LEAD_BITS[count] | (c >> (6 * count)));
		for (int shift = 6 * (count - 1); shift >= 0; shift -= 6) {
			result += static_cast<char>(0x80 | ((c >> shift) & 0x3F));
		}
	}
	return result;
}

static bool gotTrailingSlash(const std::wstring& p) {
	return (!p.empty() && p[p.size() - 1] == L'/');
}

static std::wstring appendSlash(const std::wstring& p) {
	return gotTrailingSlash(p) ? p : p + L'/';
}

static std::wstring removeSlash(const std::wstring& p) {
	return gotTrailingSlash(p) ? std::wstring(p.begin(), p.end() - 1) : p;
}

/*
	Returns the position of the dot that starts the extension of `p` (a path without a trailing slash), or npos. A dot
	starts an extension only if it is neither the first nor the last character of the name, so ".file" and "file." have
	none. All the extension functions below share this rule.
*/
static const mode_t WRITE_BITS = (S_IWUSR | S_IWGRP | S_IWOTH);

// A leading dot hides a name, for getInfo() and matchesFilter() alike.
static bool isHiddenName(const std::wstring& name) { return (!name.empty() && name[0] == L'.'); }

static size_t findExtensionDot(const std::wstring& p) {
	const size_t slash = p.find_last_of(L'/');
	const size_t nameStart = (slash == std::wstring::npos ? 0 : slash + 1);
	const size_t dot = p.find_last_of(L'.');
	return ((dot != std::wstring::npos && dot > nameStart && dot + 1 < p.size()) ? dot : std::wstring::npos);
}

/*
	A system call can fail with EINTR when a signal arrives, and a read or write can transfer fewer bytes than asked
	(network and FUSE file systems in particular). These wrappers retry and loop, so callers see one result.
*/
static int openRetrying(const char* path, int flags, mode_t mode = 0) {
	int fd;
	do {
		fd = ::open(path, flags, mode);
	} while (fd < 0 && errno == EINTR);
	return fd;
}

// Returns the number of bytes read, which is less than `count` only at the end of the file, or -1 with errno set.
static ssize_t readFully(int fd, off_t offset, size_t count, unsigned char* bytes) {
	size_t done = 0;
	while (done < count) {
		const ssize_t r = ::pread(fd, bytes + done, count - done, offset + static_cast<off_t>(done));
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		if (r == 0) {
			break;
		}
		done += static_cast<size_t>(r);
	}
	return static_cast<ssize_t>(done);
}

// Returns true once all `count` bytes are written, false with errno set otherwise.
static bool writeFully(int fd, off_t offset, size_t count, const unsigned char* bytes) {
	size_t done = 0;
	while (done < count) {
		const ssize_t w = ::pwrite(fd, bytes + done, count - done, offset + static_cast<off_t>(done));
		if (w < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		if (w == 0) {
			errno = EIO; // No progress and no error: fail rather than spin.
			return false;
		}
		done += static_cast<size_t>(w);
	}
	return true;
}

/* --- PathTime --- */

PathTime::PathTime(time_t cTime) {
	long long t = static_cast<long long>(cTime);
	high = static_cast<int>(t >> 32);
	low = static_cast<unsigned int>(t);
}

time_t PathTime::convertToCTime() const {
	// Rebuilt through an unsigned type since left-shifting a negative `high` (any pre-1970 time) is undefined.
	const unsigned long long t = (static_cast<unsigned long long>(static_cast<unsigned int>(high)) << 32) | low;
	return static_cast<time_t>(t);
}

static std::wstring canonicalize(const std::wstring& in) {
	// A relative path is appended to the current directory before any '..' is applied, so '..' can climb out of it.
	std::wstring path = in;
	if (path.empty() || path[0] != L'/') {
		char buf[PATH_MAX];
		if (!::getcwd(buf, sizeof (buf))) {
			throw Exception("Error getting cwd", Path(), errno);
		}
		path = appendSlash(fromUTF8(buf)) + path;
	}
	std::wstring result = L"/";

	bool endsSlash = gotTrailingSlash(path);
	if (!endsSlash && path.size() >= 2) {
		if (path.substr(path.size() - 2) == L"/." ||
				(path.size() >= 3 && path.substr(path.size() - 3) == L"/..")) {
			endsSlash = true;
		}
	}
	path = path.substr(1);

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

	if (result.empty() || (endsSlash && result[result.size() - 1] != L'/')) {
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
	const size_t dot = findExtensionDot(p);
	if (dot != std::wstring::npos) {
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
	const size_t dot = findExtensionDot(p);
	if (dot != std::wstring::npos) {
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
	return (findExtensionDot(removeSlash(fromUTF8(impl->path))) != std::wstring::npos);
}

std::wstring Path::getName() const {
	assert(!isNull());
	if (isRoot()) {
		return std::wstring();
	}
	const std::wstring p = removeSlash(fromUTF8(impl->path));
	const size_t slash = p.find_last_of(L'/');
	const size_t nameStart = (slash == std::wstring::npos ? 0 : slash + 1);
	const size_t dot = findExtensionDot(p);
	return p.substr(nameStart, (dot == std::wstring::npos ? std::wstring::npos : dot - nameStart));
}

std::wstring Path::getExtension() const {
	assert(!isNull());
	const std::wstring p = removeSlash(fromUTF8(impl->path));
	const size_t dot = findExtensionDot(p);
	return (dot == std::wstring::npos ? std::wstring() : p.substr(dot + 1));
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
	/*
		st_ctime is the inode change time, not the creation time. Linux reports the real birth time through statx() on
		file systems that record it; otherwise creationTime stays "not available".
	*/
#if defined(__linux__) && defined(STATX_BTIME)
	struct statx sx;
	if (::statx(AT_FDCWD, impl->path.c_str(), 0, STATX_BTIME, &sx) == 0 && (sx.stx_mask & STATX_BTIME) != 0) {
		info.creationTime = PathTime(static_cast<time_t>(sx.stx_btime.tv_sec));
	}
#endif
	info.modificationTime = PathTime(st.st_mtime);
	info.lastAccessTime = PathTime(st.st_atime);
	info.attributes.isReadOnly = ((st.st_mode & S_IWUSR) == 0);
	info.attributes.isHidden = isHiddenName(getNameWithExtension());
	return info;
}

// Only `isReadOnly` applies here. It mirrors getInfo(): read-only clears every write bit, writable sets the owner's.
void Path::updateAttributes(const PathAttributes& newAttributes) const {
	assert(!isNull());
	struct stat st;
	if (::stat(impl->path.c_str(), &st) != 0) {
		throw Exception("Error updating attributes on file or directory", *this, errno);
	}
	const mode_t mode = (newAttributes.isReadOnly ? (st.st_mode & ~WRITE_BITS) : (st.st_mode | S_IWUSR));
	if (::chmod(impl->path.c_str(), mode & 07777) != 0) {
		throw Exception("Error updating attributes on file or directory", *this, errno);
	}
}

// POSIX has no settable creation time, so `newCreationTime` is ignored.
void Path::updateTimes(const PathTime& newCreationTime, const PathTime& newModificationTime
		, const PathTime& newAccessTime) const {
	assert(!isNull());
	(void)newCreationTime;
	struct timespec times[2];
	const PathTime* const newTimes[2] = { &newAccessTime, &newModificationTime };
	for (int i = 0; i < 2; ++i) {
		times[i].tv_sec = (newTimes[i]->isAvailable() ? newTimes[i]->convertToCTime() : 0);
		times[i].tv_nsec = (newTimes[i]->isAvailable() ? 0 : UTIME_OMIT);
	}
	if (::utimensat(AT_FDCWD, impl->path.c_str(), times, 0) != 0) {
		throw Exception("Error updating time info on file or directory", *this, errno);
	}
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

/*
	Whether erase() removes `path` with rmdir() rather than unlink(). Decided with lstat() on the path without a trailing
	slash (which would make even lstat() follow a link), so a symlink to a directory is removed itself and its target is
	untouched, as on Win32 and Cocoa.
*/
static bool isRealDirectory(const std::string& path) {
	struct stat st;
	return (::lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

static std::string withoutTrailingSlash(const std::string& path) {
	return ((path.size() > 1 && path[path.size() - 1] == '/') ? path.substr(0, path.size() - 1) : path);
}

void Path::erase() const {
	assert(!isNull());
	const std::string path = withoutTrailingSlash(impl->path);
	if (isRealDirectory(path)) {
		if (::rmdir(path.c_str()) != 0) {
			throw Exception("Error deleting directory", *this, errno);
		}
	} else {
		if (::unlink(path.c_str()) != 0) {
			throw Exception("Error deleting file", *this, errno);
		}
	}
}

bool Path::tryToErase() const {
	try {
		erase();
		return true;
	}
	catch (const Exception&) {
		return false;
	}
}

/*
	moveRename() stays within one file system, as rename() does (EXDEV otherwise; nothing is copied), and refuses an
	existing destination as Win32 and Cocoa do, although rename() would silently replace it. Linux can refuse atomically
	with renameat2(RENAME_NOREPLACE); elsewhere, or where the file system lacks it, the destination is checked first (not
	atomic: something created there in between would still be replaced).
*/
void Path::moveRename(const Path& dst) const {
	assert(!isNull());
	const char* const from = impl->path.c_str();
	const char* const to = dst.impl->path.c_str();
#if defined(__linux__) && defined(RENAME_NOREPLACE)
	if (::renameat2(AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE) == 0) {
		return;
	}
	if (errno != EINVAL && errno != ENOSYS) {
		throw Exception("Error renaming", *this, errno);
	}
#endif
	struct stat st;
	if (::lstat(to, &st) == 0) {
		throw Exception("Error renaming", dst, EEXIST);
	}
	if (::rename(from, to) != 0) {
		throw Exception("Error renaming", *this, errno);
	}
}

void Path::copy(const Path& dst) const {
	assert(!isNull());
	const int infd = openRetrying(impl->path.c_str(), O_RDONLY);
	if (infd < 0) {
		throw Exception("Error opening source", *this, errno);
	}
	const int outfd = openRetrying(dst.impl->path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666); // O_EXCL: must not replace a file.
	if (outfd < 0) {
		const int error = errno;
		::close(infd);
		throw Exception("Error creating dest", dst, error);
	}
	unsigned char buf[8192];
	off_t offset = 0;
	int error = 0;
	const Path* failedPath = this;
	for (;;) {
		const ssize_t r = readFully(infd, offset, sizeof (buf), buf);
		if (r < 0) {
			error = errno;
			break;
		}
		if (r == 0) {
			break;
		}
		if (!writeFully(outfd, offset, static_cast<size_t>(r), buf)) {
			error = errno;
			failedPath = &dst;
			break;
		}
		offset += r;
	}
	::close(infd);
	if (::close(outfd) != 0 && error == 0) { // Network file systems may only report a write error here.
		error = errno;
		failedPath = &dst;
	}
	if (error != 0) {
		::unlink(dst.impl->path.c_str()); // Don't leave a partial copy behind.
		throw Exception("Error copying", *failedPath, error);
	}
}

Path Path::createTempFile() const {
	assert(!isNull());
	/*
		Created with O_EXCL and mode 0666, so the umask applies as for any other new file (mkstemp() forces 0600, which
		ExchangingFile would then give every file it saves). O_EXCL also makes creation safe against name clashes and
		symlinks, so the name only has to be unlikely to exist, not unguessable.
	*/
	const std::string directory = (isDirectoryPath() ? impl->path : getParent().impl->path);
	unsigned int seed = (static_cast<unsigned int>(::getpid()) * 2654435761u) ^ static_cast<unsigned int>(::time(0))
			^ static_cast<unsigned int>(reinterpret_cast<size_t>(&seed)); // The stack address differs per thread.
	static const char DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	for (int attempt = 0; attempt < 1000; ++attempt) {
		std::string name = directory + "tmp";
		for (int i = 0; i < 6; ++i) {
			seed = seed * 1103515245u + 12345u;
			name += DIGITS[(seed >> 16) % 36];
		}
		const int fd = openRetrying(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0666);
		if (fd >= 0) {
			::close(fd);
			return Path(fromUTF8(name));
		}
		if (errno != EEXIST) {
			throw Exception("Error creating temp", *this, errno);
		}
	}
	throw Exception("Error creating temp", *this, EEXIST);
}

// Extensions name a file type, so they match regardless of case, as on Win32 and Cocoa. Only ASCII is folded, which
// keeps the result independent of the locale.
static bool equalsIgnoringASCIICase(const std::wstring& a, const std::wstring& b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		const wchar_t x = ((a[i] >= L'A' && a[i] <= L'Z') ? a[i] + (L'a' - L'A') : a[i]);
		const wchar_t y = ((b[i] >= L'A' && b[i] <= L'Z') ? b[i] + (L'a' - L'A') : b[i]);
		if (x != y) {
			return false;
		}
	}
	return true;
}

bool Path::matchesFilter(const PathListFilter& filter) const {
	assert(!isNull());
	if (filter.excludeHidden && isHiddenName(getNameWithExtension())) return false;
	struct stat st;
	if (::stat(impl->path.c_str(), &st) != 0) {
		return false; // A path that doesn't exist (or can't be examined) matches nothing, as on Win32.
	}
	const bool isDir = S_ISDIR(st.st_mode);
	if (filter.excludeFiles && !isDir) return false;
	if (filter.excludeDirectories && isDir) return false;
	// The extension filter applies to directories too, as documented and as on Win32 and Cocoa.
	if (!filter.includeExtension.empty() && !equalsIgnoringASCIICase(getExtension(), filter.includeExtension)) {
		return false;
	}
	return true;
}

// Closes a directory stream when it goes out of scope, so a throw while listing cannot leak it.
class DirectoryStreamCloser {
	public:		DirectoryStreamCloser(DIR* dir) : dir(dir) { }
	public:		~DirectoryStreamCloser() { ::closedir(dir); }
	private:	DIR* const dir;
	private:	DirectoryStreamCloser(const DirectoryStreamCloser& copy); // N/A
	private:	DirectoryStreamCloser& operator=(const DirectoryStreamCloser& copy); // N/A
};

// Frees a glob() result when it goes out of scope, so a throw while walking it cannot leak it.
class GlobResultFreer {
	public:		GlobResultFreer(glob_t* result) : result(result) { }
	public:		~GlobResultFreer() { ::globfree(result); }
	private:	glob_t* const result;
	private:	GlobResultFreer(const GlobResultFreer& copy); // N/A
	private:	GlobResultFreer& operator=(const GlobResultFreer& copy); // N/A
};

void Path::listSubPaths(std::vector<Path>& subPaths, const PathListFilter& filter) const {
	assert(!isNull());
	DIR* dir = ::opendir(impl->path.c_str());
	if (!dir) throw Exception("Error listing file directory", *this, errno);
	DirectoryStreamCloser closer(dir);
	struct dirent* ent;
	while ((ent = ::readdir(dir)) != 0) {
		std::string name(ent->d_name);
		if (name == "." || name == "..") continue;
		// A name that isn't valid UTF-8 (legal on Linux) cannot become a Path, so it is skipped rather than failing the
		// whole listing.
		std::wstring w;
		if (!tryFromUTF8(name, w)) continue;
		// Directory paths end with a slash, as from findPaths().
		struct stat st;
		const std::string entryPath = withoutTrailingSlash(impl->path) + '/' + name;
		const bool isDirectory = (::stat(entryPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
		Path child = getRelative(isDirectory ? w + L'/' : w);
		if (child.matchesFilter(filter)) subPaths.push_back(child);
	}
}

void Path::findPaths(std::vector<Path>& paths, const std::wstring& pattern, const PathListFilter& filter) {
	std::string utf8 = toUTF8(pattern);
	glob_t g;
	int r = ::glob(utf8.c_str(), GLOB_MARK, 0, &g); // GLOB_MARK ends directories (and links to them) with a slash.
	GlobResultFreer freer(&g);
	if (r == 0) {
		for (size_t i = 0; i < g.gl_pathc; ++i) {
			std::wstring w;
			if (!tryFromUTF8(g.gl_pathv[i], w)) continue; // Cannot become a Path; skipped, as in listSubPaths().
			Path path(w);
			if (path.matchesFilter(filter)) paths.push_back(path);
		}
	}
}

/* --- ReadOnlyFile --- */

ReadOnlyFile::Impl::~Impl() {
	if (fileDescriptor >= 0) {
		::close(fileDescriptor);
	}
}

ReadOnlyFile::ReadOnlyFile(const Path& path, bool allowConcurrentWrites)
		: impl(0)
{
	(void)allowConcurrentWrites;
	const int fd = openRetrying(path.getImpl()->getPosixPath().c_str(), O_RDONLY);
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
	assert(count >= 0);
	off_t offset = (static_cast<off_t>(pos.getHigh()) << 32) | pos.getLow();
	const ssize_t r = readFully(impl->fileDescriptor, offset, static_cast<size_t>(count), bytes);
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
	const int fd = openRetrying(path.getImpl()->getPosixPath().c_str(), O_RDWR);
	if (fd < 0) {
		throw Exception("Error opening file", path, errno);
	}
	impl = new Impl(path, fd);
}

// Only `isReadOnly` applies here (see updateAttributes()). It is set through the open descriptor, which stays writable.
ReadWriteFile::ReadWriteFile(const Path& path, const PathAttributes& attributes, bool replaceExisting, bool allowReads
		, bool allowWrites)
		: ReadOnlyFile(static_cast<Impl*>(0))
{
	(void)allowReads;
	(void)allowWrites;
	const int fd = openRetrying(path.getImpl()->getPosixPath().c_str(), O_RDWR | O_CREAT | (replaceExisting ? O_TRUNC : O_EXCL)
			, 0666);
	if (fd < 0) {
		throw Exception("Error creating file", path, errno);
	}
	impl = new Impl(path, fd);
	if (attributes.isReadOnly) {
		struct stat st;
		if (::fstat(fd, &st) != 0 || ::fchmod(fd, st.st_mode & 07777 & ~WRITE_BITS) != 0) {
			const int error = errno;
			delete impl;
			impl = 0;
			throw Exception("Error setting attributes on file", path, error);
		}
	}
}

void ReadWriteFile::write(Int64 pos, int count, const unsigned char* bytes) {
	assert(count >= 0);
	off_t offset = (static_cast<off_t>(pos.getHigh()) << 32) | pos.getLow();
	if (!writeFully(impl->fileDescriptor, offset, static_cast<size_t>(count), bytes)) {
		throw Exception("Error writing", getPath(), errno);
	}
}

void ReadWriteFile::flush() {
	if (::fsync(impl->fileDescriptor) != 0) {
		throw Exception("Error flushing file", getPath(), errno);
	}
}

/* --- ExchangingFile --- */

/*
	Saving over a file keeps its permissions, as Win32's ReplaceFileW and Cocoa do. This is best effort: some file
	systems (FAT, SMB mounts) refuse fchmod(), and that should not stop a save.
*/
ExchangingFile::ExchangingFile(const Path& path, const PathAttributes& attrs)
		: ReadWriteFile(path.createTempFile(), attrs, true, false, false), originalPath(path) {
	struct stat st;
	if (::stat(path.getImpl()->getPosixPath().c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
		::fchmod(impl->fileDescriptor, st.st_mode & 07777 & (attrs.isReadOnly ? ~WRITE_BITS : ~0));
	}
}

/*
	A write error can be reported as late as fsync() or close() (a full disk, NFS, delayed allocation), so both are
	checked before the rename. On any failure the original is left untouched and the temp file is removed.
*/
void ExchangingFile::commit() {
	if (!originalPath.isNull()) {
		Path temp = getPath();
		Path original = originalPath;
		originalPath = Path(); // Clear before deleting `impl` so the destructor won't dereference a null `impl`.
		int error = (::fsync(impl->fileDescriptor) != 0 ? errno : 0);
		if (::close(impl->fileDescriptor) != 0 && error == 0) {
			error = errno;
		}
		impl->fileDescriptor = -1; // Closed (even if close() failed), so ~Impl must not close it again.
		delete impl;
		impl = 0;
		if (error == 0
				&& ::rename(temp.getImpl()->getPosixPath().c_str(), original.getImpl()->getPosixPath().c_str()) != 0) {
			error = errno;
		}
		if (error != 0) {
			::unlink(temp.getImpl()->getPosixPath().c_str()); // Don't leave the temp file behind on a failed commit.
			throw Exception("Error committing", original, error);
		}
		// Make the rename itself durable. Best effort: the commit has already succeeded, so a failure here is not reported.
		const int directory = ::open(original.getParent().getImpl()->getPosixPath().c_str(), O_RDONLY);
		if (directory >= 0) {
			::fsync(directory);
			::close(directory);
		}
		// The file stays readable after commit() (see NuXFiles.h), now as the committed original.
		const int fd = openRetrying(original.getImpl()->getPosixPath().c_str(), O_RDONLY);
		if (fd < 0) {
			throw Exception("Error opening file", original, errno);
		}
		impl = new Impl(original, fd);
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


#include "NuXFilesWin32.h"
#include <sstream>

namespace NuXFiles {

/* --- Functions --- */

template<typename T, typename U> T lossless_cast(U x) { assert(static_cast<T>(x) == x); return static_cast<T>(x); }

static std::string convertToUTF8String(const std::wstring& w) {
	const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.data(), lossless_cast<int>(w.size()), NULL, 0, NULL, NULL);
	assert(sizeNeeded > 0);
	std::string utf8(sizeNeeded, 0);
	const int result = WideCharToMultiByte(CP_UTF8, 0, w.data(), lossless_cast<int>(w.size()), &utf8[0], sizeNeeded, NULL, NULL);
	assert(result == sizeNeeded);
	return utf8;
}

static bool gotTrailingBackslash(const std::wstring& source)
{
	return (!source.empty() && (source[source.size() - 1] == L'\\' || source[source.size() - 1] == L'/'));
}

static std::wstring stripTrailingBackslash(std::wstring source)
{
	while (gotTrailingBackslash(source)) source = source.substr(0, source.size() - 1);
	return source;
}

static std::wstring addTrailingBackslash(const std::wstring& source)
{
	return (gotTrailingBackslash(source) ? source : source + L'\\');
}

static bool extensionMatches(const std::wstring& nameWithExtension, const std::wstring& extension)
{
	const wchar_t* s = nameWithExtension.data();
	const wchar_t* e = s + nameWithExtension.size();
	const wchar_t* p = e;
	while (p > s && *(p - 1) != L'.') --p;
	return (::CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, p, lossless_cast<int>(e - p), extension.data()
			, lossless_cast<int>(extension.size())) == CSTR_EQUAL);
}

bool Path::matchesFilter(const PathListFilter& filter) const {
	assert(!isNull());

	const ::DWORD fileAttributes = ::GetFileAttributesW(getFullPath().c_str());
	const bool isDirectory = ((fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

	if ((!filter.excludeFiles || isDirectory)
			&& (!filter.excludeDirectories || !isDirectory)
			&& (!filter.excludeHidden || ((fileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0))) {
		if (filter.includeExtension.empty()) {
			return true;
		}
		const std::wstring myExtension = getExtension();
		if (::CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE
				, myExtension.data(), lossless_cast<int>(myExtension.size())
				, filter.includeExtension.data(), lossless_cast<int>(filter.includeExtension.size()))
				== CSTR_EQUAL) {
			return true;
		}
	}
	return false;
}

static void appendPaths(const Path& parent, std::vector<Path>& paths, const std::wstring& wildcardPattern
		, const PathListFilter& filter)
{
	::WIN32_FIND_DATAW findFileData;
	::HANDLE findFirstHandle = ::FindFirstFileW(wildcardPattern.c_str(), &findFileData);
	if (findFirstHandle == INVALID_HANDLE_VALUE) {
		::DWORD lastError = ::GetLastError();
		if (lastError != ERROR_FILE_NOT_FOUND) {
			throw Exception("Error listing file directory", parent, lastError);
		}
	} else {
		try {
			while (true) {
				bool isDirectory = ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
				std::wstring name(findFileData.cFileName);

				if (name == L"."
					|| name == L".."
					|| (filter.excludeFiles && !isDirectory)
					|| (filter.excludeDirectories && isDirectory)
					|| (filter.excludeHidden && ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0))
					|| (!filter.includeExtension.empty() && !extensionMatches(name, filter.includeExtension))) {
					;
				} else {
					paths.push_back(parent.getRelative(isDirectory ? (name + L'\\') : name));
				}

				::BOOL findNextFileReturn = ::FindNextFileW(findFirstHandle, &findFileData);
				if (!findNextFileReturn) {
					::DWORD lastError = ::GetLastError();
					if (lastError != ERROR_NO_MORE_FILES) {
						throw Exception("Error listing file directory", parent, lastError);
					}
					break;
				}
			}
		}
		catch (...) {
			::BOOL findCloseReturn = ::FindClose(findFirstHandle);
			assert(findCloseReturn);
			throw;
		}
		::BOOL findCloseReturn = ::FindClose(findFirstHandle);
		assert(findCloseReturn);
	}
}

static ::DWORD calcFileAttributesBits(const PathAttributes& attributes)
{
	::DWORD bits = attributes.win32Attributes;
	if (attributes.isReadOnly) {
		bits |= FILE_ATTRIBUTE_READONLY;
	} else {
		bits &= ~FILE_ATTRIBUTE_READONLY;
	}
	if (attributes.isHidden) {
		bits |= FILE_ATTRIBUTE_HIDDEN;
	} else {
		bits &= ~FILE_ATTRIBUTE_HIDDEN;
	}
	return bits;
}

static ::FILETIME* convertPathTime(const PathTime& pathTime, ::FILETIME* buffer)
{
	if (!pathTime.isAvailable()) {
		return 0;
	} else {
		(*buffer).dwLowDateTime = pathTime.getLow();
		(*buffer).dwHighDateTime = pathTime.getHigh();
		return buffer;
	}
}

static std::wstring convertToAbsolutePath(const std::wstring& path)
{
	wchar_t* filePart;
	::DWORD getFullPathNameReturn = ::GetFullPathNameW(path.c_str(), 0, 0, &filePart);
	if (getFullPathNameReturn == 0) {
		throw Exception(std::string("Error interpreting path : ") + convertToUTF8String(path), Path()
				, ::GetLastError());
	}
	std::wstring absolute(getFullPathNameReturn - 1, '?');
	::DWORD getFullPathNameReturnAgain = ::GetFullPathNameW(path.c_str(), lossless_cast<int>(absolute.size() + 1)
			, &absolute[0], &filePart);
	if (getFullPathNameReturnAgain == 0 || getFullPathNameReturnAgain >= getFullPathNameReturn) {
		assert(0);
		throw Exception(std::string("Error interpreting path : ") + convertToUTF8String(path), Path()
				, ::GetLastError());
	}
	if (getFullPathNameReturnAgain < absolute.size()) {
		absolute = absolute.substr(0, getFullPathNameReturnAgain);
	}
	assert(path.size() < 4 || path.substr(0, 4) != L"\\\\?\\");
	return absolute;
}

/* --- Exception --- */

std::string Exception::describe() const
{
	if (descriptionUTF8.empty()) {
		std::wostringstream message;
		if (!path.isNull()) {
			message << L" : " << path.getFullPath();
		}
		if (errorCode != 0) {
			std::vector<wchar_t> messageBuffer(4096);
			::DWORD formatMessageReturn = ::FormatMessageW
					( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode
					, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), messageBuffer.data()
					, sizeof (messageBuffer) - 1, NULL);
			if (formatMessageReturn != 0) {
				size_t length = wcslen(messageBuffer.data());
				while (length > 0 && (messageBuffer[length - 1] == '\r' || messageBuffer[length - 1] == '\n')) {
					--length;
				}
				messageBuffer[length] = '\0';
				message << " : " << messageBuffer.data();
			}
			message << " [" << errorCode << ']';
		}
		descriptionUTF8 = errorStringUTF8 + convertToUTF8String(message.str());
	}	
	return descriptionUTF8;
}

/* --- PathTime --- */

#if (__GNUC__)
	const __int64 kWindowsFileTimeToCTimeOffset = 116444736000000000LL;
#else
	const __int64 kWindowsFileTimeToCTimeOffset = 116444736000000000i64;
#endif

PathTime::PathTime(time_t cTime)
{
	__int64 y = (static_cast<__int64>(cTime) * 10000000) + kWindowsFileTimeToCTimeOffset;
	high = static_cast<unsigned int>(y >> 32);
	low = static_cast<unsigned int>(y);
	assert(convertToCTime() == cTime);
}

time_t PathTime::convertToCTime() const
{
	assert(isAvailable());
	__int64 x = (static_cast<__int64>(high) << 32) | low;
	if (x < kWindowsFileTimeToCTimeOffset) {
		return 0;
	}
	x = ((x - kWindowsFileTimeToCTimeOffset) + 5000000) / 10000000;
	time_t y = static_cast<time_t>(x);
	if (y != x) {
		return 0x7FFFFFFF;
	}
	return y;
}

/* --- PathAttributes --- */

PathAttributes::PathAttributes()
	: isReadOnly(false)
	, isHidden(false)
	, win32Attributes(FILE_ATTRIBUTE_NORMAL)
	, macFileCreator(0)
	, macFileType(0)
{
}

/* --- Path --- */

class Path::Impl {
	friend class Path;
	protected:	Impl();
	protected:	std::wstring path;
	// 'parentOffset' is almost always identical to 'nameOffset', but with long unicode paths prepended with '\\?\' it
	// may at least in theory be different.
	protected:	size_t parentOffset;
	protected:	size_t nameOffset;
	protected:	size_t extensionOffset;
};

Path::Impl::Impl()
	: parentOffset(0)
	, nameOffset(0)
	, extensionOffset(0)
{
}

std::wstring Path::getFullPath() const { assert(!isNull()); return impl->path; }
bool Path::isRoot() const { return (!isNull() && impl->parentOffset == 0); }
Path Path::getCurrentDirectoryPath() { return Path(L".\\"); }
wchar_t Path::getSeparator() { return L'\\'; }

bool Path::isDirectoryPath() const {
	return (!isNull() && gotTrailingBackslash(impl->path));
}

bool Path::hasExtension() const {
	assert(!isNull());
	return (const_cast<const Impl*>(impl)->path[impl->extensionOffset] == L'.');
}

std::wstring Path::appendSeparator(const std::wstring& path)
{
	return (gotTrailingBackslash(path) ? path : path + L'\\');
}

std::wstring Path::removeSeparator(const std::wstring& path)
{
	return (gotTrailingBackslash(path) ? std::wstring(path.begin(), path.end() - 1) : path);
}

Path& Path::operator=(const Path& copy)
{
	Impl* oldImpl = impl;
	impl = ((copy.impl != 0) ? new Impl(*copy.impl) : 0);
	delete oldImpl;
	return (*this);
}

int Path::compare(const Path& other) const {
	if (this == &other) {
		return 0;
	} else if (impl == 0 || other.impl == 0) {
		return (impl != 0 ? 1 : 0) - (other.impl != 0 ? 1 : 0);
	} else {
		switch (
		#if (WINVER < _WIN32_WINNT_VISTA)
			::CompareStringW(LOCALE_USER_DEFAULT
		#else
			::CompareStringEx(LOCALE_NAME_USER_DEFAULT
		#endif
			#if (WINVER >= _WIN32_WINNT_WIN7)
				, NORM_IGNORECASE | SORT_DIGITSASNUMBERS
			#else
				, NORM_IGNORECASE | 8
			#endif
				, impl->path.data(), lossless_cast<int>(impl->path.size())
				, other.impl->path.data(), lossless_cast<int>(other.impl->path.size())
			#if (WINVER >= _WIN32_WINNT_VISTA)
				, NULL, NULL, NULL
			#endif
				)) {
			case CSTR_LESS_THAN: return -1;
			case CSTR_GREATER_THAN: return 1;
			case CSTR_EQUAL: {
				switch (
				#if (WINVER < _WIN32_WINNT_VISTA)
					::CompareStringW(LOCALE_USER_DEFAULT
				#else
					::CompareStringEx(LOCALE_NAME_USER_DEFAULT
				#endif
						, 0
						, impl->path.data(), lossless_cast<int>(impl->path.size())
						, other.impl->path.data(), lossless_cast<int>(other.impl->path.size())
					#if (WINVER >= _WIN32_WINNT_VISTA)
						, NULL, NULL, NULL
					#endif
						)) {
					case CSTR_LESS_THAN: return -1;
					case CSTR_GREATER_THAN: return 1;
					case CSTR_EQUAL: {
						const int d = impl->path.compare(other.impl->path);
						return (d < 0 ? -1 : (d > 0 ? 1 : 0));
					}
					default: assert(0);
				}
				break;
			}
			default: assert(0);
		}
		return 0;
	}
}

bool Path::equals(const Path& other) const {
	if (this == &other) {
		return true;
	} else if (impl == 0 || other.impl == 0) {
		return (impl == 0 && other.impl == 0);
	} else {
		return
			#if (WINVER < _WIN32_WINNT_VISTA)
				::CompareStringW(LOCALE_USER_DEFAULT
			#else
				::CompareStringEx(LOCALE_NAME_USER_DEFAULT
			#endif
				, NORM_IGNORECASE
				, impl->path.data(), lossless_cast<int>(impl->path.size())
				, other.impl->path.data(), lossless_cast<int>(other.impl->path.size())
			#if (WINVER >= _WIN32_WINNT_VISTA)
				, NULL, NULL, NULL
			#endif
				) == CSTR_EQUAL;
	}
}

bool Path::operator==(const Path& other) const {
	if (this == &other) {
		return true;
	} else if (impl == 0 || other.impl == 0) {
		return (impl == 0 && other.impl == 0);
	} else {
		return impl->path == other.impl->path;
	}
}

Path::Path(const std::wstring& pathString)
	: impl(new Impl()) // Ok to allocate single initializer
{
	try {
		std::wstring newPathString;
		if (pathString.size() >= 8 && pathString.substr(0, 8) == L"\\\\?\\UNC\\") {
			// It starts with the long unicode UNC name prefix, drop it and replace with standard UNC syntax.
			newPathString = std::wstring(L"\\") + pathString.substr(7, pathString.size() - 7);
		} else if (pathString.size() >= 4 && pathString.substr(0, 4) == L"\\\\?\\") {
			// It starts with the long unicode name prefix, drop it.
			newPathString = pathString.substr(4, pathString.size() - 4);
		} else {
			newPathString = pathString;
		}
		
		if (!newPathString.empty()) {
			size_t i = newPathString.size() - 1;
			while (i + 1 > 0 && newPathString[i] == L'.') --i;
		
			// If it ends with a relative path ('.', '..' etc) or if is a drive letter only, add directory backslash.
			if ((i + 1 <= 0 || (i < newPathString.size() - 1
					&& (newPathString[i] == L'/' || newPathString[i] == L'\\')))
					|| (i == 1 && newPathString[1] == L':')) {
				newPathString += L'\\';
			}
		}
		
		std::wstring fullPath = convertToAbsolutePath(newPathString);
		
		// Make sure it stays the same if we convert the result again, otherwise there is a problem with the path
		// string (like the occurence of "..." which shouldn't be allowed, it seems to be converted to ".." by
		// GetFullPathName()).
		std::wstring fullPathAgain = convertToAbsolutePath(fullPath);
		if (fullPathAgain != fullPath) {
			throw Exception(std::string("Error interpreting path : ") + convertToUTF8String(pathString), Path()
					, ::GetLastError());
		}
		
		size_t i = fullPath.size() - 1;
		if (i > 0 && fullPath[i] == L'\\') {
			--i;
		}
		size_t ext = i + 1;
		while (i + 1 > 0 && fullPath[i] != L'.' && fullPath[i] != L'\\') --i;
		if (i + 1 > 0 && fullPath[i] == L'.') {
			ext = i;
			while (i + 1 > 0 && fullPath[i] != L'\\') --i;
		}
		// Note: is it an UNC root? ('\\x\y\'), if so, we should never split the network name from the drive.
		if (fullPath.size() >= 2 && fullPath[0] == L'\\' && fullPath[1] == L'\\') {
			size_t j = 2;
			while (j < i && fullPath[j] != L'\\') ++j;
			if (j >= i) {
				i = -1;		// We have now reached the current backslash, so it must be a UNC root
			}
		}
		++i;
		if (fullPath.size() < MAX_PATH) {
			impl->path = fullPath;
			impl->parentOffset = i;
			impl->nameOffset = i;
			impl->extensionOffset = ext;
		} else if (fullPath[0] == L'\\' && fullPath[1] == L'\\') {
			// If we have more than MAX_PATH characters we need to prepend the '\\?\UNC\' or '\\?\' prefix.
			impl->path = std::wstring(L"\\\\?\\UNC") + fullPath.substr(1, fullPath.size() - 1);
			impl->parentOffset = ((i == 0) ? 0 : i + 6);
			impl->nameOffset = i + 6;
			impl->extensionOffset = ext + 6;
		} else {
			impl->path = std::wstring(L"\\\\?\\") + fullPath;
			impl->parentOffset = ((i == 0) ? 0 : i + 4);
			impl->nameOffset = i + 4;
			impl->extensionOffset = ext + 4;
		}
	}
	catch (...) {
		delete impl;
		throw;
	}
}

Path::Path(const Path& copy)
	: impl((copy.impl != 0) ? new Impl(*copy.impl) : 0) // Ok to allocate single initializer
{
}

void Path::listRoots(std::vector<Path>& roots)
{
	::DWORD charactersRequired = ::GetLogicalDriveStringsW(0, 0);
	std::vector<wchar_t> buffer(charactersRequired + 1);
	::DWORD charactersRequiredAgain = ::GetLogicalDriveStringsW(charactersRequired, &buffer[0]);
	assert(charactersRequiredAgain <= charactersRequired);
	size_t index = 0;
	size_t length = wcslen(&buffer[index]);
	while (length > 0) {
		assert(index < buffer.size());
		roots.push_back(Path(std::wstring(&buffer[index])));
		index += length + 1;
		length = wcslen(&buffer[index]);
	}
}

bool Path::isValidChar(wchar_t c)
{
	switch (c) {
		case L'<':
		case L'>':
		case L'"':
		case L'|': return false;
		default: return (c >= 32);
	}
}

Path Path::getParent() const
{
	assert(!isNull());
	assert(!isRoot());
	return Path(impl->path.substr(0, impl->parentOffset));
}

Path Path::getRelative(const std::wstring& pathString) const
{
	assert(!isNull());
	if (pathString.empty()) {
		return *this;
	} else if ((pathString.size() >= 1 && (pathString[0] == L'\\' || pathString[0] == L'/'))
			|| (pathString.size() >= 2 && pathString[1] == L':')) {
		// If it begins with a '\' or second char is a ':', it is not a relative path.
		return Path(pathString);
	} else {
		return Path(addTrailingBackslash(impl->path) + pathString);
	}
}

Path Path::withoutExtension() const
{
	assert(!isNull());
	std::wstring newPathString(impl->path.substr(0, impl->extensionOffset));
	if (gotTrailingBackslash(impl->path)) {
		newPathString += L'\\';
	}
	return Path(newPathString);
}

Path Path::withExtension(const std::wstring& extensionString) const
{
	assert(!isNull());
	std::wstring newPathString(impl->path.substr(0, impl->extensionOffset));
	if (extensionString.empty() || extensionString[0] != L'.') {
		newPathString += L'.';
	}
	newPathString += extensionString;
	if (gotTrailingBackslash(impl->path)) {
		newPathString += L'\\';
	}
	return Path(newPathString);
}

void Path::findPaths(std::vector<Path>& paths, const std::wstring& wildcardPattern, const PathListFilter& filter)
{
	std::wstring pattern = stripTrailingBackslash(wildcardPattern);
	const wchar_t* s = pattern.data();
	const wchar_t* p = s + pattern.size();
	while (--p >= s && *p != L'/' && *p != L'\\' && *p != L':') {
		;
	}
	appendPaths((p >= s) ? Path(std::wstring(s, p + 1)) : getCurrentDirectoryPath(), paths, pattern, filter);
}

void Path::listSubPaths(std::vector<Path>& subPaths, const PathListFilter& filter) const
{
	assert(!isNull());
	appendPaths(*this, subPaths, addTrailingBackslash(impl->path) + L"*."
			+ ((filter.includeExtension.empty() ? L"*" : filter.includeExtension)), filter);
}

std::wstring Path::getName() const
{
	assert(!isNull());
	return impl->path.substr(impl->nameOffset, impl->extensionOffset - impl->nameOffset);
}

std::wstring Path::getExtension() const
{
	assert(!isNull());
	if (!hasExtension()) {
		return L"";
	} else {
		return stripTrailingBackslash(impl->path.substr(impl->extensionOffset + 1
				, impl->path.size() - (impl->extensionOffset + 1)));
	}
}

std::wstring Path::getNameWithExtension() const
{
	assert(!isNull());
	return stripTrailingBackslash(impl->path.substr(impl->nameOffset, impl->path.size() - impl->nameOffset));
}

bool Path::exists() const
{
	assert(!isNull());
	::DWORD fileAttributes = ::GetFileAttributesW(getFullPath().c_str());
	return (fileAttributes != INVALID_FILE_ATTRIBUTES);
}

bool Path::isFile() const
{
	assert(!isNull());
	::DWORD fileAttributes = ::GetFileAttributesW(getFullPath().c_str());
	return (fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool Path::isDirectory() const
{
	assert(!isNull());
	::DWORD fileAttributes = ::GetFileAttributesW(getFullPath().c_str());
	return (fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

PathInfo Path::getInfo() const
{
	assert(!isNull());
	::WIN32_FILE_ATTRIBUTE_DATA attributes;
	::BOOL getFileAttributesExReturn = ::GetFileAttributesExW
			( getFullPath().c_str() // pointer to string that specifies a file or directory
			, GetFileExInfoStandard // value that specifies the type of attribute information to obtain
			, &attributes // pointer to buffer to receive attribute information
			);
	if (!getFileAttributesExReturn) {
		throw Exception("Error obtaining file or directory info", (*this), ::GetLastError());
	}
	PathInfo info;
	info.isDirectory = ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
	info.creationTime = PathTime(attributes.ftCreationTime.dwHighDateTime, attributes.ftCreationTime.dwLowDateTime);
	info.modificationTime = PathTime(attributes.ftLastWriteTime.dwHighDateTime
			, attributes.ftLastWriteTime.dwLowDateTime);
	info.lastAccessTime = PathTime(attributes.ftLastAccessTime.dwHighDateTime
			, attributes.ftLastAccessTime.dwLowDateTime);
	info.attributes.isReadOnly = ((attributes.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0);
	info.attributes.isHidden = ((attributes.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
	info.attributes.win32Attributes = attributes.dwFileAttributes;
	info.fileSize = Int64(attributes.nFileSizeHigh, attributes.nFileSizeLow);
	return info;
}

void Path::updateAttributes(const PathAttributes& newAttributes) const
{
	assert(!isNull());
	::BOOL setFileAttributesReturn = ::SetFileAttributesW(getFullPath().c_str()
			, calcFileAttributesBits(newAttributes));
	if (!setFileAttributesReturn) {
		throw Exception("Error updating attributes on file or directory", (*this), ::GetLastError());
	}
}

void Path::updateTimes(const PathTime& newCreationTime, const PathTime& newModificationTime
		, const PathTime& newAccessTime) const
{
	assert(!isNull());
	::HANDLE fileHandle = INVALID_HANDLE_VALUE;
	try {
		fileHandle = ::CreateFileW(getFullPath().c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE
				, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (fileHandle == INVALID_HANDLE_VALUE) {
			throw Exception("Error updating time info on file or directory", (*this), ::GetLastError());
		}
		::FILETIME times[3];
		::BOOL setFileTimeReturn = ::SetFileTime(fileHandle, convertPathTime(newCreationTime, &times[0])
				, convertPathTime(newAccessTime, &times[1]), convertPathTime(newModificationTime, &times[2]));
		if (!setFileTimeReturn) {
			throw Exception("Error updating time info on file or directory", (*this), ::GetLastError());
		}
		::BOOL closeHandleReturn = ::CloseHandle(fileHandle);
		assert(closeHandleReturn);
		fileHandle = INVALID_HANDLE_VALUE;
	}
	catch (...) {
		if (fileHandle != INVALID_HANDLE_VALUE) {
			::BOOL closeHandleReturn = ::CloseHandle(fileHandle);
			assert(closeHandleReturn);
			fileHandle = INVALID_HANDLE_VALUE;
		}
		throw;
	}
}

bool Path::tryToErase() const
{
	assert(!isNull());
	if (isDirectory()) {
		::BOOL removeDirectoryReturn = ::RemoveDirectoryW(getFullPath().c_str());
		return (removeDirectoryReturn != FALSE);
	} else {
		::BOOL deleteFileReturn = ::DeleteFileW(getFullPath().c_str());
		return (deleteFileReturn != FALSE);
	}
}

void Path::erase() const
{
	assert(!isNull());
	if (!tryToErase()) {
		throw Exception("Error deleting file or directory", (*this), ::GetLastError());
	}
}

void Path::moveRename(const Path& destination) const
{
	assert(!isNull());
	::BOOL moveFileReturn = ::MoveFileW(getFullPath().c_str(), destination.getFullPath().c_str());
	if (!moveFileReturn) {
		throw Exception("Error renaming or moving file or directory", (*this), ::GetLastError());
	}
}

void Path::create() const
{
	assert(!isNull());
	assert(!isRoot());
	::BOOL createDirectoryReturn = ::CreateDirectoryW(getFullPath().c_str(), 0);
	if (!createDirectoryReturn) {
		throw Exception("Error creating directory", (*this), ::GetLastError());
	}
}

bool Path::tryToCreate() const
{
	assert(!isNull());
	assert(!isRoot());
	::BOOL createDirectoryReturn = ::CreateDirectoryW(getFullPath().c_str(), 0);
	return (createDirectoryReturn != FALSE);
}

void Path::copy(const Path& destination) const
{
	assert(!isNull());
	::BOOL copyFileExReturn = ::CopyFileExW(getFullPath().c_str(), destination.getFullPath().c_str(), 0, 0, 0
			, COPY_FILE_FAIL_IF_EXISTS | COPY_FILE_ALLOW_DECRYPTED_DESTINATION);
	if (!copyFileExReturn) {
		throw Exception("Error copying file", (*this), ::GetLastError());
	}
}

Path Path::createTempFile() const
{
	assert(!isNull());
	std::wstring directory;
	std::wstring prefix;
	if (isDirectory()) {
		directory = getFullPath();
	} else {
		directory = getParent().getFullPath();
		prefix = getName();
	}
	wchar_t tempFilePathBuffer[MAX_PATH + 1];
	::UINT getTempFileNameReturn = ::GetTempFileNameW
			( directory.c_str() // pointer to directory name for temporary file
			, prefix.c_str() // pointer to filename prefix
			, 0 // number used to create temporary filename
			, tempFilePathBuffer // pointer to buffer that receives the new filename
			);
	if (getTempFileNameReturn == 0) {
		throw Exception("Error creating temporary file"
				, (tempFilePathBuffer[0] != 0) ? Path(tempFilePathBuffer) : Path(), ::GetLastError());
	}
	return Path(tempFilePathBuffer);
}

Path::~Path()
{
	delete impl;
}

/* --- ReadOnlyFile --- */

ReadOnlyFile::Impl::~Impl()
{
	::BOOL closeHandleReturn = ::CloseHandle(handle);
	assert(closeHandleReturn);
}

static ::HANDLE createFile(const Path& path, ::DWORD dwDesiredAccess, ::DWORD dwShareMode
		, ::LPSECURITY_ATTRIBUTES lpSecurityAttributes, ::DWORD dwCreationDisposition, ::DWORD dwFlagsAndAttributes
		, ::HANDLE hTemplateFile, int retryCount = 0, int retrySleepMS = 100)
{
	::DWORD error = NO_ERROR;
	do {
		const ::HANDLE handle = ::CreateFileW(path.getFullPath().c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes
				, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
		if (handle != INVALID_HANDLE_VALUE) {
			return handle;
		}
		const ::DWORD error = ::GetLastError();
		if ((error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION || error == ERROR_USER_MAPPED_FILE) && retryCount > 0) {
			--retryCount;
			::Sleep(retrySleepMS);
		} else {
			throw Exception((dwCreationDisposition == OPEN_EXISTING) ? "Error opening file" : "Error creating file", path, error);
		}
	} while (true);
}

ReadOnlyFile::ReadOnlyFile(const Path& path, bool allowConcurrentWrites)
	// Ok to allocate single initializer
	: impl(new ReadOnlyFile::Impl(path, createFile(path, GENERIC_READ
			, allowConcurrentWrites ? (FILE_SHARE_WRITE | FILE_SHARE_READ) : FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0)))
{
}

Int64 ReadOnlyFile::getSize() const
{
	::DWORD fileSizeHigh = 0;
	::DWORD fileSizeLow = ::GetFileSize(impl->handle, &fileSizeHigh);
	if (fileSizeLow == 0xFFFFFFFFL) {
		if (::GetLastError() != NO_ERROR) {
			throw Exception("Error obtaining size of file", getPath(), ::GetLastError());
		}
	}
	return Int64(fileSizeHigh, fileSizeLow);
}

int ReadOnlyFile::tryToRead(Int64 index, int count, unsigned char* bytes) const
{
	// Note: we use overlapped I/O since this makes it possible to read and write to the same file object from
	// multiple threads since setting file position and issuing the read or write operation is "atomic".
	::OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof (overlapped));
	overlapped.Offset = index.getLow();
	overlapped.OffsetHigh = index.getHigh();

	::DWORD bytesRead = 0;
	::BOOL readFileReturn = ::ReadFile(impl->handle, bytes, count, &bytesRead, &overlapped);

	if (!readFileReturn) {
		::DWORD error = ::GetLastError();
		if (error != ERROR_HANDLE_EOF) {
			throw Exception("Error reading from file", getPath(), error);
		}
		return bytesRead;
	} else {
		return bytesRead;
	}
}

void ReadOnlyFile::read(Int64 index, int count, unsigned char* bytes) const
{
	if (tryToRead(index, count, bytes) != count) {
		throw Exception("Error reading from file", getPath(), ERROR_HANDLE_EOF);
	}
}

Path ReadOnlyFile::getPath() const { return impl->path; }

ReadOnlyFile::~ReadOnlyFile() { delete impl; }

/* --- ReadWriteFile --- */

ReadWriteFile::ReadWriteFile(const Path& path, bool allowConcurrentReads, bool allowConcurrentWrites)
	// Ok to allocate single initializer
	: ReadOnlyFile(new ReadOnlyFile::Impl(path, createFile(path, GENERIC_WRITE | GENERIC_READ
			, ((allowConcurrentReads) ? FILE_SHARE_READ : 0) | ((allowConcurrentWrites) ? FILE_SHARE_WRITE : 0), 0
			, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)))
{
}

ReadWriteFile::ReadWriteFile(const Path& path, const PathAttributes& attributes, bool replaceExisting
		, bool allowConcurrentReads, bool allowConcurrentWrites)
	// Ok to allocate single initializer
	: ReadOnlyFile(new ReadOnlyFile::Impl(path, createFile(path, GENERIC_WRITE | GENERIC_READ
			, ((allowConcurrentReads) ? FILE_SHARE_READ : 0) | ((allowConcurrentWrites) ? FILE_SHARE_WRITE : 0), 0
			, replaceExisting ? CREATE_ALWAYS : CREATE_NEW, calcFileAttributesBits(attributes), 0)))
{
}

void ReadWriteFile::write(Int64 index, int count, const unsigned char* bytes)
{
	// Note: we use overlapped I/O since this makes it possible to read and write to the same file object from
	// multiple threads since setting file position and issuing the read or write operation is "atomic".
	::OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof (overlapped));
	overlapped.Offset = index.getLow();
	overlapped.OffsetHigh = index.getHigh();

	::DWORD bytesWritten = 0;
	::BOOL writeFileReturn = ::WriteFile(impl->handle, bytes, count, &bytesWritten, &overlapped);

	if (!writeFileReturn) {
		throw Exception("Error writing to file", getPath(), ::GetLastError());
	}
	assert(bytesWritten == count);
}

ExchangingFile::ExchangingFile(const Path& path, const PathAttributes& attributes)
	: ReadWriteFile(0)
	, originalPath(path)
{
	Path tempPath(originalPath.createTempFile());
	impl = new ReadOnlyFile::Impl(tempPath, createFile(tempPath, GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS
			, calcFileAttributesBits(attributes), 0));
}

void ExchangingFile::commit()
{
	if (!originalPath.isNull()) {
		flush();
		Path tempPath = impl->path;
		delete impl;
		impl = 0;
		::BOOL success = ::ReplaceFileW(originalPath.getFullPath().c_str(), tempPath.getFullPath().c_str()
				, NULL, 0, 0, 0);
		::DWORD error = ::GetLastError();
		bool setReadOnly = false;
		if (!success && error == ERROR_ACCESS_DENIED) {
			const ::DWORD fileAttributes = ::GetFileAttributesW(tempPath.getFullPath().c_str());
			if (fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
				::BOOL didSetAttributes = ::SetFileAttributesW(tempPath.getFullPath().c_str()
						, fileAttributes & ~FILE_ATTRIBUTE_READONLY);
				assert(didSetAttributes);
				(void)didSetAttributes;
				success = ::ReplaceFileW(originalPath.getFullPath().c_str(), tempPath.getFullPath().c_str()
						, NULL, 0, 0, 0);
				error = ::GetLastError();
				if (success) {
					const ::DWORD attributesAgain = ::GetFileAttributesW(originalPath.getFullPath().c_str());
					if (attributesAgain != INVALID_FILE_ATTRIBUTES) {
						didSetAttributes = ::SetFileAttributesW(originalPath.getFullPath().c_str()
								, attributesAgain | FILE_ATTRIBUTE_READONLY);
						assert(didSetAttributes);
						(void)didSetAttributes;
					}
				}
			}
		}
		if (!success && error == ERROR_FILE_NOT_FOUND) {
			success = ::MoveFileW(tempPath.getFullPath().c_str(), originalPath.getFullPath().c_str());
			if (!success) {
				error = ::GetLastError();
			}
		}
		if (success) {
			tempPath = originalPath;
			originalPath = Path();
		}
		/* 
			On Windows, renaming a file can briefly lock it. This issue occurs more frequently in Dropbox folders
			but is not exclusive to them. To address this, we implement a retry mechanism. On encountering a 'busy'
			error, we attempt to reopen the file every 100ms, with a 2-second timeout.
		*/
 		impl = new ReadOnlyFile::Impl(tempPath, createFile(tempPath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0, 20, 100));
		if (!success) {
			throw Exception("Error committing file", originalPath, error);
		}
	}
}

void ReadWriteFile::flush() { ::FlushFileBuffers(impl->handle); }

ExchangingFile::~ExchangingFile()
{
	if (!originalPath.isNull()) {
		assert(impl != 0);
		Path path = impl->path;
		delete impl;
		impl = 0;
		path.tryToErase();
	}
}

} /* namespace NuX */

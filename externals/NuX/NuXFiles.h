/**
	\file NuXFiles.h

	NuXFiles is a library for:
	
	1) Parsing, traversing and managing native file system paths.

	2) Finding and listing files and directories.
	
	3) Obtaining and changing information on files and directories (such as creation and modification dates etc).
	
	4) Performing certain "shell" operations on files and directories (such as moving, renaming, copying and deletion
	   etc).
	
	5) Reading and writing binary files using effective native APIs.
	
	6) Safe replacing of files using file exchanging mechanisms.
	

	NuXFiles is part of the NuEdge X-Platform Library / NuX.
	Written by Magnus Lidstroem
	(C) NuEdge Development 2005
	All Rights Reserved

	NuX design goals:
	
	1) Cross platform with effective OS-specific implementations for Windows XP and Mac OS X Carbon.

	2) Emphasis on native platform approaches and solutions. Follows rules and conventions of the supported platforms to
	   maximum extent.

	3) Light-weight with small header files that do not depend on heavy platform-specific headers.

	4) Self-contained components with few dependencies. Few source files. Easily integrated into existing projects in
	   whole or in part.

	5) Minimalistic but flexible approach, providing as few necessary building blocks as possible without sacrificing
	   versatility.

	6) Easily understood standard C++ code, avoiding complex templates and using only a small set of the Standard C++
	   Library and STL.

	7) Self-explanatory code with inline documentation. Clear and consistent naming conventions.
*/

// FIX : document thread-safety

#ifndef NuXFiles_h
#define NuXFiles_h

#include <string>
#include <vector>
#include <exception>
#include "assert.h"

namespace NuXFiles {

/**
	Int64 is an abstraction of a 64-bit integer. This class is required since certain supported platforms and compilers
	still do not conform to a standard 64-bit data type.
	
	Int64 supports conversion from and to signed 32-bit integers and all comparison operators with other Int64, but
	limited arithmetic operations.
	
	To convert a native 64-bit integer to Int64, use: { y = Int64((int)(x >> 32), (unsigned int)(x)); }
	
	To convert an Int64 to a native 64-bit integer, use: { y = ((int64_t)(x.getHigh()) << 32) | x.getLow(); }
	
	This class implementation may change in the future to simply "wrap" a native 64-bit integer instead.
*/
class Int64 {
	public:		Int64() : high(0), low(0) { }
	public:		Int64(int int32) : high((int32 < 0) ? -1 : 0), low(int32) { }
	public:		Int64(unsigned int uint32) : high(0), low(uint32) { }
	public:		Int64(int high, unsigned int low) : high(high), low(low) { }
	public:		bool operator<(const Int64& other) const { return (*this - other).high < 0; } // FIX : optimize
	public:		bool operator<=(const Int64& other) const { return (*this < other || *this == other); } // FIX : optimize
	public:		bool operator==(const Int64& other) const { return (high == other.high && low == other.low); }
	public:		bool operator!=(const Int64& other) const { return (high != other.high || low != other.low); }
	public:		bool operator>=(const Int64& other) const { return (*this - other).high >= 0; } // FIX : optimize
	public:		bool operator>(const Int64& other) const { return (*this >= other && *this != other); } // FIX : optimize
	public:		Int64 operator+=(const Int64& other) { high += other.high + (((low + other.low) >= low) ? 0 : 1); low += other.low; return (*this); }
	public:		Int64 operator++() { return ((*this) += 1); }
	public:		Int64 operator++(int) { Int64 copy(*this); (*this) += 1; return copy; }
	public:		Int64 operator+(const Int64& other) const { Int64 copy(*this); return (copy += other); }
	public:		Int64 operator-=(const Int64& other) { high -= other.high + (((low - other.low) <= low) ? 0 : 1); low -= other.low; return (*this); }
	public:		Int64 operator--() { return ((*this) -= 1); }
	public:		Int64 operator--(int) { Int64 copy(*this); (*this) -= 1; return copy; }
	public:		Int64 operator-(const Int64& other) const { Int64 copy(*this); return (copy -= other); }
	public:		int getHigh() const { return high; }
	public:		unsigned int getLow() const { return low; }
	public:		bool is32Bit() const { return (high == ((static_cast<int>(low) < 0) ? -1 : 0)); } ///< Returns true if the 64-bit value can be represented by a signed 32-bit integer.
	public:		int toInt32() const { assert(is32Bit()); return static_cast<int>(low); } ///< Converts value to 32-bit signed integer if it is possible.
	protected:	int high;
	protected:	unsigned int low;
};

/**
	PathTime is a 64-bit value used in PathInfo for creation, modification and access times (in UTC time).

	Internal resolution and representation of file time data is different on different platforms so you can convert a
	PathTime to the standard C time_t.
	
	However, if you only need to compare file times you can use the 64-bit value directly.
*/
// FIX : add conversion to some other 64-bit platform neutral date/time, like ms since 1900 or whatever, research if there is another good standard besides time_t
class PathTime : public Int64 {
	public:		PathTime() { } ///< The default constructor creates a "null time" (i.e. "not available").
	public:		PathTime(time_t cTime); ///< Creates a path time from a standard C time_t value.
	public:		PathTime(int high, unsigned int low) : Int64(high, low) { }
	public:		bool isAvailable() const { return (*this != 0); } ///< Returns true if time is available / valid (i.e. not null).
	public:		time_t convertToCTime() const; ///< Converts the path time to a standard C time_t value. It is illegal to call this function if isAvailable() returns false.
};

/**
	PathAttributes is used to specify OS-specific attributes for newly created files.
*/
class PathAttributes {
	public:		PathAttributes();
	public:		bool isReadOnly; ///< True if the path points to a file that is read-only.
	public:		bool isHidden; ///< True if the path points to an existing directory or file that is hidden.
	public:		unsigned int win32Attributes; ///< Used only under Windows. Defaults to FILE_ATTRIBUTE_NORMAL, #isReadOnly and #isHidden overrides the corresponding bits in #win32Attributes so you do not need to explicitly set #win32Attributes if you only need to set any of those.
	public:		unsigned int macFileType; ///< Used only for files under Mac OS X. 32-bit Mac type signature. 0 = n/a (default).
	public:		unsigned int macFileCreator; ///< Used only for files under Mac OS X. 32-bit Mac creator signature. 0 = n/a (default).
};

/**
	PathInfo is filled out by Path::GetInfo(). Notice that some fields are only available on certain platforms.
*/
class PathInfo {
	public:		PathInfo() : isDirectory(false) { }
	public:		bool isDirectory; ///< True if the path points to an existing directory / root.
	public:		PathTime creationTime; ///< Creation time as a PathTime. Use PathTime::isAvailable() and PathTime::convertToCTime() to get a standard C time_t value.
	public:		PathTime modificationTime; ///< Modification time as a PathTime. Use PathTime::isAvailable() and PathTime::convertToCTime() to get a standard C time_t value.
	public:		PathTime lastAccessTime; ///< Last accessed time as a PathTime. Use PathTime::isAvailable() and PathTime::convertToCTime() to get a standard C time_t value.
	public:		PathAttributes attributes;
	public:		Int64 fileSize; ///< The 64-bit file size (only set for paths that point to files).
};

/**
	PathListFilter is used for Path::listSubPaths() to filter the result set.
*/
class PathListFilter {
	public:		PathListFilter(); ///< The default constructor will create a filter that will not filter anything, i.e. all files and directories will be found.
/* FIX: drop?
	public:		PathListFilter(const std::wstring& wildcardPattern); ///< This constructor will create a filter that includes (non-hidden) files and directories according to operating system wild-card pattern conventions (e.g. "qwe*.rty" will return all files starting with "qwe" and ending with ".rty")
	public:		PathListFilter(const std::wstring& includeExtension, unsigned int includeMacFileType); ///< This constructor will create a filter that includes (non-hidden) files (not directories) of a certain type according to extension (and 32-bit type signature on Mac).
*/
	public:		bool excludeFiles; ///< Do not include files (only directories) in listing.
	public:		bool excludeDirectories; ///< Do not include directories (only files) in listing.
	public:		bool excludeHidden; ///< Do not include hidden files or directories.
	public:		std::wstring includeExtension; ///< If not empty, exclude files / directories that does not have this extension. (Do not include the '.')
	public:		unsigned int includeMacFileType; ///< If not 0, include also files that have this 32-bit type signature (regardless of any extension).
};

/**
	The Path represents a file system path pointing to an existing or non-existing root, directory or file (a root is a
	top-most directory, so a root path is a directory path as well).
	
	The paths represented by Path are always absolute and full, but you can construct them with relative path strings.
	
	For example, if the current directory is "c:\temp" and you create a path with ".\x" the path will point out
	"c:\temp\x".
	
	Internally, paths are implemented with unicode strings under Windows NT and Core Foundation URLs under Mac OS X.
*/
class Path {
	public:		class Impl;

	/// \name Utilities
	public:		static Path getCurrentDirectoryPath();							///< Creates a path from the operating system's "current directory".
	public:		static void listRoots(std::vector<Path>& roots);				///< Appends a list of all known local roots (i.e. non-network) to \p roots. Under Windows this will be all known drives, under Mac OS X this is simply a single entry list with the system root ('/').
	// FIX : idea for the future, but only if we need it (together with isVolumeRoot)
	// FIX : actually, Windows can have mount points too, e.g. SetVolumeMountPoint
	// public:		static void listVolumes(std::vector<Path>& roots);				///< On Windows this is the same as listRoots(). On Mac this will be a list of all mounted volumes.
	// FIX : I don't like this convention of basing everything from current-dir here, it could be more flexible if you had to use getCurrentDirectoryPath()?
	// FIX : also, this is kind of crippled now, often you need the wildcard pattern matching, like for example searching all dirs for '*.stuff'
	public:		static void findPaths(std::vector<Path>& paths, const std::wstring& wildcardPattern, const PathListFilter& filter = PathListFilter()); ///< Appends a list of all found paths that matches the wildcard pattern according to operating system conventions (e.g. ".\qwe*.rty" on Windows would return all files in current directory starting with "qwe" and ending with ".rty")
	public:		static wchar_t getSeparator();									///< Returns the OS directory separator (i.e. '\' under Windows, '/' under OS X)
	public:		static std::wstring appendSeparator(const std::wstring& path); // FIX : description
	public:		static std::wstring removeSeparator(const std::wstring& path);
	public:		static bool isValidChar(wchar_t c);								///< Returns true if char \p c is allowed in a directory or filename path. In this context, the directory (typically '/' or '\') and extension separator ('.') are considered valid.
	
	/// \name Creating paths
	public:		Path() : impl(0) { }											///< Default constructor that creates an empty "null-path" that you shouldn't normally use. This is required for creating arrays of paths etc... Null-paths cannot be used for any operations but state checking and comparisons. (No method of this class ever returns a null-path.)
	public:		Path(const std::wstring& pathString);							///< Creates a path that points to an existing or non-existing root, directory or file. Notice that the paths represented by Path are absolute and full, but you can construct them with relative path strings. This constructor never creates "null paths".
	public:		Path(const Path& copy);
	public:		Path& operator=(const Path& copy);
	public:		Path(Impl* impl) throw() : impl(impl) { }						///< Inherits the platform implementation pointed to by \p impl, mainly used internally but you can use it if you have a custom way of constructing the platform implementation. Ownership of \p impl is transferred to this object, so you should not dispose it after successful construction.

	/// \name Testing paths
	public:		bool isNull() const { return (impl == 0); }						///< Returns true if the path is empty / invalid / unavailable (i.e. constructed with default constructor).
	public:		bool isRoot() const;											///< Returns true if the path is a root path. A root path does not have a parent and it is illegal to call getParent(). Returns false on a null path.
	public:		bool isDirectoryPath() const;									///< Returns true if the path (seemingly) points out a directory and not a file. This is *not* based on the existence of any directory or file on the file system, but on the construction of the path only (typically whether it ends with a slash or not). Returns false on a null path.
	// FIX : idea for the future
	// FIX : actually, Windows can have mount points too, e.g. SetVolumeMountPoint
	// public:		bool isVolumeRoot() const;												///< On Windows this is the same as isRoot(). On Mac, this will be true if the path is a mount point for a volume.
	public:		int compare(const Path& other) const;										///< Compares two paths and returns 0 if they point to the same file system entity, <0 if this path is considered less in sorting order than \p other, >0 if this path is considered greater in sorting order than \p other. Notice: this is sorting order comparison. It may use localized case-insensitive comparison, but still return non-zero for two paths that are different only in case. This is to maintain a stable and deterministic sorting order. Use equals() instead to check if two paths are considered equal for the end-user.
	public:		bool equals(const Path& other) const;										///< Are the paths considered equal for the end user. (Possibly uses localized case-insensitive comparison. This is different from `==` as explained in the comment for compare().)
	public:		bool operator<(const Path& other) const { return (compare(other) < 0); }	///< Uses compare() so that stable sorting order is maintained (see compare()).
	public:		bool operator<=(const Path& other) const { return (compare(other) <= 0); }	///< Uses compare() so that stable sorting order is maintained (see compare()).
	public:		bool operator==(const Path& other) const;									///< Uses strict comparison (equivalent of compare() == 0) so that stable sorting order is maintained (see compare()).
	public:		bool operator!=(const Path& other) const { return !(*this == other); }		///< Uses strict comparison (equivalent of compare() != 0) so that stable sorting order is maintained (see compare()).
	public:		bool operator>=(const Path& other) const { return (compare(other) >= 0); }	///< Uses compare() so that stable sorting order is maintained (see compare()).
	public:		bool operator>(const Path& other) const { return (compare(other) > 0); }	///< Uses compare() so that stable sorting order is maintained (see compare()).

	/// \name Getting relative paths
	// FIX : why not return null in root instead?
	public:		Path getParent() const;																					///< Returns the parent path (i.e. the directory in which this path resides). It is illegal to call this method on a root path (use isRoot()).
	public:		Path getRelative(const std::wstring& pathString) const;													///< Returns a file, directory (or root) path relative to this path. The result is the same as changing the current directory to this path and creating a new path with the \p pathString string. You can use any relative path syntax that the operating system supports (such as "..\file" for pointing out a file in the parent directory). It is also legal for \p pathString to contain a absolute (full) path, in which case the same absolute path will be returned.
	public:		bool makeRelative(const Path& toPath, bool walkUpwards, std::wstring& pathString) const;				///< Builds a relative path to \p toPath, storing it in \p pathString. If \p walkUpwards is true, '..' can be used to navigate up directories. Returns true if the path is relative, false if the full path is needed.
	public:		Path withoutExtension() const;																			///< Creates a new path from this path without the extension.
	public:		Path withExtension(const std::wstring& extensionString) const;											///< Creates a new path from this path with a different extension. If this path doesn't have an extension to begin with it will be added. \p extensionString may or may not begin with the leading '.'.
	// FIX : change to listChildren
	public:		void listSubPaths(std::vector<Path>& subPaths, const PathListFilter& filter = PathListFilter()) const;	///< Appends all existing files and directories residing under this root or directory to \p subPaths.
	public:		bool matchesFilter(const PathListFilter& filter) const;	///< Returns true if this path matches the filter, i.e. it would be included by listSubPaths().

	/// \name Getting path string components
	public:		bool hasExtension() const; ///< Returns true if the last path component ends with an extension.
	public:		std::wstring getName() const; ///< Gets the name of the last path component (excluding any extension).
	public:		std::wstring getExtension() const; ///< Gets the extension of the last path component (returns an empty string if the last component does not have an extension or if the extension is an empty string.). The extension string returned will not begin with a leading '.'.
	public:		std::wstring getNameWithExtension() const; ///< Returns the name of the last path component including any extension.
	public:		std::wstring getFullPath() const; ///< Returns the entire absolute path as a string that can be used for operating system file calls.
	// FIX add getAsURL?

	/// \name Getting and setting info on paths
	public:		bool exists() const; ///< Returns true if the path points to an existing file, directory or root.
	public:		bool isFile() const; ///< Returns true if the path points to an existing file (and not a directory or root).
	public:		bool isDirectory() const; ///< Returns true if the path points to an existing directory (or root) and not a file.
	public:		PathInfo getInfo() const; ///< Fills out and returns a PathInfo class for an existing file or directory. Throws on failure.
	public:		void updateAttributes(const PathAttributes& newAttributes) const; ///< Updates attributes for an existing file or directory. Throws on failure.
	public:		void updateTimes(const PathTime& newCreationTime, const PathTime& newModificationTime, const PathTime& newAccessTime) const; ///< Updates date and time information on an existing file or directory. Will only change date / time if PathTime::isAvailable() returns true, thus you can pass "PathTime()" in any of the arguments to leave that date / time untouched. Throws on failure.

	/// \name "Shell operations" on paths
	public:		void create() const; ///< Creates a physical directory for the path. If the directory already exists this routine will throw. Only one directory can be created at a time, thus the parent path must refer to an existing directory.
	public:		bool tryToCreate() const; ///< Tries to creates a physical directory for the path (like create()) but does not throw if the directory couldn't be created, instead it only returns false.
	public:		void copy(const Path& destination) const; ///< Copies a file to a new name and / or place. The path must be a file path and if the file already exists this routine will throw.
	public:		void moveRename(const Path& destination) const; ///< Moves or renames a file or directory tree to a new name and / or place. Throws on failure.
	public:		void erase() const; ///< Deletes the file or directory. Directories must be empty to be deleted. Throws on failure.
	public:		bool tryToErase() const; ///< Tries to delete (like erase()) but does not throw if the path couldn't be deleted, instead it only returns false.
	public:		Path createTempFile() const; ///< Creates a temporary file. If the path is a file path, the temporary file is created "adjacent" to this file in the same directory. If the path is an existing directory path, the temporary file is created beneath this directory. Returns a path to the created temporary file.

	public:		virtual ~Path();

	// Implementation
	public:		Impl* getImpl() const { return impl; }
	protected:	Impl* impl;
};

/**
	A ReadOnlyFile is an existing open file from which you can read. The file remains open until the object is destructed.

	Internally, files are implemented with native file handles and file routines under Windows NT and the Carbon file
	API is used under Mac OS X.
*/
class ReadOnlyFile {
	public:		class Impl;

	public:		ReadOnlyFile(const Path& path, bool allowConcurrentWrites = false);	///< Opens an existing file for reading only. The file remains open until this object is destructed. \p path must point to an existing physical file. If \p allowConcurrentWrites is true another process may acquire write access to the file while it is open. Throws on failure.
	public:		ReadOnlyFile(Impl* impl) : impl(impl) { } ///< Inherits the platform implementation pointed to by \p impl, you can use it if you have a custom way of constructing the platform implementation. Ownership of \p impl is transferred to this object, so you should not dispose it after successful construction.
	public:		Int64 getSize() const; ///< Gets the current readable size of the file in bytes.
	public:		void read(Int64 position, int count, unsigned char* bytes) const; ///< Reads \p count number of bytes at position \p position into \p bytes. If you try to read beyond end of file (or if any other error occurs) this routine throws.
	public:		int tryToRead(Int64 position, int count, unsigned char* bytes) const; ///< Tries to read \p count number of bytes at position \p position into \p bytes. If you try to read beyond end of file this routine returns the number of bytes that was successfully read. If any other error occurs this routine throws.
	public:		Path getPath() const;
	public:		virtual ~ReadOnlyFile();

	// Implementation
	public:		Impl* getImpl() const { return impl; }
	protected:	Impl* impl;
	private:	ReadOnlyFile(const ReadOnlyFile& copy); // N/A
	private:	ReadOnlyFile& operator=(const ReadOnlyFile& copy); // N/A
};

/**
	A ReadWriteFile is an open file which you can both read from and write to. The file remains open until the object is
	destructed.

	There are two different constructors that you use to open a ReadWriteFile. The first constructor allows you to open
	an existing file. This constructor throws if the file does not exist. The other constructor creates and opens a new
	file, optionally replacing any already existing file with the same name.
*/
class ReadWriteFile : public ReadOnlyFile {
	public:		ReadWriteFile(const Path& path, bool allowConcurrentReads, bool allowConcurrentWrites); ///< Opens an existing file for reading and writing. The file remains open until this object is destructed. \p path must point to an existing physical file.
	public:		ReadWriteFile(const Path& path, const PathAttributes& attributes = PathAttributes(), bool replaceExisting = true, bool allowConcurrentReads = false, bool allowConcurrentWrites = false); ///< Creates a file for reading and writing. The file remains open until this object is destructed. \p path must point to a file in an existing directory. If \p replaceExisting is true and the file already exists it will be replaced. If \p replaceExisting is false and the file already exists the constructor will throw.
	public:		ReadWriteFile(Impl* impl) : ReadOnlyFile(impl) { } ///< Inherits the platform implementation pointed to by \p impl, you can use it if you have a custom way of constructing the platform implementation. Ownership of \p impl is transferred to this object, so you should not dispose it after successful construction.
// FIX : I think the behaviour of writing beyond EOF should be defined. Carbon handles it by padding the file with zeroes. Win32?
	public:		void write(Int64 position, int count, const unsigned char* bytes); ///< Writes \p count number of bytes from \p bytes at position \p position. If any error occurs this routine throws.
	public:		void flush(); ///< Flushes any buffered data physically to disk.
	private:	ReadWriteFile(const ReadWriteFile& copy); // N/A
	private:	ReadWriteFile& operator=(const ReadWriteFile& copy); // N/A
};

/**
	ExchangingFile is an invention inspired mainly from a concept in Mac OS. Instead of simply creating and overwriting
	a file directly (with ReadWriteFile) you can use this class to create and write to a temporary file that will
	replace any original existing file first when it is committed and closed. This convention ensures that the user will
	not lose any data if an error occurs while replacing old files.

	You must explicitly call commit() on a ExchangingFile before it is being destructed or the new file will not be
	kept.
	
	When the new file is being exchanged with the old file certain file attributes (such as creation date) will be
	copied from the original to the new file.
	
	Notice that it is meaningful to use ExchangingFile even if you are not replacing an old file if you wish the file to
	be thrown away automatically on any error. This is good to prevent the existence of corrupt files.
	
	You are allowed to open the original file (with the ReadOnlyFile class) before or while you are replacing it with
	ExchangingFile, but you must close it before committing.
*/
class ExchangingFile : public ReadWriteFile {
	public:		ExchangingFile(const Path& path, const PathAttributes& attributes = PathAttributes()); ///< Creates a file for reading and writing. The file remains open until this object is destructed. \p path must point to a file in an existing directory.
	public:		void commit(); ///< You should call commit() at least once before the ExchangingFile is destructed or the new file will not be kept. Calling commit() more than once makes no difference. After you have committed the file you cannot write to it anymore (reading from it is still ok).
	public:		virtual ~ExchangingFile();
	protected:	Path originalPath;
	private:	ExchangingFile(const ExchangingFile& copy); // N/A
	private:	ExchangingFile& operator=(const ExchangingFile& copy); // N/A
};

// FIX : document
class Exception : public std::exception {
	public:		Exception(const std::string& errorStringUTF8, const Path& path = Path(), int errorCode = 0) : errorStringUTF8(errorStringUTF8), path(path), errorCode(errorCode) { } ///< \p errorString is the string returned by what(). \p path and \p errorCode are optional.
	public:		virtual const char *what() const throw() { descriptionUTF8 = describe(); return descriptionUTF8.c_str(); } ///< Returns a string describing the error. This string is usually constructed from an OS error code.
	public:		std::string getErrorStringUTF8() const { return errorStringUTF8; }
	public:		Path getPath() const throw() { return path; } ///< Returns the path to the file / directory that this exception occurred with (this path is also usually present in the error-string returned by what()). Returns 0 if no path is associated with this exception or a pointer to a constant object which is only valid for as long as this exception object exists.
	public:		int getErrorCode() const throw() { return errorCode; } ///< Returns the OS-specific error code that caused this exception (or 0 if an error code was not associated with the exception).
	public:		std::string describe() const;
	public:		virtual ~Exception() throw() { }
	private:	std::string errorStringUTF8;
	private:	int errorCode;
	private:	Path path;
	private:	mutable std::string descriptionUTF8;
};

} /* namespace NuXFiles */

#endif

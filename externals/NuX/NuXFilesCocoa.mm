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
#include <sstream>
#include <limits>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "NuXFilesCocoa.h"

namespace NuXFiles {

static bool gotTrailingSlash(const std::wstring& path) {
	return (!path.empty() && path[path.size() - 1] == L'/');
}

static bool endsWith(const std::wstring& s, const std::wstring& suffix) {
	return (s.size() >= suffix.size() && std::equal(suffix.rbegin(), suffix.rend(), s.rbegin()));
}

// A bare "." or ".." counts too: NSURL resolves a relative path before Path::Impl can see the dot segment.
static bool isDirPath(const std::wstring& path) {
	return (path == L"." || path == L".." || endsWith(path, L"/") || endsWith(path, L"/.") || endsWith(path, L"/.."));
}

static NSString* toNSString(const std::wstring& s) {
	assert(sizeof (wchar_t) == 4);
	const wchar_t* bytes = (s.empty() ? 0 : &s[0]);
	return [[[NSString alloc] initWithBytes:bytes
			length:s.size() * sizeof (wchar_t) encoding:NSUTF32LittleEndianStringEncoding] autorelease];
}

// Converts a path string from a caller, refusing a NUL: the system calls would stop at it.
static NSString* pathToNSString(const std::wstring& s) {
	if (s.find(L'\0') != std::wstring::npos) {
		throw Exception("Invalid character in path");
	}
	return toNSString(s);
}

static std::wstring toStdWString(const NSString* s) {
	assert(s != nil);
	assert(sizeof (wchar_t) == 4);
	NSString* sNFC = [s precomposedStringWithCanonicalMapping];
	NSData* data = [sNFC dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
	const wchar_t* b = reinterpret_cast<const wchar_t*>([data bytes]);
	return std::wstring(b, b + [data length] / sizeof (wchar_t));
}

static std::string toUTF8String(const NSString* s) {
	assert(s != nil);
	assert(sizeof (char) == 1);
	NSString* sNFC = [s precomposedStringWithCanonicalMapping];
	NSData* data = [sNFC dataUsingEncoding:NSUTF8StringEncoding];
	const char* b = reinterpret_cast<const char*>([data bytes]);
	return std::string(b, b + [data length] / sizeof (char));
}

static bool toBool(id value) {
	return (value != nil
			&& [value isKindOfClass:[NSNumber class]]
			&& [((NSNumber*) value) boolValue] != NO);
}

std::string Exception::describe() const {
	if (descriptionUTF8.empty()) {
		std::ostringstream message;
		message << errorStringUTF8;
		if (!path.isNull()) {
			const std::wstring fullPath = path.getFullPath();
			std::string utf8Path;
			@autoreleasepool { // what() may run on a thread with no pool of its own.
				utf8Path = toUTF8String(toNSString(fullPath));
			}
			message << " : " << utf8Path;
		}
		if (errorCode != 0) {
			message << " [" << errorCode << ']';
		}
		descriptionUTF8 = message.str();
	}
	
	return descriptionUTF8;
}

static bool hasDirectoryPath(NSURL* url) {
	if ([NSURL instancesRespondToSelector:@selector(hasDirectoryPath)] != NO) {
		return ([url hasDirectoryPath] != NO);
	} else {
		return ([[url absoluteString] hasSuffix:@"/"] != NO);
	}
}

/* --- PathTime --- */

PathTime::PathTime(time_t cTime) {
	const int64_t nsSince1970 = static_cast<int64_t>(cTime) * 1000000;
	high = static_cast<int>(nsSince1970 >> 32);
	low = static_cast<unsigned int>(nsSince1970);
	assert(convertToCTime() == cTime);
}

time_t PathTime::convertToCTime() const {
	const int64_t nsSince1970 = static_cast<int64_t>((static_cast<uint64_t>(static_cast<unsigned int>(getHigh())) << 32) | getLow());
	if (nsSince1970 < 0) {
		return 0;
	} else if (static_cast<uint64_t>(nsSince1970)
			> static_cast<uint64_t>(std::numeric_limits<time_t>::max()) * 1000000) {
		return std::numeric_limits<time_t>::max();
	} else {
		return (nsSince1970 + 500000) / 1000000;
	}
}

static PathTime toPathTime(id dateValue) {
	if (dateValue != nil && [dateValue isKindOfClass:[NSDate class]]) {
		const int64_t nsSince1970 = static_cast<int64_t>(floor([((NSDate*) dateValue) timeIntervalSince1970] * 1000000.0 + 0.5));
		return PathTime(static_cast<int>(nsSince1970 >> 32), static_cast<unsigned int>(nsSince1970));
	} else {
		return PathTime();
	}
}

static NSDate* toNSDate(const PathTime& pathTime) {
	if (!pathTime.isAvailable()) {
		return nil;
	} else {
		const int64_t nsSince1970 = static_cast<int64_t>((static_cast<uint64_t>(static_cast<unsigned int>(pathTime.getHigh())) << 32)
				| pathTime.getLow());
		const NSTimeInterval secsSince1970 = nsSince1970 / 1000000.0;
		NSDate* date = [NSDate dateWithTimeIntervalSince1970:secsSince1970];
		assert(toPathTime(date) == pathTime);
		return date;
	}
}

/* --- Path::Impl --- */

/*
	Collapses ".", ".." and repeated slashes in an absolute path by text alone, like the POSIX backend. The same string
	therefore always gives the same Path, whatever exists on disk. (URLByStandardizingPath and stringByStandardizingPath
	also strip a leading /private, but only when the shorter path exists, so a Path built before a file was created
	would not equal one built after.) Sets `endsInDotSegment` if the last component was "." or "..", which makes the
	result a directory.
*/
static NSString* collapseDotSegments(NSString* absolutePath, bool& endsInDotSegment) {
	NSMutableArray* kept = [NSMutableArray array];
	endsInDotSegment = false;
	for (NSString* part in [absolutePath componentsSeparatedByString:@"/"]) {
		if ([part length] == 0) {
			continue;
		}
		endsInDotSegment = ([part isEqualToString:@"."] || [part isEqualToString:@".."]);
		if ([part isEqualToString:@".."]) {
			if ([kept count] != 0) {
				[kept removeLastObject];
			}
		} else if (!endsInDotSegment) {
			[kept addObject:part];
		}
	}
	return [@"/" stringByAppendingString:[kept componentsJoinedByString:@"/"]];
}

Path::Impl::Impl(NSURL* input) : url(nil) {
	assert(input != nil);
	NSURL* filePathURL = [input filePathURL];
	if (filePathURL == nil) {
		const std::string utf8PathString = [[input path] UTF8String];
		throw Exception(std::string("Error resolving file reference URL : ") + utf8PathString);
	}
	bool endsInDotSegment;
	NSString* path = collapseDotSegments([[filePathURL absoluteURL] path], endsInDotSegment);
	const bool isDirectory = (hasDirectoryPath(filePathURL) || endsInDotSegment || [path isEqualToString:@"/"]);
	NSURL* normalized = [NSURL fileURLWithPath:[path precomposedStringWithCanonicalMapping] isDirectory:isDirectory];
	assert(normalized != nil); // this should never happen
	url = [normalized retain];
}

Path::Impl::Impl(const Impl& that) : url(that.url) {
	assert(url != nil);
	[url retain];
}

Path::Impl& Path::Impl::operator=(const Impl& that) {
	assert(url != nil);
	assert(that.url != nil);
	NSURL* mine = url;
	[that.url retain];
	url = that.url;
	[mine release];
	return *this;
}

NSURL* Path::Impl::getNSURL() {
	return url;
}

Path::Impl::~Impl() {
	assert(url != nil);
	[url release];
	url = 0;
}

/* --- Path --- */

wchar_t Path::getSeparator() {
	return L'/';
}

std::wstring Path::appendSeparator(const std::wstring& path) {
	return (gotTrailingSlash(path) ? path : path + L'/');
}

std::wstring Path::removeSeparator(const std::wstring& path) {
	return (gotTrailingSlash(path) ? std::wstring(path.begin(), path.end() - 1) : path);
}

bool Path::isValidChar(wchar_t c) {
	return (c != L':' && c >= 32);
}

Path Path::getCurrentDirectoryPath() {
	@autoreleasepool {
		NSString* path = [[NSFileManager defaultManager] currentDirectoryPath];
		assert(path != nil); // should never happen
		NSURL* url = [NSURL fileURLWithPath:path isDirectory:YES];
		assert(url != nil); // should never happen
		return (path == nil ? Path() : Path(new Impl(url)));
	}
}

void Path::listRoots(std::vector<Path>& roots) {
	@autoreleasepool {
		NSURL* url = [NSURL fileURLWithPath:NSOpenStepRootDirectory() isDirectory:YES];
		assert(url != nil); // should never happen
		roots.push_back(Path(new Impl(url)));
	}
}

Path::Path(const std::wstring& pathString) : impl(0) {
	@autoreleasepool {
		NSString* pathStringNS = pathToNSString(pathString);
		NSURL* url = [NSURL fileURLWithPath:pathStringNS isDirectory:(isDirPath(pathString) ? YES : NO)];
		if (url == nil) {
			const std::string utf8PathString = [pathStringNS UTF8String];
			throw Exception(std::string("Error creating file path for : ") + utf8PathString);
		}
		impl = new Impl(url);
	}
}

Path::Path(const Path& copy) : impl((copy.impl != 0) ? new Impl(*copy.impl) : 0) { }

Path& Path::operator=(const Path& copy) {
	Impl* oldImpl = impl;
	impl = ((copy.impl != 0) ? new Impl(*copy.impl) : 0);
	delete oldImpl;
	return (*this);
}

bool Path::isDirectoryPath() const {
	return (!isNull() && hasDirectoryPath(impl->url));
}

bool Path::isRoot() const {
	@autoreleasepool {
		return (!isNull() && [[impl->url path] isEqual:NSOpenStepRootDirectory()] != NO);
	}
}

int Path::compare(const Path& other) const {
	@autoreleasepool {
		if (this == &other) {
			return 0;
		} else if (impl == 0 || other.impl == 0) {
			return (impl != 0 ? 1 : 0) - (other.impl != 0 ? 1 : 0);
		} else if ([impl->url isEqual:other.impl->url] != NO) {
			return 0;
		} else {
			switch ([[impl->url path] localizedStandardCompare:[other.impl->url path]]) {
				case NSOrderedAscending: return -1;
				case NSOrderedDescending: return 1;
				case NSOrderedSame: {
					assert([[impl->url path] compare:[other.impl->url path]] == NSOrderedSame);
					return 0;
				}
				default: assert(0);
			}
		}
	}
	return 0;
}

bool Path::operator==(const Path& other) const {
	return (compare(other) == 0);
}

bool Path::equals(const Path& other) const {
	@autoreleasepool {
		if (this == &other) {
			return true;
		} else if (impl == 0 || other.impl == 0) {
			return (impl == 0 && other.impl == 0);
		} else if ([impl->url isEqual:other.impl->url] != NO) {
			return true;
		} else {
			return [[impl->url path] compare:[other.impl->url path]
					options:NSCaseInsensitiveSearch | kCFCompareWidthInsensitive] == NSOrderedSame;
		}
	}
}

Path Path::getParent() const {
	assert(!isNull());
	assert(!isRoot());
	@autoreleasepool {
		return Path(new Impl([impl->url URLByDeletingLastPathComponent]));
	}
}

Path Path::getRelative(const std::wstring& pathString) const {
	assert(!isNull());
	@autoreleasepool {
		if (pathString.size() >= 1 && pathString[0] == '/') {
			return Path(pathString);
		} else {
			NSString* pathStringNS = pathToNSString(pathString);
			NSURL* url = [impl->url URLByAppendingPathComponent:pathStringNS
					isDirectory:(isDirPath(pathString) ? YES : NO)];
			if (url == nil) {
				const std::string utf8PathString = [pathStringNS UTF8String];
				throw Exception(std::string("Error creating file path for : ") + utf8PathString);
			}
			return Path(new Impl(url));
		}
	}
}

Path Path::withoutExtension() const {
	assert(!isNull());
	@autoreleasepool {
		if ([[impl->url pathExtension] length] != 0) {
			return Path(new Impl([impl->url URLByDeletingPathExtension]));
		} else {
			return *this;
		}
	}
}

Path Path::withExtension(const std::wstring& extensionString) const {
	assert(!isNull());
	@autoreleasepool {
		NSString* extension = pathToNSString(extensionString);
		NSURL* url = impl->url;
		if ([[impl->url pathExtension] length] != 0) {
			url = [url URLByDeletingPathExtension];
		}
		return Path(new Impl([url URLByAppendingPathExtension:extension]));
	}
}

bool Path::hasExtension() const {
	assert(!isNull());
	@autoreleasepool {
		return ([[impl->url pathExtension] length] != 0);
	}
}

std::wstring Path::getName() const {
	assert(!isNull());
	@autoreleasepool {
		NSURL* url = impl->url;
		if ([[impl->url pathExtension] length] != 0) {
			url = [url URLByDeletingPathExtension];
		}
		const std::wstring name = toStdWString([url lastPathComponent]);
		return (name == L"/" ? std::wstring() : name); // NuXFiles doesn't consider the name of the root to be /, but empty
	}
}

std::wstring Path::getExtension() const {
	assert(!isNull());
	@autoreleasepool {
		return toStdWString([impl->url pathExtension]);
	}
}

std::wstring Path::getNameWithExtension() const {
	assert(!isNull());
	@autoreleasepool {
		const std::wstring name = toStdWString([impl->url lastPathComponent]);
		return (name == L"/" ? std::wstring() : name); // NuXFiles doesn't consider the name of the root to be /, but empty
	}
}

std::wstring Path::getFullPath() const {
	assert(!isNull());
	@autoreleasepool {
		std::wstring path = toStdWString([impl->url path]);
		if (hasDirectoryPath(impl->url)) {
			path = appendSeparator(path);
		}
		return path;
	}
}

static bool urlMatches(NSURL* fileURL, bool excludeHidden, bool excludeDirectories, bool excludeFiles
		, NSString* includeExtension, NSString* includeHFSType) {
	if (excludeHidden) {
		NSValue* value;
		NSError* error = nil;
		const BOOL success = [fileURL getResourceValue:&value forKey:NSURLIsHiddenKey error:&error];
		if (success && toBool(value)) {
			return false;
		}
	}
	const bool isDir = hasDirectoryPath(fileURL);
	if (excludeDirectories && isDir) {
		return false;
	}
	if (excludeFiles && !isDir) {
		return false;
	}
	if ((includeExtension == nil && includeHFSType == nil)
			|| (includeExtension != nil && [[fileURL pathExtension] caseInsensitiveCompare:includeExtension] == NSOrderedSame)
			|| (includeHFSType != nil && [NSHFSTypeOfFile([fileURL path]) isEqual:includeHFSType] != NO)) {
		return true;
	}
	return false;
}

/*
	As on Win32 and POSIX, a path that doesn't exist matches nothing, and whether it is a directory comes from the file
	system rather than from a trailing slash in the path.
*/
bool Path::matchesFilter(const PathListFilter& filter) const {
	assert(!isNull());

	@autoreleasepool {
		BOOL isDirectory = NO;
		if ([[NSFileManager defaultManager] fileExistsAtPath:[impl->url path] isDirectory:&isDirectory] == NO) {
			return false;
		}
		NSURL* url = [NSURL fileURLWithPath:[impl->url path] isDirectory:isDirectory];
		NSString* includeExtension = (filter.includeExtension.empty() ? nil : toNSString(filter.includeExtension));
		NSString* includeHFSType = (filter.includeMacFileType == 0 ? nil : NSFileTypeForHFSTypeCode(filter.includeMacFileType));
		return urlMatches(url, filter.excludeHidden, filter.excludeDirectories, filter.excludeFiles
				, includeExtension, includeHFSType);
	}
}

void Path::listSubPaths(std::vector<Path>& subPaths, const PathListFilter& filter) const {
	assert(!isNull());

	@autoreleasepool {
		NSError* error = nil;
		NSFileManager* fileManager = [NSFileManager defaultManager];
		NSArray<NSString*>* filenames = [fileManager contentsOfDirectoryAtPath:[impl->url path] error:&error];
		if (filenames == nil) {
			assert(error != nil);
			throw Exception("Error listing file directory", *this, static_cast<int>([error code]));
		}

		NSString* includeExtension = (filter.includeExtension.empty() ? nil : toNSString(filter.includeExtension));
		NSString* includeHFSType = (filter.includeMacFileType == 0 ? nil : NSFileTypeForHFSTypeCode(filter.includeMacFileType));
		
		for (id name in filenames) {
			NSURL* fileURL = [impl->url URLByAppendingPathComponent:name];
			assert(fileURL != nil); // should not happen
			if (urlMatches(fileURL, filter.excludeHidden, filter.excludeDirectories, filter.excludeFiles
					, includeExtension, includeHFSType)) {
				subPaths.push_back(Path(new Impl(fileURL)));
			}
		}
	}
}

bool Path::exists() const {
	assert(!isNull());
	@autoreleasepool {
		return ([[NSFileManager defaultManager] fileExistsAtPath:[impl->url path]] != NO);
	}
}

bool Path::isFile() const {
	assert(!isNull());
	@autoreleasepool {
		BOOL isDir;
		return ([[NSFileManager defaultManager] fileExistsAtPath:[impl->url path] isDirectory:&isDir] != NO && isDir == NO);
	}
}

bool Path::isDirectory() const {
	assert(!isNull());
	@autoreleasepool {
		BOOL isDir;
		return ([[NSFileManager defaultManager] fileExistsAtPath:[impl->url path] isDirectory:&isDir] != NO && isDir != NO);
	}
}

PathInfo Path::getInfo() const {
	assert(!isNull());

	@autoreleasepool {
		/*
			An NSURL caches resource values until a pass through the run loop, and copies of a Path share their NSURL, so
			a worker thread or a command-line tool would see stale values. A fresh NSURL has nothing cached.
		*/
		NSURL* url = [NSURL fileURLWithPath:[impl->url path] isDirectory:(hasDirectoryPath(impl->url) ? YES : NO)];
		NSError* error = nil;
		NSDictionary<NSFileAttributeKey, id>* attributes;
		NSDictionary<NSURLResourceKey, id>* resourceValues
				= [url resourceValuesForKeys:@[ NSURLIsDirectoryKey, NSURLFileSizeKey
				, NSURLContentModificationDateKey, NSURLCreationDateKey, NSURLIsHiddenKey
				, NSURLIsUserImmutableKey, NSURLContentAccessDateKey ] error:&error];
		if (resourceValues != nil) {
			attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[impl->url path] error:&error];
		}
		if (resourceValues == nil || attributes == nil) {
			assert(error != nil);
			throw Exception("Error obtaining file or directory info", *this, static_cast<int>([error code]));
		}
		
		PathInfo info;
		
		info.isDirectory = toBool(resourceValues[NSURLIsDirectoryKey]);
		info.fileSize = Int64(0, 0);
		if (!info.isDirectory) {
			id fileSizeValue = resourceValues[NSURLFileSizeKey];
			if (fileSizeValue != nil && [fileSizeValue isKindOfClass:[NSNumber class]]) {
				const uint64_t fileSize = [((NSNumber*) fileSizeValue) unsignedLongLongValue];
				info.fileSize = Int64(static_cast<int>(fileSize >> 32), static_cast<unsigned int>(fileSize));
			}
		}
		info.creationTime = toPathTime(resourceValues[NSURLCreationDateKey]);
		info.modificationTime = toPathTime(resourceValues[NSURLContentModificationDateKey]);
		info.lastAccessTime = toPathTime(resourceValues[NSURLContentAccessDateKey]);
		info.attributes.isReadOnly = toBool(resourceValues[NSURLIsUserImmutableKey]);
		info.attributes.isHidden = toBool(resourceValues[NSURLIsHiddenKey]);

		/* alternatives:
		
		NSString* fileType = [attributes fileType];
		info.isDirectory = (fileType != nil && [fileType isEqual:NSFileTypeDirectory] != NO);

		if (info.isDirectory) {
			info.fileSize = Int64(0, 0);
		} else {
			const uint64_t fileSize = [attributes fileSize];
			info.fileSize = Int64(static_cast<int>(fileSize >> 32), static_cast<unsigned int>(fileSize));
		}

		info.creationTime = toPathTime([attributes fileCreationDate]);
		info.modificationTime = toPathTime([attributes fileModificationDate]);
		info.attributes.isReadOnly = ([attributes fileIsImmutable] != NO);
		info.attributes.isHidden = ([[[impl->url URLByDeletingPathExtension] lastPathComponent] characterAtIndex:0] == L'.');

		*/
		
		info.attributes.win32Attributes = 0;
		info.attributes.macFileType = [attributes fileHFSTypeCode];
		info.attributes.macFileCreator = [attributes fileHFSCreatorCode];
		
		return info;
	}
}

void Path::updateAttributes(const PathAttributes& newAttributes) const {
	assert(!isNull());
	
	@autoreleasepool {
		NSDictionary<NSURLResourceKey, id>* resourceValues = @{
			NSURLIsHiddenKey: [NSNumber numberWithBool:newAttributes.isHidden],
			NSURLIsUserImmutableKey: [NSNumber numberWithBool:newAttributes.isReadOnly]
		};
		NSDictionary<NSFileAttributeKey, id>* attributesDictionary = @{
			NSFileHFSTypeCode: [NSNumber numberWithUnsignedInt:newAttributes.macFileType],
			NSFileHFSCreatorCode: [NSNumber numberWithUnsignedInt:newAttributes.macFileCreator]
		};
		NSError* error = nil;
		BOOL success = false;
		if (!newAttributes.isReadOnly) {
			success = ([impl->url setResourceValues:resourceValues error:&error] != NO
					&& [[NSFileManager defaultManager] setAttributes:attributesDictionary ofItemAtPath:[impl->url path] error:&error] != NO);
		} else {
			success = ([[NSFileManager defaultManager] setAttributes:attributesDictionary ofItemAtPath:[impl->url path] error:&error] != NO
					&& [impl->url setResourceValues:resourceValues error:&error] != NO);
		}
		if (!success) {
			assert(error != nil);
			throw Exception("Error updating attributes on file or directory", *this, static_cast<int>([error code]));
		}
	}
}

void Path::updateTimes(const PathTime& newCreationTime, const PathTime& newModificationTime, const PathTime& newAccessTime) const {
	@autoreleasepool {
		NSMutableDictionary<NSURLResourceKey, id>* resourceValues = [NSMutableDictionary dictionaryWithCapacity:3];
		if (newCreationTime.isAvailable()) {
			resourceValues[NSURLCreationDateKey] = toNSDate(newCreationTime);
		}
		if (newModificationTime.isAvailable()) {
			resourceValues[NSURLContentModificationDateKey] = toNSDate(newModificationTime);
		}
		if (newAccessTime.isAvailable()) {
			resourceValues[NSURLContentAccessDateKey] = toNSDate(newAccessTime);
		}
		NSError* error = nil;
		if ([impl->url setResourceValues:resourceValues error:&error] == NO) {
			assert(error != nil);
			throw Exception("Error updating time info on file or directory", *this, static_cast<int>([error code]));
		}
	}
}

void Path::create() const {
	NSError* error;
	@autoreleasepool {
		if ([[NSFileManager defaultManager] createDirectoryAtURL:impl->url withIntermediateDirectories:NO
				attributes:nil error:&error] == NO) {
			throw Exception("Error creating directory", (*this), static_cast<int>([error code]));
		}
	}
}

bool Path::tryToCreate() const {
	@autoreleasepool {
		NSError* error;
		return ([[NSFileManager defaultManager] createDirectoryAtURL:impl->url withIntermediateDirectories:NO
				attributes:nil error:&error] != NO);
	}
}

void Path::copy(const Path& destination) const {
	assert(!isNull());
	assert(!destination.isNull());
	
	@autoreleasepool {
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		NSError* error = nil;
		BOOL isDir;
		if ([manager fileExistsAtPath:[impl->url path] isDirectory:&isDir] && isDir) {
			throw Exception("Error copying file (path is a directory)", (*this), NSFeatureUnsupportedError);
		}
		if ([manager copyItemAtURL:impl->url toURL:destination.impl->url error:&error] == NO) {
			assert(error != nil);
			throw Exception("Error copying file", (*this), static_cast<int>([error code]));
		}
	}
}

/*
	moveRename() stays within one volume on every backend. moveItemAtURL: would copy a file or a whole directory tree to
	another volume and then delete the original, so the volumes are compared first. (Within a volume it is a rename, and
	it refuses an existing destination.)
*/
void Path::moveRename(const Path& destination) const {
	assert(!isNull());
	assert(!destination.isNull());

	@autoreleasepool {
		id sourceVolume = nil;
		id destinationVolume = nil;
		[impl->url getResourceValue:&sourceVolume forKey:NSURLVolumeIdentifierKey error:nil];
		[[destination.impl->url URLByDeletingLastPathComponent] getResourceValue:&destinationVolume
				forKey:NSURLVolumeIdentifierKey error:nil];
		if (sourceVolume != nil && destinationVolume != nil && [sourceVolume isEqual:destinationVolume] == NO) {
			throw Exception("Error renaming or moving file or directory (different volume)", (*this), EXDEV);
		}
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		NSError* error = nil;
		if ([manager moveItemAtURL:impl->url toURL:destination.impl->url error:&error] == NO) {
			assert(error != nil);
			throw Exception("Error renaming or moving file or directory", (*this), static_cast<int>([error code]));
		}
	}
}

enum Erasability { ERASABLE, NOT_EMPTY, UNEXAMINABLE };

/*
	removeItemAtURL: deletes directories recursively, but erase() may only remove an empty directory, so a directory is
	listed first. The type comes from attributesOfItemAtPath:, which does not follow a final symlink: a link is removed
	itself (removeItemAtURL: never touches its target), whatever the target holds. A directory that cannot be listed is
	refused rather than deleted recursively.
*/
static Erasability checkErasable(NSFileManager* manager, NSURL* url, NSError** error) {
	NSDictionary* attributes = [manager attributesOfItemAtPath:[url path] error:error];
	if (attributes == nil) {
		return UNEXAMINABLE;
	}
	if ([[attributes fileType] isEqualToString:NSFileTypeDirectory]) {
		NSArray* contents = [manager contentsOfDirectoryAtURL:url includingPropertiesForKeys:nil options:0 error:error];
		if (contents == nil) {
			return UNEXAMINABLE;
		}
		if ([contents count] != 0) {
			return NOT_EMPTY;
		}
	}
	return ERASABLE;
}

bool Path::tryToErase() const {
	assert(!isNull());
	@autoreleasepool {
		NSError* dummyError = nil;
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		if (checkErasable(manager, impl->url, &dummyError) != ERASABLE) {
			return NO;
		}
		return ([manager removeItemAtURL:impl->url error:&dummyError] != NO);
	}
}

void Path::erase() const {
	assert(!isNull());
	@autoreleasepool {
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		NSError* error = nil;
		switch (checkErasable(manager, impl->url, &error)) {
			case ERASABLE: break;
			case NOT_EMPTY: throw Exception("Error deleting directory (not empty)", (*this), NSFileWriteFileExistsError);
			case UNEXAMINABLE: {
				assert(error != nil);
				throw Exception("Error deleting file or directory", (*this), static_cast<int>([error code]));
			}
		}
		error = nil;
		if ([manager removeItemAtURL:impl->url error:&error] == NO) {
			assert(error != nil);
			throw Exception("Error deleting file or directory", (*this), static_cast<int>([error code]));
		}
	}
}

Path Path::createTempFile() const {
	assert(!isNull());

	@autoreleasepool {
		NSURL* underURL = impl->url;
		if (!hasDirectoryPath(underURL)) {
			underURL = [underURL URLByDeletingLastPathComponent];
		}

		// O_EXCL checks for an existing file and creates the new one in a single step, and a failure leaves errno.
		// Mode 0666 lets the umask apply as for any other new file.
		for (int attempt = 0; attempt < 100; ++attempt) {
			NSString* uuidString = [[NSUUID UUID] UUIDString];
			NSURL* tempURL = [[underURL URLByAppendingPathComponent:uuidString isDirectory:NO] URLByAppendingPathExtension:[impl->url pathExtension]];
			assert(tempURL != nil); // should not happen
			int fd;
			do {
				fd = ::open([[tempURL path] fileSystemRepresentation], O_RDWR | O_CREAT | O_EXCL, 0666);
			} while (fd < 0 && errno == EINTR);
			if (fd >= 0) {
				::close(fd);
				return Path(new Impl(tempURL));
			}
			if (errno != EEXIST) {
				throw Exception("Error creating temp", *this, errno);
			}
		}
		throw Exception("Error creating temp", *this, EEXIST);
	}
}

/*
	glob() reads wildcards in every part of a pattern, so the directory part gets [ ] * ? and the escape character itself
	escaped and names its folders literally, as on Windows. Only the last name is a pattern; a trailing slash is not one.
*/
static std::string escapeGlobDirectory(const std::string& pattern) {
	const size_t searchFrom = (pattern.size() >= 2 && pattern[pattern.size() - 1] == '/'
			? pattern.size() - 2 : std::string::npos);
	const size_t lastSlash = pattern.rfind('/', searchFrom);
	if (lastSlash == std::string::npos) {
		return pattern;
	}
	std::string escaped;
	for (size_t i = 0; i <= lastSlash; ++i) {
		const char c = pattern[i];
		if (c == '[' || c == ']' || c == '*' || c == '?' || c == '\\') {
			escaped += '\\';
		}
		escaped += c;
	}
	escaped.append(pattern, lastSlash + 1, std::string::npos);
	return escaped;
}

/*
	Expands the wildcard pattern with glob(), as the POSIX backend does, and keeps the matches that pass `filter`.
	GLOB_MARK appends a slash to directories so they become directory paths.
*/
void Path::findPaths(std::vector<Path>& paths, const std::wstring& wildcardPattern, const PathListFilter& filter) {
	@autoreleasepool {
		glob_t g;
		const std::string pattern = escapeGlobDirectory([pathToNSString(wildcardPattern) fileSystemRepresentation]);
		const int result = ::glob(pattern.c_str(), GLOB_MARK, 0, &g);
		try {
			if (result == 0) {
				NSFileManager* manager = [NSFileManager defaultManager];
				for (size_t i = 0; i < static_cast<size_t>(g.gl_pathc); ++i) {
					NSString* found = [manager stringWithFileSystemRepresentation:g.gl_pathv[i]
							length:strlen(g.gl_pathv[i])];
					const Path path(toStdWString(found));
					if (path.matchesFilter(filter)) {
						paths.push_back(path);
					}
				}
			}
		}
		catch (...) {
			::globfree(&g);
			throw;
		}
		::globfree(&g);
	}
}

Path::~Path() {
	delete impl;
	impl = 0;
}

/* --- ReadOnlyFile::Impl --- */

ReadOnlyFile::Impl::Impl(const Path& path, NSFileHandle* fileHandle) : path(path), fileHandle(fileHandle) {
	assert(fileHandle != 0);
	[fileHandle retain];
}

NSFileHandle* ReadOnlyFile::Impl::getNSFileHandle() const {
	return fileHandle;
}

ReadOnlyFile::Impl::~Impl() {
	assert(fileHandle != 0);
	[fileHandle release];
	fileHandle = 0;
}

#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
// Returns an autoreleased handle that owns the new descriptor, or nil with `errorCode` set to errno.
static NSFileHandle* openFileHandle(NSURL* url, int flags, int& errorCode) {
	int fd;
	do {
		fd = ::open([url fileSystemRepresentation], flags, 0666);
	} while (fd < 0 && errno == EINTR);
	if (fd < 0) {
		errorCode = errno;
		return nil;
	}
	return [[[NSFileHandle alloc] initWithFileDescriptor:fd closeOnDealloc:YES] autorelease];
}
#else
// Returns an autoreleased handle for reading, or for updating when `flags` is O_RDWR, or nil with a Cocoa `errorCode`.
static NSFileHandle* openFileHandle(NSURL* url, int flags, int& errorCode) {
	assert((flags == O_RDONLY || flags == O_RDWR) && "files are created by createForWriting() here");
	NSError* error = nil;
	NSFileHandle* handle = (flags == O_RDWR ? [NSFileHandle fileHandleForUpdatingURL:url error:&error]
			: [NSFileHandle fileHandleForReadingFromURL:url error:&error]);
	if (handle == nil) {
		assert(error != nil);
		errorCode = static_cast<int>([error code]);
	}
	return handle;
}
#endif

static ReadOnlyFile::Impl* openForReading(const Path& path) {
	assert(!path.isNull());
	@autoreleasepool {
		int errorCode = 0;
		NSFileHandle* handle = openFileHandle(path.getImpl()->getNSURL(), O_RDONLY, errorCode);
		if (handle == nil) {
			throw Exception("Error opening file", path, errorCode);
		}
		return new ReadOnlyFile::Impl(path, handle);
	}
}

static ReadOnlyFile::Impl* openForUpdating(const Path& path) {
	assert(!path.isNull());
	@autoreleasepool {
		int errorCode = 0;
		NSFileHandle* handle = openFileHandle(path.getImpl()->getNSURL(), O_RDWR, errorCode);
		if (handle == nil) {
			throw Exception("Error opening file", path, errorCode);
		}
		return new ReadOnlyFile::Impl(path, handle);
	}
}

static ReadOnlyFile::Impl* createForWriting(const Path& path, const PathAttributes& attributes, bool replaceExisting) {
	assert(!path.isNull());
	@autoreleasepool {
	#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
		/*
			O_EXCL refuses an existing file in the same step that creates the new one, as on Win32 and POSIX.
			O_TRUNC keeps the old file's flags and Finder info, so every attribute is set again by path.
		*/
		int errorCode = 0;
		NSFileHandle* handle = openFileHandle(path.getImpl()->getNSURL()
				, O_RDWR | O_CREAT | (replaceExisting ? O_TRUNC : O_EXCL), errorCode);
		if (handle == nil) {
			throw Exception("Error creating file", path, errorCode);
		}
		path.updateAttributes(attributes);
		return new ReadOnlyFile::Impl(path, handle);
	#else
		NSURL* url = path.getImpl()->getNSURL();
		NSFileManager* fileManager = [NSFileManager defaultManager];
		if (!replaceExisting && [fileManager fileExistsAtPath:[url path]] != NO) {
			throw Exception("Error creating file", path, -1);
		}
		NSDictionary<NSFileAttributeKey, id>* attributesDictionary = @{
			NSFileHFSTypeCode: [NSNumber numberWithUnsignedInt:attributes.macFileType],
			NSFileHFSCreatorCode: [NSNumber numberWithUnsignedInt:attributes.macFileCreator]
		};
		[fileManager createFileAtPath:[url path] contents:[NSData data] attributes:attributesDictionary];
		NSError* error = nil;
		NSFileHandle* handle = [NSFileHandle fileHandleForUpdatingURL:url error:&error];
		if (handle == nil) {
			assert(error != nil);
			throw Exception("Error creating file", path, static_cast<int>([error code]));
		}
		/*
			createFileAtPath can fail while the old file stays writable (it writes atomically), and the handle doesn't
			truncate, so truncate here or a shorter file would keep the old tail.
		*/
		if (::ftruncate([handle fileDescriptor], 0) != 0) {
			throw Exception("Error creating file", path, errno);
		}
		NSDictionary<NSURLResourceKey, id>* resourceValues = @{
			NSURLIsHiddenKey: [NSNumber numberWithBool:attributes.isHidden],
			NSURLIsUserImmutableKey: [NSNumber numberWithBool:attributes.isReadOnly]
		};
		if ([url setResourceValues:resourceValues error:&error] == NO) {
			assert(error != nil);
			throw Exception("Error creating file", path, static_cast<int>([error code]));
		}
		return new ReadOnlyFile::Impl(path, handle);
	#endif
	}
}

static int getNSExceptionErrorCode(NSException* exception) {
	NSDictionary* dictionary = [exception userInfo];
	if (dictionary != nil) {
		NSError* error = (NSError*) dictionary[@"NSFileHandleOperationExceptionUnderlyingError"];
		if (error != nil) {
			return static_cast<int>([error code]);
		}
	}
	return 0;
}

#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
/*
	As in the POSIX backend: pread and pwrite leave the shared file position alone, so threads sharing a file can't move
	each other's reads, and the loops retry EINTR and the short transfers that network and FUSE file systems make.
*/

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
#endif

/* --- ReadOnlyFile --- */

ReadOnlyFile::ReadOnlyFile(const Path& path, bool allowConcurrentWrites) : impl(openForReading(path)) {
	assert(!path.isNull());
	(void)allowConcurrentWrites; // not supported by MacOS, concurrent writes are always legal
}

Int64 ReadOnlyFile::getSize() const {
#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
	struct stat st;
	if (::fstat([impl->fileHandle fileDescriptor], &st) != 0) {
		throw Exception("Error obtaining file size", getPath(), errno);
	}
	return Int64(static_cast<int>(st.st_size >> 32), static_cast<unsigned int>(st.st_size));
#else
	@autoreleasepool {
		@try {
			const uint64_t offset = [impl->fileHandle seekToEndOfFile];
			return Int64(static_cast<int>(offset >> 32), static_cast<unsigned int>(offset));
		}
		@catch (NSException* exception) {
			throw Exception("Error obtaining file size", getPath(), getNSExceptionErrorCode(exception));
		}
	}
#endif
}

int ReadOnlyFile::tryToRead(Int64 position, int count, unsigned char* bytes) const {
#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
	assert(count >= 0);
	const off_t fileOffset = (static_cast<off_t>(position.getHigh()) << 32) | position.getLow();
	const int fileDescriptor = [impl->fileHandle fileDescriptor];
	const ssize_t readResult = readFully(fileDescriptor, fileOffset, static_cast<size_t>(count), bytes);
	if (readResult < 0) {
		throw Exception("Error reading from file", getPath(), errno);
	}
	return static_cast<int>(readResult);
#else
	@autoreleasepool {
		@try {
			const uint64_t fileOffset = (static_cast<uint64_t>(position.getHigh()) << 32) | position.getLow();
			[impl->fileHandle seekToFileOffset:fileOffset];
			NSData* data = [impl->fileHandle readDataOfLength:count];
			const int readCount = static_cast<int>([data length]);
			[data getBytes:bytes length:readCount];
			return readCount;
		}
		@catch (NSException* exception) {
			throw Exception("Error reading from file", getPath(), getNSExceptionErrorCode(exception));
		}
	}
#endif
}

void ReadOnlyFile::read(Int64 position, int count, unsigned char* bytes) const {
	if (tryToRead(position, count, bytes) != count) {
		throw Exception("Error reading from file", getPath(), EOVERFLOW);
	}
}

Path ReadOnlyFile::getPath() const {
	return impl->path;
}

ReadOnlyFile::~ReadOnlyFile() {
	assert(impl != 0);
	@autoreleasepool {
		@try {
			[impl->fileHandle closeFile];
		}
		@catch (NSException* exception) {
			assert(0);
		}
		delete impl;
		impl = 0;
	}
}

/* --- ReadWriteFile --- */

ReadWriteFile::ReadWriteFile(const Path& path, bool allowConcurrentReads, bool allowConcurrentWrites) : ReadOnlyFile(openForUpdating(path)) {
	(void)allowConcurrentReads; // not supported by MacOS, concurrent read/writes are always legal
	(void)allowConcurrentWrites; // not supported by MacOS, concurrent read/writes are always legal
}

ReadWriteFile::ReadWriteFile(const Path& path, const PathAttributes& attributes
		, bool replaceExisting, bool allowConcurrentReads, bool allowConcurrentWrites) : ReadOnlyFile(createForWriting(path, attributes, replaceExisting)) {
	(void)allowConcurrentReads; // not supported by MacOS, concurrent read/writes are always legal
	(void)allowConcurrentWrites; // not supported by MacOS, concurrent read/writes are always legal
}

void ReadWriteFile::write(Int64 position, int count, const unsigned char* bytes) {
#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
	assert(count >= 0);
	const off_t fileOffset = (static_cast<off_t>(position.getHigh()) << 32) | position.getLow();
	if (!writeFully([impl->fileHandle fileDescriptor], fileOffset, static_cast<size_t>(count), bytes)) {
		throw Exception("Error writing to file", getPath(), errno);
	}
#else
	@autoreleasepool {
		@try {
			const uint64_t fileOffset = (static_cast<uint64_t>(position.getHigh()) << 32) | position.getLow();
			[impl->fileHandle seekToFileOffset:fileOffset];
			NSData* data = [NSData dataWithBytes:bytes length:count];
			[impl->fileHandle writeData:data];
		}
		@catch (NSException* exception) {
			throw Exception("Error writing to file", getPath(), getNSExceptionErrorCode(exception));
		}
	}
#endif
}

void ReadWriteFile::flush() {
	@autoreleasepool {
		@try {
			[impl->fileHandle synchronizeFile];
		}
		@catch (NSException* exception) {
			throw Exception("Error flushing file", getPath(), getNSExceptionErrorCode(exception));
		}
	}
}

/* --- ExchangingFile --- */

// Saving through a symlink replaces its target and keeps the link; a dangling link is replaced like any name.
static Path resolveFileLink(const Path& path) {
	@autoreleasepool {
		const char* const linkPath = [path.getImpl()->getNSURL() fileSystemRepresentation];
		struct stat st;
		if (::lstat(linkPath, &st) != 0 || !S_ISLNK(st.st_mode)) {
			return path;
		}
		char* const resolved = ::realpath(linkPath, 0);
		if (resolved == 0) {
			return path;
		}
		NSURL* const url = [NSURL fileURLWithFileSystemRepresentation:resolved isDirectory:NO relativeToURL:nil];
		::free(resolved);
		return Path(new Path::Impl(url));
	}
}

ExchangingFile::ExchangingFile(const Path& path, const PathAttributes& attributes)
		: ReadWriteFile(resolveFileLink(path).createTempFile(), attributes), originalPath(resolveFileLink(path))
		, requestedPath(path) {
	assert(!path.isNull());
	// A link retargeted between the two resolutions above would leave the temp file beside another file.
	if (!(getPath().getParent() == originalPath.getParent())) {
		getPath().tryToErase();
		throw Exception("Error creating file (the link changed while opening)", path);
	}
}

void ExchangingFile::commit() {
	bool failed = false;
	int errorCode = 0;
	@autoreleasepool {
		@try {
			if (!originalPath.isNull()) { // Already committed otherwise, and the file handle is closed.
				flush();
				NSURL* newURL = nil;
				NSError* error = nil;
				[impl->fileHandle closeFile];
				NSURL* tempURL = getPath().getImpl()->getNSURL();
				NSURL* originalURL = originalPath.getImpl()->getNSURL();
				id value;
				BOOL success = [tempURL getResourceValue:&value forKey:NSURLIsUserImmutableKey error:&error];
				assert(success != NO);
				const bool wasReadOnly = (success && toBool(value));
				if (wasReadOnly) {
					success = [tempURL setResourceValue:[NSNumber numberWithBool:NO] forKey:NSURLIsUserImmutableKey error:&error];
					assert(success != NO);
				}
				/*
					The default options keep the original's metadata (creation date, permissions, extended attributes),
					as NuXFiles.h promises. The merge can take the hidden flag and the type and creator codes from the
					original too, so the ones the caller asked for are applied again.
				*/
				NSFileManager* fileManager = [NSFileManager defaultManager];
				NSDictionary* tempAttributes = [fileManager attributesOfItemAtPath:[tempURL path] error:nil];
				const OSType typeCode = (tempAttributes != nil ? [tempAttributes fileHFSTypeCode] : 0);
				const OSType creatorCode = (tempAttributes != nil ? [tempAttributes fileHFSCreatorCode] : 0);
				id hiddenValue = nil;
				const bool wasHidden = ([tempURL getResourceValue:&hiddenValue forKey:NSURLIsHiddenKey error:nil] != NO
						&& toBool(hiddenValue));
				success = [fileManager replaceItemAtURL:originalURL
						withItemAtURL:tempURL
						backupItemName:nil
						options:0
						resultingItemURL:&newURL error:&error];
				if (success != NO) {
					assert([newURL isEqual:originalPath.getImpl()->getNSURL()]);
					// Best effort, like the permissions in the POSIX backend: some volumes can't store these.
					if (typeCode != 0 || creatorCode != 0) {
						[fileManager setAttributes:@{
								NSFileHFSTypeCode: [NSNumber numberWithUnsignedInt:typeCode],
								NSFileHFSCreatorCode: [NSNumber numberWithUnsignedInt:creatorCode]
							} ofItemAtPath:[originalURL path] error:nil];
					}
					if (wasHidden) {
						[originalURL setResourceValue:[NSNumber numberWithBool:YES] forKey:NSURLIsHiddenKey error:nil];
					}
					if (wasReadOnly) {
						success = [originalURL setResourceValue:[NSNumber numberWithBool:YES] forKey:NSURLIsUserImmutableKey error:&error];
						assert(success != NO);
					}
				}
				if (success != NO) {
					// The file stays readable after commit() (see NuXFiles.h), now as the committed original.
					ReadOnlyFile::Impl* reopened = openForReading(requestedPath);
					delete impl;
					impl = reopened;
					originalPath = NuXFiles::Path();
				} else {
					assert(error != nil);
					failed = true;
					errorCode = static_cast<int>([error code]);
				}
			}
		}
		@catch (NSException* exception) {
			failed = true;
			errorCode = getNSExceptionErrorCode(exception);
		}
	}
	if (failed) {
		assert(!originalPath.isNull());
		const NuXFiles::Path original = originalPath;
		originalPath = NuXFiles::Path();
		getPath().tryToErase(); // Don't leave the temp file behind on a failed commit.
		throw Exception("Error committing file", original, errorCode);
	}
}

ExchangingFile::~ExchangingFile() {
	if (!originalPath.isNull()) {
		getPath().tryToErase();
	}
}

} /* namespace NuX */

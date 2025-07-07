#include <sstream>
#include <limits>
#include "NuXFilesCocoa.h"

namespace NuXFiles {

static bool gotTrailingSlash(const std::wstring& path) {
	return (!path.empty() && path.back() == L'/');
}

static bool endsWith(const std::wstring& s, const std::wstring& suffix) {
    return (s.size() >= suffix.size() && std::equal(suffix.rbegin(), suffix.rend(), s.rbegin()));
}

static bool isDirPath(const std::wstring& path) {
	return (endsWith(path, L"/") || endsWith(path, L"/.") || endsWith(path, L"/.."));
}

static NSString* toNSString(const std::wstring& s) {
	assert(sizeof (wchar_t) == 4);
	return [[[NSString alloc]
			initWithBytes:s.data()
			length:s.size() * sizeof (wchar_t) encoding:NSUTF32LittleEndianStringEncoding] autorelease];
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
			const std::string utf8Path = toUTF8String(toNSString(fullPath));
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

static NSURL* fixedStandardizePath(NSURL* url) {
	if (url != nil) {
		url = [url filePathURL];
		assert(url != nil); // url must be a file path url
		url = [url URLByStandardizingPath];
		assert(url != nil); // this should never happen
		if ([[url path] isEqual:@"/."]) {		// URLByStandardizingPath has a "bug" where /./ doesn't resolve into /
			url = [NSURL fileURLWithPath:@"/" isDirectory:YES];
		} else {
			NSString* nfcPath = [[url path] precomposedStringWithCanonicalMapping];
			url = [NSURL fileURLWithPath:nfcPath isDirectory:hasDirectoryPath(url)];
			assert(url != nil); // this should never happen
		}
	}
	return url;
}

/* --- PathTime --- */

PathTime::PathTime(time_t cTime) {
	const long long nsSince1970 = static_cast<long long>(cTime) * 1000000;
	high = static_cast<int>(nsSince1970 >> 32);
	low = static_cast<unsigned int>(nsSince1970);
	assert(convertToCTime() == cTime);
}

time_t PathTime::convertToCTime() const {
	const long long nsSince1970 = (static_cast<long long>(getHigh()) << 32) | getLow();
	if (nsSince1970 < 0) {
		return 0;
	} else if (static_cast<unsigned long long>(nsSince1970)
			> static_cast<unsigned long long>(std::numeric_limits<time_t>::max()) * 1000000) {
		return std::numeric_limits<time_t>::max();
	} else {
		return (nsSince1970 + 500000) / 1000000;
	}
}

static PathTime toPathTime(id dateValue) {
	if (dateValue != nil && [dateValue isKindOfClass:[NSDate class]]) {
		const long long nsSince1970 = static_cast<long long>(floor([((NSDate*) dateValue) timeIntervalSince1970] * 1000000.0 + 0.5));
		return PathTime(static_cast<int>(nsSince1970 >> 32), static_cast<unsigned int>(nsSince1970));
	} else {
		return PathTime();
	}
}

static NSDate* toNSDate(const PathTime& pathTime) {
	if (!pathTime.isAvailable()) {
		return nil;
	} else {
		const long long nsSince1970 = (static_cast<unsigned long long>(pathTime.getHigh()) << 32) | pathTime.getLow();
		const NSTimeInterval secsSince1970 = nsSince1970 / 1000000.0;
		NSDate* date = [NSDate dateWithTimeIntervalSince1970:secsSince1970];
		assert(toPathTime(date) == pathTime);
		return date;
	}
}

/* --- PathAttributes --- */

PathAttributes::PathAttributes()
	: isReadOnly(false)
	, isHidden(false)
	, win32Attributes(0)
	, macFileType(0)
	, macFileCreator(0)
{
}

/* --- Path::Impl --- */

Path::Impl::Impl(NSURL* url) : url(url) {
	assert(url != nil);
	assert(![url isFileReferenceURL]);
	[url retain];
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
	(void)c;
	// FIX
	return true;
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
		NSString* pathStringNS = toNSString(pathString);
		NSURL* url = [NSURL fileURLWithPath:pathStringNS isDirectory:(isDirPath(pathString) ? YES : NO)];
		if (url == nil) {
			const std::string utf8PathString = [pathStringNS UTF8String];
			throw Exception(std::string("Error creating file path for : ") + utf8PathString);
		}
		impl = new Impl(fixedStandardizePath(url));
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
			NSString* pathStringNS = toNSString(pathString);
			NSURL* url = [impl->url URLByAppendingPathComponent:pathStringNS
					isDirectory:(isDirPath(pathString) ? YES : NO)];
			if (url == nil) {
				const std::string utf8PathString = [pathStringNS UTF8String];
				throw Exception(std::string("Error creating file path for : ") + utf8PathString);
			}
			return Path(new Impl(fixedStandardizePath(url)));
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
		NSURL* url = impl->url;
		if ([[impl->url pathExtension] length] != 0) {
			url = [url URLByDeletingPathExtension];
		}
		return Path(new Impl([url URLByAppendingPathExtension:toNSString(extensionString)]));
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

bool Path::matchesFilter(const PathListFilter& filter) const {
	assert(!isNull());

	@autoreleasepool {
		NSString* includeExtension = (filter.includeExtension.empty() ? nil : toNSString(filter.includeExtension));
		NSString* includeHFSType = (filter.includeMacFileType == 0 ? nil : NSFileTypeForHFSTypeCode(filter.includeMacFileType));
		return urlMatches(impl->url, filter.excludeHidden, filter.excludeDirectories, filter.excludeFiles
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
		NSError* error = nil;
		NSDictionary<NSFileAttributeKey, id>* attributes;
		NSDictionary<NSURLResourceKey, id>* resourceValues
				= [impl->url resourceValuesForKeys:@[ NSURLIsDirectoryKey, NSURLFileSizeKey
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
				const unsigned long long fileSize = [((NSNumber*) fileSizeValue) unsignedLongLongValue];
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
			const unsigned long long fileSize = [attributes fileSize];
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

void Path::moveRename(const Path& destination) const {
	assert(!isNull());
	assert(!destination.isNull());
	
	@autoreleasepool {
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		NSError* error = nil;
		if ([manager moveItemAtURL:impl->url toURL:destination.impl->url error:&error] == NO) {
			assert(error != nil);
			throw Exception("Error renaming or moving file or directory", (*this), static_cast<int>([error code]));
		}
	}
}

bool Path::tryToErase() const {
	assert(!isNull());
	@autoreleasepool {
		NSError* dummyError = nil;
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		BOOL isDir;
		if ([manager fileExistsAtPath:[impl->url path] isDirectory:&isDir] && isDir) {
			NSArray* contents = [manager contentsOfDirectoryAtURL:impl->url includingPropertiesForKeys:nil options:0 error:&dummyError];
			if (contents != nil && [contents count] != 0) {
				return NO;
			}
		}
		return ([manager removeItemAtURL:impl->url error:&dummyError] != NO);
	}
}

void Path::erase() const {
	assert(!isNull());
	@autoreleasepool {
		NSFileManager* manager = [[[NSFileManager alloc] init] autorelease];	// removeItemAtURL uses delegate so better create our own manager
		BOOL isDir;
		if ([manager fileExistsAtPath:[impl->url path] isDirectory:&isDir] && isDir) {
			NSError* dummyError = nil;
			NSArray* contents = [manager contentsOfDirectoryAtURL:impl->url includingPropertiesForKeys:nil options:0 error:&dummyError];
			if (contents != nil && [contents count] != 0) {
				throw Exception("Error deleting directory (not empty)", (*this), NSFileWriteFileExistsError);
			}
		}
		NSError* error = nil;
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

		NSFileManager* fileManager = [NSFileManager defaultManager];
		
		NSURL* tempURL;
		NSString* tempPath;
		do {
			NSString* uuidString = [[NSUUID UUID] UUIDString];
			tempURL = [[underURL URLByAppendingPathComponent:uuidString isDirectory:NO] URLByAppendingPathExtension:[impl->url pathExtension]];
			assert(tempURL != nil); // should not happen
			tempPath = [tempURL path];
			// I mean, this really should never ever happen with UUID, but well...
			if ([fileManager fileExistsAtPath:tempPath] == NO) {
				break;
			}
		} while (true);
		
		[fileManager createFileAtPath:tempPath contents:[NSData data] attributes:nil];
		return Path(new Impl(tempURL));
	}
}

void Path::findPaths(std::vector<Path>& paths, const std::wstring& wildcardPattern, const PathListFilter& filter) {
	(void)filter;
	/* FIX : should we allow wildcards or not? */
	Path testPath(wildcardPattern);
	if (testPath.exists()) {
		paths.push_back(testPath);
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

static ReadOnlyFile::Impl* openForReading(const Path& path) {
	assert(!path.isNull());
	@autoreleasepool {
		NSError* error = nil;
		NSFileHandle* handle = [NSFileHandle fileHandleForReadingFromURL:path.getImpl()->getNSURL() error:&error];
		if (handle == nil) {
			assert(error != nil);
			throw Exception("Error opening file", path, static_cast<int>([error code]));
		}
		return new ReadOnlyFile::Impl(path, handle);
	}
}

static ReadOnlyFile::Impl* openForUpdating(const Path& path) {
	assert(!path.isNull());
	@autoreleasepool {
		NSError* error = nil;
		NSFileHandle* handle = [NSFileHandle fileHandleForUpdatingURL:path.getImpl()->getNSURL() error:&error];
		if (handle == nil) {
			assert(error != nil);
			throw Exception("Error opening file", path, static_cast<int>([error code]));
		}
		return new ReadOnlyFile::Impl(path, handle);
	}
}

static ReadOnlyFile::Impl* createForWriting(const Path& path, const PathAttributes& attributes, bool replaceExisting) {
	assert(!path.isNull());
	@autoreleasepool {
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
		NSFileHandle* handle = [NSFileHandle fileHandleForWritingToURL:url error:&error];
		if (handle == nil) {
			assert(error != nil);
			throw Exception("Error creating file", path, static_cast<int>([error code]));
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

/* --- ReadOnlyFile --- */

ReadOnlyFile::ReadOnlyFile(const Path& path, bool allowConcurrentWrites) : impl(openForReading(path)) {
	assert(!path.isNull());
	(void)allowConcurrentWrites; // not supported by MacOS, concurrent writes are always legal
}

Int64 ReadOnlyFile::getSize() const {
#if (NUXFILES_COCOA_USE_POSIX_FOR_IO)
	const int fileDescriptor = [impl->fileHandle fileDescriptor];
	const off_t fileOffset = lseek(fileDescriptor, 0, SEEK_END);
	assert(fileOffset != -1);
	return Int64(static_cast<int>(fileOffset >> 32), static_cast<unsigned int>(fileOffset));
#else
	@autoreleasepool {
		@try {
			const unsigned long long offset = [impl->fileHandle seekToEndOfFile];
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
	const int fileDescriptor = [impl->fileHandle fileDescriptor];
	const off_t fileOffset = (static_cast<off_t>(position.getHigh()) << 32) | position.getLow();
	const off_t seekResult = lseek(fileDescriptor, fileOffset, SEEK_SET);
	assert(seekResult == -1 || seekResult == fileOffset);
	if (seekResult == -1) {
		throw Exception("Error reading from file", getPath(), errno);
	}
	const ssize_t readResult = ::read(fileDescriptor, bytes, count);
	if (readResult == -1) {
		throw Exception("Error reading from file", getPath(), errno);
	}
	return static_cast<int>(readResult);
#else
	@autoreleasepool {
		@try {
			const unsigned long long fileOffset = (static_cast<unsigned long long>(position.getHigh()) << 32) | position.getLow();
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
	const int fileDescriptor = [impl->fileHandle fileDescriptor];
	const off_t fileOffset = (static_cast<off_t>(position.getHigh()) << 32) | position.getLow();
	const off_t seekResult = lseek(fileDescriptor, fileOffset, SEEK_SET);
	assert(seekResult == -1 || seekResult == fileOffset);
	if (seekResult == -1) {
		throw Exception("Error writing to file", getPath(), errno);
	}
	const ssize_t writeResult = ::write(fileDescriptor, bytes, count);
	if (writeResult != count) {
		throw Exception("Error writing to file", getPath(), errno);
	}
#else
	@autoreleasepool {
		@try {
			const unsigned long long fileOffset = (static_cast<unsigned long long>(position.getHigh()) << 32) | position.getLow();
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

ExchangingFile::ExchangingFile(const Path& path, const PathAttributes& attributes)
		: ReadWriteFile(path.createTempFile(), attributes), originalPath(path) {
	assert(!path.isNull());
}

void ExchangingFile::commit() {
	@autoreleasepool {
		@try {
			flush();
			if (!originalPath.isNull()) {
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
				success = [[NSFileManager defaultManager] replaceItemAtURL:originalURL
						withItemAtURL:tempURL
						backupItemName:nil
						options:NSFileManagerItemReplacementUsingNewMetadataOnly
						resultingItemURL:&newURL error:&error];
				if (success != NO) {
					assert([newURL isEqual:originalPath.getImpl()->getNSURL()]);
					if (wasReadOnly) {
						success = [originalURL setResourceValue:[NSNumber numberWithBool:YES] forKey:NSURLIsUserImmutableKey error:&error];
						assert(success != NO);
					}
				}
				if (success != NO) {
					originalPath = NuXFiles::Path();
				} else {
					assert(error != nil);
					throw Exception("Error committing file", getPath(), static_cast<int>([error code]));
				}
			}
		}
		@catch (NSException* exception) {
			throw Exception("Error committing file", getPath(), getNSExceptionErrorCode(exception));
		}
	}
}

ExchangingFile::~ExchangingFile() {
	if (!originalPath.isNull()) {
		getPath().tryToErase();
	}
}

} /* namespace NuX */

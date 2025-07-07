#include <locale>
#include <codecvt>
#include <cstdlib>

#include "NuXFiles.h"

namespace NuXFiles {

static std::string toUTF8(const std::wstring &s) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    const wchar_t* p = reinterpret_cast<const wchar_t*>(s.data());
    return convert.to_bytes(p, p + s.size());
}

/* --- Misc utilities --- */

static bool gotTrailingSlash(const std::wstring& source)
{
	return (!source.empty() && source[source.size() - 1] == L'/');
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/realpath.3.html
// https://opensource.apple.com/source/Libc/Libc-498/stdlib/FreeBSD/realpath.c.auto.html

static ::CFURLRef createAbsoluteURL(::CFURLRef baseURL, const std::wstring& pathString)
{
	bool isDirectory = false;
	wchar_t lastChar = (pathString.empty() ? 0 : pathString[pathString.size() - 1]);
	if (lastChar == '/') isDirectory = true;
	else if (lastChar != '.') isDirectory = false;
	else if (pathString.size() == 1 || pathString[pathString.size() - 2] == L'/') isDirectory = true;
	else if (pathString.size() >= 2 && pathString[pathString.size() - 2] == L'.'
			&& (pathString.size() == 2 || pathString[pathString.size() - 3] == L'/')) {
		isDirectory = true;
	}
	CFHolder< ::CFStringRef > stringRef(convertWStringToCFString(pathString));
	CFHolder< ::CFURLRef > relativeURLRef;
	if (baseURL == 0) {
		relativeURLRef = ::CFURLCreateWithFileSystemPath(0, stringRef, kCFURLPOSIXPathStyle, isDirectory);
	} else if (!::CFURLHasDirectoryPath(baseURL)) {
		CFHolder< ::CFURLRef > baseURLRef(::CFURLCreateCopyAppendingPathComponent(0, baseURL, CFSTR("."), true)); // FIX : is there a better way?
		relativeURLRef = ::CFURLCreateWithFileSystemPathRelativeToBase(0, stringRef, kCFURLPOSIXPathStyle, isDirectory, baseURLRef);
	} else {
		relativeURLRef = ::CFURLCreateWithFileSystemPathRelativeToBase(0, stringRef, kCFURLPOSIXPathStyle, isDirectory, baseURL);
	}
	CFHolder< ::CFURLRef > absoluteURLRef;
	if (relativeURLRef != 0) {
		absoluteURLRef = ::CFURLCopyAbsoluteURL(relativeURLRef);
	}
	if (relativeURLRef == 0 || absoluteURLRef == 0) {
		throw Exception("Error converting string to path");
	}
	assert(::CFURLHasDirectoryPath(absoluteURLRef) == isDirectory);
	return absoluteURLRef.release();
}

/* --- Exception --- */

// FIX : HACK how to do real conversion?
static std::string convertToByteString(const std::wstring& w)
{
	// FIX real conversion
	std::string s(w.size(), '?');
	for (size_t i = 0; i < w.size(); ++i) {
		s[i] = static_cast<char>(w[i]);
	}
	return s;
}

std::string Exception::describe() const
{
	if (description.empty()) {
		std::ostringstream message;
		message << errorString;
		if (!path.isNull()) {
			message << " : " << convertToByteString(path.getFullPath());
		}
		if (errorCode != 0) {
			// TODO use GetMacOSStatusErrorString(OSStatus err) and GetMacOSStatusCommentString() in OS X 10.4
/*
			char messageBuffer[4096];
			::DWORD formatMessageReturn = ::FormatMessage
					( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS // Source and processing options.
					, NULL // Pointer to  message source.
					, error // Requested message identifier.
					, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) // Language identifier for requested.
					, messageBuffer // Pointer to message buffer.
					, sizeof (messageBuffer) - 1 // Maximum size of message buffer.
					, NULL // Pointer to array of message inserts.
					);
			if (formatMessageReturn != 0) {
				int length = lossless_cast<int>(strlen(messageBuffer));
				while (length > 0 && (messageBuffer[length - 1] == '\r' || messageBuffer[length - 1] == '\n')) {
					--length;
				}
				messageBuffer[length] = '\0';
				message << " : " << messageBuffer;
			}
*/
			message << " [" << errorCode << ']';
		}
		message << std::ends;
		description = message.str();
	}
	
	return description;
}

/* --- PathTime --- */

const int kCarbonFileTimeToCTimeOffset = 2082844800;

PathTime::PathTime(time_t cTime)
{
	::UInt64 time64 = ((static_cast< ::UInt64 >(cTime) + kCarbonFileTimeToCTimeOffset) << 16);
	high = static_cast<unsigned int>(time64 >> 32);
	low = static_cast<unsigned int>(time64);
	assert(convertToCTime() == cTime);
}

time_t PathTime::convertToCTime() const
{
	::UInt64 time64 = ((static_cast< ::UInt64 >(high) << 32) | low);
	time64 = ((time64 + 0x8000) >> 16) - kCarbonFileTimeToCTimeOffset;
	time_t time32 = static_cast<time_t>(time64);
	if (static_cast< ::UInt64 >(time32) != time64) {
		return (static_cast< ::SInt64 >(time64) < 0) ? 0 : 0x7FFFFFFF;
	} else {
		return time32;
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

/* --- Path --- */

wchar_t Path::getSeparator()
{
	return L'/';
}

std::wstring Path::appendSeparator(const std::wstring& path)
{
	return (gotTrailingSlash(path) ? path : path + L'/');
}

std::wstring Path::removeSeparator(const std::wstring& path)
{
	return (gotTrailingSlash(path) ? std::wstring(path.begin(), path.end() - 1) : path);
}

bool Path::isValidChar(wchar_t c)
{
	(void)c;
	// FIX
	return true;
}

Path::Impl::Impl(::CFURLRef baseURL, const std::wstring& pathString)
	: urlRef(createAbsoluteURL(baseURL, pathString))
{
}

Path::Impl::Impl(const ::FSRef& fsRef)
	: urlRef(::CFURLCreateFromFSRef(0, &fsRef))
{
	if (urlRef == 0) {
		throw Exception("Error creating path from FSRef");
	}
	assert(::CFURLCanBeDecomposed(urlRef));
}

Path::Impl& Path::Impl::operator=(const Path::Impl& copy)
{
	assert(urlRef != 0);
	assert(copy.urlRef != 0);
	::CFRetain(copy.urlRef);
	::CFRelease(urlRef);
	urlRef = copy.urlRef;
	return (*this);
}

Path::Impl::~Impl()
{
	assert(urlRef != 0);
	::CFRelease(urlRef);
}

Path Path::getCurrentDirectoryPath()
{
	return Path(new Impl(0, L"./")); // Constructor will never throw, so allocation without exception handling is ok here.
}

void Path::listRoots(std::vector<Path>& roots)
{
#if 1
	roots.push_back(Path(L"/"));
#else
	// FIX : this was listRoots() for a long while, but it should be the future listVolumes() instead
	int volumeIndex = 1;
	::OSErr fsGetVolumeInfoReturn = noErr;
	do {
		::FSRef rootFSRef;
		fsGetVolumeInfoReturn = ::FSGetVolumeInfo(kFSInvalidVolumeRefNum, volumeIndex, 0, kFSVolInfoNone, 0, 0, &rootFSRef);
		if (fsGetVolumeInfoReturn == noErr) {
			roots.push_back(Path(new Impl(rootFSRef)));
		}
		++volumeIndex;
	} while (fsGetVolumeInfoReturn != nsvErr);
#endif
}

void Path::findPaths(std::vector<Path>& paths, const std::wstring& wildcardPattern, const PathListFilter& filter)
{
	(void)filter;
	/* FIX : should we allow wildcards or not? */
	Path testPath(wildcardPattern);
	if (testPath.exists()) {
		paths.push_back(testPath);
	}
}

Path::Path(const std::wstring& pathString)
	: impl(new Impl(0, pathString)) // Ok to allocate without exception handling as last statement.
{
}

Path::Path(const Path& copy)
	: impl((copy.impl != 0) ? new Impl(*copy.impl) : 0) // Ok to allocate without exception handling as last statement.
{
}

Path& Path::operator=(const Path& copy)
{
	Impl* oldImpl = impl;
	impl = ((copy.impl != 0) ? new Impl(*copy.impl) : 0);
	delete oldImpl;
	return (*this);
}

bool Path::isRoot() const
{
#if 1 // FIX : should we use "system root" or Carbon "volume root"?
	// FIX : slow?
	return (!isNull() && compare(Path(L"/")) == 0);
#else
	// FIX : this was isRoot() for a long while, but it should be the future isVolumeRoot() instead
	if (isNull()) {
		return false;
	}
	::LSItemInfoRecord itemInfo;
	return (::LSCopyItemInfoForURL(impl->urlRef, kLSRequestBasicFlagsOnly, &itemInfo) == noErr && (itemInfo.flags & kLSItemInfoIsVolume) != 0);
#endif
}

int Path::compare(const Path& other) const
{
	if (this == &other) {
		return 0;
	} else if (impl == 0 || other.impl == 0) {
		return (impl != 0 ? 1 : 0) - (other.impl != 0 ? 1 : 0);
	} else if (::CFEqual(impl->urlRef, other.impl->urlRef)) { // If same according to CFEqual, they are the same, regardless of string sorting order
		return 0;
	} else {
		CFHolder< ::CFStringRef > stringRefA(::CFURLCopyFileSystemPath(impl->urlRef, kCFURLPOSIXPathStyle));
		CFHolder< ::CFStringRef > stringRefB(::CFURLCopyFileSystemPath(other.impl->urlRef, kCFURLPOSIXPathStyle));
		if (stringRefA == 0 || stringRefB == 0) {
			throw Exception("Error converting path to string for comparison", (stringRefA == 0) ? (*this) : other);
		}
		::CFComparisonResult cfStringCompareReturn = ::CFStringCompare(stringRefA, stringRefB, kCFCompareCaseInsensitive | kCFCompareLocalized | kCFCompareNumerically);
		return lossless_cast<int>(cfStringCompareReturn);
	}
}

Path Path::getParent() const
{
	assert(!isNull());
	assert(!isRoot());
	
	CFHolder< ::CFURLRef > urlRef(::CFURLCreateCopyDeletingLastPathComponent(0, impl->urlRef));
	if (urlRef == 0) {
		throw Exception("Error obtaining parent path", (*this));
	}
	assert(::CFURLHasDirectoryPath(urlRef) == true);
	return Path(new Impl(urlRef));
}

Path Path::getRelative(const std::wstring& pathString) const
{
	assert(!isNull());
	return (pathString.empty() ? *this : Path(new Impl(impl->urlRef, pathString)));
}

Path Path::withoutExtension() const
{
	assert(!isNull());

	// FIX : what happens if you do this twice?
	CFHolder< ::CFURLRef > urlRef(::CFURLCreateCopyDeletingPathExtension(0, impl->urlRef));
	if (urlRef == 0) {
		throw Exception("Error removing extension from path", (*this));
	}
	return Path(new Impl(urlRef));
}

Path Path::withExtension(const std::wstring& extensionString) const
{
	assert(!isNull());
	
	CFHolder< ::CFURLRef > urlRefA(::CFURLCreateCopyDeletingPathExtension(0, impl->urlRef));
	CFHolder< ::CFStringRef > stringRef(convertWStringToCFString(extensionString));
	CFHolder< ::CFURLRef > urlRefB((urlRefA == 0) ? 0 : ::CFURLCreateCopyAppendingPathExtension(0, urlRefA, stringRef));
	if (urlRefA == 0 || urlRefB == 0) {
		throw Exception("Error adding extension to path", (*this));
	}
	return Path(new Impl(urlRefB));
}

void Path::listSubPaths(std::vector<Path>& subPaths, const PathListFilter& filter) const
{
	assert(!isNull());
	
	// FIX : all these tryToGetCarbonFSRef, do they fail only if file not found, if so shouldn't we set the err of the exception?
	
	::FSRef directoryFSRef;
	if (!impl->tryToGetCarbonFSRef(directoryFSRef)) {
		throw Exception("Error listing file directory", (*this));
	}
	::FSIterator iterator;
	::OSErr fsOpenIteratorReturn = ::FSOpenIterator(&directoryFSRef, kFSIterateFlat, &iterator);
	if (fsOpenIteratorReturn != noErr) {
		throw Exception("Error listing file directory", (*this), fsOpenIteratorReturn);
	}
	try {
		CFHolder< ::CFStringRef > includeExtensionStringRef(filter.includeExtension.empty() ? 0 : convertWStringToCFString(filter.includeExtension));
		
		::OSErr fsGetCatalogInfoBulkReturn = noErr;
		do {
			::FSRef fsRefs[64];
			::FSCatalogInfo catalogInfos[64];
			::ItemCount itemCount;
			fsGetCatalogInfoBulkReturn = ::FSGetCatalogInfoBulk(iterator, 64, &itemCount, 0, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo, catalogInfos, fsRefs, 0, 0);
			if (fsGetCatalogInfoBulkReturn != noErr && fsGetCatalogInfoBulkReturn != errFSNoMoreItems) {
				throw Exception("Error listing file directory", (*this), fsGetCatalogInfoBulkReturn);
			}
			assert(itemCount <= 64);
			
			for (::ItemCount i = 0; i < itemCount; ++i) {
				bool isDirectory = ((catalogInfos[i].nodeFlags & kFSNodeIsDirectoryMask) != 0);
				bool isInvisible = ((reinterpret_cast<FileInfo*>(catalogInfos[i].finderInfo)->finderFlags & kIsInvisible) != 0);
				if ((filter.excludeFiles && !isDirectory) || (filter.excludeDirectories && isDirectory) || (filter.excludeHidden && isInvisible)) {
					continue;
				}
				Path filePath(new Impl(fsRefs[i]));
/* FIX: a test, makes the filePath point to the target directory for unix symbolic links
FSRef newFSRef;
filePath.impl->tryToGetCarbonFSRef(newFSRef);
filePath = Path(new Impl(newFSRef));*/
				if ((isDirectory || filter.includeMacFileType == 0 || reinterpret_cast<FileInfo*>(catalogInfos[i].finderInfo)->fileType != filter.includeMacFileType) && includeExtensionStringRef != 0) {
					CFHolder< ::CFStringRef > extensionStringRef(::CFURLCopyPathExtension(filePath.impl->urlRef));
					if (extensionStringRef == 0 || ::CFStringCompare(includeExtensionStringRef, extensionStringRef, kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
						continue;
					}
				}
				subPaths.push_back(filePath);
			}
		} while (fsGetCatalogInfoBulkReturn == noErr);
	}
	catch (...) {
		::OSErr fsCloseIteratorReturn = ::FSCloseIterator(iterator);
		(void)fsCloseIteratorReturn;
		assert(fsCloseIteratorReturn == noErr);
		throw;
	}
	::OSErr fsCloseIteratorReturn = ::FSCloseIterator(iterator);
	(void)fsCloseIteratorReturn;
	assert(fsCloseIteratorReturn == noErr);
}

bool Path::hasExtension() const
{
	assert(!isNull());

	CFHolder< ::CFStringRef > stringRef(::CFURLCopyPathExtension(impl->urlRef));
	return (stringRef != 0);
}

std::wstring Path::getName() const
{
	assert(!isNull());

	// FIX : what happens if you do this twice?
	CFHolder< ::CFURLRef > urlRef(::CFURLCreateCopyDeletingPathExtension(0, impl->urlRef));
	CFHolder< ::CFStringRef > stringRef((urlRef == 0) ? 0 : ::CFURLCopyLastPathComponent(urlRef));
	if (urlRef == 0 || stringRef == 0) {
		throw Exception("Error obtaining name from path", (*this));
	}
	return convertCFStringToWString(stringRef);
}

std::wstring Path::getExtension() const
{
	assert(!isNull());

	CFHolder< ::CFStringRef > stringRef(::CFURLCopyPathExtension(impl->urlRef));
	return (stringRef != 0) ? convertCFStringToWString(stringRef) : std::wstring();
}

std::wstring Path::getNameWithExtension() const
{
	assert(!isNull());

	CFHolder< ::CFStringRef > stringRef(::CFURLCopyLastPathComponent(impl->urlRef));
	if (stringRef == 0) {
		throw Exception("Error obtaining name from path", (*this));
	}
	return convertCFStringToWString(stringRef);
}

std::wstring Path::getFullPath() const
{
	assert(!isNull());

	CFHolder< ::CFStringRef > stringRef(::CFURLCopyFileSystemPath(impl->urlRef, kCFURLPOSIXPathStyle));
	if (stringRef == 0) {
		throw Exception("Error converting path to string", (*this));
	}
	std::wstring fullPathString = convertCFStringToWString(stringRef);
	if (::CFURLHasDirectoryPath(impl->urlRef)) {
		fullPathString = appendSeparator(fullPathString);
	}
	return fullPathString;
}

static bool getCatalogInfoForURL(::CFURLRef urlRef, ::FSCatalogInfo& catalogInfo, ::FSCatalogInfoBitmap infoBitmap)
{
	assert(urlRef != 0);
	::FSRef fsRef;
	return (::CFURLGetFSRef(urlRef, &fsRef) && ::FSGetCatalogInfo(&fsRef, infoBitmap, &catalogInfo, 0, 0, 0) == noErr);
}

bool Path::exists() const
{
	assert(!isNull());

	::FSCatalogInfo catalogInfo;
	return getCatalogInfoForURL(impl->urlRef, catalogInfo, kFSCatInfoNone);
}

bool Path::isFile() const
{
	assert(!isNull());

	::FSCatalogInfo catalogInfo;
	return (getCatalogInfoForURL(impl->urlRef, catalogInfo, kFSCatInfoNodeFlags) && ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) == 0));
}

bool Path::isDirectory() const
{
	assert(!isNull());

	::FSCatalogInfo catalogInfo;
	return (getCatalogInfoForURL(impl->urlRef, catalogInfo, kFSCatInfoNodeFlags) && ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0));
}

// FIX : how is an UTCDateTime composed on Intel Macs? Does this code work?

static PathTime convertUTCDateTimeToPathTime(const ::UTCDateTime& utcDateTime)
{
	::UInt64 utcDateTime64 = ((static_cast< ::UInt64 >(utcDateTime.highSeconds) << 48) | (static_cast< ::UInt64 >(utcDateTime.lowSeconds) << 16) | utcDateTime.fraction);
	if (static_cast< ::SInt64 >(utcDateTime64) < 0) { // If too high we limit instead of getting a negative value, this should never happen in reality, we got 2^47 secs = which is 4 459 797 years!
		utcDateTime64 = ~(static_cast< ::UInt64 >(1) << 63);
		assert(0);
	} 
	return PathTime(static_cast<int>(utcDateTime64 >> 32), static_cast<unsigned int>(utcDateTime64));
}

static ::UTCDateTime convertPathTimeToUTCDateTime(const PathTime& pathTime)
{
	::UInt64 utcDateTime64 = (static_cast< ::UInt64 >(pathTime.getHigh()) << 32) | pathTime.getLow();
	::UTCDateTime dateTime;
	dateTime.highSeconds = static_cast< ::UInt16 >(utcDateTime64 >> 48);
	dateTime.lowSeconds = static_cast< ::UInt32 >(utcDateTime64 >> 16);
	dateTime.fraction = static_cast< ::UInt16 >(utcDateTime64 >> 0);
	assert(convertUTCDateTimeToPathTime(dateTime) == pathTime);
	return dateTime;
}

PathInfo Path::getInfo() const
{
	assert(!isNull());
   
	::FSCatalogInfo catalogInfo;
	::FSRef fsRef;
	::OSErr err = 0;
	if (!impl->tryToGetCarbonFSRef(fsRef)
			|| (err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags | kFSCatInfoCreateDate | kFSCatInfoContentMod
			| kFSCatInfoAccessDate | kFSCatInfoFinderInfo | kFSCatInfoDataSizes, &catalogInfo, 0, 0, 0)) != noErr) {
		throw Exception("Error obtaining file or directory info", (*this), err);
	}
	
	PathInfo info;
	info.isDirectory = ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0);
	info.creationTime = convertUTCDateTimeToPathTime(catalogInfo.createDate);
	info.modificationTime = convertUTCDateTimeToPathTime(catalogInfo.contentModDate);
	info.lastAccessTime = convertUTCDateTimeToPathTime(catalogInfo.accessDate);
	
	info.attributes.isReadOnly = ((catalogInfo.nodeFlags & kFSNodeLockedMask) != 0);
	info.attributes.win32Attributes = 0;
	if (!info.isDirectory) {
		info.attributes.isHidden = ((reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->finderFlags & kIsInvisible) != 0);
		info.attributes.macFileType = reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->fileType;
		info.attributes.macFileCreator = reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->fileCreator;
	} else {
		info.attributes.isHidden = ((reinterpret_cast<FolderInfo*>(catalogInfo.finderInfo)->finderFlags & kIsInvisible) != 0);
		info.attributes.macFileType = 0;
		info.attributes.macFileCreator = 0;
	}
	info.fileSize = Int64(static_cast<int>(catalogInfo.dataLogicalSize >> 32), static_cast<unsigned int>(catalogInfo.dataLogicalSize));
	
	return info;

/* FIX : another version which uses LSCopyItemInfoForRef, seems unnecessary, isn't any better than catalog-info from first tests, trash?
	assert(!isNull());
	assert(impl->urlRef != 0);
   
	::FSCatalogInfo catalogInfo;
	::LSItemInfoRecord itemInfo;
	::FSRef fsRef;
	::OSErr err = 0;
	if (!impl->tryToGetCarbonFSRef(fsRef)
			|| (err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags | kFSCatInfoCreateDate | kFSCatInfoContentMod | kFSCatInfoAccessDate | kFSCatInfoDataSizes, &catalogInfo, 0, 0, 0)) != noErr
			|| (err = ::LSCopyItemInfoForRef(&fsRef, kLSRequestAllInfo, &itemInfo)) != noErr) {
		throw Exception("Error obtaining file or directory info", this, err);
	}
	
	PathInfo info;
	info.isDirectory = ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0);
	assert(info.isDirectory == ((itemInfo.flags & kLSItemInfoIsContainer) != 0));

	info.creationTime = convertUTCDateTimeToPathTime(catalogInfo.createDate);
	info.modificationTime = convertUTCDateTimeToPathTime(catalogInfo.contentModDate);
	info.lastAccessTime = convertUTCDateTimeToPathTime(catalogInfo.accessDate);
	
	info.attributes.isReadOnly = ((catalogInfo.nodeFlags & kFSNodeLockedMask) != 0);
	info.attributes.isHidden = ((itemInfo.flags & kLSItemInfoIsInvisible) != 0);
	info.attributes.win32Attributes = 0;
	info.attributes.macType = itemInfo.filetype;
	info.attributes.macCreator = itemInfo.creator;

	info.fileSizeHigh = catalogInfo.dataLogicalSize >> 32;
	info.fileSizeLow = static_cast<unsigned int>(catalogInfo.dataLogicalSize);
	
	return info;
*/
}

static void updateCatalogInfoWithAttributes(bool isDirectory, ::FSCatalogInfo& catalogInfo, const PathAttributes& attributes)
{
	catalogInfo.nodeFlags &= ~kFSNodeLockedMask;
	catalogInfo.nodeFlags |= (attributes.isReadOnly ? kFSNodeLockedMask : 0);
	if (!isDirectory) {
		reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->fileType = attributes.macFileType;
		reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->fileCreator = attributes.macFileCreator;
		reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->finderFlags &= ~kIsInvisible;
		reinterpret_cast<FileInfo*>(catalogInfo.finderInfo)->finderFlags |= ((attributes.isHidden) ? kIsInvisible : 0);
	} else {
		reinterpret_cast<FolderInfo*>(catalogInfo.finderInfo)->finderFlags &= ~kIsInvisible;
		reinterpret_cast<FolderInfo*>(catalogInfo.finderInfo)->finderFlags |= ((attributes.isHidden) ? kIsInvisible : 0);
	}
}

void Path::updateAttributes(const PathAttributes& newAttributes) const
{
	assert(!isNull());

	::FSCatalogInfo catalogInfo;
	::FSRef fsRef;
	::OSErr err = 0;
	if (!impl->tryToGetCarbonFSRef(fsRef)
			|| (err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo, &catalogInfo, 0, 0, 0)) != noErr) {
		throw Exception("Error updating attributes on file or directory", (*this), err);
	}

	updateCatalogInfoWithAttributes(((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0), catalogInfo, newAttributes);
	err = ::FSSetCatalogInfo(&fsRef, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo, &catalogInfo);
	if (err != noErr) {
		throw Exception("Error updating attributes on file or directory", (*this), err);
	}
}

void Path::updateTimes(const PathTime& newCreationTime, const PathTime& newModificationTime, const PathTime& newAccessTime) const
{
	assert(!isNull());

	::FSCatalogInfoBitmap infoBitmap = kFSCatInfoNone;
	::FSCatalogInfo catalogInfo;
	if (newCreationTime.isAvailable()) {
		infoBitmap |= kFSCatInfoCreateDate;
		catalogInfo.createDate = convertPathTimeToUTCDateTime(newCreationTime);
	}
	if (newModificationTime.isAvailable()) {
		infoBitmap |= kFSCatInfoContentMod;
		catalogInfo.contentModDate = convertPathTimeToUTCDateTime(newModificationTime);
	}
	if (newAccessTime.isAvailable()) {
		infoBitmap |= kFSCatInfoAccessDate;
		catalogInfo.accessDate = convertPathTimeToUTCDateTime(newAccessTime);
	}
	
	::FSRef fsRef;
	::OSErr err = 0;
	if (!impl->tryToGetCarbonFSRef(fsRef)
			|| (err = ::FSSetCatalogInfo(&fsRef, infoBitmap, &catalogInfo)) != noErr) {
		throw Exception("Error updating time info on file or directory", (*this), err);
	}
}

static ::OSErr tryToCreatePath(::CFURLRef urlRef, ::OSErr& err)
{
	CFHolder< ::CFStringRef > nameStringRef(::CFURLCopyLastPathComponent(urlRef));
	int nameLength;
	std::vector< ::UniChar > buffer;
	const ::UniChar* nameChars = ((nameStringRef != 0) ? getUniCharsOfCFString(nameStringRef, nameLength, buffer) : 0);

	CFHolder< ::CFURLRef > parentURLRef(::CFURLCreateCopyDeletingLastPathComponent(0, urlRef));
	
	::FSRef parentFSRef;
	err = noErr;
	return (nameStringRef != 0
			&& parentURLRef != 0
			&& ::CFURLGetFSRef(parentURLRef, &parentFSRef)
			&& (err = ::FSCreateDirectoryUnicode(&parentFSRef, nameLength, nameChars, kFSCatInfoNone, 0, 0, 0, 0)) == noErr);
}

void Path::create() const
{
	assert(!isNull());
	assert(!isRoot());
	::OSErr err;
	if (!tryToCreatePath(impl->urlRef, err)) {
		throw Exception("Error creating directory", (*this), err);
	}
}

bool Path::tryToCreate() const
{
	assert(!isNull());
	assert(!isRoot());
	::OSErr err;
	return tryToCreatePath(impl->urlRef, err);
}

void Path::copy(const Path& /*destination*/) const
{
/* FIX */
assert(0);
}

void Path::moveRename(const Path& /*destination*/) const
{
/* FIX */
//	assert(0);
}

void Path::erase() const
{
	assert(!isNull());
	::OSErr err = 0;
	::FSRef fsRef;
	if (!impl->tryToGetCarbonFSRef(fsRef) || (err = ::FSDeleteObject(&fsRef)) != noErr) {
		throw Exception("Error deleting file or directory", (*this), err);
	}
}

bool Path::tryToErase() const
{
	assert(!isNull());
	::FSRef fsRef;
	return (impl->tryToGetCarbonFSRef(fsRef) && ::FSDeleteObject(&fsRef) == noErr);
}

Path Path::createTempFile() const
{
	assert(!isNull());

	::FSRef tempFolderFSRef;
	::FSCatalogInfo catalogInfo;
	::CFURLRef urlRef = impl->getCarbonURLRef();
	assert(urlRef != 0);
	if (!::CFURLGetFSRef(urlRef, &tempFolderFSRef)
			|| ::FSGetCatalogInfo(&tempFolderFSRef, kFSCatInfoNodeFlags, &catalogInfo, 0, 0, 0) != noErr
			|| (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) == 0) {
		CFHolder< ::CFURLRef > parentURLRef(::CFURLCreateCopyDeletingLastPathComponent(0, urlRef));
		if (parentURLRef == 0 || !::CFURLGetFSRef(parentURLRef, &tempFolderFSRef)) {
			throw Exception("Error creating temporary file");
		}
	}
	
	int y = static_cast<int>(static_cast<long long>(::GetCurrentEventTime() * 1000.0 + 0.5) & 0xFFFFFFFF);
	static int z = 0xEA46C711;
	::OSErr err;
	::FSRef tempFSRef;
	do {
		int x = y ^ z++;
		::UniChar tempName[8] = { 't', 'm', 'p', '0', '0', '0', '0', '0' };
		for (int i = 0; i < 5; ++i) {
			tempName[3 + i] = "0123456789ABCDEF"[(x >> (i * 4)) & 0x0F];
		}
		err = ::FSCreateFileUnicode(&tempFolderFSRef, 8, tempName, kFSCatInfoNone, 0, &tempFSRef, 0);
		if (err != noErr && err != dupFNErr) {
			throw Exception("Error creating temporary file", Path(), err);
		}
	} while (err == dupFNErr);

	return Path(new Impl(tempFSRef));
}

Path::~Path()
{
	delete impl;
}

/* --- ReadOnlyFile --- */

ReadOnlyFile::Impl::~Impl()
{
	::OSErr fsCloseForkReturn = ::FSCloseFork(forkRefNum);
	(void)fsCloseForkReturn;
	assert(fsCloseForkReturn == noErr);
}

static ::SInt16 openDataFork(const ::FSRef& fsRef, ::SInt8 permissions)
{
	::HFSUniStr255 dataForkName;
	::OSErr fsGetDataForkNameReturn = ::FSGetDataForkName(&dataForkName);
	(void)fsGetDataForkNameReturn;
	assert(fsGetDataForkNameReturn == noErr);

	::FSIORefNum forkRefNum;
	::OSErr fsOpenForkReturn = ::FSOpenFork(&fsRef, dataForkName.length, dataForkName.unicode, permissions, &forkRefNum);
	if (fsOpenForkReturn != noErr) {
		throw Exception("Error opening file", Path(new Path::Impl(fsRef)), fsOpenForkReturn);
	}
	return forkRefNum;
}

static ::SInt16 openFile(const Path& path, ::SInt8 permissions)
{
	::FSRef fsRef;
	if (!path.getImpl()->tryToGetCarbonFSRef(fsRef)) {
		throw Exception("Error opening file", path, fnfErr);
	}
	return openDataFork(fsRef, permissions);
}

static ::SInt16 createFile(const Path& path, const PathAttributes& attributes, bool replaceExisting, ::SInt8 permissions)
{
	if (replaceExisting) {
		path.tryToErase();
	}

	::FSCatalogInfo catalogInfo;
	memset(&catalogInfo, 0, sizeof (catalogInfo));
	updateCatalogInfoWithAttributes(false, catalogInfo, attributes);

	::CFURLRef urlRef = path.getImpl()->getCarbonURLRef();
	assert(urlRef != 0);

	CFHolder< ::CFURLRef > parentURLRef(::CFURLCreateCopyDeletingLastPathComponent(0, urlRef));
	CFHolder< ::CFStringRef > nameStringRef(::CFURLCopyLastPathComponent(urlRef));
	int nameLength;
	std::vector< ::UniChar > buffer;
	const ::UniChar* nameChars = ((nameStringRef != 0) ? getUniCharsOfCFString(nameStringRef, nameLength, buffer) : 0);

	::FSRef fsRef;
	::FSRef parentFSRef;
	::OSErr err = 0;
	if (nameStringRef == 0
			|| parentURLRef == 0
			|| !::CFURLGetFSRef(parentURLRef, &parentFSRef)
			|| (err = ::FSCreateFileUnicode(&parentFSRef, nameLength, nameChars, kFSCatInfoFinderInfo, &catalogInfo, &fsRef, 0)) != noErr) {
		throw Exception("Error creating file", path, err);
	}

	::SInt16 forkRefNum;
	try {
		forkRefNum = openDataFork(fsRef, permissions);
		try {
			// Must set read-only flag after we have opened the file or we will not be able to open for writing.
			if (attributes.isReadOnly) {
				::OSErr fsSetCatalogInfoReturn = ::FSSetCatalogInfo(&fsRef, kFSCatInfoNodeFlags, &catalogInfo);
				(void)fsSetCatalogInfoReturn;
				assert(fsSetCatalogInfoReturn == noErr);
			}
		}
		catch (...) {
			::OSErr fsCloseForkReturn = ::FSCloseFork(forkRefNum);
			(void)fsCloseForkReturn;
			assert(fsCloseForkReturn == noErr);
			throw;
		}
	}
	catch (...) {
		::OSErr fsDeleteObjectReturn = ::FSDeleteObject(&fsRef);
		(void)fsDeleteObjectReturn;
		assert(fsDeleteObjectReturn == noErr || fsDeleteObjectReturn == fnfErr);
		throw;
	}
	return forkRefNum;
}

ReadOnlyFile::ReadOnlyFile(const Path& path, bool allowConcurrentWrites)
	: impl(new Impl(openFile(path, fsRdPerm))) {
	(void)allowConcurrentWrites;
}

Int64 ReadOnlyFile::getSize() const
{
	assert(impl != 0);
	::SInt64 forkSize;
	::OSErr fsGetForkSizeReturn = ::FSGetForkSize(impl->forkRefNum, &forkSize);
	if (fsGetForkSizeReturn != noErr) {
		throw Exception("Error obtaining size of file", getPath(), fsGetForkSizeReturn);
	}
	return Int64(static_cast<int>(forkSize >> 32), static_cast<unsigned int>(forkSize));
}

void ReadOnlyFile::read(Int64 position, int count, unsigned char* bytes) const
{
	assert(impl != 0);
	assert(position >= 0);
	assert(count >= 0);
	if (tryToRead(position, count, bytes) != count) throw Exception("Error reading from file", getPath(), eofErr);
}

int ReadOnlyFile::tryToRead(Int64 position, int count, unsigned char* bytes) const
{
	assert(impl != 0);
	assert(position >= 0);
	assert(count >= 0);
	if (count == 0) return 0;
	::ByteCount actualCount;
	::SInt64 index64 = (static_cast< ::SInt64 >(position.getHigh()) << 32) | position.getLow();
	::OSErr fsReadForkReturn = ::FSReadFork(impl->forkRefNum, fsFromStart, index64, count, bytes, &actualCount);
	if (fsReadForkReturn != noErr && fsReadForkReturn != eofErr) {
		throw Exception("Error reading from file", getPath(), fsReadForkReturn);
	}
	return lossless_cast<int>(actualCount);
}

Path ReadOnlyFile::getPath() const
{
	::FSRef fsRef;
	::OSErr fsGetForkCBInfoReturn = ::FSGetForkCBInfo(impl->forkRefNum, 0, 0, 0, 0, &fsRef, 0);
	(void)fsGetForkCBInfoReturn;
	assert(fsGetForkCBInfoReturn == noErr);
	return Path(new Path::Impl(fsRef));
}

ReadOnlyFile::~ReadOnlyFile()
{
	delete impl;
}

/* --- ReadWriteFile --- */

ReadWriteFile::ReadWriteFile(const Path& path, bool allowConcurrentReads, bool allowConcurrentWrites)
	: ReadOnlyFile(new Impl(openFile(path, allowConcurrentWrites ? fsRdWrShPerm : fsRdWrPerm)))
{
	(void)allowConcurrentReads;
}

ReadWriteFile::ReadWriteFile(const Path& path, const PathAttributes& attributes, bool replaceExisting, bool allowConcurrentReads, bool allowConcurrentWrites)
	: ReadOnlyFile(new Impl(createFile(path, attributes, replaceExisting, allowConcurrentWrites ? fsRdWrShPerm : fsRdWrPerm)))
{
	(void)allowConcurrentReads;
}

void ReadWriteFile::write(Int64 position, int count, const unsigned char* bytes)
{
	assert(impl != 0);
	assert(position >= 0);
	assert(count >= 0);
	if (count == 0) return;
	::ByteCount actualCount;
	::SInt64 index64 = (static_cast< ::SInt64 >(position.getHigh()) << 32) | position.getLow();
	::OSErr fsWriteForkReturn = ::FSWriteFork(impl->forkRefNum, fsFromStart, index64, count, bytes, &actualCount);
	if (fsWriteForkReturn != noErr) throw Exception("Error writing to file", getPath(), fsWriteForkReturn);
}

void ReadWriteFile::flush()
{
	assert(impl != 0);
	 ::FSFlushFork(impl->forkRefNum); // Note: ignore error
}

/* --- ExchangingFile --- */

static Path createTempFile(const Path& path)
{
	Path tempPath(path.createTempFile());
	return tempPath;
}

// TODO : if this constructor or the constructor from ReadWriteFile fails to open the temp file it will be left undeleted, not so good
ExchangingFile::ExchangingFile(const Path& path, const PathAttributes& attributes)
	: ReadWriteFile(createTempFile(path), false, false)
	, originalPath(path)
{
	// Must update the attributes after we have opened the file for writing, in case the attributes says read-only.
	::FSCatalogInfo catalogInfo;
	::FSRef fsRef;
	::OSErr err = ::FSGetForkCBInfo(impl->forkRefNum, 0, 0, 0, 0, &fsRef, 0);
	assert(err == noErr);
	if ((err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo, &catalogInfo, 0, 0, 0)) == noErr) {
		updateCatalogInfoWithAttributes(((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0), catalogInfo, attributes);
		err = ::FSSetCatalogInfo(&fsRef, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo, &catalogInfo);
	}
	if (err != noErr) {
		throw Exception("Error updating attributes on file or directory", Path(new Path::Impl(fsRef)), err);
	}
}

void ExchangingFile::commit()
{
	if (!originalPath.isNull()) {
		flush();

		::FSRef tempFSRef;
		::FSRef originalFSRef;

		::OSErr fsGetForkCBInfoReturn = ::FSGetForkCBInfo(impl->forkRefNum, 0, 0, 0, 0, &tempFSRef, 0);
		(void)fsGetForkCBInfoReturn;
		assert(fsGetForkCBInfoReturn == noErr);

		if (originalPath.getImpl()->tryToGetCarbonFSRef(originalFSRef)) {

			::FSCatalogInfo tempInfo;
			::FSCatalogInfo originalInfo;
			::HFSUniStr255 originalName;
			::OSErr err;
			if ((err = ::FSGetCatalogInfo(&tempFSRef, (kFSCatInfoSettableInfo | kFSCatInfoVolume | kFSCatInfoParentDirID) & ~(kFSCatInfoContentMod | kFSCatInfoAttrMod), &tempInfo, 0, 0, 0)) != noErr
					|| (err = ::FSGetCatalogInfo(&originalFSRef, (kFSCatInfoSettableInfo | kFSCatInfoVolume | kFSCatInfoParentDirID) & ~(kFSCatInfoContentMod | kFSCatInfoAttrMod), &originalInfo, &originalName, 0, 0)) != noErr) {
				throw Exception("Error committing file", getPath(), err);
			}
			
			originalInfo.nodeFlags &= ~kFSNodeLockedMask;
			originalInfo.nodeFlags |= (tempInfo.nodeFlags & kFSNodeLockedMask);
			reinterpret_cast<FileInfo*>(originalInfo.finderInfo)->fileType = reinterpret_cast<FileInfo*>(tempInfo.finderInfo)->fileType;
			reinterpret_cast<FileInfo*>(originalInfo.finderInfo)->fileCreator = reinterpret_cast<FileInfo*>(tempInfo.finderInfo)->fileCreator;
			reinterpret_cast<FileInfo*>(originalInfo.finderInfo)->finderFlags &= ~kIsInvisible;
			reinterpret_cast<FileInfo*>(originalInfo.finderInfo)->finderFlags |= reinterpret_cast<FileInfo*>(tempInfo.finderInfo)->finderFlags & kIsInvisible;
			
			// Must clear lock to be able to exchange file.
			
			if ((tempInfo.nodeFlags & kFSNodeLockedMask) != 0) {
				tempInfo.nodeFlags &= ~kFSNodeLockedMask;
				if ((err = ::FSSetCatalogInfo(&tempFSRef, kFSCatInfoNodeFlags, &tempInfo)) != noErr) {
					throw Exception("Error committing file", getPath(), err);
				}
			}

			// Not all volumes supports the FSExchangeObjects mechanism, in which case we must solve it a different way.

			bool didFSExchange = false;
			::FSCatalogInfoBitmap infoBitmap = kFSCatInfoNodeFlags | kFSCatInfoFinderInfo;

			::GetVolParmsInfoBuffer volumeParams;

		#if (__MAC_OS_X_VERSION_MAX_ALLOWED < 1050)
			::HParamBlockRec pb;
			pb.ioParam.ioNamePtr = 0;
			pb.ioParam.ioVRefNum = originalInfo.volume;
			pb.ioParam.ioBuffer = reinterpret_cast<Ptr>(&volumeParams);
			pb.ioParam.ioReqCount = sizeof (volumeParams);
			::OSErr getVolParmsSyncReturn = ::PBHGetVolParmsSync(&pb);
		#else
			::OSErr getVolParmsSyncReturn = ::FSGetVolumeParms(originalInfo.volume, &volumeParams, sizeof (volumeParams));
		#endif
		
			if (getVolParmsSyncReturn == noErr && volumeParams.vMVersion >= 3
					&& (volumeParams.vMExtendedAttributes & (1 << bSupportsFSExchangeObjects)) != 0) {
				::OSErr fsExchangeObjectsReturn = ::FSExchangeObjects(&tempFSRef, &originalFSRef);
				if (fsExchangeObjectsReturn != paramErr) {
					if (fsExchangeObjectsReturn != noErr) {
						throw Exception("Error committing file", getPath(), fsExchangeObjectsReturn);
					}
					::OSErr fsDeleteObjectReturn = ::FSDeleteObject(&tempFSRef); // Delete temporary file (which is now old original)
					if (fsDeleteObjectReturn != noErr) { // Maybe file was busy! Exchange back...
						fsExchangeObjectsReturn = ::FSExchangeObjects(&tempFSRef, &originalFSRef);
						assert(fsExchangeObjectsReturn == noErr);
						throw Exception("Error committing file", getPath(), fsDeleteObjectReturn);
					}
					didFSExchange = true;
				}
			}
			if (!didFSExchange) {
				infoBitmap |= kFSCatInfoSettableInfo & ~(kFSCatInfoContentMod | kFSCatInfoAttrMod);
				::OSErr err;
				if ((err = ::FSDeleteObject(&originalFSRef)) != noErr
						|| (err = ::FSRenameUnicode(&tempFSRef, originalName.length, originalName.unicode, kTextEncodingUnknown, &originalFSRef)) != noErr) {
					throw Exception("Error committing file", getPath(), err);
				}
			}
			::OSErr fsSetCatalogInfoReturn = ::FSSetCatalogInfo(&originalFSRef, infoBitmap, &originalInfo);
			if (fsSetCatalogInfoReturn != noErr) {
				throw Exception("Error committing file", getPath(), fsSetCatalogInfoReturn);
			}
		} else {
			CFHolder< ::CFStringRef > nameStringRef(::CFURLCopyLastPathComponent(originalPath.getImpl()->getCarbonURLRef()));
			if (nameStringRef == 0) {
				throw Exception("Error committing file", getPath());
			}
			std::vector< ::UniChar > buffer;
			int originalNameLength;
			const ::UniChar* originalNameChars = getUniCharsOfCFString(nameStringRef, originalNameLength, buffer);
			::OSErr err;

			::FSCatalogInfo tempInfo;
			if ((err = ::FSGetCatalogInfo(&tempFSRef, kFSCatInfoNodeFlags, &tempInfo, 0, 0, 0)) != noErr) {
				throw Exception("Error committing file", getPath(), err);
			}
			if ((tempInfo.nodeFlags & kFSNodeLockedMask) != 0) {
				::FSCatalogInfo infoCopy = tempInfo;
				infoCopy.nodeFlags &= ~kFSNodeLockedMask;
				if ((err = ::FSSetCatalogInfo(&tempFSRef, kFSCatInfoNodeFlags, &infoCopy)) != noErr
						|| (err = ::FSRenameUnicode(&tempFSRef, originalNameLength, originalNameChars, kTextEncodingUnknown, 0)) != noErr
						|| (err = ::FSSetCatalogInfo(&tempFSRef, kFSCatInfoNodeFlags, &tempInfo)) != noErr) {
					throw Exception("Error committing file", getPath(), err);
				}
			} else if ((err = ::FSRenameUnicode(&tempFSRef, originalNameLength, originalNameChars, kTextEncodingUnknown, 0)) != noErr) {
				throw Exception("Error committing file", getPath(), err);
			}
		}

		originalPath = Path();
	}
}

ExchangingFile::~ExchangingFile()
{
	if (!originalPath.isNull()) {
		::FSRef fsRef;
		::OSErr fsGetForkCBInfoReturn = ::FSGetForkCBInfo(impl->forkRefNum, 0, 0, 0, 0, &fsRef, 0);
		(void)fsGetForkCBInfoReturn;
		assert(fsGetForkCBInfoReturn == noErr);
		delete impl;
		impl = 0;
		::OSErr fsDeleteObjectReturn = ::FSDeleteObject(&fsRef);
		(void)fsDeleteObjectReturn;
		assert(fsDeleteObjectReturn == noErr);		
	}
}

} /* namespace NuX */

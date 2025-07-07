#include "NuXFiles.h"

namespace NuXFiles {

PathListFilter::PathListFilter()
	: excludeFiles(false)
	, excludeDirectories(false)
	, excludeHidden(false)
	, includeMacFileType(0)
{
}

// Notice that this function works on both Mac and PC only because they both use '.' and '..'. We may have to copy and
// paste it to the specific platform files in case we need to support a platform that doesn't use '.' and '..'.

bool Path::makeRelative(const Path& toPath, bool walkUpwards, std::wstring& pathString) const {
	assert(!isNull());
	assert(!toPath.isNull());
	
	// Build path component lists of names and extensions of all intermediate directories
	// (backwards so first element will be the last component, i.e. the name of the final file / directory).
	
	std::vector<std::wstring> fromComponents;
	std::vector<std::wstring> toComponents;
	Path currentPath(*this);
	while (!currentPath.isRoot()) {
		fromComponents.push_back(currentPath.getNameWithExtension());
		currentPath = currentPath.getParent();
	}
	currentPath = toPath;
	while (!currentPath.isRoot()) {
		toComponents.push_back(currentPath.getNameWithExtension());
		currentPath = currentPath.getParent();
	}
	std::vector<std::wstring>::reverse_iterator fromIt = fromComponents.rbegin();
	std::vector<std::wstring>::reverse_iterator toIt = toComponents.rbegin();
	while (fromIt != fromComponents.rend() && toIt != toComponents.rend() && *fromIt == *toIt) {
		++fromIt;
		++toIt;
	}
	
	// Return full path if first component mismatched or if we are not allowed to use '..' when we have to.
	
	if (fromIt == fromComponents.rbegin() || (!walkUpwards && toIt != toComponents.rend())) {
		pathString = getFullPath();
		return false;
	}
	
	pathString = std::wstring();
	while (toIt != toComponents.rend()) {
		pathString += L"..";
		pathString += getSeparator();
		++toIt;
	}
	
	if (pathString.empty()) pathString = std::wstring(L".") + getSeparator();

	while (fromIt != fromComponents.rend()) {
		pathString += *fromIt;
		++fromIt;
		if (fromIt != fromComponents.rend()) pathString += getSeparator();
	}
	
	return true;
}

} /* namespace NuXFiles */

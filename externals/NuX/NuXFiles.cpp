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

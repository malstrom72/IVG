/**
IVG is released under the BSD 2-Clause License.

Copyright (c) 2013-2025, Magnus Lidström

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include <algorithm>
#include <cerrno>
#include <climits>
#include <codecvt>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <png.h>
#include <zlib.h>

#include "externals/NuX/NuXFiles.h"
#include "externals/NuX/NuXThreads.h"
#include "src/IMPD.h"
#include "src/IVG.h"

using namespace IMPD;

/**
		Loads an IVG source once and replays it on demand so snapshot
		playback can reuse parsed scripts without touching the core library.
**/
class CachedDocument {
  public:
	CachedDocument() {}

	bool loadFromFile(const std::string &path) {
		std::ifstream stream(path.c_str(), std::ios::binary);
		if (!stream.good()) {
			source.clear();
			return false;
		}

		const std::string buffer((std::istreambuf_iterator<char>(stream)),
								 std::istreambuf_iterator<char>());
		source.assign(buffer.begin(), buffer.end());
		return true;
	}

	void setSource(const IMPD::String &newSource) { source = newSource; }

	const IMPD::String &getSource() const { return source; }

	void render(IVG::IVGExecutor &executor) const {
		IMPD::STLMapVariables variables;
		IMPD::FormatInfo formatInfo;
		IMPD::Interpreter interpreter(executor, variables, formatInfo);
		interpreter.run(IMPD::StringRange(source));
	}

  private:
	IMPD::String source;
};

struct SnapshotBlock {
	bool validate;
	String scenario;
	StringVector statements;
	uint32_t sourceLine;
};

struct SnapshotInvocation {
	uint32_t blockIndex;
	uint32_t sourceLine;
	uint32_t statementOrdinal;
	String statements;
};

struct SnapshotEntry {
	uint32_t entryOrdinal;
	bool validate;
	String scenarioName;
	std::vector<SnapshotInvocation> invocations;
};

struct SnapshotScenario {
	String name;
	bool validate;
	bool explicitScenario;
	std::vector<SnapshotEntry> entries;
};
struct CachedImage {
	NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> raster;
	double xResolution;
	double yResolution;
};

struct SharedResources {
	NuXThreads::Lockable<std::map<WideString, IVG::Font> > fonts;
	NuXThreads::Lockable<std::map<std::string, CachedImage> > images;
};

struct SnapshotDiffStats {
	SnapshotDiffStats()
		: width(0), height(0), differingPixels(0), maxAlphaDiff(0),
		  maxRedDiff(0), maxGreenDiff(0), maxBlueDiff(0), meanAlphaDiff(0.0),
		  meanRedDiff(0.0), meanGreenDiff(0.0), meanBlueDiff(0.0) {}

	uint32_t width;
	uint32_t height;
	uint32_t differingPixels;
	uint32_t maxAlphaDiff;
	uint32_t maxRedDiff;
	uint32_t maxGreenDiff;
	uint32_t maxBlueDiff;
	double meanAlphaDiff;
	double meanRedDiff;
	double meanGreenDiff;
	double meanBlueDiff;
};

struct SnapshotEntryResult {
	SnapshotEntryResult()
		: entryOrdinal(0), validate(false), rendered(false), diffed(false),
		  skipped(false), updated(false), success(false), hasDiffStats(false),
		  planOrdinal(0), blockIndex(0) {}

	std::string ivgPath;
	std::string scenarioName;
	uint32_t entryOrdinal;
	bool validate;
	bool rendered;
	bool diffed;
	bool skipped;
	bool updated;
	bool success;
	bool hasDiffStats;
	std::string goldenPath;
	std::string oldPath;
	std::string actualPath;
	std::string diffPath;
	std::string backupPath;
	std::string message;
	SnapshotDiffStats diffStats;
	uint32_t planOrdinal;
	uint32_t blockIndex;
	std::string identifier;
};

struct SnapshotRunResult {
	SnapshotRunResult()
		: totalEntries(0), draftEntries(0), validatedEntries(0),
		  updatedEntries(0), failedEntries(0), diffFailures(0), exitCode(0),
		  fileFailed(false) {}

	std::vector<SnapshotEntryResult> entries;
	uint32_t totalEntries;
	uint32_t draftEntries;
	uint32_t validatedEntries;
	uint32_t updatedEntries;
	uint32_t failedEntries;
	uint32_t diffFailures;
	int exitCode;
	bool fileFailed;
	std::string fileError;
};

struct SnapshotTotals {
	SnapshotTotals()
		: filesProcessed(0), failedFiles(0), totalEntries(0), draftEntries(0),
		  validatedEntries(0), updatedEntries(0), failedEntries(0),
		  diffFailures(0) {}

	void accumulate(const SnapshotRunResult &run) {
		++filesProcessed;
		totalEntries += run.totalEntries;
		draftEntries += run.draftEntries;
		validatedEntries += run.validatedEntries;
		updatedEntries += run.updatedEntries;
		failedEntries += run.failedEntries;
		diffFailures += run.diffFailures;
		if (run.exitCode != 0 || run.fileFailed) {
			++failedFiles;
		}
	}

	uint32_t filesProcessed;
	uint32_t failedFiles;
	uint32_t totalEntries;
	uint32_t draftEntries;
	uint32_t validatedEntries;
	uint32_t updatedEntries;
	uint32_t failedEntries;
	uint32_t diffFailures;
};

struct CommandLineOptions {
	std::vector<std::string> includeDirs;
	std::vector<std::string> fontDirs;
	std::vector<std::string> imageDirs;
	std::string snapshotDir;
	NuXFiles::Path rootDir;
	bool forceUpdate;
	bool listOnly;
	bool verbose;
	bool exitOnFirstFailure;
	uint32_t threads;
	std::vector<std::string> ivgPaths;

	CommandLineOptions()
		: rootDir(NuXFiles::Path::getCurrentDirectoryPath()),
		  forceUpdate(false), listOnly(false), verbose(false),
		  exitOnFirstFailure(false), threads(0) {}
};

static std::string stringFromIMPD(const String &value) {
	return std::string(value.begin(), value.end());
}

static std::wstring pathStringToWide(const std::string &path) {
		if (path.empty()) {
				return std::wstring();
		}
#if defined(_WIN32)
	const int sourceLength = static_cast<int>(path.size());
	const int wideLength =
		::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path.data(), sourceLength, 0, 0);
	if (wideLength <= 0) {
		throw std::range_error("failed to convert native path to wide characters");
	}
	std::wstring wide(static_cast<size_t>(wideLength), L'\0');
	const int converted = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path.data(), sourceLength
			, &wide[0], wideLength);
	if (converted != wideLength) {
		throw std::range_error("failed to convert native path to wide characters");
	}
	return wide;
#else
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.from_bytes(path);
#endif
}

static std::string pathStringFromWide(const std::wstring &path) {
	if (path.empty()) {
		return std::string();
	}
#if defined(_WIN32)
	const int sourceLength = static_cast<int>(path.size());
	const int narrowLength =
			::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path.data(), sourceLength,
							0, 0, 0, 0);
	if (narrowLength <= 0) {
		throw std::range_error("failed to convert wide path to native characters");
	}
	std::string narrow(static_cast<size_t>(narrowLength), '\0');
	const int converted =
			::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path.data(), sourceLength,
							&narrow[0], narrowLength, 0, 0);
	if (converted != narrowLength) {
		throw std::range_error("failed to convert wide path to native characters");
	}
	return narrow;
#else
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(path);
#endif
}
static NuXFiles::Path pathFromNativeString(const std::string &path) {
		if (path.empty()) {
				return NuXFiles::Path();
		}
		try {
		return NuXFiles::Path(pathStringToWide(path));
	} catch (const std::range_error &) {
		return NuXFiles::Path();
	}
}

static std::string extractDirectory(const std::string &path) {
	const size_t slash = path.find_last_of("/\\");
	if (slash == std::string::npos) {
		return std::string();
	}
	return path.substr(0, slash);
}

static std::string joinPath(const std::string &base,
							const std::string &component) {
	if (base.empty()) {
		return component;
	}
	if (component.empty()) {
		return base;
	}
	const char last = base[base.size() - 1];
	if (last == '/' || last == '\\') {
		return base + component;
	}
	return base + "/" + component;
}

static std::string sanitizeFileComponent(const std::string &name) {
	std::string sanitized;
	sanitized.reserve(name.size() * 2);
	for (size_t i = 0; i < name.size(); ++i) {
		const char ch = name[i];
		if (ch == '_') {
			sanitized.push_back('_');
			sanitized.push_back('_');
		} else if (ch == '/' || ch == '\\' || ch == ':') {
			sanitized.push_back('_');
		} else {
			sanitized.push_back(ch);
		}
	}
	return sanitized;
}

static bool tryBuildRelativeSnapshotTag(const NuXFiles::Path &rootDir,
						const NuXFiles::Path &withoutExtension,
						std::string &relative) {
	if (rootDir.isNull()) {
		return false;
	}

	std::wstring relativeWide;
	try {
		if (rootDir.makeRelative(withoutExtension, false, relativeWide)) {
			if (!relativeWide.empty()) {
				try {
					relative = pathStringFromWide(relativeWide);
				} catch (const std::exception &) {
					relative.clear();
					return false;
				}
				for (size_t i = 0; i < relative.size(); ++i) {
					if (relative[i] == '\\') {
						relative[i] = '/';
					}
				}
				return true;
			}
		}
	} catch (const NuXFiles::Exception &) {
		relativeWide.clear();
	} catch (const std::exception &) {
		relativeWide.clear();
	}

	std::wstring rootWide;
	std::wstring targetWide;
	try {
		rootWide = rootDir.getFullPath();
		targetWide = withoutExtension.getFullPath();
	} catch (const NuXFiles::Exception &) {
		return false;
	} catch (const std::exception &) {
		return false;
	}

	if (rootWide.empty() || targetWide.empty()) {
		return false;
	}

	std::wstring normalizedRoot;
	try {
		normalizedRoot = NuXFiles::Path::appendSeparator(rootWide);
	} catch (const NuXFiles::Exception &) {
		normalizedRoot = rootWide;
	} catch (const std::exception &) {
		normalizedRoot = rootWide;
	}
	if (!normalizedRoot.empty()) {
		const wchar_t separator = NuXFiles::Path::getSeparator();
		if (normalizedRoot[normalizedRoot.size() - 1] != separator) {
			normalizedRoot.push_back(separator);
		}
	}

	if (normalizedRoot.empty()) {
		return false;
	}
	if (targetWide.size() <= normalizedRoot.size()) {
		return false;
	}
	if (targetWide.compare(0, normalizedRoot.size(), normalizedRoot) != 0) {
		return false;
	}

	const std::wstring remainder = targetWide.substr(normalizedRoot.size());
	if (remainder.empty()) {
		return false;
	}

	try {
		relative = pathStringFromWide(remainder);
	} catch (const std::exception &) {
		return false;
	}
	for (size_t i = 0; i < relative.size(); ++i) {
		if (relative[i] == '\\') {
			relative[i] = '/';
		}
	}
	return true;
}



static std::string buildSnapshotSourceTag(const std::string &ivgPath,
						const NuXFiles::Path &rootDir) {
	try {
		const NuXFiles::Path sourcePath = pathFromNativeString(ivgPath);
		if (!sourcePath.isNull()) {
			NuXFiles::Path withoutExtension(sourcePath);
			try {
				withoutExtension = sourcePath.withoutExtension();
			} catch (const NuXFiles::Exception &) {
				withoutExtension = sourcePath;
			} catch (const std::exception &) {
				withoutExtension = sourcePath;
			}

			std::string relative;
			if (tryBuildRelativeSnapshotTag(rootDir, withoutExtension, relative)) {
				return sanitizeFileComponent(relative);
			}
		}
	} catch (const std::exception &) {
	}

	std::string normalized = ivgPath;
	for (size_t i = 0; i < normalized.size(); ++i) {
		if (normalized[i] == '\\') {
			normalized[i] = '/';
		}
	}
	const size_t dot = normalized.find_last_of('.');
	if (dot != std::string::npos) {
		normalized.resize(dot);
	}
	return sanitizeFileComponent(normalized);
}


static std::string buildEntryIdentifier(const std::string &snapshotBase,
										const SnapshotEntry &entry) {
	const uint32_t blockIndex =
		(entry.invocations.empty() ? 0 : entry.invocations[0].blockIndex);
	std::ostringstream stream;
	stream << snapshotBase << '#' << stringFromIMPD(entry.scenarioName) << '#'
			<< blockIndex << '#' << entry.entryOrdinal;
	return stream.str();
}

static bool fileExists(const std::string &path) {
		if (path.empty()) {
				return false;
		}
		try {
				const NuXFiles::Path filePath = pathFromNativeString(path);
				return (!filePath.isNull() && filePath.isFile());
		} catch (const NuXFiles::Exception &) {
				return false;
		} catch (const std::exception &) {
				return false;
		}
}

static void ensureDirectoryTree(const NuXFiles::Path &directory) {
		if (directory.isNull() || directory.isRoot()) {
				return;
		}

		std::vector<NuXFiles::Path> toCreate;
		NuXFiles::Path current(directory);
		while (!current.exists()) {
				toCreate.push_back(current);
				if (current.isRoot()) {
						break;
				}
				current = current.getParent();
		}

		if (current.exists() && !current.isDirectory()) {
				throw NuXFiles::Exception("path is not a directory", current);
		}

		for (std::vector<NuXFiles::Path>::reverse_iterator it = toCreate.rbegin();
			 it != toCreate.rend(); ++it) {
				if (it->isRoot()) {
						continue;
				}
				if (!it->tryToCreate()) {
						if (it->exists() && it->isDirectory()) {
								continue;
						}
						throw NuXFiles::Exception("failed to create directory", *it);
				}
		}

		if (!directory.exists() || !directory.isDirectory()) {
				throw NuXFiles::Exception("failed to create directory", directory);
		}
}

static void ensureDirectoryTree(const std::string &path) {
		if (path.empty()) {
				return;
		}
		ensureDirectoryTree(pathFromNativeString(path));
}

static void ensureParentDirectory(const std::string &filePath) {
		if (filePath.empty()) {
				return;
		}
		const NuXFiles::Path target = pathFromNativeString(filePath);
		if (target.isNull() || target.isRoot()) {
				return;
		}
		ensureDirectoryTree(target.getParent());
}

static void removeFileIfExists(const std::string &path) {
	if (path.empty()) {
		return;
	}
	try {
		const NuXFiles::Path filePath = pathFromNativeString(path);
		if (!filePath.isNull() && filePath.exists() && filePath.isFile()) {
			filePath.erase();
		}
	} catch (const NuXFiles::Exception &) {
		// Ignore failures to match previous behaviour.
	} catch (const std::exception &) {
		// Ignore failures to match previous behaviour.
	}
}

static bool renameFile(const std::string &from, const std::string &to,
					   std::string &error) {
	if (from.empty() || from == to) {
		return true;
	}
	removeFileIfExists(to);
	try {
		const NuXFiles::Path fromPath = pathFromNativeString(from);
		const NuXFiles::Path toPath = pathFromNativeString(to);
		if (fromPath.isNull() || toPath.isNull()) {
			return true;
		}
		fromPath.moveRename(toPath);
		return true;
	} catch (const NuXFiles::Exception &e) {
		error = e.describe();
	} catch (const std::exception &e) {
		error = e.what();
	}
	return false;
}

static std::string abbreviatePathForDisplay(const std::string &path) {
	if (path.empty()) {
		return path;
	}
	const char *homeEnv = std::getenv("HOME");
	if (homeEnv == nullptr || homeEnv[0] == '\0') {
		return path;
	}
	std::string home(homeEnv);
	while (!home.empty() && (home.back() == '/' || home.back() == '\\')) {
		home.pop_back();
	}
	if (home.empty()) {
		return path;
	}
	if (path.size() == home.size() && path.compare(0, home.size(), home) == 0) {
		return "~";
	}
	if (path.size() > home.size() && path.compare(0, home.size(), home) == 0) {
		const char next = path[home.size()];
		if (next == '/' || next == '\\') {
			return "~" + path.substr(home.size());
		}
	}
	return path;
}

static std::vector<std::string> splitLines(const std::string &text) {
	std::vector<std::string> lines;
	if (text.empty()) {
		return lines;
	}
	std::string current;
	for (size_t i = 0; i < text.size(); ++i) {
		const char c = text[i];
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			lines.push_back(current);
			current.clear();
			continue;
		}
		current += c;
	}
	if (!current.empty()) {
		lines.push_back(current);
	}
	return lines;
}

static std::vector<std::string> formatErrorMessage(const std::string &message) {
	const std::string prefix = "failed to prepare directory for ";
	if (message.compare(0, prefix.size(), prefix) == 0) {
		const size_t colon = message.find(':', prefix.size());
		if (colon != std::string::npos) {
			std::vector<std::string> lines;
			lines.push_back("failed to prepare directory for");
			const std::string rawPath = message.substr(prefix.size(), colon - prefix.size());
			lines.push_back(abbreviatePathForDisplay(rawPath));
			std::string reason = message.substr(colon + 1);
			while (!reason.empty() && reason[0] == ' ') {
				reason.erase(0, 1);
			}
			if (!reason.empty()) {
				lines.push_back("(" + reason + ")");
			}
			return lines;
		}
	}
	return splitLines(message);
}

static std::string describeEntryStatus(const SnapshotEntryResult &result) {
	if (result.skipped) {
		return "SKIPPED";
	}
	if (!result.success) {
		return "FAILED";
	}
	if (result.updated) {
		return "UPDATED";
	}
	if (result.diffed) {
		return "VALIDATED";
	}
	if (result.rendered) {
		return "RENDERED";
	}
	return "NO ACTION";
}

static void printDetailLines(const std::string &label,
			const std::vector<std::string> &lines) {
	if (lines.empty()) {
		return;
	}
	static const size_t labelWidth = 12;
	const std::string indent = "	";
	std::ostringstream firstLine;
	firstLine << indent << std::left
			  << std::setw(static_cast<int>(labelWidth)) << label;
	firstLine << " : " << lines[0];
	std::cout << firstLine.str() << std::endl;
	const std::string continuation(indent.size() + labelWidth + 3, ' ');
	for (size_t i = 1; i < lines.size(); ++i) {
		std::cout << continuation << lines[i] << std::endl;
	}
}

static void printDetail(const std::string &label, const std::string &value) {
	if (value.empty()) {
		return;
	}
	printDetailLines(label, splitLines(value));
}

static void printPathDetail(const std::string &label, const std::string &path) {
	if (path.empty()) {
		return;
	}
	printDetailLines(label, std::vector<std::string>(1, abbreviatePathForDisplay(path)));
}

static void printEntryReport(const SnapshotEntryResult &result) {
	const std::string status = describeEntryStatus(result);
	std::cout << "	Entry " << result.entryOrdinal << " (block "
			  << result.blockIndex << ") - " << status << std::endl;
	printDetail("Snapshot ID", result.identifier);
	if (result.skipped) {
		printPathDetail("Draft path", result.oldPath);
	}
	if (result.updated) {
		printPathDetail("Golden path", result.goldenPath);
	}
	if (result.diffed) {
		printPathDetail("Actual path", result.actualPath);
		printPathDetail("Diff path", result.diffPath);
	}
	if (!result.success && !result.message.empty()) {
		printDetailLines("Error", formatErrorMessage(result.message));
	} else if (!result.message.empty()) {
		printDetail("Message", result.message);
	}
	if (result.hasDiffStats) {
		std::ostringstream stats;
		stats << result.diffStats.differingPixels << " pixel"
			  << (result.diffStats.differingPixels == 1 ? "" : "s")
			  << " changed within a " << result.diffStats.width << 'x'
			  << result.diffStats.height << " image.";
		printDetail("Diff stats", stats.str());
		std::ostringstream maxDelta;
		maxDelta << result.diffStats.maxAlphaDiff << '/'
				<< result.diffStats.maxRedDiff << '/'
				<< result.diffStats.maxGreenDiff << '/'
				<< result.diffStats.maxBlueDiff;
		printDetail("Max delta", maxDelta.str());
		std::ostringstream meanDelta;
		meanDelta.setf(std::ios::fixed);
		meanDelta << std::setprecision(4) << result.diffStats.meanAlphaDiff << '/'
				  << result.diffStats.meanRedDiff << '/'
				  << result.diffStats.meanGreenDiff << '/'
				  << result.diffStats.meanBlueDiff;
		printDetail("Mean delta", meanDelta.str());
	}
}

static void printSummaryLine(const std::string &label, const std::string &value) {
	static const size_t labelWidth = 15;
	const std::string indent = "  ";
	std::ostringstream line;
	line << indent << std::left
		 << std::setw(static_cast<int>(labelWidth)) << label;
	line << " : " << value;
	std::cout << line.str() << std::endl;
}

static void logFileReport(const std::string &path, const SnapshotRunResult &run) {
	std::cout << "# " << path << std::endl << std::endl;
	bool printedSection = false;
	if (!run.entries.empty()) {
		std::string previousScenario;
		bool firstScenario = true;
		bool firstEntry = true;
		for (size_t i = 0; i < run.entries.size(); ++i) {
			const SnapshotEntryResult &entry = run.entries[i];
			const std::string scenarioName =
			(entry.scenarioName.empty() ? "(unnamed)" : entry.scenarioName);
			if (firstScenario || scenarioName != previousScenario) {
				if (!firstScenario) {
					std::cout << std::endl;
				}
				std::cout << "Scenario: " << scenarioName << std::endl;
				previousScenario = scenarioName;
				firstScenario = false;
				firstEntry = true;
			}
			if (!firstEntry) {
				std::cout << std::endl;
			}
			printEntryReport(entry);
			firstEntry = false;
		}
		printedSection = true;
	}
	if (run.entries.empty() && run.fileFailed && !run.fileError.empty()) {
		printDetailLines("Error", formatErrorMessage(run.fileError));
		printedSection = true;
	}
	if (printedSection) {
		std::cout << std::endl;
	}
	std::cout << "Summary" << std::endl;
	std::ostringstream snapshotLine;
	snapshotLine << run.totalEntries << " total (" << run.validatedEntries
				 << " validated)";
	printSummaryLine("Snapshots", snapshotLine.str());
	printSummaryLine("Updated", Interpreter::toString(static_cast<int32_t>(run.updatedEntries)));
	std::ostringstream failedLine;
	failedLine << run.failedEntries << " (diff failures: " << run.diffFailures << ')';
	printSummaryLine("Failed", failedLine.str());
	std::cout << std::endl;
}

static void logTotalsSummary(const SnapshotTotals &totals) {
	std::cout << "# Overall Summary" << std::endl << std::endl;
	std::ostringstream processedLine;
	processedLine << totals.filesProcessed << " (" << totals.failedFiles
				  << " failed)";
	printSummaryLine("Processed files", processedLine.str());
	std::ostringstream snapshotLine;
	snapshotLine << totals.totalEntries << " total (" << totals.validatedEntries
				 << " validated)";
	printSummaryLine("Snapshots", snapshotLine.str());
	printSummaryLine("Updated", Interpreter::toString(static_cast<int32_t>(totals.updatedEntries)));
	std::ostringstream failedLine;
	failedLine << totals.failedEntries << " (diff failures: " << totals.diffFailures
			   << ')';
	printSummaryLine("Failed", failedLine.str());
}
static void PNGAPI snapshotPNGError(png_structp png, png_const_charp message) {
	throw std::runtime_error(std::string("Error reading PNG image: ") +
					 message);
}

static bool isLittleEndian() {
	static const unsigned char bytes[4] = {0x4A, 0x3B, 0x2C, 0x1D};
	return (*reinterpret_cast<const unsigned int *>(bytes) == 0x1D2C3B4A);
}

class ScopedFileHandle {
  public:
	ScopedFileHandle(const std::string &path, const char *mode)
		: file(0) {
		file = std::fopen(path.c_str(), mode);
	}

	~ScopedFileHandle() {
		if (file != 0) {
			std::fclose(file);
		}
	}

	FILE *get() const {
		return file;
	}

  private:
	FILE *file;
};

class ScopedPngReadStruct {
  public:
	ScopedPngReadStruct() : png(0), info(0) {}

	~ScopedPngReadStruct() {
		reset();
	}

	void initialize() {
		reset();
		png_structp newPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0,
				snapshotPNGError, 0);
		if (newPng == 0) {
			throw std::runtime_error("could not initialize PNG reader");
		}
		png_infop newInfo = png_create_info_struct(newPng);
		if (newInfo == 0) {
			png_destroy_read_struct(&newPng, 0, 0);
			throw std::runtime_error("could not initialize PNG info struct");
		}
		png = newPng;
		info = newInfo;
	}

	void reset() {
		if (png != 0) {
			png_destroy_read_struct(&png, &info, 0);
			png = 0;
			info = 0;
		}
	}

	png_structp getPng() const {
		return png;
	}

	png_infop getInfo() const {
		return info;
	}

  private:
	png_structp png;
	png_infop info;
};

class ScopedPngWriteStruct {
  public:
ScopedPngWriteStruct() : png(0), info(0) {}

	~ScopedPngWriteStruct() {
		reset();
	}

	void initialize() {
		reset();
		png_structp newPng = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0,
				snapshotPNGError, 0);
		if (newPng == 0) {
			throw std::runtime_error("could not initialize PNG writer");
		}
		png_infop newInfo = png_create_info_struct(newPng);
		if (newInfo == 0) {
			png_destroy_write_struct(&newPng, 0);
			throw std::runtime_error("could not initialize PNG info struct");
		}
		png = newPng;
		info = newInfo;
	}

	void reset() {
		if (png != 0) {
			png_destroy_write_struct(&png, &info);
			png = 0;
			info = 0;
		}
	}

	png_structp getPng() const {
		return png;
	}

	png_infop getInfo() const {
		return info;
	}

  private:
	png_structp png;
	png_infop info;
};

static unsigned int convertPremultipliedChannelToStraight(unsigned int value,
							 unsigned int alpha) {
	if (alpha == 0 || value == 0) {
		return 0;
	}
	unsigned int numerator = value * 255u + (alpha / 2u);
	unsigned int result = numerator / alpha;
	if (result > 255u) {
		result = 255u;
	}
	return result;
}

static unsigned int convertStraightChannelToPremultiplied(unsigned int value,
 unsigned int alpha) {
if (alpha == 0 || value == 0) {
return 0;
}
return (value * alpha + 127u) / 255u;
}

static SnapshotEntryResult
renderEntry(const CommandLineOptions &options, const std::string &path,
const std::string &snapshotBase, const CachedDocument &document,
SharedResources &sharedResources, const SnapshotScenario &scenario,
const SnapshotEntry &entry);

static bool
loadPngRaster(const std::string &path,
NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &outRaster) {
	ScopedFileHandle file(path, "rb");
	if (file.get() == 0) {
		return false;
	}

	ScopedPngReadStruct png;
	try {
		png.initialize();
		png_init_io(png.getPng(), file.get());
		png_set_add_alpha(png.getPng(), 0xFF, PNG_FILLER_AFTER);
		if (isLittleEndian()) {
			png_set_bgr(png.getPng());
		} else {
			png_set_swap_alpha(png.getPng());
		}

		png_read_png(png.getPng(), png.getInfo(), PNG_TRANSFORM_EXPAND, 0);
		const png_uint_32 width = png_get_image_width(png.getPng(), png.getInfo());
		const png_uint_32 height = png_get_image_height(png.getPng(), png.getInfo());
		png_bytep *rows = png_get_rows(png.getPng(), png.getInfo());

		NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> tempRaster(
			NuXPixels::IntRect(0, 0, static_cast<int>(width),
						static_cast<int>(height)));

		for (png_uint_32 y = 0; y < height; ++y) {
			NuXPixels::ARGB32::Pixel *dest =
				tempRaster.getPixelPointer() + y * tempRaster.getStride();
			png_bytep src = rows[y];
			for (png_uint_32 x = 0; x < width; ++x) {
				unsigned int b = src[x * 4 + 0];
				unsigned int g = src[x * 4 + 1];
				unsigned int r = src[x * 4 + 2];
				unsigned int a = src[x * 4 + 3];
				if (a != 0xFF) {
					r = convertStraightChannelToPremultiplied(r, a);
					g = convertStraightChannelToPremultiplied(g, a);
					b = convertStraightChannelToPremultiplied(b, a);
				}
				dest[x] = (a << 24) | (r << 16) | (g << 8) | b;
			}
		}

		outRaster = tempRaster;
		return true;
	} catch (...) {
		return false;
	}
}

static bool writeRasterToPng(
	const std::string &path,
	const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &raster,
	std::string &error) {
	const NuXPixels::IntRect bounds = raster.calcBounds();
	if (bounds.width <= 0 || bounds.height <= 0) {
		error = std::string("raster has no visible pixels: ") + path;
		return false;
	}

	const int width = bounds.width;
	const int height = bounds.height;
	const int stride = raster.getStride();
	const NuXPixels::ARGB32::Pixel *base =
		raster.getPixelPointer() + bounds.top * stride + bounds.left;

	std::vector<png_bytep> rows(static_cast<size_t>(height));
	std::vector<unsigned char> pixels(static_cast<size_t>(width) *
		static_cast<size_t>(height) * 4u);
	for (int y = 0; y < height; ++y) {
		const NuXPixels::ARGB32::Pixel *src = base + y * stride;
		unsigned char *dest =
			&pixels[static_cast<size_t>(y) * static_cast<size_t>(width) * 4u];
		rows[static_cast<size_t>(y)] = dest;
		for (int x = 0; x < width; ++x) {
			const NuXPixels::ARGB32::Pixel pixel = src[x];
			unsigned int a = (pixel >> 24) & 0xFF;
			unsigned int r = (pixel >> 16) & 0xFF;
			unsigned int g = (pixel >> 8) & 0xFF;
			unsigned int b = pixel & 0xFF;
			if (a != 0 && a != 0xFF) {
				r = convertPremultipliedChannelToStraight(r, a);
				g = convertPremultipliedChannelToStraight(g, a);
				b = convertPremultipliedChannelToStraight(b, a);
			}
			dest[x * 4 + 0] = static_cast<unsigned char>(r);
			dest[x * 4 + 1] = static_cast<unsigned char>(g);
			dest[x * 4 + 2] = static_cast<unsigned char>(b);
			dest[x * 4 + 3] = static_cast<unsigned char>(a);
		}
	}

	ScopedFileHandle file(path, "wb");
	if (file.get() == 0) {
		error =
			std::string("failed to open ") + path + ": " + std::strerror(errno);
		return false;
	}

	ScopedPngWriteStruct png;
	try {
		png.initialize();
		png_init_io(png.getPng(), file.get());
		png_set_IHDR(png.getPng(), png.getInfo(), static_cast<png_uint_32>(width),
			static_cast<png_uint_32>(height), 8,
			PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_sRGB_gAMA_and_cHRM(png.getPng(), png.getInfo(),
			PNG_sRGB_INTENT_ABSOLUTE);
		png_set_oFFs(png.getPng(), png.getInfo(), static_cast<png_int_32>(bounds.left),
			static_cast<png_int_32>(bounds.top), PNG_OFFSET_PIXEL);
		png_set_rows(png.getPng(), png.getInfo(), &rows[0]);
		png_write_png(png.getPng(), png.getInfo(), PNG_TRANSFORM_IDENTITY, 0);
		return true;
	} catch (const std::exception &e) {
		error = std::string("failed to write PNG: ") + e.what();
	} catch (...) {
		error = "failed to write PNG: unknown error";
	}

	return false;
}

class SnapshotGolden {
  public:
	SnapshotGolden(const std::string &ivgPath, const std::string &snapshotBase,
			const SnapshotScenario &scenario, const SnapshotEntry &entry,
			const CommandLineOptions &options) {
		const std::string root =
			(options.snapshotDir.empty() ? extractDirectory(ivgPath)
				: options.snapshotDir);
		std::string scenarioName = stringFromIMPD(entry.scenarioName);
if (scenario.entries.size() > 1) {
			scenarioName += "-";
			scenarioName +=
				Interpreter::toString(static_cast<int32_t>(entry.entryOrdinal));
		}
		scenarioName = sanitizeFileComponent(scenarioName);
		std::string fileStem = scenarioName;
		if (!snapshotBase.empty()) {
			fileStem = snapshotBase + "__" + fileStem;
		}
		const std::string stem = joinPath(root, fileStem);
		goldenPath = stem + ".png";
		oldPath = stem + ".png.old";
		actualPath = stem + ".actual.png";
		diffPath = stem + ".diff.png";
		backupPath = stem + ".png.bak";
	}

	void populateResult(SnapshotEntryResult &result) const {
		result.goldenPath = goldenPath;
		result.oldPath = oldPath;
		result.actualPath = actualPath;
		result.diffPath = diffPath;
		result.backupPath = backupPath;
	}

	bool
	writeDraft(const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &raster,
		SnapshotEntryResult &result) const {
		populateResult(result);
		result.skipped = true;
		result.updated = false;
		result.diffed = false;
		result.hasDiffStats = false;
		result.success = true;

		const NuXPixels::IntRect bounds = raster.calcBounds();
		const ArtifactRef goldenRef = makeArtifact("golden", goldenPath);
		const ArtifactRef oldRef = makeArtifact("old draft", oldPath);
		const ArtifactRef actualRef = makeArtifact("actual", actualPath);
		const ArtifactRef diffRef = makeArtifact("diff", diffPath);
		const ArtifactRef emptyCleanup[] = {oldRef, actualRef, diffRef, goldenRef};
		const size_t emptyCleanupCount = sizeof(emptyCleanup) / sizeof(emptyCleanup[0]);
		if (bounds.width <= 0 || bounds.height <= 0) {
			removeArtifacts(emptyCleanup, emptyCleanupCount);
			return true;
		}

		if (!ensureParentFor(oldRef, result)) {
			return false;
		}

		if (fileExists(*goldenRef.path)) {
			if (!renameArtifact(goldenRef, oldRef, result)) {
				return false;
			}
		}

		if (!writeRasterToArtifact(oldRef, raster, result)) {
			return false;
		}
		const ArtifactRef staleRefs[] = {actualRef, diffRef, goldenRef};
		const size_t staleCount = sizeof(staleRefs) / sizeof(staleRefs[0]);
		removeArtifacts(staleRefs, staleCount);
		return true;
	}

	bool
	validate(const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &raster,
		bool forceUpdate, SnapshotEntryResult &result) const {
		populateResult(result);
		result.skipped = false;
		result.diffed = false;
		result.updated = false;
		result.hasDiffStats = false;

		const ArtifactRef goldenRef = makeArtifact("golden", goldenPath);
		const ArtifactRef oldRef = makeArtifact("old draft", oldPath);
		const ArtifactRef actualRef = makeArtifact("actual", actualPath);
		const ArtifactRef diffRef = makeArtifact("diff", diffPath);
		const ArtifactRef backupRef = makeArtifact("backup", backupPath);
		const ArtifactRef cleanupRefs[] = {actualRef, diffRef};
		const size_t cleanupCount = sizeof(cleanupRefs) / sizeof(cleanupRefs[0]);

		if (!ensureParentFor(goldenRef, result)) {
			return false;
		}

		const bool goldenExists = fileExists(*goldenRef.path);
		const bool oldExists = fileExists(*oldRef.path);

		if (forceUpdate) {
			const NuXPixels::IntRect bounds = raster.calcBounds();
			if (bounds.width <= 0 || bounds.height <= 0) {
				const ArtifactRef purgeRefs[] = {goldenRef, oldRef, actualRef,
					diffRef, backupRef};
				const size_t purgeCount = sizeof(purgeRefs) / sizeof(purgeRefs[0]);
				removeArtifacts(purgeRefs, purgeCount);
				result.updated = true;
				result.success = true;
				return true;
			}

			if (goldenExists) {
				if (!renameArtifact(goldenRef, backupRef, result)) {
					return false;
				}
			} else if (oldExists) {
				if (!renameArtifact(oldRef, backupRef, result)) {
					return false;
				}
			}

			removeArtifacts(cleanupRefs, cleanupCount);
			if (!writeRasterToArtifact(goldenRef, raster, result)) {
				return false;
			}
			const ArtifactRef purgeOld[] = {oldRef};
			const size_t purgeOldCount = sizeof(purgeOld) / sizeof(purgeOld[0]);
			removeArtifacts(purgeOld, purgeOldCount);
			result.updated = true;
			result.success = true;
			return true;
		}

		if (!goldenExists) {
			if (!oldExists) {
				result.success = false;
				result.message = std::string("missing golden: ") + *goldenRef.path +
					" (no .old fallback present)";
				return false;
			}

			const NuXPixels::IntRect bounds = raster.calcBounds();
			if (bounds.width <= 0 || bounds.height <= 0) {
				const ArtifactRef purgeRefs[] = {goldenRef, oldRef, actualRef, diffRef};
				const size_t purgeCount = sizeof(purgeRefs) / sizeof(purgeRefs[0]);
				removeArtifacts(purgeRefs, purgeCount);
				result.updated = true;
				result.success = true;
				result.message =
					std::string("promoted draft image to golden: ") +
					*goldenRef.path + '.';
				return true;
			}

			if (!writeRasterToArtifact(goldenRef, raster, result)) {
				return false;
			}
			const ArtifactRef cleanupDraft[] = {oldRef, actualRef, diffRef};
			const size_t cleanupDraftCount = sizeof(cleanupDraft) /
					sizeof(cleanupDraft[0]);
			removeArtifacts(cleanupDraft, cleanupDraftCount);
			result.updated = true;
			result.success = true;
			result.message = std::string("promoted draft image to golden: ") +
				*goldenRef.path + '.';
			return true;
		}

		NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> goldenRaster;
		if (!loadPngRaster(*goldenRef.path, goldenRaster)) {
			result.success = false;
			result.message =
				std::string("failed to read golden PNG: ") + *goldenRef.path;
			return false;
		}

		const NuXPixels::IntRect actualBounds = raster.calcBounds();
		const NuXPixels::IntRect goldenBounds = goldenRaster.calcBounds();
		const int left =
			NuXPixels::minValue(actualBounds.left, goldenBounds.left);
		const int top = NuXPixels::minValue(actualBounds.top, goldenBounds.top);
		const int right = NuXPixels::maxValue(actualBounds.calcRight(),
			goldenBounds.calcRight());
		const int bottom = NuXPixels::maxValue(actualBounds.calcBottom(),
			goldenBounds.calcBottom());
		const int width = (right > left ? right - left : 0);
		const int height = (bottom > top ? bottom - top : 0);

		SnapshotDiffStats stats;
		stats.width = static_cast<uint32_t>(width);
		stats.height = static_cast<uint32_t>(height);

		if (width <= 0 || height <= 0) {
			result.success = true;
			removeArtifacts(cleanupRefs, cleanupCount);
			return true;
		}

		NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> diffRaster(
			NuXPixels::IntRect(left, top, width, height));
		const NuXPixels::ARGB32::Pixel *actualPixels = raster.getPixelPointer();
		const NuXPixels::ARGB32::Pixel *goldenPixelsPtr =
			goldenRaster.getPixelPointer();
		const int actualStride = raster.getStride();
		const int goldenStride = goldenRaster.getStride();
		NuXPixels::ARGB32::Pixel *diffPixels = diffRaster.getPixelPointer();
		const int diffStride = diffRaster.getStride();

		uint64_t sumAlpha = 0;
		uint64_t sumRed = 0;
		uint64_t sumGreen = 0;
		uint64_t sumBlue = 0;
		bool match = true;

		for (int y = top; y < bottom; ++y) {
			NuXPixels::ARGB32::Pixel *diffRow = diffPixels + y * diffStride;
			const NuXPixels::ARGB32::Pixel *actualRow =
				(y >= actualBounds.top && y < actualBounds.calcBottom())
					? actualPixels + y * actualStride
					: 0;
			const NuXPixels::ARGB32::Pixel *goldenRow =
				(y >= goldenBounds.top && y < goldenBounds.calcBottom())
					? goldenPixelsPtr + y * goldenStride
					: 0;
			for (int x = left; x < right; ++x) {
				const NuXPixels::ARGB32::Pixel actualPixel =
					(actualRow != 0 && x >= actualBounds.left &&
					 x < actualBounds.calcRight())
						? actualRow[x]
						: 0;
				const NuXPixels::ARGB32::Pixel goldenPixel =
					(goldenRow != 0 && x >= goldenBounds.left &&
					 x < goldenBounds.calcRight())
						? goldenRow[x]
						: 0;
				if (actualPixel == goldenPixel) {
					diffRow[x] = 0;
					continue;
				}
				match = false;
				++stats.differingPixels;
				const unsigned int actualA = (actualPixel >> 24) & 0xFF;
				const unsigned int actualR = (actualPixel >> 16) & 0xFF;
				const unsigned int actualG = (actualPixel >> 8) & 0xFF;
				const unsigned int actualB = actualPixel & 0xFF;
				const unsigned int goldenA = (goldenPixel >> 24) & 0xFF;
				const unsigned int goldenR = (goldenPixel >> 16) & 0xFF;
				const unsigned int goldenG = (goldenPixel >> 8) & 0xFF;
				const unsigned int goldenB = goldenPixel & 0xFF;
				const unsigned int diffA =
					(actualA > goldenA ? actualA - goldenA : goldenA - actualA);
				const unsigned int diffR =
					(actualR > goldenR ? actualR - goldenR : goldenR - actualR);
				const unsigned int diffG =
					(actualG > goldenG ? actualG - goldenG : goldenG - actualG);
				const unsigned int diffB =
					(actualB > goldenB ? actualB - goldenB : goldenB - actualB);
				if (diffA > stats.maxAlphaDiff) {
					stats.maxAlphaDiff = diffA;
				}
				if (diffR > stats.maxRedDiff) {
					stats.maxRedDiff = diffR;
				}
				if (diffG > stats.maxGreenDiff) {
					stats.maxGreenDiff = diffG;
				}
				if (diffB > stats.maxBlueDiff) {
					stats.maxBlueDiff = diffB;
				}
				sumAlpha += diffA;
				sumRed += diffR;
				sumGreen += diffG;
				sumBlue += diffB;
				unsigned int scaledR = diffR * 4;
				unsigned int scaledG = diffG * 4;
				unsigned int scaledB = diffB * 4;
				if (scaledR > 255) {
					scaledR = 255;
				}
				if (scaledG > 255) {
					scaledG = 255;
				}
				if (scaledB > 255) {
					scaledB = 255;
				}
				diffRow[x] =
					(0xFFu << 24) | (scaledR << 16) | (scaledG << 8) | scaledB;
			}
		}

		const uint64_t pixelCount =
			static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
		if (pixelCount > 0) {
			stats.meanAlphaDiff = static_cast<double>(sumAlpha) / pixelCount;
			stats.meanRedDiff = static_cast<double>(sumRed) / pixelCount;
			stats.meanGreenDiff = static_cast<double>(sumGreen) / pixelCount;
			stats.meanBlueDiff = static_cast<double>(sumBlue) / pixelCount;
		}

		if (match) {
			result.success = true;
			removeArtifacts(cleanupRefs, cleanupCount);
			return true;
		}

		result.success = false;
		result.diffed = true;
		result.hasDiffStats = true;
		result.diffStats = stats;

		std::ostringstream summary;
		summary << "differs from golden (pixels: " << stats.differingPixels
			<< "/" << (stats.width * stats.height) << ")";
		result.message = summary.str();

		removeArtifacts(cleanupRefs, cleanupCount);
		if (!writeRasterToArtifact(actualRef, raster, result)) {
			return false;
		}
		if (!writeRasterToArtifact(diffRef, diffRaster, result)) {
			return false;
		}

		return false;
	}

  private:
	struct ArtifactRef {
		const char *role;
		const std::string *path;
	};

	static void removeArtifacts(const ArtifactRef *artifacts, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			removeFileIfExists(*artifacts[i].path);
		}
	}

	ArtifactRef makeArtifact(const char *role, const std::string &path) const {
		ArtifactRef artifact;
		artifact.role = role;
		artifact.path = &path;
		return artifact;
	}

	bool ensureParentFor(const ArtifactRef &artifact,
			SnapshotEntryResult &result) const {
		try {
			ensureParentDirectory(*artifact.path);
			return true;
		} catch (const std::exception &e) {
			result.success = false;
			result.message =
				std::string("failed to prepare directory for ") +
					*artifact.path + " (" + artifact.role + "): " +
					e.what();
			return false;
		} catch (...) {
			result.success = false;
			result.message =
				std::string("failed to prepare directory for ") +
					*artifact.path + " (" + artifact.role + ')';
			return false;
		}
	}

	bool renameArtifact(const ArtifactRef &from, const ArtifactRef &to,
			SnapshotEntryResult &result) const {
		std::string error;
		if (!renameFile(*from.path, *to.path, error)) {
			result.success = false;
			result.message = error;
			return false;
		}
		return true;
	}

	bool writeRasterToArtifact(const ArtifactRef &artifact,
		const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &raster,
		SnapshotEntryResult &result) const {
		std::string error;
		if (!writeRasterToPng(*artifact.path, raster, error)) {
			result.success = false;
			result.message = error;
			return false;
		}
		return true;
	}

	std::string goldenPath;
	std::string oldPath;
	std::string actualPath;
	std::string diffPath;
	std::string backupPath;
};

class SnapshotPlan {
  public:
	explicit SnapshotPlan(const std::string &ivgPath)
		: baseName(extractBaseName(ivgPath)), nextBlockOrdinal(1),
		  collectingPlan(false), activeScenarioIndex(0), activeEntryOrdinal(1),
		  collectionRunCursor(0), collectionRunsBuilt(false),
		  recordedBlockCursor(0) {}

	uint32_t addBlock(Interpreter &interpreter, const SnapshotBlock &block) {
		if (block.statements.empty()) {
			Interpreter::throwBadSyntax(
				"snapshot meta requires at least one statement block.");
		}

		uint32_t blockOrdinal = nextBlockOrdinal;
		if (collectionRunsBuilt) {
			if (recordedBlockCursor >= recordedBlockOrdinals.size()) {
				Interpreter::throwBadSyntax(
					"snapshot replay encountered an unexpected block.");
			}
			blockOrdinal = recordedBlockOrdinals[recordedBlockCursor++];
			return blockOrdinal;
		}

		recordedBlockOrdinals.push_back(blockOrdinal);
		const bool hasExplicitScenario = !block.scenario.empty();
		if (hasExplicitScenario) {
			const uint32_t scenarioIndex = resolveScenario(
				interpreter, block.scenario, block.validate, true);
			SnapshotScenario &scenario = scenarios[scenarioIndex];
			const uint32_t statementCount =
				static_cast<uint32_t>(block.statements.size());
if (!scenario.entries.empty() &&
statementCount != scenario.entries.size()) {
				Interpreter::throwBadSyntax(
					"scenario entry count does not match previous blocks.");
			}

			for (uint32_t i = 0; i < statementCount; ++i) {
				const uint32_t entryOrdinal = i + 1;
SnapshotEntry &entry =
ensureEntry(scenario, entryOrdinal, block.validate, block.scenario);

				SnapshotInvocation invocation;
				invocation.blockIndex = blockOrdinal;
				invocation.sourceLine = block.sourceLine;
				invocation.statementOrdinal = entryOrdinal;
				invocation.statements = block.statements[i];
				entry.invocations.push_back(invocation);
			}
		} else {
			const uint32_t statementCount =
				static_cast<uint32_t>(block.statements.size());
			for (uint32_t i = 0; i < statementCount; ++i) {
				const uint32_t entryOrdinal = 1;
				const String scenarioName =
					synthesizeScenarioName(blockOrdinal, statementCount, i + 1);
				const uint32_t scenarioIndex = resolveScenario(
					interpreter, scenarioName, block.validate, false);
				SnapshotScenario &scenario = scenarios[scenarioIndex];
SnapshotEntry &entry =
ensureEntry(scenario, entryOrdinal, block.validate, scenarioName);

				SnapshotInvocation invocation;
				invocation.blockIndex = blockOrdinal;
				invocation.sourceLine = block.sourceLine;
				invocation.statementOrdinal = i + 1;
				invocation.statements = block.statements[i];
				entry.invocations.push_back(invocation);
			}
		}

		++nextBlockOrdinal;
		return blockOrdinal;
	}

	const std::vector<SnapshotScenario> &getScenarios() const {
		return scenarios;
	}
	size_t getTotalEntryCount() const {
		size_t total = 0;
		for (size_t i = 0; i < scenarios.size(); ++i) {
			total += scenarios[i].entries.size();
		}
		return total;
	}
	const String &getBaseName() const { return baseName; }

	void beginCollection() {
		collectingPlan = true;
		activeScenarioIndex = 0;
		activeEntryOrdinal = 1;
		collectionRuns.clear();
		collectionRunCursor = 0;
		collectionRunsBuilt = false;
		recordedBlockOrdinals.clear();
		recordedBlockCursor = 0;
	}

	void completeCollectionPass() { collectingPlan = false; }

	bool prepareNextCollectionPass() {
		if (!collectionRunsBuilt) {
			buildCollectionRuns();
			collectionRunsBuilt = true;
			if (collectionRuns.empty()) {
				return false;
			}
			collectionRunCursor = 0;
			recordedBlockCursor = 0;
		}

		if (collectionRuns.empty()) {
			return false;
		}
		if (collectionRunCursor + 1 >= collectionRuns.size()) {
			return false;
		}

		++collectionRunCursor;
		const CollectionRun &run = collectionRuns[collectionRunCursor];
		activeScenarioIndex = run.scenarioIndex;
		activeEntryOrdinal = run.entryOrdinal;
		collectingPlan = true;
		recordedBlockCursor = 0;
		return true;
	}

	bool isCollectingPlan() const { return collectingPlan; }

	uint32_t getActiveScenarioIndex() const { return activeScenarioIndex; }

	uint32_t getActiveEntryOrdinal() const { return activeEntryOrdinal; }

		const SnapshotInvocation *lookupInvocation(uint32_t blockOrdinal,
											   uint32_t scenarioIndex,
											   uint32_t entryOrdinal) const {
		if (scenarioIndex >= scenarios.size()) {
			return 0;
		}

		const SnapshotScenario &scenario = scenarios[scenarioIndex];
		if (entryOrdinal == 0) {
			return 0;
		}

		for (size_t i = 0; i < scenario.entries.size(); ++i) {
			const SnapshotEntry &entry = scenario.entries[i];
			if (entry.entryOrdinal != entryOrdinal) {
				continue;
			}
			for (size_t j = 0; j < entry.invocations.size(); ++j) {
				if (entry.invocations[j].blockIndex == blockOrdinal) {
					return &entry.invocations[j];
				}
			}
			return 0;
		}
		return 0;
	}

  private:
	String extractBaseName(const std::string &path) const {
		const size_t slash = path.find_last_of("/\\");
		const size_t baseOffset = (slash == std::string::npos ? 0 : slash + 1);
		size_t dot = path.find_last_of('.');
		if (dot == std::string::npos || dot < baseOffset) {
			dot = path.size();
		}
		return String(path.c_str() + baseOffset, path.c_str() + dot);
	}

	String synthesizeScenarioName(uint32_t blockOrdinal, uint32_t blockCount,
								  uint32_t entryOrdinal) const {
		String name = baseName;
		name += '-';
		name += Interpreter::toString(static_cast<int32_t>(blockOrdinal));
		if (blockCount > 1) {
			name += '-';
			name += Interpreter::toString(static_cast<int32_t>(entryOrdinal));
		}
		return name;
	}

	uint32_t resolveScenario(Interpreter &interpreter, const String &name,
							 bool validate, bool explicitScenario) {
		const std::map<String, uint32_t>::const_iterator it =
			scenarioLookup.find(name);
		if (it != scenarioLookup.end()) {
			SnapshotScenario &existing = scenarios[it->second];
			if (existing.validate != validate) {
				Interpreter::throwBadSyntax(
					"scenario switches between validate yes/no.");
			}
			return it->second;
		}

		SnapshotScenario scenario;
		scenario.name = name;
		scenario.validate = validate;
		scenario.explicitScenario = explicitScenario;

		scenarios.push_back(scenario);
		const uint32_t index = static_cast<uint32_t>(scenarios.size() - 1);
		scenarioLookup.insert(std::make_pair(name, index));
		return index;
	}

	SnapshotEntry &ensureEntry(SnapshotScenario &scenario,
		uint32_t entryOrdinal, bool validate,
		const String &scenarioName) {
		for (size_t i = 0; i < scenario.entries.size(); ++i) {
			SnapshotEntry &existing = scenario.entries[i];
			if (existing.entryOrdinal == entryOrdinal) {
				return existing;
			}
			if (existing.entryOrdinal > entryOrdinal) {
				SnapshotEntry entry;
				entry.entryOrdinal = entryOrdinal;
				entry.validate = validate;
				entry.scenarioName = scenarioName;
				scenario.entries.insert(scenario.entries.begin() + i, entry);
				return scenario.entries[i];
			}
		}

		SnapshotEntry entry;
		entry.entryOrdinal = entryOrdinal;
		entry.validate = validate;
		entry.scenarioName = scenarioName;
		scenario.entries.push_back(entry);
		return scenario.entries.back();
	}

	String baseName;
	std::vector<SnapshotScenario> scenarios;
	std::map<String, uint32_t> scenarioLookup;
	uint32_t nextBlockOrdinal;
	bool collectingPlan;
	uint32_t activeScenarioIndex;
	uint32_t activeEntryOrdinal;
	std::vector<uint32_t> recordedBlockOrdinals;
	size_t recordedBlockCursor;

	struct CollectionRun {
		uint32_t scenarioIndex;
	uint32_t entryOrdinal;
	};

	std::vector<CollectionRun> collectionRuns;
	size_t collectionRunCursor;
	bool collectionRunsBuilt;

		void buildCollectionRuns() {
			collectionRuns.clear();
			for (uint32_t scenarioIndex = 0; scenarioIndex < scenarios.size();
					 ++scenarioIndex) {
				const SnapshotScenario &scenario = scenarios[scenarioIndex];
				if (scenario.entries.empty()) {
					continue;
				}

				for (size_t entryIndex = 0;
					 entryIndex < scenario.entries.size(); ++entryIndex) {
					CollectionRun run;
					run.scenarioIndex = scenarioIndex;
					run.entryOrdinal =
						scenario.entries[entryIndex].entryOrdinal;
					collectionRuns.push_back(run);
				}
			}

		if (!collectionRuns.empty()) {
			const CollectionRun &first = collectionRuns[0];
			activeScenarioIndex = first.scenarioIndex;
			activeEntryOrdinal = first.entryOrdinal;
		}
	}
};
static bool isWhitespace(Char c) {
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static StringRange trimRange(const StringRange &range) {
	StringIt b = range.b;
	StringIt e = range.e;
	while (b != e && isWhitespace(*b)) {
		++b;
	}
	while (e != b && isWhitespace(e[-1])) {
		--e;
	}
	return StringRange(b, e);
}

static String stripBrackets(const String &value) {
	if (!Interpreter::isBracketBlock(value)) {
		Interpreter::throwBadSyntax(
			"snapshot statements must be enclosed in [ ].");
	}
	return String(value.begin() + 1, value.end() - 1);
}

static bool parseValidateFlag(Interpreter &interpreter, const String *value) {
	if (value == 0) {
		return true;
	}
	return interpreter.toBool(*value);
}

static StringVector parseSnapshotStatements(Interpreter &interpreter,
											const String &raw) {
	const StringRange trimmed = trimRange(StringRange(raw));
	if (trimmed.b == trimmed.e) {
		Interpreter::throwBadSyntax(
			"snapshot meta requires a bracketed statement list.");
	}

	const String outer(trimmed);
	if (!Interpreter::isBracketBlock(outer)) {
		Interpreter::throwBadSyntax(
			"snapshot statements must start with [ and end with ].");
	}

	String inner = stripBrackets(outer);
	const StringRange innerRange(inner);
	const StringRange innerTrimmed = trimRange(innerRange);

	StringVector result;
	if (innerTrimmed.b != innerTrimmed.e && *innerTrimmed.b == '[') {
		StringVector tuple;
		interpreter.parseList(StringRange(inner), tuple, false, false, 1,
							  INT_MAX);
		result.reserve(tuple.size());
		for (size_t i = 0; i < tuple.size(); ++i) {
			result.push_back(stripBrackets(tuple[i]));
		}
	} else {
		result.push_back(inner);
	}
	return result;
}

static bool readFile(const std::string &path, String &contents);

template <typename Reader>
static bool visitSearchPaths(const std::string &localPath,
				const std::vector<std::string> &directories,
				const std::string &name,
				Reader &reader) {
	if (!localPath.empty() && reader(localPath)) {
		return true;
	}
	for (size_t i = 0; i < directories.size(); ++i) {
		const std::string candidate = directories[i] + "/" + name;
		if (reader(candidate)) {
			return true;
		}
	}
	return false;
}

class SnapshotCollector : public Executor {
  public:
	SnapshotCollector(SnapshotPlan &plan, const std::string &sourcePath,
					  const String &sourceText,
					  const std::vector<std::string> &includeDirs)
		: plan(plan), sourcePath(sourcePath), sourceText(sourceText),
		  includeDirs(includeDirs), scanOffset(0) {}

	bool format(Interpreter &interpreter,
				const FormatInfo &formatInfo) override {
		(void)interpreter;
		(void)formatInfo;
		return true;
	}

	bool execute(Interpreter &interpreter, const String &instruction,
				 const String &arguments) override {
		(void)interpreter;
		(void)instruction;
		(void)arguments;
		return true;
	}

	bool progress(Interpreter &interpreter, int maxStatementsLeft) override {
		(void)interpreter;
		(void)maxStatementsLeft;
		return true;
	}

	bool load(Interpreter &interpreter, const WideString &filename,
			  String &contents) override {
		(void)interpreter;
		const std::string utf8(filename.begin(), filename.end());
		struct IncludeLoader {
			String &contents;
			bool operator()(const std::string &path) {
				return readFile(path, contents);
			}
		};
		IncludeLoader loader = { contents };
		return visitSearchPaths(resolveRelativePath(utf8), includeDirs, utf8, loader);
	}

	void trace(Interpreter &interpreter, const WideString &s) override {
		(void)interpreter;
		(void)s;
	}

	bool meta(Interpreter &interpreter, const String &key,
			  const String &arguments) override {
		static const String SNAPSHOT_KEY("snapshot-1");
		if (key != SNAPSHOT_KEY) {
			return false;
		}

		ArgumentsContainer args(
			ArgumentsContainer::parse(interpreter, StringRange(arguments)));

		SnapshotBlock block;
		block.validate =
			parseValidateFlag(interpreter, args.fetchOptional("validate"));
		const String *scenarioLabel = args.fetchOptional("scenario");
		if (scenarioLabel != 0) {
			block.scenario = *scenarioLabel;
		}

		const String &rawStatements = args.fetchRequired(0, false);

		block.statements = parseSnapshotStatements(interpreter, rawStatements);
		block.sourceLine = locateMetaLine();

		args.throwIfAnyUnfetched();

		const uint32_t blockOrdinal = plan.addBlock(interpreter, block);

		executeCollectionInvocation(interpreter, blockOrdinal);

		return true;
	}

  private:
	void executeCollectionInvocation(Interpreter &interpreter,
									 uint32_t blockOrdinal) {
		if (!plan.isCollectingPlan()) {
			return;
		}

		const SnapshotInvocation *invocation =
			plan.lookupInvocation(blockOrdinal, plan.getActiveScenarioIndex(),
								  plan.getActiveEntryOrdinal());
		if (invocation == 0) {
			return;
		}

		const StringRange trimmed =
			trimRange(StringRange(invocation->statements));
		if (trimmed.b == trimmed.e) {
			return;
		}

		interpreter.run(StringRange(invocation->statements));
	}

	std::string resolveRelativePath(const std::string &requested) const {
		const size_t slash = sourcePath.find_last_of("/\\");
		if (slash == std::string::npos) {
			return requested;
		}
		return sourcePath.substr(0, slash + 1) + requested;
	}

	uint32_t locateMetaLine() {
		static const String TOKEN("meta snapshot");
		const size_t position = sourceText.find(TOKEN, scanOffset);
		if (position == String::npos) {
			return 0;
		}

		scanOffset = position + TOKEN.size();
		uint32_t line = 1;
		for (size_t i = 0; i < position; ++i) {
			if (sourceText[i] == '\n') {
				++line;
			}
		}
		return line;
	}

	SnapshotPlan &plan;
	std::string sourcePath;
	String sourceText;
	std::vector<std::string> includeDirs;
	size_t scanOffset;
};

class SnapshotPlaybackExecutor : public IVG::IVGExecutor {
  public:
	SnapshotPlaybackExecutor(IVG::Canvas &canvas,
							 const SnapshotScenario &scenario,
							 const SnapshotEntry &entry,
							 const CommandLineOptions &options,
							 const std::string &sourcePath,
							 SharedResources &sharedResources)
		: IVG::IVGExecutor(canvas), scenario(scenario), entry(entry),
		  includeDirs(options.includeDirs), fontDirs(options.fontDirs),
		  imageDirs(options.imageDirs), sourcePath(sourcePath),
		  verbose(options.verbose), sharedResources(sharedResources),
		  nextBlockOrdinal(0), invocationCursor(0) {}

	bool load(Interpreter &interpreter, const WideString &filename,
			  String &contents) override {
		(void)interpreter;
		const std::string utf8(filename.begin(), filename.end());
		struct IncludeLoader {
			String &contents;
			bool operator()(const std::string &path) {
				return readFile(path, contents);
			}
		};
		IncludeLoader loader = { contents };
		return visitSearchPaths(resolveRelativePath(utf8), includeDirs, utf8, loader);
	}

        std::vector<const IVG::Font *>
        lookupFonts(Interpreter &interpreter, const WideString &fontName,
                                const UniString &forString) override {
                (void)interpreter;
                (void)forString;

                const IVG::Font *cached = findCachedFont(fontName);
                if (cached != 0) {
                        return singleFontResult(cached);
                }

                IVG::Font font;
                if (!loadExternalFont(fontName, font)) {
                        return std::vector<const IVG::Font *>();
                }

                return singleFontResult(insertFont(fontName, font));
        }

	IVG::Image loadImage(Interpreter &interpreter,
						 const WideString &imageSource,
						 const NuXPixels::IntRect *sourceRectangle,
						 bool forStretching, double forXSize,
						 bool xSizeIsRelative, double forYSize,
						 bool ySizeIsRelative) override {
		(void)interpreter;
		(void)sourceRectangle;
		(void)forStretching;
		(void)forXSize;
		(void)xSizeIsRelative;
		(void)forYSize;
		(void)ySizeIsRelative;

		const std::string requested(imageSource.begin(), imageSource.end());
		const CachedImage *cached = resolveImage(requested);
		if (cached == 0) {
			return IVG::Image();
		}

		IVG::Image image;
		image.raster = &cached->raster;
		image.xResolution = cached->xResolution;
		image.yResolution = cached->yResolution;
		return image;
	}

	bool meta(Interpreter &interpreter, const String &key,
			  const String &arguments) override {
		static const String SNAPSHOT_KEY("snapshot-1");
		if (key != SNAPSHOT_KEY) {
			return false;
		}

		ArgumentsContainer args(
			ArgumentsContainer::parse(interpreter, StringRange(arguments)));

		const String *validateFlag = args.fetchOptional("validate");
		const bool blockValidate = parseValidateFlag(interpreter, validateFlag);
		const String *scenarioLabel = args.fetchOptional("scenario");
		const String &rawStatements = args.fetchRequired(0, false);

		const bool hasLabel = (scenarioLabel != 0);

		const uint32_t blockOrdinal = ++nextBlockOrdinal;

		const SnapshotInvocation *invocation = 0;
		if (invocationCursor < entry.invocations.size()) {
			const SnapshotInvocation &candidate =
				entry.invocations[invocationCursor];
			if (candidate.blockIndex == blockOrdinal) {
				invocation = &candidate;
				++invocationCursor;
			}
		}

		const bool blockTargetsScenario =
			(scenario.explicitScenario
				 ? (hasLabel && *scenarioLabel == scenario.name)
				 : (!hasLabel && invocation != 0));

		if (!blockTargetsScenario) {
			args.throwIfAnyUnfetched();
			if (invocation != 0) {
				Interpreter::throwBadSyntax(
								"unexpected snapshot invocation for scenario.");
			}
			return true;
		}

		if (invocation == 0) {
			Interpreter::throwBadSyntax(
								"missing snapshot invocation for scenario block.");
		}

		if (blockValidate != entry.validate) {
			Interpreter::throwBadSyntax("snapshot validate flag changed "
										"between collection and playback.");
		}

		StringVector statements =
			parseSnapshotStatements(interpreter, rawStatements);
		if (invocation->statementOrdinal == 0 ||
			invocation->statementOrdinal > statements.size()) {
			Interpreter::throwBadSyntax(
				"snapshot statement ordinal exceeds available entries.");
		}

		const String &statementBody =
			statements[invocation->statementOrdinal - 1];
		if (statementBody != invocation->statements) {
			Interpreter::throwBadSyntax(
				"snapshot statements changed between collection and playback.");
		}

		args.throwIfAnyUnfetched();

		if (verbose) {
			std::cout << sourcePath << ": scenario " << entry.scenarioName
					  << " entry " << entry.entryOrdinal << " block "
					  << invocation->blockIndex << " (statement "
					  << invocation->statementOrdinal << ")" << std::endl;
		}

		IVG::Context invocationContext(currentContext->accessCanvas(),
									   *currentContext);
		runInNewContext(interpreter, invocationContext, statementBody);
		return true;
	}

	bool finished() const {
		return (invocationCursor == entry.invocations.size());
	}

  private:
	std::string resolveRelativePath(const std::string &requested) const {
		const size_t slash = sourcePath.find_last_of("/\\");
		if (slash == std::string::npos) {
			return requested;
		}
		return sourcePath.substr(0, slash + 1) + requested;
	}

	const SnapshotScenario &scenario;
	const SnapshotEntry &entry;
	const std::vector<std::string> &includeDirs;
	const std::vector<std::string> &fontDirs;
	const std::vector<std::string> &imageDirs;
	std::string sourcePath;
	bool verbose;
	SharedResources &sharedResources;
	uint32_t nextBlockOrdinal;
	size_t invocationCursor;

	bool loadExternalFont(const WideString &fontName, IVG::Font &font) {
		const std::string fontName8(fontName.begin(), fontName.end());
		const std::string fileName = fontName8 + ".ivgfont";

		String contents;
		struct FontLoader {
			String &contents;
			bool operator()(const std::string &path) {
				return readFile(path, contents);
			}
		};
		FontLoader loader = { contents };
		if (!visitSearchPaths(resolveRelativePath(fileName), fontDirs, fileName, loader)) {
			return false;
		}
		return parseFont(contents, font);
	}

	bool parseFont(const String &source, IVG::Font &font) {
		try {
			IVG::FontParser parser;
			STLMapVariables variables;
			FormatInfo formatInfo;
			Interpreter interpreter(parser, variables, formatInfo);
			interpreter.run(StringRange(source));
			font = parser.finalizeFont();
			return true;
		} catch (...) {
			return false;
		}
	}

	const CachedImage *resolveImage(const std::string &requested) {
		struct ImageLoader {
			SnapshotPlaybackExecutor &executor;
			const CachedImage *result;
			ImageLoader(SnapshotPlaybackExecutor &executor)
					: executor(executor), result(0) {}
			bool operator()(const std::string &path) {
				result = executor.loadImageFromPath(path);
				return (result != 0);
			}
		};
		ImageLoader loader(*this);
		if (visitSearchPaths(resolveRelativePath(requested), imageDirs, requested, loader)) {
			return loader.result;
		}
		return 0;
	}

        const CachedImage *loadImageFromPath(const std::string &path) {
                const CachedImage *cached = findCachedImage(path);
                if (cached != 0) {
                        return cached;
                }

                CachedImage loaded;
                if (!loadPngRaster(path, loaded.raster)) {
                        return 0;
                }
                loaded.xResolution = 1.0;
                loaded.yResolution = 1.0;

                return insertImage(path, loaded);
        }

        const IVG::Font *findCachedFont(const WideString &fontName) {
                NuXThreads::Lockable<std::map<WideString, IVG::Font> >::Lock lock(sharedResources.fonts);
                std::map<WideString, IVG::Font> &fonts = lock.access();
                const std::map<WideString, IVG::Font>::iterator cached = fonts.find(fontName);
                if (cached == fonts.end()) {
                        return 0;
                }
                return &cached->second;
        }

        const IVG::Font *insertFont(const WideString &fontName, const IVG::Font &font) {
                NuXThreads::Lockable<std::map<WideString, IVG::Font> >::Lock lock(sharedResources.fonts);
                std::map<WideString, IVG::Font> &fonts = lock.access();
                const std::map<WideString, IVG::Font>::iterator inserted =
                        fonts.insert(std::make_pair(fontName, font)).first;
                return &inserted->second;
        }

        std::vector<const IVG::Font *> singleFontResult(const IVG::Font *font) {
                if (font == 0) {
                        return std::vector<const IVG::Font *>();
                }
                std::vector<const IVG::Font *> result(1);
                result[0] = font;
                return result;
        }

        const CachedImage *findCachedImage(const std::string &path) {
                NuXThreads::Lockable<std::map<std::string, CachedImage> >::Lock lock(sharedResources.images);
                std::map<std::string, CachedImage> &images = lock.access();
                const std::map<std::string, CachedImage>::iterator it = images.find(path);
                if (it == images.end()) {
                        return 0;
                }
                return &it->second;
        }

        const CachedImage *insertImage(const std::string &path, const CachedImage &image) {
                NuXThreads::Lockable<std::map<std::string, CachedImage> >::Lock lock(sharedResources.images);
                std::map<std::string, CachedImage> &images = lock.access();
                const std::map<std::string, CachedImage>::iterator existing = images.find(path);
                if (existing != images.end()) {
                        return &existing->second;
                }
                const std::map<std::string, CachedImage>::iterator inserted =
                        images.insert(std::make_pair(path, image)).first;
                return &inserted->second;
        }
};

static void printUsage(const char *program) {
	std::cout << "Usage: " << program << " [options] <ivg> [<ivg> ...]"
			  << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "\t--include-dir <path>\tAdd include search path."
			  << std::endl;
	std::cout << "\t--font-dir <path>\tAdd font search path." << std::endl;
	std::cout << "\t--image-dir <path>\tAdd image search path." << std::endl;
	std::cout << "\t--snapshot-dir <path>\tOverride snapshot directory."
			  << std::endl;
	std::cout << "\t--root-dir <path>\t\tRoot for snapshot name generation."
			  << std::endl;
	std::cout << "\t--force-update\t\tOverwrite goldens." << std::endl;
	std::cout << "\t--threads <n>\t\tNumber of worker threads." << std::endl;
	std::cout << "\t--list-only\t\tList collected snapshots without rendering."
			  << std::endl;
	std::cout << "\t--verbose\t\tPrint verbose diagnostics." << std::endl;
	std::cout << "\t--exit-on-first-failure\tAbort after first failure."
			  << std::endl;
	std::cout << "\t--help\t\t\tShow this message." << std::endl;
}
static bool parseUnsigned(const std::string &text, uint32_t &value) {
	if (text.empty()) {
		return false;
	}

	uint64_t parsed = 0;
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] < '0' || text[i] > '9') {
			return false;
		}
		parsed = parsed * 10 + static_cast<uint32_t>(text[i] - '0');
		if (parsed > UINT32_MAX) {
			return false;
		}
	}

	value = static_cast<uint32_t>(parsed);
	return true;
}

static const char *requireOptionValue(const char *flag,
                                                            const char *errorMessage, int &index, int argc,
                                                            char **argv) {
        if (index + 1 >= argc) {
                std::cerr << errorMessage << std::endl;
                return 0;
        }
        ++index;
        return argv[index];
}

static bool parseCommandLine(int argc, char **argv,
                                                    CommandLineOptions &options) {
        for (int i = 1; i < argc; ++i) {
                const std::string arg(argv[i]);
                if (arg == "--help") {
                        printUsage(argv[0]);
                        return false;
                } else if (arg == "--include-dir") {
                        const char *value = requireOptionValue(
                                "--include-dir", "--include-dir requires a path.", i, argc, argv);
                        if (value == 0) {
                                return false;
                        }
                        options.includeDirs.push_back(value);
                } else if (arg == "--font-dir") {
                        const char *value = requireOptionValue(
                                "--font-dir", "--font-dir requires a path.", i, argc, argv);
                        if (value == 0) {
                                return false;
                        }
                        options.fontDirs.push_back(value);
                } else if (arg == "--image-dir") {
                        const char *value = requireOptionValue(
                                "--image-dir", "--image-dir requires a path.", i, argc, argv);
                        if (value == 0) {
                                return false;
                        }
                        options.imageDirs.push_back(value);
                } else if (arg == "--snapshot-dir") {
                        const char *value = requireOptionValue(
                                "--snapshot-dir", "--snapshot-dir requires a path.", i, argc, argv);
                        if (value == 0) {
                                return false;
                        }
                        options.snapshotDir = value;
                } else if (arg == "--root-dir") {
                        const char *value = requireOptionValue(
                                "--root-dir", "--root-dir requires a path.", i, argc, argv);
                        if (value == 0) {
                                return false;
                        }
                        const std::string rootArgument(value);
                        options.rootDir = pathFromNativeString(rootArgument);
                        if (options.rootDir.isNull()) {
                                std::cerr << "failed to parse root directory: " << rootArgument
                                                << std::endl;
                                return false;
                        }
                } else if (arg == "--force-update") {
                        options.forceUpdate = true;
                } else if (arg == "--threads") {
                        const char *value = requireOptionValue(
                                "--threads", "--threads requires a numeric value.", i, argc, argv);
                        if (value == 0) {
                                return false;
                        }
                        uint32_t threads = 0;
                        if (!parseUnsigned(value, threads)) {
                                std::cerr << "invalid thread count: " << value
                                                  << std::endl;
                                return false;
                        }
                        options.threads = threads;
                } else if (arg == "--list-only") {
                        options.listOnly = true;
		} else if (arg == "--verbose") {
			options.verbose = true;
		} else if (arg == "--exit-on-first-failure") {
			options.exitOnFirstFailure = true;
		} else if (!arg.empty() && arg[0] == '-') {
			std::cerr << "unrecognized option: " << arg << std::endl;
			return false;
		} else {
			options.ivgPaths.push_back(arg);
		}
	}

	if (options.ivgPaths.empty()) {
		if (argc <= 1) {
			printUsage(argv[0]);
		} else {
			std::cerr << "no IVG files specified." << std::endl;
		}
		return false;
	}
	return true;
}

static bool readFile(const std::string &path, String &contents) {
	std::ifstream stream(path.c_str(), std::ios::binary);
	if (!stream.good()) {
		return false;
	}
	std::string buffer((std::istreambuf_iterator<char>(stream)),
					   std::istreambuf_iterator<char>());
	contents.assign(buffer.begin(), buffer.end());
	return true;
}

static void printPlan(const std::string &path, const SnapshotPlan &plan) {
	std::cout << path << std::endl;
	const std::vector<SnapshotScenario> &scenarios = plan.getScenarios();

	for (size_t i = 0; i < scenarios.size(); ++i) {
		const SnapshotScenario &scenario = scenarios[i];
		std::cout << "	Scenario " << scenario.name
				  << " (validate: " << (scenario.validate ? "yes" : "no") << ")"
				  << std::endl;
		for (size_t j = 0; j < scenario.entries.size(); ++j) {
			const SnapshotEntry &entry = scenario.entries[j];
			std::cout << "		Entry " << entry.entryOrdinal << std::endl;
			for (size_t k = 0; k < entry.invocations.size(); ++k) {
				const SnapshotInvocation &invocation = entry.invocations[k];
				std::cout << "			Block " << invocation.blockIndex
						  << " (statement " << invocation.statementOrdinal
						  << "), line " << invocation.sourceLine << std::endl;

				std::istringstream snippet(invocation.statements);
				std::string line;
				while (std::getline(snippet, line)) {
					std::cout << "				[ " << line << " ]"
							  << std::endl;
				}
			}
		}
	}
}
struct SnapshotJob {
	SnapshotJob()
		: options(0), ivgPath(0), snapshotBase(0), document(0), sharedResources(0),
		  scenario(0), entry(0), planOrdinal(0), sentinel(false) {}

	const CommandLineOptions *options;
	const std::string *ivgPath;
	const std::string *snapshotBase;
	const CachedDocument *document;
	SharedResources *sharedResources;
	const SnapshotScenario *scenario;
	const SnapshotEntry *entry;
	uint32_t planOrdinal;
	bool sentinel;
};

class SnapshotScheduler {
  public:
        SnapshotScheduler(uint32_t threadCount, bool exitOnFirstFailure)
                : threadCount(threadCount == 0 ? 1 : threadCount),
                  exitOnFirstFailure(exitOnFirstFailure), started(false),
                  finalizing(false), stopScheduling(false), runningWorkers(0),
                  nextSlot(0), nextResultSlot(0) {}

        ~SnapshotScheduler() { finalize(); }

        void start() {
                if (started) {
                        return;
                }

                slots.reserve(threadCount);
                workers.reserve(threadCount);
                threads.reserve(threadCount);
                for (uint32_t i = 0; i < threadCount; ++i) {
                        slots.push_back(std::unique_ptr<WorkerSlot>(new WorkerSlot()));
                        workers.push_back(
                                std::unique_ptr<Worker>(new Worker(*this, static_cast<uint32_t>(i))));
                        threads.push_back(std::unique_ptr<NuXThreads::Thread>(
                                new NuXThreads::Thread(*workers.back())));
                        threads.back()->start();
                }
                started = true;
        }

        bool enqueue(const SnapshotJob &job) {
                while (true) {
                        {
                                NuXThreads::MutexLock lock(mutex);
                                if (!started || finalizing ||
                                                (exitOnFirstFailure && stopScheduling)) {
                                        return false;
                                }
                                for (uint32_t offset = 0; offset < threadCount; ++offset) {
                                        const uint32_t candidate =
                                                static_cast<uint32_t>((nextSlot + offset) % threadCount);
                                        WorkerSlot &slot = *slots[candidate];
                                        if (!slot.hasJob && !slot.hasResult && !slot.inProgress &&
                                                        !slot.terminate) {
                                                slot.job = job;
                                                slot.hasJob = true;
                                                slot.inProgress = false;
                                                ++runningWorkers;
                                                slot.ready.signal();
                                                nextSlot = static_cast<uint32_t>((candidate + 1) % threadCount);
                                                return true;
                                        }
                                }
                        }
                        slotFreed.wait();
                }
        }

        bool fetchResult(SnapshotEntryResult &out, bool wait) {
                while (true) {
                        {
                                NuXThreads::MutexLock lock(mutex);
                                for (uint32_t offset = 0; offset < threadCount; ++offset) {
                                        const uint32_t candidate = static_cast<uint32_t>(
                                                (nextResultSlot + offset) % threadCount);
                                        WorkerSlot &slot = *slots[candidate];
                                        if (slot.hasResult) {
                                                out = slot.result;
                                                slot.hasResult = false;
                                                slotFreed.signal();
                                                nextResultSlot = static_cast<uint32_t>(
                                                        (candidate + 1) % threadCount);
                                                return true;
                                        }
                                }
                                if (!wait) {
                                        return false;
                                }
                                if (finalizing && runningWorkers == 0) {
                                        bool pending = false;
                                        for (uint32_t i = 0; i < threadCount; ++i) {
                                                if (slots[i]->hasResult) {
                                                        pending = true;
                                                        break;
                                                }
                                        }
                                        if (!pending) {
                                                return false;
                                        }
                                }
                        }
                        resultAvailable.wait();
                }
        }

        void finalize() {
                if (!started) {
                        return;
                }

                {
                        NuXThreads::MutexLock lock(mutex);
                        if (finalizing) {
                                return;
                        }
                        finalizing = true;
                        stopScheduling = true;
                        for (uint32_t i = 0; i < threadCount; ++i) {
                                slots[i]->terminate = true;
                                slots[i]->ready.signal();
                        }
                }

                resultAvailable.signal();
                slotFreed.signal();

                for (size_t i = 0; i < threads.size(); ++i) {
                        threads[i]->join();
                }
                workers.clear();
                threads.clear();
                slots.clear();
                started = false;
        }

        bool shouldStopScheduling() {
                NuXThreads::MutexLock lock(mutex);
                return stopScheduling;
        }

  private:
        class Worker : public NuXThreads::Runnable {
          public:
                Worker(SnapshotScheduler &scheduler, uint32_t slotIndex)
                        : scheduler(scheduler), slotIndex(slotIndex) {}

                void run() override { scheduler.workerLoop(slotIndex); }

          private:
                SnapshotScheduler &scheduler;
                uint32_t slotIndex;
        };

        struct WorkerSlot {
                WorkerSlot()
                        : hasJob(false), hasResult(false), terminate(false), inProgress(false) {}

                SnapshotJob job;
                SnapshotEntryResult result;
                bool hasJob;
                bool hasResult;
                bool terminate;
                bool inProgress;
                NuXThreads::Event ready;
        };

        void workerLoop(uint32_t slotIndex) {
                WorkerSlot &slot = *slots[slotIndex];
                while (true) {
                        slot.ready.wait();
                        SnapshotJob job;
                        {
                                NuXThreads::MutexLock lock(mutex);
                                if (slot.terminate && !slot.hasJob) {
                                        return;
                                }
                                if (!slot.hasJob) {
                                        continue;
                                }
                                job = slot.job;
                                slot.hasJob = false;
                                slot.inProgress = true;
                        }

                        SnapshotEntryResult result = renderEntry(
                                *job.options, *job.ivgPath, *job.snapshotBase, *job.document,
                                *job.sharedResources, *job.scenario, *job.entry);
                        result.planOrdinal = job.planOrdinal;

                        const bool success = result.success;
                        {
                                NuXThreads::MutexLock lock(mutex);
                                slot.result = result;
                                slot.hasResult = true;
                                slot.inProgress = false;
                                if (!success && exitOnFirstFailure) {
                                        stopScheduling = true;
                                        slotFreed.signal();
                                }
                                if (runningWorkers > 0) {
                                        --runningWorkers;
                                }
                                if (slot.terminate) {
                                        slot.ready.signal();
                                }
                        }
                        resultAvailable.signal();
                }
        }

        uint32_t threadCount;
        bool exitOnFirstFailure;
        bool started;
        bool finalizing;
        bool stopScheduling;
        NuXThreads::Mutex mutex;
        NuXThreads::Event resultAvailable;
        NuXThreads::Event slotFreed;
        std::vector<std::unique_ptr<WorkerSlot>> slots;
        std::vector<std::unique_ptr<Worker>> workers;
        std::vector<std::unique_ptr<NuXThreads::Thread>> threads;
        uint32_t runningWorkers;
        uint32_t nextSlot;
        uint32_t nextResultSlot;
};
static void logScenarioFailure(const std::string &ivgPath,
                                                        SnapshotEntryResult &result,
                                                        const char *defaultMessage) {
        if (result.message.empty() && defaultMessage != 0) {
                result.message = defaultMessage;
        }
        const std::string &message = result.message;
        std::cerr << ivgPath << ": scenario " << result.scenarioName << ": " << message
                                  << std::endl;
}

static SnapshotEntryResult
renderEntry(const CommandLineOptions &options, const std::string &path,
                        const std::string &snapshotBase, const CachedDocument &document,
                        SharedResources &sharedResources, const SnapshotScenario &scenario,
                        const SnapshotEntry &entry) {
        SnapshotEntryResult result;
        result.ivgPath = path;
        result.scenarioName = stringFromIMPD(entry.scenarioName);
	result.entryOrdinal = entry.entryOrdinal;
	result.validate = entry.validate;
	result.blockIndex =
		(entry.invocations.empty() ? 0 : entry.invocations[0].blockIndex);
		result.identifier = buildEntryIdentifier(snapshotBase, entry);

	IVG::SelfContainedARGB32Canvas canvas;
	SnapshotPlaybackExecutor executor(canvas, scenario, entry, options, path,
									  sharedResources);
        try {
                document.render(executor);
                result.rendered = true;
        } catch (Exception &e) {
                std::ostringstream message;
                message << e.getError();
                if (e.hasStatement()) {
                        message << " near \"" << e.getStatement() << "\"";
                }
                result.message = message.str();
                logScenarioFailure(path, result, 0);
                return result;
        } catch (std::exception &e) {
                result.message = e.what();
                logScenarioFailure(path, result, 0);
                return result;
        }

        if (!executor.finished()) {
                result.message = "did not execute all snapshot invocations.";
                logScenarioFailure(path, result,
                                                   "did not execute all snapshot invocations.");
                return result;
        }

        NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> *raster =
                canvas.accessRaster();
        if (raster == 0) {
                result.message = "rendered image is empty.";
                logScenarioFailure(path, result, "rendered image is empty.");
                return result;
        }

                SnapshotGolden golden(path, snapshotBase, scenario, entry, options);
        if (!entry.validate) {
                if (!golden.writeDraft(*raster, result)) {
                        logScenarioFailure(path, result, "failed to write draft.");
                }
                return result;
        }

        if (!golden.validate(*raster, options.forceUpdate, result)) {
                logScenarioFailure(path, result, "validation failed.");
                return result;
        }

        return result;
}

static void flushSchedulerResults(SnapshotScheduler &scheduler, bool wait,
								  std::vector<SnapshotEntryResult> &ordered,
								  std::vector<bool> &ready,
								  size_t &nextLogIndex,
								  SnapshotRunResult &run) {
	SnapshotEntryResult fetched;
	bool waitFlag = wait;
	while (scheduler.fetchResult(fetched, waitFlag)) {
		waitFlag = false;
		const uint32_t ordinal = fetched.planOrdinal;
		if (ordinal >= ordered.size()) {
			continue;
		}

		ordered[ordinal] = fetched;
		ready[ordinal] = true;

		while (nextLogIndex < ordered.size() && ready[nextLogIndex]) {
			SnapshotEntryResult &recorded = ordered[nextLogIndex];
			run.entries.push_back(recorded);
			++run.totalEntries;
			if (recorded.validate) {
				++run.validatedEntries;
			} else {
				++run.draftEntries;
			}
			if (recorded.updated) {
				++run.updatedEntries;
			}
			if (!recorded.success) {
				++run.failedEntries;
				if (recorded.diffed) {
					++run.diffFailures;
				}
			}
			++nextLogIndex;
		}
	}
}
static SnapshotRunResult renderPlan(const CommandLineOptions &options,
																		const std::string &path,
																		const CachedDocument &document,
																		const SnapshotPlan &plan) {
		SnapshotRunResult run;
		const std::vector<SnapshotScenario> &scenarios = plan.getScenarios();
		const std::string snapshotBase = buildSnapshotSourceTag(path, options.rootDir);
	SharedResources sharedResources;

	uint32_t threadCount = options.threads;
	if (threadCount == 0) {
		const unsigned int hardware = std::thread::hardware_concurrency();
		threadCount = (hardware > 0 ? hardware : 1);
	}

	const size_t totalJobs = plan.getTotalEntryCount();
	if (totalJobs > 0 && threadCount > totalJobs) {
		threadCount = static_cast<uint32_t>(totalJobs);
	}

	SnapshotScheduler scheduler(threadCount, options.exitOnFirstFailure);
	scheduler.start();

	std::vector<SnapshotEntryResult> ordered;
	std::vector<bool> ready;
	size_t nextLogIndex = 0;
	bool schedulingStopped = false;

	for (size_t i = 0; i < scenarios.size() && !schedulingStopped; ++i) {
		const SnapshotScenario &scenario = scenarios[i];
		if (options.verbose) {
			std::cout << path << ": scenario " << scenario.name
					  << " (validate: " << (scenario.validate ? "yes" : "no")
					  << ")" << std::endl;
		}

		for (size_t j = 0; j < scenario.entries.size(); ++j) {
			if (options.exitOnFirstFailure &&
				scheduler.shouldStopScheduling()) {
				schedulingStopped = true;
				break;
			}

			const SnapshotEntry &entry = scenario.entries[j];
			if (options.verbose) {
				std::cout << path << ":	  entry " << entry.entryOrdinal
						  << " name:" << entry.scenarioName
						  << " (validate: " << (entry.validate ? "yes" : "no")
						  << ")" << std::endl;
			}

			SnapshotJob job;
			job.options = &options;
			job.ivgPath = &path;
			job.snapshotBase = &snapshotBase;
			job.document = &document;
			job.sharedResources = &sharedResources;
			job.scenario = &scenario;
			job.entry = &entry;
			job.planOrdinal = static_cast<uint32_t>(ordered.size());

			if (!scheduler.enqueue(job)) {
				schedulingStopped = true;
				break;
			}

			ordered.push_back(SnapshotEntryResult());
			ready.push_back(false);
			flushSchedulerResults(scheduler, false, ordered, ready,
								  nextLogIndex, run);
		}

		flushSchedulerResults(scheduler, false, ordered, ready, nextLogIndex,
							  run);
	}

	flushSchedulerResults(scheduler, false, ordered, ready, nextLogIndex, run);
	while (nextLogIndex < ordered.size()) {
		flushSchedulerResults(scheduler, true, ordered, ready, nextLogIndex,
							  run);
	}

	scheduler.finalize();
	flushSchedulerResults(scheduler, true, ordered, ready, nextLogIndex, run);

	if (run.failedEntries > 0) {
		run.exitCode = 1;
	}
	return run;
}
static SnapshotRunResult processFile(const CommandLineOptions &options,
									 const std::string &path) {
	SnapshotRunResult run;
	CachedDocument document;
	if (!document.loadFromFile(path)) {
		run.fileFailed = true;
		run.exitCode = 1;
		run.fileError = "failed to read IVG file";
		std::cerr << "failed to read IVG file: " << path << std::endl;
		return run;
	}

	SnapshotPlan plan(path);
	const String &source = document.getSource();

	plan.beginCollection();
	while (true) {
		SnapshotCollector collector(plan, path, source, options.includeDirs);
		STLMapVariables variables;
		FormatInfo formatInfo;
		Interpreter interpreter(collector, variables, formatInfo);

		try {
			interpreter.run(StringRange(source));
		} catch (Exception &e) {
			std::ostringstream message;
			message << e.getError();
			if (e.hasStatement()) {
				message << " near \"" << e.getStatement() << "\"";
			}
			run.fileFailed = true;
			run.exitCode = 1;
			run.fileError = message.str();
			std::cerr << path << ": " << run.fileError << std::endl;
			return run;
		} catch (std::exception &e) {
			run.fileFailed = true;
			run.exitCode = 1;
			run.fileError = e.what();
			std::cerr << path << ": " << run.fileError << std::endl;
			return run;
		}

		plan.completeCollectionPass();
		if (!plan.prepareNextCollectionPass()) {
			break;
		}
	}

	if (options.listOnly || options.verbose) {
		printPlan(path, plan);
	}

	const std::vector<SnapshotScenario> &scenarios = plan.getScenarios();
	if (options.listOnly) {
		for (size_t i = 0; i < scenarios.size(); ++i) {
			const SnapshotScenario &scenario = scenarios[i];
			for (size_t j = 0; j < scenario.entries.size(); ++j) {
				const SnapshotEntry &entry = scenario.entries[j];
				++run.totalEntries;
				if (entry.validate) {
					++run.validatedEntries;
				} else {
					++run.draftEntries;
				}
			}
		}
		run.exitCode = 0;
		return run;
	}

	if (options.verbose) {
		std::cout << path << ": include dirs:";
		if (options.includeDirs.empty()) {
			std::cout << " (none)";
		} else {
			for (size_t i = 0; i < options.includeDirs.size(); ++i) {
				std::cout << ' ' << options.includeDirs[i];
			}
		}
		std::cout << std::endl;
		std::cout << path << ": font dirs:";
		if (options.fontDirs.empty()) {
			std::cout << " (none)";
		} else {
			for (size_t i = 0; i < options.fontDirs.size(); ++i) {
				std::cout << ' ' << options.fontDirs[i];
			}
		}
		std::cout << std::endl;
		std::cout << path << ": image dirs:";
		if (options.imageDirs.empty()) {
			std::cout << " (none)";
		} else {
			for (size_t i = 0; i < options.imageDirs.size(); ++i) {
				std::cout << ' ' << options.imageDirs[i];
			}
		}
		std::cout << std::endl;
	}

	return renderPlan(options, path, document, plan);
}

#if !defined(IVG_SNAPSHOT_TESTING)

int main(int argc, char **argv) {
	CommandLineOptions options;
	if (!parseCommandLine(argc, argv, options)) {
		return 1;
	}

	SnapshotTotals totals;
	int exitCode = 0;
	for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
		const std::string &path = options.ivgPaths[i];
		SnapshotRunResult run = processFile(options, path);
		totals.accumulate(run);
		logFileReport(path, run);
		if (run.exitCode != 0 || run.fileFailed) {
			if (exitCode == 0) {
				exitCode = (run.exitCode != 0 ? run.exitCode : 1);
			}
			if (options.exitOnFirstFailure) {
				break;
			}
		}
	}
	logTotalsSummary(totals);
	return exitCode;
}

#endif // !defined(IVG_SNAPSHOT_TESTING)

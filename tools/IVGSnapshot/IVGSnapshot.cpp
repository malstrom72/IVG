/**
IVG is released under the BSD 2-Clause License.

Copyright (c) 2013-2025, Magnus Lidström

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#include <png.h>
#include <zlib.h>

#include "src/IMPD.h"
#include "src/IVG.h"
#include "externals/NuX/NuXThreads.h"

using namespace IMPD;

	/**
			Loads an IVG source once and replays it on demand so snapshot
			playback can reuse parsed scripts without touching the core library.
	**/
	class CachedDocument {
	public:
		CachedDocument()
		{
		}

		bool loadFromFile(const std::string& path)
		{
			std::ifstream stream(path.c_str(), std::ios::binary);
			if (!stream.good()) {
				source.clear();
				return false;
			}

			const std::string buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			source.assign(buffer.begin(), buffer.end());
			return true;
		}

		void setSource(const IMPD::String& newSource)
		{
			source = newSource;
		}

		const IMPD::String& getSource() const
		{
			return source;
		}

		void render(IVG::IVGExecutor& executor) const
		{
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
		uint32_t scenarioIndex;
		uint32_t entryOrdinal;
		bool validate;
		String scenarioName;
		std::vector<SnapshotInvocation> invocations;
	};

	struct SnapshotScenario {
		String name;
		bool validate;
		bool explicitScenario;
		std::vector<uint32_t> entryIndices;
		std::map<uint32_t, uint32_t> entryLookup;
	};
	struct CachedImage {
		NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> raster;
		double xResolution;
		double yResolution;
	};

		struct SharedResources {
				std::map<WideString, IVG::Font> fonts;
				std::map<std::string, CachedImage> images;
				NuXThreads::Mutex fontMutex;
				NuXThreads::Mutex imageMutex;
		};

	struct SnapshotDiffStats {
		SnapshotDiffStats()
		: width(0)
		, height(0)
		, differingPixels(0)
		, maxAlphaDiff(0)
		, maxRedDiff(0)
		, maxGreenDiff(0)
		, maxBlueDiff(0)
		, meanAlphaDiff(0.0)
		, meanRedDiff(0.0)
		, meanGreenDiff(0.0)
		, meanBlueDiff(0.0)
		{
		}

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
				: entryOrdinal(0)
				, validate(false)
				, rendered(false)
				, diffed(false)
				, skipped(false)
				, updated(false)
				, success(false)
				, hasDiffStats(false)
				, planOrdinal(0)
				, blockIndex(0)
				{
				}

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
				std::string disabledPath;
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
		: totalEntries(0)
		, draftEntries(0)
		, validatedEntries(0)
		, updatedEntries(0)
		, failedEntries(0)
		, diffFailures(0)
		, exitCode(0)
		, fileFailed(false)
		{
		}

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
		: filesProcessed(0)
		, failedFiles(0)
		, totalEntries(0)
		, draftEntries(0)
		, validatedEntries(0)
		, updatedEntries(0)
		, failedEntries(0)
		, diffFailures(0)
		{
		}

		void accumulate(const SnapshotRunResult& run)
		{
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
std::string outputDir;
				bool forceUpdate;
				bool listOnly;
				bool verbose;
				bool exitOnFirstFailure;
				uint32_t threads;
				std::vector<std::string> ivgPaths;

				CommandLineOptions()
				: forceUpdate(false)
				, listOnly(false)
				, verbose(false)
				, exitOnFirstFailure(false)
				, threads(0)
				{
				}
};

	static std::string stringFromIMPD(const String& value)
	{
		return std::string(value.begin(), value.end());
	}

static std::string extractDirectory(const std::string& path)
{
const size_t slash = path.find_last_of("/\\");
if (slash == std::string::npos) {
return std::string();
}
return path.substr(0, slash);
}

	static std::string joinPath(const std::string& base, const std::string& component)
	{
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
		
static std::string sanitizeFileComponent(const std::string& name)
{
std::string sanitized = name;
for (size_t i = 0; i < sanitized.size(); ++i) {
if (sanitized[i] == '/' || sanitized[i] == '\\') {
sanitized[i] = '_';
}
}
return sanitized;
}

		static std::string buildEntryIdentifier(const std::string& baseName, const SnapshotEntry& entry)
		{
				const uint32_t blockIndex = (entry.invocations.empty() ? 0 : entry.invocations[0].blockIndex);
				std::ostringstream stream;
				stream << baseName << '#'
						<< stringFromIMPD(entry.scenarioName) << '#'
						<< blockIndex << '#'
						<< entry.entryOrdinal;
				return stream.str();
		}

	static bool fileExists(const std::string& path)
	{
	if (path.empty()) {
	return false;
	}
	#if defined(_WIN32)
	struct _stat info;
	if (_stat(path.c_str(), &info) != 0) {
	return false;
	}
	return ((info.st_mode & _S_IFREG) != 0);
	#else
	struct stat info;
	if (stat(path.c_str(), &info) != 0) {
	return false;
	}
	return S_ISREG(info.st_mode);
	#endif
	}
	
	static bool directoryExists(const std::string& path)
	{
	if (path.empty()) {
	return false;
	}
	#if defined(_WIN32)
	struct _stat info;
	if (_stat(path.c_str(), &info) != 0) {
	return false;
	}
	return ((info.st_mode & _S_IFDIR) != 0);
	#else
	struct stat info;
	if (stat(path.c_str(), &info) != 0) {
	return false;
	}
	return S_ISDIR(info.st_mode);
	#endif
	}
	
	static bool makeDirectory(const std::string& path)
	{
	if (path.empty()) {
	return true;
	}
	#if defined(_WIN32)
	if (_mkdir(path.c_str()) == 0) {
	return true;
	}
	if (errno == EEXIST) {
	return directoryExists(path);
	}
	#else
	if (::mkdir(path.c_str(), 0777) == 0) {
	return true;
	}
	if (errno == EEXIST) {
	return directoryExists(path);
	}
	#endif
	return false;
	}
	
	static bool ensureDirectory(const std::string& path)
	{
	if (path.empty() || directoryExists(path)) {
	return true;
	}
	
	std::string normalized = path;
	for (size_t i = 0; i < normalized.size(); ++i) {
	if (normalized[i] == '\\') {
	normalized[i] = '/';
	}
	}
	
	std::string prefix;
	size_t start = 0;
	if (normalized.size() >= 2 && normalized[1] == ':') {
	prefix = normalized.substr(0, 2);
	if (normalized.size() > 2 && normalized[2] == '/') {
	prefix += '/';
	start = 3;
	} else {
	start = 2;
	}
	} else if (!normalized.empty() && normalized[0] == '/') {
	prefix = "/";
	start = 1;
	}
	
	for (size_t i = start; i <= normalized.size(); ++i) {
	if (i == normalized.size() || normalized[i] == '/') {
	const std::string segment = normalized.substr(start, i - start);
	if (!segment.empty()) {
	if (!prefix.empty() && prefix[prefix.size() - 1] != '/') {
	prefix += '/';
	}
	prefix += segment;
	}
	if (!prefix.empty() && !directoryExists(prefix)) {
	if (!makeDirectory(prefix)) {
	return false;
	}
	}
	start = i + 1;
	}
	}
	return directoryExists(path);
	}
	
	static bool ensureParentDirectory(const std::string& filePath)
	{
	const size_t slash = filePath.find_last_of("/\\");
	if (slash == std::string::npos) {
	return true;
	}
	return ensureDirectory(filePath.substr(0, slash));
	}
	
	static void removeFileIfExists(const std::string& path)
	{
	if (!path.empty()) {
	std::remove(path.c_str());
	}
	}
	
	static bool renameFile(const std::string& from, const std::string& to, std::string& error)
	{
	if (from.empty() || from == to) {
	return true;
	}
	removeFileIfExists(to);
	if (std::rename(from.c_str(), to.c_str()) != 0) {
	error = std::string("failed to rename ") + from + " to " + to + ": " + std::strerror(errno);
	return false;
	}
	return true;
	}

	static std::string jsonEscape(const std::string& value)
	{
	std::ostringstream stream;
	for (size_t i = 0; i < value.size(); ++i) {
	const unsigned char c = static_cast<unsigned char>(value[i]);
	switch (c) {
	case '\\':
	stream << "\\\\";
	break;
	case '"':
	stream << "\\\"";
	break;
	case '\n':
	stream << "\\n";
	break;
	case '\r':
	stream << "\\r";
	break;
	case '\t':
	stream << "\\t";
	break;
	default:
	if (c < 0x20) {
	char buffer[7];
	std::snprintf(buffer, sizeof(buffer), "\\u%04X", static_cast<unsigned int>(c));
	stream << buffer;
	} else {
	stream << value[i];
	}
	break;
	}
	}
	return stream.str();
	}
	
	static void logEntryResult(const SnapshotEntryResult& result)
	{
	std::ostringstream stream;
	stream << "{\"event\":\"snapshot-entry\"";
	stream << ",\"ivg\":\"" << jsonEscape(result.ivgPath) << "\"";
	stream << ",\"scenario\":\"" << jsonEscape(result.scenarioName) << "\"";
		stream << ",\"entry\":" << result.entryOrdinal;
		stream << ",\"block\":" << result.blockIndex;
		stream << ",\"validate\":" << (result.validate ? "true" : "false");
		stream << ",\"rendered\":" << (result.rendered ? "true" : "false");
		stream << ",\"diffed\":" << (result.diffed ? "true" : "false");
		stream << ",\"skipped\":" << (result.skipped ? "true" : "false");
		stream << ",\"updated\":" << (result.updated ? "true" : "false");
		stream << ",\"success\":" << (result.success ? "true" : "false");
		stream << ",\"identifier\":\"" << jsonEscape(result.identifier) << "\"";
		stream << ",\"golden\":\"" << jsonEscape(result.goldenPath) << "\"";
	stream << ",\"disabled\":\"" << jsonEscape(result.disabledPath) << "\"";
	stream << ",\"actual\":\"" << jsonEscape(result.actualPath) << "\"";
	stream << ",\"diff\":\"" << jsonEscape(result.diffPath) << "\"";
	stream << ",\"backup\":\"" << jsonEscape(result.backupPath) << "\"";
	if (!result.message.empty()) {
	stream << ",\"message\":\"" << jsonEscape(result.message) << "\"";
	}
	if (result.hasDiffStats) {
	std::ostringstream diff;
	diff.setf(std::ios::fixed);
	diff << "{\"width\":" << result.diffStats.width
	<< ",\"height\":" << result.diffStats.height
	<< ",\"pixels\":" << result.diffStats.differingPixels
	<< ",\"max\":{\"alpha\":" << result.diffStats.maxAlphaDiff
	<< ",\"red\":" << result.diffStats.maxRedDiff
	<< ",\"green\":" << result.diffStats.maxGreenDiff
	<< ",\"blue\":" << result.diffStats.maxBlueDiff << "}"
	<< ",\"mean\":{\"alpha\":" << std::setprecision(4) << result.diffStats.meanAlphaDiff
	<< ",\"red\":" << result.diffStats.meanRedDiff
	<< ",\"green\":" << result.diffStats.meanGreenDiff
	<< ",\"blue\":" << result.diffStats.meanBlueDiff << "}}";
	stream << ",\"diffStats\":" << diff.str();
	}
	stream << "}";
	std::cout << stream.str() << std::endl;
	}
	
	static void logFileSummary(const std::string& path, const SnapshotRunResult& run)
	{
	std::ostringstream stream;
	stream << "{\"event\":\"snapshot-file-summary\"";
	stream << ",\"ivg\":\"" << jsonEscape(path) << "\"";
	stream << ",\"entries\":" << run.totalEntries;
	stream << ",\"draft\":" << run.draftEntries;
	stream << ",\"validated\":" << run.validatedEntries;
	stream << ",\"updated\":" << run.updatedEntries;
	stream << ",\"failed\":" << run.failedEntries;
	stream << ",\"diffFailures\":" << run.diffFailures;
	stream << ",\"exitCode\":" << run.exitCode;
	if (run.fileFailed && !run.fileError.empty()) {
	stream << ",\"error\":\"" << jsonEscape(run.fileError) << "\"";
	}
	stream << "}";
	std::cout << stream.str() << std::endl;
	}
	
		static void logRunSummary(const SnapshotTotals& totals)
		{
		std::ostringstream stream;
		stream << "{\"event\":\"snapshot-run-summary\"";
		stream << ",\"files\":" << totals.filesProcessed;
		stream << ",\"failedFiles\":" << totals.failedFiles;
		stream << ",\"entries\":" << totals.totalEntries;
		stream << ",\"draft\":" << totals.draftEntries;
		stream << ",\"validated\":" << totals.validatedEntries;
		stream << ",\"updated\":" << totals.updatedEntries;
		stream << ",\"failed\":" << totals.failedEntries;
		stream << ",\"diffFailures\":" << totals.diffFailures;
		stream << "}";
		std::cout << stream.str() << std::endl;
		}



static bool loadPngRaster(const std::string& path, NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>& outRaster);
static bool writeRasterToPng(const std::string& path, const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>& raster, std::string& error);
static SnapshotEntryResult renderEntry(const CommandLineOptions& options,
				const std::string& path,
				const std::string& baseName,
				const CachedDocument& document,
				SharedResources& sharedResources,
				const SnapshotScenario& scenario,
				const SnapshotEntry& entry);


		class SnapshotGolden {
public:
SnapshotGolden(const std::string& ivgPath,
const std::string& baseName,
const SnapshotScenario& scenario,
const SnapshotEntry& entry,
const CommandLineOptions& options)
{
const std::string sanitizedBase = sanitizeFileComponent(baseName);
std::string root = (options.outputDir.empty() ? extractDirectory(ivgPath) : options.outputDir);
if (!sanitizedBase.empty()) {
root = joinPath(root, sanitizedBase);
}
std::string scenarioName = stringFromIMPD(entry.scenarioName);
if (scenario.entryIndices.size() > 1) {
scenarioName += "-";
scenarioName += Interpreter::toString(static_cast<int32_t>(entry.entryOrdinal));
}
scenarioName = sanitizeFileComponent(scenarioName);
const std::string stem = joinPath(root, scenarioName);
goldenPath = stem + ".png";
disabledPath = stem + ".png.disabled";
actualPath = stem + ".actual.png";
diffPath = stem + ".diff.png";
backupPath = stem + ".png.bak";
}

void populateResult(SnapshotEntryResult& result) const
{
result.goldenPath = goldenPath;
result.disabledPath = disabledPath;
result.actualPath = actualPath;
result.diffPath = diffPath;
result.backupPath = backupPath;
}

bool writeDraft(const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>& raster, SnapshotEntryResult& result) const
{
populateResult(result);
result.skipped = true;
result.updated = false;
result.diffed = false;
result.hasDiffStats = false;
result.success = true;

const NuXPixels::IntRect bounds = raster.calcBounds();
if (bounds.width <= 0 || bounds.height <= 0) {
removeFileIfExists(disabledPath);
removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
return true;
}

if (!ensureParentDirectory(disabledPath)) {
result.success = false;
result.message = std::string("failed to prepare directory for ") + disabledPath + ": " + std::strerror(errno);
return false;
}

std::string error;
if (!writeRasterToPng(disabledPath, raster, error)) {
result.success = false;
result.message = error;
return false;
}
removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
return true;
}

bool validate(const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>& raster, bool forceUpdate, SnapshotEntryResult& result) const
{
populateResult(result);
result.skipped = false;
result.diffed = false;
result.updated = false;
result.hasDiffStats = false;

std::string error;
if (!ensureParentDirectory(goldenPath)) {
result.success = false;
result.message = std::string("failed to prepare directory for ") + goldenPath + ": " + std::strerror(errno);
return false;
}

const bool goldenExists = fileExists(goldenPath);
const bool disabledExists = fileExists(disabledPath);

if (forceUpdate) {
const NuXPixels::IntRect bounds = raster.calcBounds();
if (bounds.width <= 0 || bounds.height <= 0) {
removeFileIfExists(goldenPath);
removeFileIfExists(disabledPath);
removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
removeFileIfExists(backupPath);
result.updated = true;
result.success = true;
return true;
}

if (goldenExists) {
if (!renameFile(goldenPath, backupPath, error)) {
result.success = false;
result.message = error;
return false;
}
} else if (disabledExists) {
if (!renameFile(disabledPath, backupPath, error)) {
result.success = false;
result.message = error;
return false;
}
}

removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
if (!writeRasterToPng(goldenPath, raster, error)) {
result.success = false;
result.message = error;
return false;
}
removeFileIfExists(disabledPath);
result.updated = true;
result.success = true;
return true;
}

if (!goldenExists) {
result.success = false;
if (disabledExists) {
result.message = std::string("missing golden: ") + goldenPath + " (draft exists; rerun with --force-update)";
} else {
result.message = std::string("missing golden: ") + goldenPath;
}
return false;
}

NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> goldenRaster;
if (!loadPngRaster(goldenPath, goldenRaster)) {
result.success = false;
result.message = std::string("failed to read golden PNG: ") + goldenPath;
return false;
}

const NuXPixels::IntRect actualBounds = raster.calcBounds();
const NuXPixels::IntRect goldenBounds = goldenRaster.calcBounds();
const int left = NuXPixels::minValue(actualBounds.left, goldenBounds.left);
const int top = NuXPixels::minValue(actualBounds.top, goldenBounds.top);
const int right = NuXPixels::maxValue(actualBounds.calcRight(), goldenBounds.calcRight());
const int bottom = NuXPixels::maxValue(actualBounds.calcBottom(), goldenBounds.calcBottom());
const int width = (right > left ? right - left : 0);
const int height = (bottom > top ? bottom - top : 0);

SnapshotDiffStats stats;
stats.width = static_cast<uint32_t>(width);
stats.height = static_cast<uint32_t>(height);

if (width <= 0 || height <= 0) {
result.success = true;
removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
return true;
}

NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> diffRaster(NuXPixels::IntRect(left, top, width, height));
const NuXPixels::ARGB32::Pixel* actualPixels = raster.getPixelPointer();
const NuXPixels::ARGB32::Pixel* goldenPixelsPtr = goldenRaster.getPixelPointer();
const int actualStride = raster.getStride();
const int goldenStride = goldenRaster.getStride();
NuXPixels::ARGB32::Pixel* diffPixels = diffRaster.getPixelPointer();
const int diffStride = diffRaster.getStride();

uint64_t sumAlpha = 0;
uint64_t sumRed = 0;
uint64_t sumGreen = 0;
uint64_t sumBlue = 0;
bool match = true;

for (int y = top; y < bottom; ++y) {
NuXPixels::ARGB32::Pixel* diffRow = diffPixels + y * diffStride;
const NuXPixels::ARGB32::Pixel* actualRow = (y >= actualBounds.top && y < actualBounds.calcBottom()) ? actualPixels + y * actualStride : 0;
const NuXPixels::ARGB32::Pixel* goldenRow = (y >= goldenBounds.top && y < goldenBounds.calcBottom()) ? goldenPixelsPtr + y * goldenStride : 0;
for (int x = left; x < right; ++x) {
const NuXPixels::ARGB32::Pixel actualPixel = (actualRow != 0 && x >= actualBounds.left && x < actualBounds.calcRight()) ? actualRow[x] : 0;
const NuXPixels::ARGB32::Pixel goldenPixel = (goldenRow != 0 && x >= goldenBounds.left && x < goldenBounds.calcRight()) ? goldenRow[x] : 0;
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
const unsigned int diffA = (actualA > goldenA ? actualA - goldenA : goldenA - actualA);
const unsigned int diffR = (actualR > goldenR ? actualR - goldenR : goldenR - actualR);
const unsigned int diffG = (actualG > goldenG ? actualG - goldenG : goldenG - actualG);
const unsigned int diffB = (actualB > goldenB ? actualB - goldenB : goldenB - actualB);
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
diffRow[x] = (0xFFu << 24) | (scaledR << 16) | (scaledG << 8) | scaledB;
}
}

const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
if (pixelCount > 0) {
stats.meanAlphaDiff = static_cast<double>(sumAlpha) / pixelCount;
stats.meanRedDiff = static_cast<double>(sumRed) / pixelCount;
stats.meanGreenDiff = static_cast<double>(sumGreen) / pixelCount;
stats.meanBlueDiff = static_cast<double>(sumBlue) / pixelCount;
}

if (match) {
result.success = true;
removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
return true;
}

result.success = false;
result.diffed = true;
result.hasDiffStats = true;
result.diffStats = stats;

std::ostringstream summary;
summary << "differs from golden (pixels: " << stats.differingPixels << "/" << (stats.width * stats.height) << ")";
result.message = summary.str();

removeFileIfExists(actualPath);
removeFileIfExists(diffPath);
if (!writeRasterToPng(actualPath, raster, error)) {
result.message = error;
return false;
}
if (!writeRasterToPng(diffPath, diffRaster, error)) {
result.message = error;
return false;
}

return false;
}

private:
std::string goldenPath;
std::string disabledPath;
std::string actualPath;
std::string diffPath;
std::string backupPath;
};

	static void PNGAPI snapshotPNGError(png_structp png, png_const_charp message)
			{
				throw std::runtime_error(std::string("Error reading PNG image: ") + message);
			}
		
			static bool isLittleEndian()
			{
				static const unsigned char bytes[4] = { 0x4A, 0x3B, 0x2C, 0x1D };
				return (*reinterpret_cast<const unsigned int*>(bytes) == 0x1D2C3B4A);
			}
		
static bool loadPngRaster(const std::string& path, NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>& outRaster)
{
FILE* file = std::fopen(path.c_str(), "rb");
if (file == 0) {
return false;
}
		
				png_structp png = 0;
				png_infop info = 0;
				try {
					png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, snapshotPNGError, 0);
					if (png == 0) {
						throw std::runtime_error("could not initialize PNG reader");
					}
		
					info = png_create_info_struct(png);
					if (info == 0) {
						throw std::runtime_error("could not initialize PNG info struct");
					}
		
					png_init_io(png, file);
					png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
					if (isLittleEndian()) {
						png_set_bgr(png);
					} else {
						png_set_swap_alpha(png);
					}
		
					png_read_png(png, info, PNG_TRANSFORM_EXPAND, 0);
					const png_uint_32 width = png_get_image_width(png, info);
					const png_uint_32 height = png_get_image_height(png, info);
					png_bytep* rows = png_get_rows(png, info);
		
					outRaster = NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>(NuXPixels::IntRect(0, 0,
						static_cast<int>(width), static_cast<int>(height)));
		
					for (png_uint_32 y = 0; y < height; ++y) {
						NuXPixels::ARGB32::Pixel* dest = outRaster.getPixelPointer() + y * outRaster.getStride();
						png_bytep src = rows[y];
						for (png_uint_32 x = 0; x < width; ++x) {
							unsigned int b = src[x * 4 + 0];
							unsigned int g = src[x * 4 + 1];
							unsigned int r = src[x * 4 + 2];
							unsigned int a = src[x * 4 + 3];
							if (a != 0xFF) {
								r = (r * a + 0x7F) >> 8;
								g = (g * a + 0x7F) >> 8;
								b = (b * a + 0x7F) >> 8;
							}
							dest[x] = (a << 24) | (r << 16) | (g << 8) | b;
}
}

					png_destroy_read_struct(&png, &info, 0);
					std::fclose(file);
					return true;
				} catch (...) {
					png_destroy_read_struct(&png, &info, 0);
					std::fclose(file);
					return false;
						}
}

static bool writeRasterToPng(const std::string& path,
const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>& raster,
std::string& error)
{
const NuXPixels::IntRect bounds = raster.calcBounds();
if (bounds.width <= 0 || bounds.height <= 0) {
error = std::string("raster has no visible pixels: ") + path;
return false;
}

const int width = bounds.width;
const int height = bounds.height;
const int stride = raster.getStride();
const NuXPixels::ARGB32::Pixel* base = raster.getPixelPointer() + bounds.top * stride + bounds.left;

std::vector<png_bytep> rows(static_cast<size_t>(height));
std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
for (int y = 0; y < height; ++y) {
const NuXPixels::ARGB32::Pixel* src = base + y * stride;
unsigned char* dest = &pixels[static_cast<size_t>(y) * static_cast<size_t>(width) * 4u];
rows[static_cast<size_t>(y)] = dest;
for (int x = 0; x < width; ++x) {
const NuXPixels::ARGB32::Pixel pixel = src[x];
unsigned int a = (pixel >> 24) & 0xFF;
unsigned int r = (pixel >> 16) & 0xFF;
unsigned int g = (pixel >> 8) & 0xFF;
unsigned int b = pixel & 0xFF;
if (a != 0 && a != 0xFF) {
const unsigned int m = 0xFFFFu / a;
r = (r * m) >> 8;
g = (g * m) >> 8;
b = (b * m) >> 8;
}
dest[x * 4 + 0] = static_cast<unsigned char>(r);
dest[x * 4 + 1] = static_cast<unsigned char>(g);
dest[x * 4 + 2] = static_cast<unsigned char>(b);
dest[x * 4 + 3] = static_cast<unsigned char>(a);
}
}

FILE* file = std::fopen(path.c_str(), "wb");
if (file == 0) {
error = std::string("failed to open ") + path + ": " + std::strerror(errno);
return false;
}

png_structp png = 0;
png_infop info = 0;

try {
png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, snapshotPNGError, 0);
if (png == 0) {
throw std::runtime_error("could not initialize PNG writer");
}

info = png_create_info_struct(png);
if (info == 0) {
throw std::runtime_error("could not initialize PNG info struct");
}

png_init_io(png, file);
png_set_IHDR(png, info,
static_cast<png_uint_32>(width),
static_cast<png_uint_32>(height),
8,
PNG_COLOR_TYPE_RGB_ALPHA,
PNG_INTERLACE_NONE,
PNG_COMPRESSION_TYPE_DEFAULT,
PNG_FILTER_TYPE_DEFAULT);
png_set_sRGB_gAMA_and_cHRM(png, info, PNG_sRGB_INTENT_ABSOLUTE);
png_set_oFFs(png, info,
static_cast<png_int_32>(bounds.left),
static_cast<png_int_32>(bounds.top),
PNG_OFFSET_PIXEL);
png_set_rows(png, info, &rows[0]);
png_write_png(png, info, PNG_TRANSFORM_IDENTITY, 0);
png_destroy_write_struct(&png, &info);
std::fclose(file);
return true;
} catch (const std::exception& e) {
error = std::string("failed to write PNG: ") + e.what();
} catch (...) {
error = "failed to write PNG: unknown error";
}

png_destroy_write_struct(&png, &info);
std::fclose(file);
return false;
}


class SnapshotPlan {
			public:
			explicit SnapshotPlan(const std::string& ivgPath)
			: baseName(extractBaseName(ivgPath))
			, nextBlockOrdinal(1)
			, collectingPlan(false)
                       , activeScenarioIndex(0)
                       , activeEntryOrdinal(1)
                       , collectionRunCursor(0)
                       , collectionRunsBuilt(false)
                       , recordedBlockCursor(0)
			{
			}

			uint32_t addBlock(Interpreter& interpreter, const SnapshotBlock& block)
			{
				if (block.statements.empty()) {
					Interpreter::throwBadSyntax("snapshot meta requires at least one statement block.");
				}

                               uint32_t blockOrdinal = nextBlockOrdinal;
                               if (collectionRunsBuilt) {
                                       if (recordedBlockCursor >= recordedBlockOrdinals.size()) {
                                               Interpreter::throwBadSyntax("snapshot replay encountered an unexpected block.");
                                       }
                                       blockOrdinal = recordedBlockOrdinals[recordedBlockCursor++];
                                       return blockOrdinal;
                               }

                               recordedBlockOrdinals.push_back(blockOrdinal);
                               const bool hasExplicitScenario = !block.scenario.empty();
				if (hasExplicitScenario) {
					const uint32_t scenarioIndex = resolveScenario(interpreter, block.scenario, block.validate, true);
					SnapshotScenario& scenario = scenarios[scenarioIndex];
					const uint32_t statementCount = static_cast<uint32_t>(block.statements.size());
					if (!scenario.entryIndices.empty() && statementCount != scenario.entryIndices.size()) {
						Interpreter::throwBadSyntax("scenario entry count does not match previous blocks.");
					}

					for (uint32_t i = 0; i < statementCount; ++i) {
						const uint32_t entryOrdinal = i + 1;
						SnapshotEntry& entry = ensureEntry(scenarioIndex, scenario, entryOrdinal, block.validate, block.scenario);

						SnapshotInvocation invocation;
						invocation.blockIndex = blockOrdinal;
						invocation.sourceLine = block.sourceLine;
						invocation.statementOrdinal = entryOrdinal;
						invocation.statements = block.statements[i];
						entry.invocations.push_back(invocation);
					}
				} else {
					const uint32_t statementCount = static_cast<uint32_t>(block.statements.size());
					for (uint32_t i = 0; i < statementCount; ++i) {
						const uint32_t entryOrdinal = 1;
						const String scenarioName = synthesizeScenarioName(blockOrdinal, statementCount, i + 1);
						const uint32_t scenarioIndex = resolveScenario(interpreter, scenarioName, block.validate, false);
						SnapshotScenario& scenario = scenarios[scenarioIndex];
						SnapshotEntry& entry = ensureEntry(scenarioIndex, scenario, entryOrdinal, block.validate, scenarioName);

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

			const std::vector<SnapshotScenario>& getScenarios() const { return scenarios; }
			const std::vector<SnapshotEntry>& getEntries() const { return entries; }
			const String& getBaseName() const { return baseName; }

                       void beginCollection()
                       {
                               collectingPlan = true;
                               activeScenarioIndex = 0;
                               activeEntryOrdinal = 1;
                               collectionRuns.clear();
                               collectionRunCursor = 0;
                               collectionRunsBuilt = false;
                               recordedBlockOrdinals.clear();
                               recordedBlockCursor = 0;
                       }

			void completeCollectionPass()
			{
				collectingPlan = false;
			}

                       bool prepareNextCollectionPass()
                       {
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
                               const CollectionRun& run = collectionRuns[collectionRunCursor];
                               activeScenarioIndex = run.scenarioIndex;
                               activeEntryOrdinal = run.entryOrdinal;
                               collectingPlan = true;
                               recordedBlockCursor = 0;
                               return true;
                       }

			bool isCollectingPlan() const
			{
				return collectingPlan;
			}

			uint32_t getActiveScenarioIndex() const
			{
				return activeScenarioIndex;
			}

			uint32_t getActiveEntryOrdinal() const
			{
				return activeEntryOrdinal;
			}

			const SnapshotInvocation* lookupInvocation(uint32_t blockOrdinal, uint32_t scenarioIndex, uint32_t entryOrdinal) const
			{
				if (scenarioIndex >= scenarios.size()) {
					return 0;
				}

				const SnapshotScenario& scenario = scenarios[scenarioIndex];
				if (entryOrdinal == 0 || entryOrdinal > scenario.entryIndices.size()) {
					return 0;
				}

				const uint32_t entryIndex = scenario.entryIndices[entryOrdinal - 1];
				const SnapshotEntry& entry = entries[entryIndex];
				for (size_t i = 0; i < entry.invocations.size(); ++i) {
					if (entry.invocations[i].blockIndex == blockOrdinal) {
						return &entry.invocations[i];
					}
				}
				return 0;
			}

			private:
			String extractBaseName(const std::string& path) const
			{
				const size_t slash = path.find_last_of("/\\");
				const size_t baseOffset = (slash == std::string::npos ? 0 : slash + 1);
				size_t dot = path.find_last_of('.');
				if (dot == std::string::npos || dot < baseOffset) {
					dot = path.size();
				}
				return String(path.c_str() + baseOffset, path.c_str() + dot);
			}

			String synthesizeScenarioName(uint32_t blockOrdinal, uint32_t blockCount, uint32_t entryOrdinal) const
			{
				String name = baseName;
				name += '-';
				name += Interpreter::toString(static_cast<int32_t>(blockOrdinal));
				if (blockCount > 1) {
					name += '-';
					name += Interpreter::toString(static_cast<int32_t>(entryOrdinal));
				}
				return name;
			}

			uint32_t resolveScenario(Interpreter& interpreter, const String& name, bool validate, bool explicitScenario)
			{
				const std::map<String, uint32_t>::const_iterator it = scenarioLookup.find(name);
				if (it != scenarioLookup.end()) {
					SnapshotScenario& existing = scenarios[it->second];
					if (existing.validate != validate) {
						Interpreter::throwBadSyntax("scenario switches between validate yes/no.");
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

			SnapshotEntry& ensureEntry(uint32_t scenarioIndex, SnapshotScenario& scenario, uint32_t entryOrdinal, bool validate, const String& scenarioName)
			{
				const std::map<uint32_t, uint32_t>::const_iterator existing = scenario.entryLookup.find(entryOrdinal);
				if (existing != scenario.entryLookup.end()) {
					return entries[existing->second];
				}

				SnapshotEntry entry;
				entry.scenarioIndex = scenarioIndex;
				entry.entryOrdinal = entryOrdinal;
				entry.validate = validate;
				entry.scenarioName = scenarioName;

				entries.push_back(entry);
				const uint32_t entryIndex = static_cast<uint32_t>(entries.size() - 1);
				scenario.entryLookup.insert(std::make_pair(entryOrdinal, entryIndex));

				size_t insertPosition = scenario.entryIndices.size();
				for (size_t i = 0; i < scenario.entryIndices.size(); ++i) {
					const SnapshotEntry& existingEntry = entries[scenario.entryIndices[i]];
					if (existingEntry.entryOrdinal > entryOrdinal) {
						insertPosition = i;
						break;
					}
				}
				scenario.entryIndices.insert(scenario.entryIndices.begin() + insertPosition, entryIndex);
				return entries.back();
			}

			String baseName;
			std::vector<SnapshotEntry> entries;
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

			void buildCollectionRuns()
			{
				collectionRuns.clear();
				for (uint32_t scenarioIndex = 0; scenarioIndex < scenarios.size(); ++scenarioIndex) {
					const SnapshotScenario& scenario = scenarios[scenarioIndex];
					if (scenario.entryIndices.empty()) {
						continue;
					}

					for (uint32_t entryOrdinal = 1; entryOrdinal <= scenario.entryIndices.size(); ++entryOrdinal) {
						CollectionRun run;
						run.scenarioIndex = scenarioIndex;
						run.entryOrdinal = entryOrdinal;
						collectionRuns.push_back(run);
					}
				}

				if (!collectionRuns.empty()) {
					const CollectionRun& first = collectionRuns[0];
					activeScenarioIndex = first.scenarioIndex;
					activeEntryOrdinal = first.entryOrdinal;
				}
			}
};
static bool isWhitespace(Char c)
			{
				return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
			}
		
			static StringRange trimRange(const StringRange& range)
			{
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
		
			static String stripBrackets(const String& value)
			{
				if (!Interpreter::isBracketBlock(value)) {
					Interpreter::throwBadSyntax("snapshot statements must be enclosed in [ ].");
				}
				return String(value.begin() + 1, value.end() - 1);
			}
		
			static bool parseValidateFlag(Interpreter& interpreter, const String* value)
			{
				if (value == 0) {
					return true;
				}
				return interpreter.toBool(*value);
			}
		
			static StringVector parseSnapshotStatements(Interpreter& interpreter, const String& raw)
			{
				const StringRange trimmed = trimRange(StringRange(raw));
				if (trimmed.b == trimmed.e) {
					Interpreter::throwBadSyntax("snapshot meta requires a bracketed statement list.");
				}
		
				const String outer(trimmed);
				if (!Interpreter::isBracketBlock(outer)) {
					Interpreter::throwBadSyntax("snapshot statements must start with [ and end with ].");
				}
		
				String inner = stripBrackets(outer);
				const StringRange innerRange(inner);
				const StringRange innerTrimmed = trimRange(innerRange);
		
				StringVector result;
				if (innerTrimmed.b != innerTrimmed.e && *innerTrimmed.b == '[') {
					StringVector tuple;
					interpreter.parseList(StringRange(inner), tuple, false, false, 1, INT_MAX);
					result.reserve(tuple.size());
					for (size_t i = 0; i < tuple.size(); ++i) {
						result.push_back(stripBrackets(tuple[i]));
					}
				} else {
					result.push_back(inner);
				}
				return result;
			}
		
		
			static bool readFile(const std::string& path, String& contents);
		
			class SnapshotCollector : public Executor {
				public:
				SnapshotCollector(SnapshotPlan& plan, const std::string& sourcePath, const String& sourceText, const std::vector<std::string>& includeDirs)
				: plan(plan)
				, sourcePath(sourcePath)
				, sourceText(sourceText)
				, includeDirs(includeDirs)
				, scanOffset(0)
				{
				}
		
				bool format(Interpreter& interpreter, const FormatInfo& formatInfo) override
				{
					(void)interpreter;
					(void)formatInfo;
					return true;
				}
		
				bool execute(Interpreter& interpreter, const String& instruction, const String& arguments) override
				{
					(void)interpreter;
					(void)instruction;
					(void)arguments;
					return true;
				}
		
				bool progress(Interpreter& interpreter, int maxStatementsLeft) override
				{
					(void)interpreter;
					(void)maxStatementsLeft;
					return true;
				}
		
				
				bool load(Interpreter& interpreter, const WideString& filename, String& contents) override
				{
					(void)interpreter;
					const std::string utf8(filename.begin(), filename.end());
					if (readFile(resolveRelativePath(utf8), contents)) {
						return true;
					}
					for (size_t i = 0; i < includeDirs.size(); ++i) {
						if (readFile(includeDirs[i] + "/" + utf8, contents)) {
							return true;
						}
					}
					return false;
				}
		
				void trace(Interpreter& interpreter, const WideString& s) override
				{
					(void)interpreter;
					(void)s;
				}
		
				bool meta(Interpreter& interpreter, const String& key, const String& arguments) override
				{
					static const String SNAPSHOT_KEY("snapshot-1");
					if (key != SNAPSHOT_KEY) {
						return false;
					}

					ArgumentsContainer args(ArgumentsContainer::parse(interpreter, StringRange(arguments)));

					SnapshotBlock block;
					block.validate = parseValidateFlag(interpreter, args.fetchOptional("validate"));
					const String* scenarioLabel = args.fetchOptional("scenario");
					if (scenarioLabel != 0) {
						block.scenario = *scenarioLabel;
					}

					const String* rawStatements = args.fetchOptional(0, false);
					if (rawStatements == 0) {
						Interpreter::throwBadSyntax("snapshot meta requires a statement list.");
					}

					block.statements = parseSnapshotStatements(interpreter, *rawStatements);
					block.sourceLine = locateMetaLine();

					args.throwIfAnyUnfetched();

					const uint32_t blockOrdinal = plan.addBlock(interpreter, block);

					executeCollectionInvocation(interpreter, blockOrdinal);

					return true;
				}

			private:

			void executeCollectionInvocation(Interpreter& interpreter, uint32_t blockOrdinal)
			{
				if (!plan.isCollectingPlan()) {
					return;
				}

				const SnapshotInvocation* invocation = plan.lookupInvocation(blockOrdinal, plan.getActiveScenarioIndex(), plan.getActiveEntryOrdinal());
				if (invocation == 0) {
					return;
				}

				const StringRange trimmed = trimRange(StringRange(invocation->statements));
				if (trimmed.b == trimmed.e) {
					return;
				}

				interpreter.run(StringRange(invocation->statements));
			}

				std::string resolveRelativePath(const std::string& requested) const
				{
					const size_t slash = sourcePath.find_last_of("/\\");
					if (slash == std::string::npos) {
						return requested;
					}
					return sourcePath.substr(0, slash + 1) + requested;
				}
		
				uint32_t locateMetaLine()
				{
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
		
				SnapshotPlan& plan;
				std::string sourcePath;
				String sourceText;
				std::vector<std::string> includeDirs;
				size_t scanOffset;
			};
		
				class SnapshotPlaybackExecutor : public IVG::IVGExecutor {
						public:
						SnapshotPlaybackExecutor(IVG::Canvas& canvas,
						const SnapshotScenario& scenario,
						const SnapshotEntry& entry,
						const CommandLineOptions& options,
						const std::string& sourcePath,
						SharedResources& sharedResources)
						: IVG::IVGExecutor(canvas)
						, scenario(scenario)
						, entry(entry)
						, includeDirs(options.includeDirs)
						, fontDirs(options.fontDirs)
						, imageDirs(options.imageDirs)
						, sourcePath(sourcePath)
						, verbose(options.verbose)
						, sharedResources(sharedResources)
						, nextBlockOrdinal(0)
						, invocationCursor(0)
						{
						}
		
				bool load(Interpreter& interpreter, const WideString& filename, String& contents) override
				{
					(void)interpreter;
					const std::string utf8(filename.begin(), filename.end());
					if (readFile(resolveRelativePath(utf8), contents)) {
						return true;
					}
					for (size_t i = 0; i < includeDirs.size(); ++i) {
						if (readFile(includeDirs[i] + "/" + utf8, contents)) {
							return true;
						}
					}
					return false;
				}
		
								std::vector<const IVG::Font*> lookupFonts(Interpreter& interpreter, const WideString& fontName, const UniString& forString) override
								{
										(void)interpreter;
										(void)forString;

										{
												NuXThreads::MutexLock lock(sharedResources.fontMutex);
												const std::map<WideString, IVG::Font>::iterator cached = sharedResources.fonts.find(fontName);
												if (cached != sharedResources.fonts.end()) {
														return std::vector<const IVG::Font*>(1, &cached->second);
												}
										}

										IVG::Font font;
										if (!loadExternalFont(fontName, font)) {
												return std::vector<const IVG::Font*>();
										}

										NuXThreads::MutexLock lock(sharedResources.fontMutex);
										const std::map<WideString, IVG::Font>::iterator cached = sharedResources.fonts.find(fontName);
										if (cached != sharedResources.fonts.end()) {
												return std::vector<const IVG::Font*>(1, &cached->second);
										}

										const std::map<WideString, IVG::Font>::iterator inserted = sharedResources.fonts.insert(std::make_pair(fontName, font)).first;
										return std::vector<const IVG::Font*>(1, &inserted->second);
								}
		
				IVG::Image loadImage(Interpreter& interpreter, const WideString& imageSource, const NuXPixels::IntRect* sourceRectangle,
					bool forStretching, double forXSize, bool xSizeIsRelative, double forYSize, bool ySizeIsRelative) override
				{
					(void)interpreter;
					(void)sourceRectangle;
					(void)forStretching;
					(void)forXSize;
					(void)xSizeIsRelative;
					(void)forYSize;
					(void)ySizeIsRelative;
		
					const std::string requested(imageSource.begin(), imageSource.end());
					const CachedImage* cached = resolveImage(requested);
					if (cached == 0) {
						return IVG::Image();
					}
		
					IVG::Image image;
					image.raster = &cached->raster;
					image.xResolution = cached->xResolution;
					image.yResolution = cached->yResolution;
					return image;
				}
		
				bool meta(Interpreter& interpreter, const String& key, const String& arguments) override
				{
					static const String SNAPSHOT_KEY("snapshot-1");
					if (key != SNAPSHOT_KEY) {
						return false;
					}
		
					ArgumentsContainer args(ArgumentsContainer::parse(interpreter, StringRange(arguments)));
		
					const String* scenarioLabel = args.fetchOptional("scenario");
					const String* rawStatements = args.fetchOptional(0, false);
					if (rawStatements == 0) {
						Interpreter::throwBadSyntax("snapshot meta requires a statement list.");
					}
		
					const bool hasLabel = (scenarioLabel != 0);
					const bool blockTargetsScenario = (scenario.explicitScenario ? (hasLabel && *scenarioLabel == scenario.name) : !hasLabel);
		
					++nextBlockOrdinal;
		
					const SnapshotInvocation* invocation = 0;
					if (invocationCursor < entry.invocations.size()) {
						const SnapshotInvocation& candidate = entry.invocations[invocationCursor];
						if (candidate.blockIndex == nextBlockOrdinal) {
							invocation = &candidate;
							++invocationCursor;
						}
					}
		
					if (!blockTargetsScenario) {
						args.throwIfAnyUnfetched();
						if (invocation != 0) {
							Interpreter::throwBadSyntax("unexpected snapshot invocation for scenario.");
						}
						return true;
					}
		
					if (invocation == 0) {
						Interpreter::throwBadSyntax("missing snapshot invocation for scenario block.");
					}
		
					StringVector statements = parseSnapshotStatements(interpreter, *rawStatements);
					if (invocation->statementOrdinal == 0 || invocation->statementOrdinal > statements.size()) {
						Interpreter::throwBadSyntax("snapshot statement ordinal exceeds available entries.");
					}
		
					const String& statementBody = statements[invocation->statementOrdinal - 1];
					if (statementBody != invocation->statements) {
						Interpreter::throwBadSyntax("snapshot statements changed between collection and playback.");
					}
		
					args.throwIfAnyUnfetched();
		
					if (verbose) {
						std::cout << sourcePath << ": scenario " << entry.scenarioName
							<< " entry " << entry.entryOrdinal << " block " << invocation->blockIndex
							<< " (statement " << invocation->statementOrdinal << ")" << std::endl;
					}
		
					IVG::Context invocationContext(currentContext->accessCanvas(), *currentContext);
					runInNewContext(interpreter, invocationContext, statementBody);
					return true;
				}
		
				bool finished() const
				{
					return (invocationCursor == entry.invocations.size());
				}
		
				private:
				std::string resolveRelativePath(const std::string& requested) const
				{
					const size_t slash = sourcePath.find_last_of("/\\");
					if (slash == std::string::npos) {
						return requested;
					}
					return sourcePath.substr(0, slash + 1) + requested;
				}
		
				const SnapshotScenario& scenario;
				const SnapshotEntry& entry;
				const std::vector<std::string>& includeDirs;
				const std::vector<std::string>& fontDirs;
				const std::vector<std::string>& imageDirs;
				std::string sourcePath;
				bool verbose;
				SharedResources& sharedResources;
				uint32_t nextBlockOrdinal;
				size_t invocationCursor;
		
				bool loadExternalFont(const WideString& fontName, IVG::Font& font)
				{
					const std::string fontName8(fontName.begin(), fontName.end());
					const std::string fileName = fontName8 + ".ivgfont";
		
					String contents;
					if (readFile(resolveRelativePath(fileName), contents) || loadFromDirectories(fontDirs, fileName, contents)) {
						return parseFont(contents, font);
					}
					return false;
				}
		
				bool loadFromDirectories(const std::vector<std::string>& dirs, const std::string& name, String& contents) const
				{
					for (size_t i = 0; i < dirs.size(); ++i) {
						if (readFile(dirs[i] + "/" + name, contents)) {
							return true;
						}
					}
					return false;
				}
		
				bool parseFont(const String& source, IVG::Font& font)
				{
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
		
				const CachedImage* resolveImage(const std::string& requested)
				{
					const std::string local = resolveRelativePath(requested);
					const CachedImage* cached = loadImageFromPath(local);
					if (cached != 0) {
						return cached;
					}
		
					for (size_t i = 0; i < imageDirs.size(); ++i) {
						cached = loadImageFromPath(imageDirs[i] + "/" + requested);
						if (cached != 0) {
							return cached;
						}
					}
					return 0;
				}
		
								const CachedImage* loadImageFromPath(const std::string& path)
								{
										{
												NuXThreads::MutexLock lock(sharedResources.imageMutex);
												const std::map<std::string, CachedImage>::iterator it = sharedResources.images.find(path);
												if (it != sharedResources.images.end()) {
														return &it->second;
												}
										}

										CachedImage cached;
										if (!loadPngRaster(path, cached.raster)) {
												return 0;
										}
										cached.xResolution = 1.0;
										cached.yResolution = 1.0;

										NuXThreads::MutexLock lock(sharedResources.imageMutex);
										const std::map<std::string, CachedImage>::iterator existing = sharedResources.images.find(path);
										if (existing != sharedResources.images.end()) {
												return &existing->second;
										}

										const std::map<std::string, CachedImage>::iterator inserted = sharedResources.images.insert(std::make_pair(path, cached)).first;
										return &inserted->second;
								}
			};
		
		
				static void printUsage(const char* program)
				{
				std::cout << "Usage: " << program << " [options] <ivg> [<ivg> ...]" << std::endl;
				std::cout << "Options:" << std::endl;
				std::cout << "\t--include-dir <path>\tAdd include search path." << std::endl;
				std::cout << "\t--font-dir <path>\tAdd font search path." << std::endl;
				std::cout << "\t--image-dir <path>\tAdd image search path." << std::endl;
				std::cout << "\t--output-dir <path>\tOverride output directory." << std::endl;
std::cout << "\t--force-update\t\tOverwrite goldens." << std::endl;
				std::cout << "\t--threads <n>\t\tNumber of worker threads." << std::endl;
				std::cout << "\t--list-only\t\tList collected snapshots without rendering." << std::endl;
				std::cout << "\t--verbose\t\tPrint verbose diagnostics." << std::endl;
				std::cout << "\t--exit-on-first-failure\tAbort after first failure." << std::endl;
				std::cout << "\t--help\t\t\tShow this message." << std::endl;
			}
		
			static bool parseUnsigned(const std::string& text, uint32_t& value)
			{
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
		
			static bool parseCommandLine(int argc, char** argv, CommandLineOptions& options)
			{
				for (int i = 1; i < argc; ++i) {
					const std::string arg(argv[i]);
					if (arg == "--help") {
						printUsage(argv[0]);
						return false;
					} else if (arg == "--include-dir") {
						if (i + 1 >= argc) {
							std::cerr << "--include-dir requires a path." << std::endl;
							return false;
						}
						options.includeDirs.push_back(argv[++i]);
					} else if (arg == "--font-dir") {
						if (i + 1 >= argc) {
							std::cerr << "--font-dir requires a path." << std::endl;
							return false;
						}
						options.fontDirs.push_back(argv[++i]);
					} else if (arg == "--image-dir") {
						if (i + 1 >= argc) {
							std::cerr << "--image-dir requires a path." << std::endl;
							return false;
						}
						options.imageDirs.push_back(argv[++i]);
					} else if (arg == "--output-dir") {
						if (i + 1 >= argc) {
							std::cerr << "--output-dir requires a path." << std::endl;
							return false;
						}
						options.outputDir = argv[++i];
					} else if (arg == "--force-update") {
						options.forceUpdate = true;
					} else if (arg == "--threads") {
						if (i + 1 >= argc) {
							std::cerr << "--threads requires a numeric value." << std::endl;
							return false;
						}
						uint32_t threads = 0;
						if (!parseUnsigned(argv[i + 1], threads)) {
							std::cerr << "invalid thread count: " << argv[i + 1] << std::endl;
							return false;
						}
						options.threads = threads;
						++i;
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
					std::cerr << "no IVG files specified." << std::endl;
					return false;
				}
				return true;
			}
		
			static bool readFile(const std::string& path, String& contents)
			{
				std::ifstream stream(path.c_str(), std::ios::binary);
				if (!stream.good()) {
					return false;
				}
				std::string buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
				contents.assign(buffer.begin(), buffer.end());
				return true;
			}
		
		static void printPlan(const std::string& path, const SnapshotPlan& plan)
			{
				std::cout << path << std::endl;
				const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
				const std::vector<SnapshotEntry>& entries = plan.getEntries();
		
				for (size_t i = 0; i < scenarios.size(); ++i) {
					const SnapshotScenario& scenario = scenarios[i];
					std::cout << "	Scenario " << scenario.name << " (validate: " << (scenario.validate ? "yes" : "no") << ")" << std::endl;
					for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
						const SnapshotEntry& entry = entries[scenario.entryIndices[j]];
						std::cout << "		Entry " << entry.entryOrdinal << std::endl;
						for (size_t k = 0; k < entry.invocations.size(); ++k) {
							const SnapshotInvocation& invocation = entry.invocations[k];
							std::cout << "			Block " << invocation.blockIndex << " (statement " << invocation.statementOrdinal
							<< "), line " << invocation.sourceLine << std::endl;
		
							std::istringstream snippet(invocation.statements);
							std::string line;
							while (std::getline(snippet, line)) {
								std::cout << "				[ " << line << " ]" << std::endl;
							}
						}
					}
				}
			}
		struct SnapshotJob {
				SnapshotJob()
				: options(0)
				, ivgPath(0)
				, baseName(0)
				, document(0)
				, sharedResources(0)
				, scenario(0)
				, entry(0)
				, planOrdinal(0)
				, sentinel(false)
				{
				}

				const CommandLineOptions* options;
				const std::string* ivgPath;
				const std::string* baseName;
				const CachedDocument* document;
				SharedResources* sharedResources;
				const SnapshotScenario* scenario;
				const SnapshotEntry* entry;
				uint32_t planOrdinal;
				bool sentinel;
		};

		class SnapshotScheduler {
		public:
				SnapshotScheduler(uint32_t threadCount, bool exitOnFirstFailure)
				: threadCount(threadCount == 0 ? 1 : threadCount)
				, exitOnFirstFailure(exitOnFirstFailure)
				, started(false)
				, finalizing(false)
				, stopScheduling(false)
				, activeWorkers(0)
				{
				}

				~SnapshotScheduler()
				{
						finalize();
				}

				void start()
				{
						if (started) {
								return;
						}

						workers.reserve(threadCount);
						threads.reserve(threadCount);
						for (uint32_t i = 0; i < threadCount; ++i) {
								workers.push_back(std::unique_ptr<Worker>(new Worker(*this)));
								threads.push_back(std::unique_ptr<NuXThreads::Thread>(new NuXThreads::Thread(*workers.back())));
								threads.back()->start();
						}
						started = true;
				}

				bool enqueue(const SnapshotJob& job)
				{
						NuXThreads::MutexLock lock(mutex);
						if (!started || finalizing || (exitOnFirstFailure && stopScheduling)) {
								return false;
						}

						pendingJobs.push_back(job);
						jobAvailable.signal();
						return true;
				}

				bool fetchResult(SnapshotEntryResult& out, bool wait)
				{
						while (true) {
								{
										NuXThreads::MutexLock lock(mutex);
										if (!completedResults.empty()) {
out = completedResults.front();
												completedResults.pop_front();
												return true;
										}
										if (!wait) {
												return false;
										}
										if (finalizing && pendingJobs.empty() && activeWorkers == 0) {
												return false;
										}
								}
								resultAvailable.wait();
						}
				}

				void finalize()
				{
						if (!started) {
								return;
						}

						uint32_t sentinelCount = 0;
						{
								NuXThreads::MutexLock lock(mutex);
								if (!finalizing) {
										finalizing = true;
										stopScheduling = true;
										sentinelCount = threadCount;
										for (uint32_t i = 0; i < sentinelCount; ++i) {
												SnapshotJob sentinelJob;
												sentinelJob.sentinel = true;
												pendingJobs.push_back(sentinelJob);
										}
								}
						}

						for (uint32_t i = 0; i < sentinelCount; ++i) {
								jobAvailable.signal();
						}

						for (size_t i = 0; i < threads.size(); ++i) {
								threads[i]->join();
						}
						workers.clear();
						threads.clear();
						started = false;
				}

				bool shouldStopScheduling()
				{
						NuXThreads::MutexLock lock(mutex);
						return stopScheduling;
				}

		private:
				class Worker : public NuXThreads::Runnable {
				public:
						explicit Worker(SnapshotScheduler& scheduler)
						: scheduler(scheduler)
						{
						}

						void run() override
						{
								scheduler.workerLoop();
						}

				private:
						SnapshotScheduler& scheduler;
				};

				void workerLoop()
				{
						while (true) {
								SnapshotJob job;
								if (!takeJob(job)) {
										return;
								}
								if (job.sentinel) {
										completeSentinel();
										return;
								}

								SnapshotEntryResult result = renderEntry(*job.options, *job.ivgPath, *job.baseName, *job.document, *job.sharedResources, *job.scenario, *job.entry);
								result.planOrdinal = job.planOrdinal;
								submitResult(result);
						}
				}

				bool takeJob(SnapshotJob& job)
				{
						while (true) {
								{
										NuXThreads::MutexLock lock(mutex);
										if (!pendingJobs.empty()) {
												job = pendingJobs.front();
												pendingJobs.pop_front();
												++activeWorkers;
												return true;
										}
										if (finalizing) {
												return false;
										}
								}
								jobAvailable.wait();
						}
				}

				void submitResult(SnapshotEntryResult& result)
				{
						const bool success = result.success;
						{
								NuXThreads::MutexLock lock(mutex);
completedResults.push_back(result);
								if (!success && exitOnFirstFailure) {
										stopScheduling = true;
								}
								if (activeWorkers > 0) {
										--activeWorkers;
								}
						}
						resultAvailable.signal();
				}

				void completeSentinel()
				{
						{
								NuXThreads::MutexLock lock(mutex);
								if (activeWorkers > 0) {
										--activeWorkers;
								}
						}
						resultAvailable.signal();
				}

				uint32_t threadCount;
				bool exitOnFirstFailure;
				bool started;
				bool finalizing;
				bool stopScheduling;
				NuXThreads::Mutex mutex;
				NuXThreads::Event jobAvailable;
				NuXThreads::Event resultAvailable;
				std::deque<SnapshotJob> pendingJobs;
				std::deque<SnapshotEntryResult> completedResults;
				std::vector<std::unique_ptr<Worker> > workers;
				std::vector<std::unique_ptr<NuXThreads::Thread> > threads;
				uint32_t activeWorkers;
		};
static SnapshotEntryResult renderEntry(const CommandLineOptions& options,
const std::string& path,
const std::string& baseName,
const CachedDocument& document,
SharedResources& sharedResources,
const SnapshotScenario& scenario,
const SnapshotEntry& entry)
{
SnapshotEntryResult result;
result.ivgPath = path;
result.scenarioName = stringFromIMPD(entry.scenarioName);
result.entryOrdinal = entry.entryOrdinal;
result.validate = entry.validate;
result.blockIndex = (entry.invocations.empty() ? 0 : entry.invocations[0].blockIndex);
result.identifier = buildEntryIdentifier(baseName, entry);

IVG::SelfContainedARGB32Canvas canvas;
SnapshotPlaybackExecutor executor(canvas, scenario, entry, options, path, sharedResources);
try {
document.render(executor);
result.rendered = true;
} catch (Exception& e) {
std::ostringstream message;
message << e.getError();
if (e.hasStatement()) {
message << " near \"" << e.getStatement() << "\"";
}
result.message = message.str();
std::cerr << path << ": scenario " << result.scenarioName << ": " << result.message << std::endl;
return result;
} catch (std::exception& e) {
result.message = e.what();
std::cerr << path << ": scenario " << result.scenarioName << ": " << result.message << std::endl;
return result;
}

if (!executor.finished()) {
result.message = "did not execute all snapshot invocations";
std::cerr << path << ": scenario " << result.scenarioName << " did not execute all snapshot invocations." << std::endl;
return result;
}

NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>* raster = canvas.accessRaster();
if (raster == 0) {
result.message = "rendered image is empty";
std::cerr << path << ": scenario " << result.scenarioName << " produced no raster output." << std::endl;
return result;
}

SnapshotGolden golden(path, baseName, scenario, entry, options);
if (!entry.validate) {
if (!golden.writeDraft(*raster, result)) {
if (result.message.empty()) {
result.message = "failed to write draft";
}
std::cerr << path << ": scenario " << result.scenarioName << ": " << result.message << std::endl;
}
return result;
}

if (!golden.validate(*raster, options.forceUpdate, result)) {
if (result.message.empty()) {
result.message = "validation failed";
}
std::cerr << path << ": scenario " << result.scenarioName << ": " << result.message << std::endl;
return result;
}

return result;
}



static void flushSchedulerResults(
	SnapshotScheduler& scheduler,
	bool wait,
	std::vector<SnapshotEntryResult>& ordered,
	std::vector<bool>& ready,
	size_t& nextLogIndex,
	SnapshotRunResult& run)
{
	SnapshotEntryResult fetched;
	while (scheduler.fetchResult(fetched, wait)) {
		const uint32_t ordinal = fetched.planOrdinal;
		if (ordinal >= ordered.size()) {
			continue;
		}

		ordered[ordinal] = fetched;
		ready[ordinal] = true;

		while (nextLogIndex < ordered.size() && ready[nextLogIndex]) {
			SnapshotEntryResult& recorded = ordered[nextLogIndex];
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
			logEntryResult(recorded);
			++nextLogIndex;
		}
	}
}
static SnapshotRunResult renderPlan(const CommandLineOptions& options,
const std::string& path,
const CachedDocument& document,
const SnapshotPlan& plan)
{
SnapshotRunResult run;
const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
const std::vector<SnapshotEntry>& entries = plan.getEntries();
const std::string baseName = stringFromIMPD(plan.getBaseName());
SharedResources sharedResources;

		uint32_t threadCount = options.threads;
		if (threadCount == 0) {
			const unsigned int hardware = std::thread::hardware_concurrency();
			threadCount = (hardware > 0 ? hardware : 1);
		}

		SnapshotScheduler scheduler(threadCount, options.exitOnFirstFailure);
		scheduler.start();

		std::vector<SnapshotEntryResult> ordered;
		std::vector<bool> ready;
		size_t nextLogIndex = 0;
		bool schedulingStopped = false;

		for (size_t i = 0; i < scenarios.size() && !schedulingStopped; ++i) {
			const SnapshotScenario& scenario = scenarios[i];
			if (options.verbose) {
				std::cout << path << ": scenario " << scenario.name << " (validate: " << (scenario.validate ? "yes" : "no") << ")" << std::endl;
			}

			for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
				if (options.exitOnFirstFailure && scheduler.shouldStopScheduling()) {
					schedulingStopped = true;
					break;
				}

				const SnapshotEntry& entry = entries[scenario.entryIndices[j]];
				if (options.verbose) {
					std::cout << path << ":	  entry " << entry.entryOrdinal << " name:" << entry.scenarioName
						<< " (validate: " << (entry.validate ? "yes" : "no") << ")" << std::endl;
				}

				SnapshotJob job;
				job.options = &options;
				job.ivgPath = &path;
				job.baseName = &baseName;
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
				flushSchedulerResults(scheduler, false, ordered, ready, nextLogIndex, run);
			}

			flushSchedulerResults(scheduler, false, ordered, ready, nextLogIndex, run);
		}

		flushSchedulerResults(scheduler, false, ordered, ready, nextLogIndex, run);
		scheduler.finalize();
		flushSchedulerResults(scheduler, true, ordered, ready, nextLogIndex, run);

		if (run.failedEntries > 0) {
				run.exitCode = 1;
		}
		return run;
}
static SnapshotRunResult processFile(const CommandLineOptions& options, const std::string& path)
{
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
const String& source = document.getSource();

plan.beginCollection();
while (true) {
	SnapshotCollector collector(plan, path, source, options.includeDirs);
	STLMapVariables variables;
	FormatInfo formatInfo;
	Interpreter interpreter(collector, variables, formatInfo);

	try {
		interpreter.run(StringRange(source));
	} catch (Exception& e) {
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
	} catch (std::exception& e) {
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

if (options.listOnly) {
const std::vector<SnapshotEntry>& entries = plan.getEntries();
for (size_t i = 0; i < entries.size(); ++i) {
++run.totalEntries;
if (entries[i].validate) {
++run.validatedEntries;
} else {
++run.draftEntries;
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

int main(int argc, char** argv)
{
CommandLineOptions options;
if (!parseCommandLine(argc, argv, options)) {
return 1;
}

SnapshotTotals totals;
int exitCode = 0;
for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
const std::string& path = options.ivgPaths[i];
SnapshotRunResult run = processFile(options, path);
totals.accumulate(run);
logFileSummary(path, run);
if (run.exitCode != 0 || run.fileFailed) {
if (exitCode == 0) {
exitCode = (run.exitCode != 0 ? run.exitCode : 1);
}
if (options.exitOnFirstFailure) {
break;
}
}
}
logRunSummary(totals);
return exitCode;
}

#endif // !defined(IVG_SNAPSHOT_TESTING)

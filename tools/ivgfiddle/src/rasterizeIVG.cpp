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

#include <emscripten.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
#endif
#include <iostream>
#include <algorithm>
#include <fstream>
#include <string>
#include <istream>
#include <ostream>
#include <iterator>
#include <sstream>
#include <cstdint>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <png.h>
#include <zlib.h>
#include "../src/IVG.h"

using namespace std;
using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

namespace {
bool resolveIncludeAssetPath(const std::string& requested, std::string& resolvedPath);
bool decodePngIntoRaster(const std::string& path, SelfContainedRaster<ARGB32>& target, std::string& errorMessage);
bool normalizeIncludeRelativePath(const std::string& candidate, std::string& output, std::string& errorReason);
void logIncludeResolutionFailure(const std::string& includePath, const std::string& reason);
}

class IVGExecutorWithExternalFonts : public IVGExecutor {
	public:
		IVGExecutorWithExternalFonts(Canvas& canvas, const AffineTransformation& xform)
			: IVGExecutor(canvas, xform) {
		}
		virtual std::vector<const Font*> lookupFonts(Interpreter& interpreter, const WideString& fontName
				, const UniString& forString) {
			(void)interpreter;
			(void)forString;
                        std::pair< FontMap::iterator, bool > insertResult = loadedFonts.insert(std::make_pair(fontName, Font()));
                        if (insertResult.second) {
                                const std::string fontName8Bit(fontName.begin(), fontName.end());
                                String fontCode;
                                {
                                        const std::string requested = fontName8Bit + ".ivgfont";
                                        std::string resolved;
                                        if (!resolveIncludeAssetPath(requested, resolved)) {
                                                logIncludeResolutionFailure(requested, "font asset not found");
                                                return std::vector<const Font*>();
                                        }
                                        std::ifstream fileStream(resolved.c_str());
                                        if (!fileStream.good()) {
                                                logIncludeResolutionFailure(requested, "unable to open font asset");
                                                return std::vector<const Font*>();
                                        }
                                        fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
                                        const std::istreambuf_iterator<Char> it(fileStream);
                                        const std::istreambuf_iterator<Char> end;
                                        fontCode = std::string(it, end);
                                }
                                FontParser fontParser;
                                STLMapVariables vars;
                                FormatInfo formatInfo;
                                Interpreter impd(fontParser, vars, formatInfo);
                                impd.run(fontCode);
                                insertResult.first->second = fontParser.finalizeFont();
                        }
                        return std::vector<const Font*>(1, &insertResult.first->second);
                }

                virtual Image loadImage(Interpreter& interpreter, const WideString& imageSource
                                , const IntRect* sourceRectangle, bool forStretching, double forXSize, bool xSizeIsRelative
                                , double forYSize, bool ySizeIsRelative) {
                        (void)interpreter;
                        (void)sourceRectangle;
                        (void)forStretching;
                        (void)forXSize;
                        (void)xSizeIsRelative;
                        (void)forYSize;
                        (void)ySizeIsRelative;

                        const std::string imageName(imageSource.begin(), imageSource.end());
                        std::string resolved;
                        if (!resolveIncludeAssetPath(imageName, resolved)) {
                                logIncludeResolutionFailure(imageName, "image asset not found");
                                return Image();
                        }

                        SelfContainedRaster<ARGB32>& raster = loadedImages[resolved];
                        std::string decodeError;
                        if (!decodePngIntoRaster(resolved, raster, decodeError)) {
                                if (!decodeError.empty()) {
                                        logIncludeResolutionFailure(imageName, decodeError);
                                } else {
                                        logIncludeResolutionFailure(imageName, "failed to decode image asset");
                                }
                                loadedImages.erase(resolved);
                                return Image();
                        }

                        Image img;
                        img.raster = &raster;
                        img.xResolution = 1.0;
                        img.yResolution = 1.0;
                        return img;
                }

        protected:
                FontMap loadedFonts;
                std::map<std::string, SelfContainedRaster<ARGB32> > loadedImages;
};

namespace {
const int MAX_RASTER_DIMENSION = 16384;
const long long MAX_RASTER_PIXELS = 67108864LL;
const size_t VECTOR_HEAP_RESERVE_BYTES = 2 * 1024 * 1024;
static const char* const SNAPSHOT_SOURCE_PATH = "/ivgfiddle/source.ivg";
static const char* const INCLUDE_ARCHIVE_ROOT = "/__ivg/includes";

size_t computeFreeHeapBytes();

class SnapshotPlan;

static const String SNAPSHOT_META_KEY("snapshot-1");

static std::vector<std::string> buildCollectorIncludeDirectories()
{
std::vector<std::string> includeDirs;
includeDirs.push_back(".");
includeDirs.push_back(INCLUDE_ARCHIVE_ROOT);
includeDirs.push_back("/");
return includeDirs;
}

static void appendUniqueCandidate(std::vector<std::string>& candidates, const std::string& candidate)
{
if (candidate.empty()) {
return;
}
for (size_t index = 0; index < candidates.size(); ++index) {
if (candidates[index] == candidate) {
return;
}
}
candidates.push_back(candidate);
}

static bool resolveIncludeAssetPath(const std::string& requested, std::string& resolvedPath)
{
resolvedPath.clear();
if (requested.empty()) {
return false;
}

std::vector<std::string> candidates;
appendUniqueCandidate(candidates, requested);

std::string normalized;
std::string normalizationError;
const bool hasNormalized = normalizeIncludeRelativePath(requested, normalized, normalizationError);

if (hasNormalized) {
appendUniqueCandidate(candidates, std::string(INCLUDE_ARCHIVE_ROOT) + "/" + normalized);
appendUniqueCandidate(candidates, normalized);
const std::vector<std::string> includeDirs = buildCollectorIncludeDirectories();
for (size_t index = 0; index < includeDirs.size(); ++index) {
const std::string& dir = includeDirs[index];
if (dir.empty() || dir == ".") {
appendUniqueCandidate(candidates, normalized);
continue;
}
if (dir == "/") {
appendUniqueCandidate(candidates, std::string("/") + normalized);
continue;
}
if (!dir.empty() && dir[dir.size() - 1] == '/') {
appendUniqueCandidate(candidates, dir + normalized);
} else {
appendUniqueCandidate(candidates, dir + "/" + normalized);
}
}
}

for (size_t index = 0; index < candidates.size(); ++index) {
const std::string& candidate = candidates[index];
std::ifstream stream(candidate.c_str(), std::ios::binary);
if (stream.good()) {
resolvedPath = candidate;
return true;
}
}

return false;
}

static bool isLittleEndian()
{
static const unsigned char bytes[4] = { 0x4A, 0x3B, 0x2C, 0x1D };
return (*reinterpret_cast<const unsigned int*>(bytes) == 0x1D2C3B4A);
}

static void PNGAPI ivgPngError(png_structp png_ptr, png_const_charp error_msg)
{
(void)png_ptr;
const char* message = (error_msg ? error_msg : "Unknown PNG error");
throw std::runtime_error(message);
}

static bool decodePngIntoRaster(const std::string& path, SelfContainedRaster<ARGB32>& target, std::string& errorMessage)
{
errorMessage.clear();
FILE* file = fopen(path.c_str(), "rb");
if (file == 0) {
errorMessage = "unable to open image asset";
return false;
}

png_structp png_ptr = 0;
png_infop info_ptr = 0;
bool success = false;
try {
png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, ivgPngError, 0);
if (png_ptr == 0) {
throw std::runtime_error("could not initialize PNG reader");
}
info_ptr = png_create_info_struct(png_ptr);
if (info_ptr == 0) {
throw std::runtime_error("could not initialize PNG info");
}
png_init_io(png_ptr, file);
png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
if (isLittleEndian()) {
png_set_bgr(png_ptr);
} else {
png_set_swap_alpha(png_ptr);
}
png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
png_bytep* rows = png_get_rows(png_ptr, info_ptr);
target = SelfContainedRaster<ARGB32>(IntRect(0, 0, static_cast<int>(width), static_cast<int>(height)));
for (png_uint_32 y = 0; y < height; ++y) {
ARGB32::Pixel* dest = target.getPixelPointer() + y * target.getStride();
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
success = true;
} catch (const std::exception& ex) {
errorMessage = ex.what();
} catch (...) {
errorMessage = "unknown PNG decoding error";
}

if (png_ptr != 0 || info_ptr != 0) {
png_destroy_read_struct(&png_ptr, (info_ptr != 0 ? &info_ptr : 0), 0);
}
fclose(file);
return success;
}

static bool normalizeIncludeRelativePath(const std::string& candidate, std::string& output, std::string& errorReason)
{
output.clear();
errorReason.clear();
std::string sanitized(candidate);
std::replace(sanitized.begin(), sanitized.end(), '\\', '/');
std::vector<std::string> segments;
size_t cursor = 0;
while (cursor <= sanitized.size()) {
size_t slash = sanitized.find('/', cursor);
if (slash == std::string::npos) {
slash = sanitized.size();
}
std::string part = sanitized.substr(cursor, slash - cursor);
cursor = slash + 1;
if (part.empty() || part == ".") {
continue;
}
if (part == "..") {
errorReason = "contains parent directory traversal";
return false;
}
segments.push_back(part);
}
if (segments.empty()) {
errorReason = "resolved to an empty path";
return false;
}
output.clear();
for (size_t index = 0; index < segments.size(); ++index) {
if (index > 0) {
output.append("/");
}
output.append(segments[index]);
}
return true;
}

void logIncludeResolutionFailure(const std::string& includePath, const std::string& reason)
{
std::cout << "[IVGFiddle] Include missing: " << includePath;
if (!reason.empty()) {
std::cout << " (" << reason << ")";
}
std::cout << std::endl;
}

class SnapshotPlanCache {
public: SnapshotPlanCache();

public: ~SnapshotPlanCache();

public: SnapshotPlan& ensure(const std::string& sourceTextUtf8, const String& sourceText, const std::vector<std::string>& includeDirs);

private: void rebuild(const std::string& sourceTextUtf8, const String& sourceText, const std::vector<std::string>& includeDirs);

private: SnapshotPlan* plan;
private: std::string cachedSourceText;
private: std::vector<std::string> cachedIncludeDirs;
};

static SnapshotPlanCache snapshotPlanCache;

IntRect computeScaledBounds(const IntRect& bounds, double rescale)
{
	if (rescale == 1.0) {
		return bounds;
	}
	const double scaledLeft = static_cast<double>(bounds.left) * rescale;
	const double scaledTop = static_cast<double>(bounds.top) * rescale;
	const double scaledRight = static_cast<double>(bounds.left + bounds.width) * rescale;
	const double scaledBottom = static_cast<double>(bounds.top + bounds.height) * rescale;
	IntRect result;
	result.left = static_cast<int>(std::floor(scaledLeft));
	result.top = static_cast<int>(std::floor(scaledTop));
	result.width = static_cast<int>(std::ceil(scaledRight) - result.left);
	result.height = static_cast<int>(std::ceil(scaledBottom) - result.top);
	return result;
}

class GuardedSelfContainedARGB32Canvas : public SelfContainedARGB32Canvas {
	public:
		GuardedSelfContainedARGB32Canvas(double rescaleBounds, long long pixelBudget, size_t heapReserveBytes)
				: SelfContainedARGB32Canvas(rescaleBounds)
				, maxRasterPixels(pixelBudget)
				, heapReserve(heapReserveBytes) {
		}

		virtual void defineBounds(const IntRect& newBounds) {
			const IntRect scaledBounds = computeScaledBounds(newBounds, rescaleBounds);
			preflightBounds(scaledBounds);
			SelfContainedARGB32Canvas::defineBounds(newBounds);
		}

	private:
		void preflightBounds(const IntRect& scaledBounds) const {
			if (scaledBounds.width <= 0 || scaledBounds.height <= 0) {
				return;
			}
			const long long pixelCount = static_cast<long long>(scaledBounds.width) * static_cast<long long>(scaledBounds.height);
			if (scaledBounds.width > MAX_RASTER_DIMENSION || scaledBounds.height > MAX_RASTER_DIMENSION) {
				std::ostringstream message;
				message << "Rasterization aborted: scaled bounds " << scaledBounds.width << "x"
						<< scaledBounds.height << " exceed the " << MAX_RASTER_DIMENSION
						<< "px dimension cap.";
				throw runtime_error(message.str());
			}
			if (maxRasterPixels > 0 && pixelCount > maxRasterPixels) {
				std::ostringstream message;
				message << "Rasterization aborted: " << scaledBounds.width << "x" << scaledBounds.height
						<< " = " << pixelCount << " pixels exceeds the " << maxRasterPixels
						<< " pixel budget.";
				throw runtime_error(message.str());
			}
			const size_t requiredPixelBytes = static_cast<size_t>(scaledBounds.width) * static_cast<size_t>(scaledBounds.height) * 4u;
			const size_t requiredBytes = 4u * 4u + requiredPixelBytes;
#ifdef __EMSCRIPTEN__
			const size_t freeHeapBytes = computeFreeHeapBytes();
			if (freeHeapBytes > 0 && requiredBytes + heapReserve > freeHeapBytes) {
				std::ostringstream message;
				message << "Rasterization aborted: " << requiredBytes << " bytes required but only "
						<< freeHeapBytes << " bytes free in the WebAssembly heap.";
				throw runtime_error(message.str());
			}
#else
			(void)requiredBytes;
#endif
		}

	private: const long long maxRasterPixels;
	private: const size_t heapReserve;
};

size_t computeFreeHeapBytes()
{
#ifdef __EMSCRIPTEN__
	uintptr_t* sbrkPointer = emscripten_get_sbrk_ptr();
	if (sbrkPointer != 0) {
		const uintptr_t currentBrk = *sbrkPointer;
		const size_t heapBytes = emscripten_get_heap_size();
		if (heapBytes > currentBrk) {
			return heapBytes - static_cast<size_t>(currentBrk);
		}
	}
	return 0;
#else
	return 0;
#endif
}

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

static bool parseValidateFlag(Interpreter& interpreter, const String* value)
{
	if (value == 0) {
		return true;
	}
	return interpreter.toBool(*value);
}

struct ParsedSnapshotMeta {
	bool validate;
	bool hasScenario;
	String scenario;
	StringVector statements;
	String common;
};

static StringVector parseSnapshotStatements(Interpreter& interpreter, ArgumentsContainer& args)
{
	const String* listArg = args.fetchOptional("list", false);
	if (listArg != 0) {
		const String expandedOuter = interpreter.expand(StringRange(*listArg));
		StringVector elements;
		interpreter.parseList(StringRange(expandedOuter), elements, false, false, 1, INT_MAX);
		return elements;
	}

	StringVector statements;
	const String* singleStatement = args.fetchOptional(0, false);
	if (singleStatement != 0) {
		statements.push_back(*singleStatement);
	}
	return statements;
}

static ParsedSnapshotMeta parseSnapshotMetaArguments(Interpreter& interpreter, const String& arguments)
{
	ArgumentsContainer args(ArgumentsContainer::parse(interpreter, StringRange(arguments)));
	ParsedSnapshotMeta result;
	result.validate = parseValidateFlag(interpreter, args.fetchOptional("validate"));
	const String* scenarioLabel = args.fetchOptional("scenario");
	if (scenarioLabel != 0) {
		result.hasScenario = true;
		result.scenario = *scenarioLabel;
	} else {
		result.hasScenario = false;
		result.scenario.clear();
	}

	const String* commonBody = args.fetchOptional("common", false);
	if (commonBody != 0) {
		result.common = *commonBody;
	} else {
		result.common.clear();
	}
	result.statements = parseSnapshotStatements(interpreter, args);
	args.throwIfAnyUnfetched();
	return result;
}


struct SnapshotBlock {
	bool validate;
	String scenario;
	StringVector statements;
	String common;
	uint32_t sourceLine;
};

enum SnapshotInvocationKind {
	SnapshotInvocationKindScenario,
	SnapshotInvocationKindCommon
};

struct SnapshotInvocation {
	SnapshotInvocation()
		: kind(SnapshotInvocationKindScenario)
		, blockIndex(0)
		, sourceLine(0)
		, statementOrdinal(0)
	{
	}

	SnapshotInvocationKind kind;
	uint32_t blockIndex;
	uint32_t sourceLine;
	uint32_t statementOrdinal;
	String statements;
};

struct SnapshotEntry {
	SnapshotEntry()
			: scenarioIndex(0)
			, entryOrdinal(0)
			, validate(true)
			, listIndex(0)
			, nextInvocationCursor(0)
	{
	}

	uint32_t scenarioIndex;
	uint32_t entryOrdinal;
	bool validate;
	String scenarioName;
	uint32_t listIndex;
	std::vector<SnapshotInvocation> invocations;
	uint32_t nextInvocationCursor;
};

struct SnapshotScenario {
	String name;
	bool validate;
	bool explicitScenario;
	std::vector<uint32_t> entryIndices;
	std::map<uint32_t, uint32_t> entryLookup;
};

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
			, anyCommonBlocks(false)
			, commonOnlyBlocks(false)
		{
		}

		uint32_t addBlock(Interpreter& interpreter, const SnapshotBlock& block)
		{
			const bool hasCommon = !block.common.empty();
			if (hasCommon) {
				anyCommonBlocks = true;
				if (block.statements.empty()) {
					commonOnlyBlocks = true;
			}
			}
			if (block.statements.empty() && !hasCommon) {
Interpreter::throwBadSyntax("snapshot meta requires at least one statement block.");
			}

			uint32_t blockOrdinal = nextBlockOrdinal;
			if (collectionRunsBuilt) {
				if (recordedBlockCursor >= recordedBlockOrdinals.size()) {
					Interpreter::throwBadSyntax("snapshot replay encountered an unexpected block.");
				}
				blockOrdinal = recordedBlockOrdinals[recordedBlockCursor++];
				const SnapshotInvocation* recordedCommon = lookupCommonInvocation(blockOrdinal);
				if (hasCommon) {
					if (recordedCommon == 0 || recordedCommon->statements != block.common) {
						Interpreter::throwBadSyntax("snapshot common block changed between collection runs.");
					}
				} else if (recordedCommon != 0) {
					Interpreter::throwBadSyntax("snapshot common block changed between collection runs.");
				}
				return blockOrdinal;
			}

			recordedBlockOrdinals.push_back(blockOrdinal);
			if (hasCommon) {
				SnapshotInvocation commonInvocation;
				commonInvocation.kind = SnapshotInvocationKindCommon;
				commonInvocation.blockIndex = blockOrdinal;
				commonInvocation.sourceLine = block.sourceLine;
				commonInvocation.statementOrdinal = 0;
				commonInvocation.statements = block.common;
				commonInvocations.insert(std::make_pair(blockOrdinal, commonInvocation));
			}

			const bool hasExplicitScenario = !block.scenario.empty();
			const uint32_t statementCount = static_cast<uint32_t>(block.statements.size());
			if (hasExplicitScenario) {
				const uint32_t scenarioIndex = resolveScenario(interpreter, block.scenario, block.validate, true);
				SnapshotScenario& scenario = scenarios[scenarioIndex];
				if (!scenario.entryIndices.empty() && statementCount != scenario.entryIndices.size()) {
					Interpreter::throwBadSyntax("scenario entry count does not match previous blocks.");
				}

				for (uint32_t i = 0; i < statementCount; ++i) {
					const uint32_t entryOrdinal = i + 1;
					uint32_t entryIndex = 0;
					SnapshotEntry& entry = ensureEntry(scenarioIndex, scenario, entryOrdinal, block.validate, block.scenario, i, entryIndex);

					if (hasCommon) {
						SnapshotInvocation sharedInvocation;
						sharedInvocation.kind = SnapshotInvocationKindCommon;
						sharedInvocation.blockIndex = blockOrdinal;
						sharedInvocation.sourceLine = block.sourceLine;
						sharedInvocation.statementOrdinal = 0;
						sharedInvocation.statements = block.common;
						entry.invocations.push_back(sharedInvocation);
					}

					SnapshotInvocation invocation;
					invocation.kind = SnapshotInvocationKindScenario;
					invocation.blockIndex = blockOrdinal;
					invocation.sourceLine = block.sourceLine;
					invocation.statementOrdinal = entryOrdinal;
					invocation.statements = block.statements[i];
					entry.invocations.push_back(invocation);
				}
			} else {
				for (uint32_t i = 0; i < statementCount; ++i) {
					const uint32_t entryOrdinal = 1;
					const String scenarioName = synthesizeScenarioName(blockOrdinal, statementCount, i + 1);
					const uint32_t scenarioIndex = resolveScenario(interpreter, scenarioName, block.validate, false);
					SnapshotScenario& scenario = scenarios[scenarioIndex];
					uint32_t entryIndex = 0;
					SnapshotEntry& entry = ensureEntry(scenarioIndex, scenario, entryOrdinal, block.validate, scenarioName, i, entryIndex);

					if (hasCommon) {
						SnapshotInvocation sharedInvocation;
						sharedInvocation.kind = SnapshotInvocationKindCommon;
						sharedInvocation.blockIndex = blockOrdinal;
						sharedInvocation.sourceLine = block.sourceLine;
						sharedInvocation.statementOrdinal = 0;
						sharedInvocation.statements = block.common;
						entry.invocations.push_back(sharedInvocation);
					}

					SnapshotInvocation invocation;
					invocation.kind = SnapshotInvocationKindScenario;
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

		const std::vector<SnapshotScenario>& getScenarios() const
		{
			return scenarios;
		}

		const std::vector<SnapshotEntry>& getEntries() const
		{
			return entries;
		}

		bool hasCommonBlocks() const
		{
			return anyCommonBlocks;
		}

		bool hasCommonOnlyBlocks() const
		{
			return commonOnlyBlocks;
		}

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
			anyCommonBlocks = false;
			commonOnlyBlocks = false;
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

		const SnapshotInvocation* lookupInvocation(uint32_t blockOrdinal, uint32_t scenarioIndex, uint32_t entryOrdinal, SnapshotInvocationKind kind) const
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
				const SnapshotInvocation& invocation = entry.invocations[i];
				if (invocation.blockIndex == blockOrdinal && invocation.kind == kind) {
					return &entry.invocations[i];
				}
			}
			return 0;
		}

		const SnapshotInvocation* lookupCommonInvocation(uint32_t blockOrdinal) const
		{
			const std::map<uint32_t, SnapshotInvocation>::const_iterator it = commonInvocations.find(blockOrdinal);
			if (it == commonInvocations.end()) {
				return 0;
			}
			return &it->second;
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

		SnapshotEntry& ensureEntry(uint32_t scenarioIndex, SnapshotScenario& scenario, uint32_t entryOrdinal, bool validate, const String& scenarioName, uint32_t listIndex, uint32_t& entryIndex)
		{
			const std::map<uint32_t, uint32_t>::const_iterator existing = scenario.entryLookup.find(entryOrdinal);
			if (existing != scenario.entryLookup.end()) {
				entryIndex = existing->second;
				SnapshotEntry& entry = entries[entryIndex];
				if (entry.validate != validate) {
					Interpreter::throwBadSyntax("scenario switches between validate yes/no.");
				}
				return entry;
			}

			SnapshotEntry entry;
			entry.scenarioIndex = scenarioIndex;
			entry.entryOrdinal = entryOrdinal;
			entry.validate = validate;
			entry.scenarioName = scenarioName;
			entry.listIndex = listIndex;
			entry.nextInvocationCursor = 0;
			entries.push_back(entry);
			entryIndex = static_cast<uint32_t>(entries.size() - 1);
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
std::map<uint32_t, SnapshotInvocation> commonInvocations;
bool anyCommonBlocks;
bool commonOnlyBlocks;

		struct CollectionRun {
			uint32_t scenarioIndex;
			uint32_t entryOrdinal;
		};

		std::vector<CollectionRun> collectionRuns;
		size_t collectionRunCursor;
		bool collectionRunsBuilt;
};

static bool readFile(const std::string& path, String& contents);

enum SnapshotExecutorMode {
        SnapshotExecutorModeCollect,
        SnapshotExecutorModePlayback
};

class SnapshotExecutor : public IVGExecutorWithExternalFonts {
        public:
                SnapshotExecutor(GuardedSelfContainedARGB32Canvas& canvas, const AffineTransformation& xform, const std::vector<std::string>& includeDirs, const std::string& sourcePath, SnapshotPlan& plan, const String& sourceText)
                                : IVGExecutorWithExternalFonts(canvas, xform)
                                , mode(SnapshotExecutorModeCollect)
                                , plan(&plan)
                                , sourceText(&sourceText)
                                , scenario(0)
                                , entry(0)
                                , includeDirs(includeDirs)
                                , sourcePath(sourcePath)
                                , scanOffset(0)
                                , nextBlockOrdinal(0)
                                , invocationCursor(0)
                {
                }

                SnapshotExecutor(GuardedSelfContainedARGB32Canvas& canvas, const AffineTransformation& xform, const std::vector<std::string>& includeDirs, const std::string& sourcePath, SnapshotPlan& planRef, const SnapshotScenario& scenario, const SnapshotEntry& entry)
                                : IVGExecutorWithExternalFonts(canvas, xform)
                                , mode(SnapshotExecutorModePlayback)
                                , plan(&planRef)
                                , sourceText(0)
                                , scenario(&scenario)
                                , entry(&entry)
                                , includeDirs(includeDirs)
                                , sourcePath(sourcePath)
                                , scanOffset(0)
                                , nextBlockOrdinal(0)
                                , invocationCursor(0)
                {
                }

bool load(Interpreter& interpreter, const WideString& filename, String& contents)
{
const std::string utf8(filename.begin(), filename.end());
if (readFile(resolveRelativePath(utf8), contents)) {
return true;
}
std::string normalized;
std::string normalizationError;
const bool hasNormalized = normalizeIncludeRelativePath(utf8, normalized, normalizationError);
if (hasNormalized) {
const std::string archivePath = std::string(INCLUDE_ARCHIVE_ROOT) + "/" + normalized;
if (readFile(archivePath, contents)) {
return true;
}
}
for (size_t i = 0; i < includeDirs.size(); ++i) {
if (hasNormalized) {
if (readFile(includeDirs[i] + "/" + normalized, contents)) {
return true;
}
}
if (readFile(includeDirs[i] + "/" + utf8, contents)) {
return true;
}
}
if (mode == SnapshotExecutorModeCollect) {
if (IVGExecutorWithExternalFonts::load(interpreter, filename, contents)) {
return true;
}
}
if (hasNormalized) {
logIncludeResolutionFailure(utf8, "not found in synchronized bundle");
} else {
logIncludeResolutionFailure(utf8, normalizationError);
}
return false;
}

                bool meta(Interpreter& interpreter, const String& key, const String& arguments)
                {
                        if (key != SNAPSHOT_META_KEY) {
                                return IVGExecutorWithExternalFonts::meta(interpreter, key, arguments);
                        }

                        if (mode == SnapshotExecutorModeCollect) {
                                ParsedSnapshotMeta parsed = parseSnapshotMetaArguments(interpreter, arguments);

                                SnapshotBlock block;
                                block.validate = parsed.validate;
                                if (parsed.hasScenario) {
                                        block.scenario = parsed.scenario;
                                }
                                block.statements = parsed.statements;
                                block.common = parsed.common;
                                block.sourceLine = locateMetaLine();

                                const uint32_t blockOrdinal = plan->addBlock(interpreter, block);
                                executeCollectionInvocation(interpreter, blockOrdinal);
                                return true;
                        }

                        ArgumentsContainer args(ArgumentsContainer::parse(interpreter, StringRange(arguments)));
                        const String* validateFlag = args.fetchOptional("validate");
                        const bool blockValidate = parseValidateFlag(interpreter, validateFlag);
                        const String* scenarioLabel = args.fetchOptional("scenario");
                        const bool hasLabel = (scenarioLabel != 0);
                        const String* commonArg = args.fetchOptional("common", false);

                        const uint32_t blockOrdinal = ++nextBlockOrdinal;

                        const SnapshotInvocation* commonInvocation = 0;
                        const SnapshotInvocation* scenarioInvocation = 0;
                        if (entry != 0) {
                                while (invocationCursor < entry->invocations.size()) {
                                        const SnapshotInvocation& candidate = entry->invocations[invocationCursor];
                                        if (candidate.blockIndex != blockOrdinal) {
                                                break;
                                        }
                                        if (candidate.kind == SnapshotInvocationKindCommon) {
                                                commonInvocation = &candidate;
                                                ++invocationCursor;
                                                continue;
                                        }
                                        scenarioInvocation = &candidate;
                                        ++invocationCursor;
                                        break;
                                }
                        }

                        const SnapshotInvocation* planCommon = (plan != 0 ? plan->lookupCommonInvocation(blockOrdinal) : 0);
                        if (commonInvocation != 0 && planCommon != 0 && commonInvocation->statements != planCommon->statements) {
                                Interpreter::throwBadSyntax("snapshot common block changed between collection and playback.");
                        }
                        const SnapshotInvocation* effectiveCommon = (planCommon != 0 ? planCommon : commonInvocation);
                        if (effectiveCommon != 0) {
                                if (commonArg == 0) {
                                        Interpreter::throwBadSyntax("snapshot common block missing during playback.");
                                }
                                if (*commonArg != effectiveCommon->statements) {
                                        Interpreter::throwBadSyntax("snapshot common block changed between collection and playback.");
                                }
                                const StringRange trimmedCommon = trimRange(StringRange(effectiveCommon->statements));
                                if (trimmedCommon.b != trimmedCommon.e) {
                                        interpreter.run(trimmedCommon);
                                }
                        } else if (commonArg != 0) {
                                Interpreter::throwBadSyntax("snapshot common block changed between collection and playback.");
                        }

                        const bool blockTargetsScenario = (scenario->explicitScenario ? (hasLabel && *scenarioLabel == scenario->name) : (!hasLabel && scenarioInvocation != 0));
                        const String* listArg = args.fetchOptional("list", false);
                        StringVector statements;
                        if (listArg != 0) {
                                const String expandedOuter = interpreter.expand(StringRange(*listArg));
                                interpreter.parseList(StringRange(expandedOuter), statements, false, false, 1, INT_MAX);
                        } else {
                                const String* singleStatement = args.fetchOptional(0, false);
                                if (singleStatement != 0) {
                                        statements.push_back(*singleStatement);
                                }
                        }
                        if (!blockTargetsScenario) {
                                args.throwIfAnyUnfetched();
                                if (scenarioInvocation != 0) {
                                        Interpreter::throwBadSyntax("unexpected snapshot invocation for scenario.");
                                }
                                return true;
                        }

                        if (blockValidate != entry->validate) {
                                Interpreter::throwBadSyntax("snapshot validate flag changed between collection and playback.");
                        }

                        if (scenarioInvocation == 0) {
                                if (!statements.empty()) {
                                        Interpreter::throwBadSyntax("missing snapshot invocation for scenario block.");
                                }
                                args.throwIfAnyUnfetched();
                                return true;
                        }

                        if (scenarioInvocation->statementOrdinal == 0 || scenarioInvocation->statementOrdinal > statements.size()) {
                                Interpreter::throwBadSyntax("snapshot statement ordinal exceeds available entries.");
                        }

                        const String& statementBody = statements[scenarioInvocation->statementOrdinal - 1];
                        if (statementBody != scenarioInvocation->statements) {
                                Interpreter::throwBadSyntax("snapshot statements changed between collection and playback.");
                        }

                        args.throwIfAnyUnfetched();

                        const StringRange trimmed = trimRange(StringRange(statementBody));
                        if (trimmed.b != trimmed.e) {
                                interpreter.run(trimmed);
                        }
                        return true;
                }

                bool finished() const
                {
                        if (mode != SnapshotExecutorModePlayback || entry == 0) {
                                return true;
                        }
                        return invocationCursor >= entry->invocations.size();
                }

        private:
                void executeCollectionInvocation(Interpreter& interpreter, uint32_t blockOrdinal)
                {
                        if (mode != SnapshotExecutorModeCollect || plan == 0 || !plan->isCollectingPlan()) {
                                return;
                        }

                        const SnapshotInvocation* commonInvocation = plan->lookupCommonInvocation(blockOrdinal);
                        if (commonInvocation != 0) {
                                const StringRange trimmedCommon = trimRange(StringRange(commonInvocation->statements));
                                if (trimmedCommon.b != trimmedCommon.e) {
                                        interpreter.run(StringRange(commonInvocation->statements));
                                }
                        }

                        const SnapshotInvocation* invocation = plan->lookupInvocation(blockOrdinal, plan->getActiveScenarioIndex(), plan->getActiveEntryOrdinal(), SnapshotInvocationKindScenario);
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
                        if (sourceText == 0) {
                                return 0;
                        }

                        static const String TOKEN("meta snapshot");
                        const size_t position = sourceText->find(TOKEN, scanOffset);
                        if (position == String::npos) {
                                return 0;
                        }

                        scanOffset = position + TOKEN.size();
                        uint32_t line = 1;
                        for (size_t i = 0; i < position; ++i) {
                                if ((*sourceText)[i] == '\n') {
                                        ++line;
                                }
                        }
                        return line;
                }

                SnapshotExecutorMode mode;
                SnapshotPlan* plan;
                const String* sourceText;
                const SnapshotScenario* scenario;
                const SnapshotEntry* entry;
                std::vector<std::string> includeDirs;
                std::string sourcePath;
                size_t scanOffset;
                uint32_t nextBlockOrdinal;
                size_t invocationCursor;
};

SnapshotPlanCache::SnapshotPlanCache()
        : plan(0)
{
}

SnapshotPlanCache::~SnapshotPlanCache()
{
	delete plan;
}

SnapshotPlan& SnapshotPlanCache::ensure(const std::string& sourceTextUtf8, const String& sourceText, const std::vector<std::string>& includeDirs)
{
	if (plan == 0 || sourceTextUtf8 != cachedSourceText || includeDirs != cachedIncludeDirs) {
		rebuild(sourceTextUtf8, sourceText, includeDirs);
	}
	return *plan;
}

void SnapshotPlanCache::rebuild(const std::string& sourceTextUtf8, const String& sourceText, const std::vector<std::string>& includeDirs)
{
        std::auto_ptr<SnapshotPlan> newPlan(new SnapshotPlan("ivgfiddle"));
        newPlan->beginCollection();
        while (true) {
                GuardedSelfContainedARGB32Canvas canvas(1.0, MAX_RASTER_PIXELS, VECTOR_HEAP_RESERVE_BYTES);
                SnapshotExecutor collector(canvas, AffineTransformation(), includeDirs, SNAPSHOT_SOURCE_PATH, *newPlan, sourceText);
                STLMapVariables variables;
                FormatInfo formatInfo;
                Interpreter planInterpreter(collector, variables, formatInfo);
                planInterpreter.run(StringRange(sourceText));
                newPlan->completeCollectionPass();
		if (!newPlan->prepareNextCollectionPass()) {
			break;
		}
	}

	cachedSourceText = sourceTextUtf8;
	cachedIncludeDirs = includeDirs;
	delete plan;
	plan = newPlan.release();
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

static std::string stringFromIMPD(const String& value)
{
	return std::string(value.begin(), value.end());
}

static std::string escapeJsonString(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size() + 8);
	for (size_t i = 0; i < value.size(); ++i) {
		const unsigned char ch = static_cast<unsigned char>(value[i]);
		switch (ch) {
		case '\\':
			escaped += "\\\\";
			break;
		case '"':
			escaped += "\\\"";
			break;
		case '\b':
			escaped += "\\b";
			break;
		case '\f':
			escaped += "\\f";
			break;
		case '\n':
			escaped += "\\n";
			break;
		case '\r':
			escaped += "\\r";
			break;
		case '\t':
			escaped += "\\t";
			break;
		default:
			if (ch < 0x20u) {
				static const char HEX_DIGITS[] = "0123456789abcdef";
				escaped += "\\u00";
				escaped += HEX_DIGITS[(ch >> 4) & 0xF];
				escaped += HEX_DIGITS[ch & 0xF];
			} else {
				escaped += static_cast<char>(ch);
			}
			break;
		}
	}
	return escaped;
}

static std::string buildSnapshotCatalogJson(const SnapshotPlan& plan, uint32_t defaultScenarioIndex, uint32_t defaultEntryOrdinal)
{
	const uint32_t sentinel = std::numeric_limits<uint32_t>::max();
	const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
	const std::vector<SnapshotEntry>& entries = plan.getEntries();
	std::ostringstream json;
	json << '{';
	if (defaultScenarioIndex == sentinel || defaultEntryOrdinal == sentinel) {
		json << "\"defaultScenarioIndex\":null,\"defaultEntryOrdinal\":null";
	} else {
		json << "\"defaultScenarioIndex\":" << defaultScenarioIndex;
		json << ",\"defaultEntryOrdinal\":" << defaultEntryOrdinal;
	}
json << ",\"hasCommon\":" << (plan.hasCommonBlocks() ? "true" : "false");
json << ",\"hasCommonOnly\":" << (plan.hasCommonOnlyBlocks() ? "true" : "false");
json << ",\"scenarios\":[";
	bool firstScenario = true;
	for (size_t i = 0; i < scenarios.size(); ++i) {
		const SnapshotScenario& scenario = scenarios[i];
		if (scenario.entryIndices.empty()) {
			continue;
		}
		if (!firstScenario) {
			json << ',';
		}
		firstScenario = false;
		json << '{';
		json << "\"index\":" << static_cast<uint32_t>(i);
		json << ",\"name\":\"" << escapeJsonString(stringFromIMPD(scenario.name)) << "\"";
		json << ",\"explicit\":" << (scenario.explicitScenario ? "true" : "false");
		json << ",\"entries\":[";
		bool firstEntry = true;
		for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
			const SnapshotEntry& entry = entries[scenario.entryIndices[j]];
			if (!firstEntry) {
				json << ',';
			}
			firstEntry = false;
			json << '{';
			json << "\"entryOrdinal\":" << entry.entryOrdinal;
			const uint32_t listIndex = (entry.entryOrdinal > 0 ? entry.entryOrdinal - 1 : 0);
			json << ",\"listIndex\":" << listIndex;
			json << ",\"validate\":" << (entry.validate ? "true" : "false");
			json << ",\"label\":\"" << escapeJsonString(stringFromIMPD(entry.scenarioName)) << "\"";
			json << '}';
		}
		json << ']';
		json << '}';
	}
	json << "]}";
	return json.str();
}

}

extern "C" {

EMSCRIPTEN_KEEPALIVE
uint8_t* rasterizeIVG(const char* ivgSource, double scaling, int scenarioIndex, int entryOrdinal) {
	uint8_t* pixelsArray = 0;
	try {
		const uint32_t sentinel = std::numeric_limits<uint32_t>::max();
		const std::string sourceText = (ivgSource != 0 ? std::string(ivgSource) : std::string());
		String sourceString;
		sourceString.assign(sourceText.begin(), sourceText.end());

		const std::vector<std::string> includeDirs = buildCollectorIncludeDirectories();
		SnapshotPlan& snapshotPlan = snapshotPlanCache.ensure(sourceText, sourceString, includeDirs);

		const std::vector<SnapshotScenario>& scenarios = snapshotPlan.getScenarios();
		const std::vector<SnapshotEntry>& entries = snapshotPlan.getEntries();

		uint32_t defaultScenarioIndex = sentinel;
		uint32_t defaultEntryOrdinal = sentinel;
		for (size_t i = 0; i < scenarios.size(); ++i) {
			if (scenarios[i].entryIndices.empty()) {
				continue;
			}
			defaultScenarioIndex = static_cast<uint32_t>(i);
			const uint32_t entryIndex = scenarios[i].entryIndices[0];
			if (entryIndex < entries.size()) {
					defaultEntryOrdinal = entries[entryIndex].entryOrdinal;
			}
			break;
		}

		uint32_t selectedScenarioIndex = defaultScenarioIndex;
		uint32_t selectedEntryOrdinal = defaultEntryOrdinal;
		if (defaultScenarioIndex == sentinel || defaultEntryOrdinal == sentinel) {
			selectedScenarioIndex = sentinel;
			selectedEntryOrdinal = sentinel;
		}

		if (selectedScenarioIndex != sentinel && selectedEntryOrdinal != sentinel && scenarioIndex >= 0 && entryOrdinal >= 1) {
			const uint32_t requestedScenarioIndex = static_cast<uint32_t>(scenarioIndex);
			const uint32_t requestedEntryOrdinal = static_cast<uint32_t>(entryOrdinal);
			if (requestedScenarioIndex < scenarios.size()) {
				const SnapshotScenario& scenario = scenarios[requestedScenarioIndex];
				if (requestedEntryOrdinal >= 1 && requestedEntryOrdinal <= scenario.entryIndices.size()) {
					selectedScenarioIndex = requestedScenarioIndex;
					selectedEntryOrdinal = requestedEntryOrdinal;
				}
			}
		}

		const std::string catalogJson = buildSnapshotCatalogJson(snapshotPlan, defaultScenarioIndex, defaultEntryOrdinal);

		GuardedSelfContainedARGB32Canvas canvas(scaling, MAX_RASTER_PIXELS, VECTOR_HEAP_RESERVE_BYTES);
		{
			STLMapVariables topVars;
			FormatInfo formatInfo;
			if (selectedScenarioIndex != sentinel && selectedEntryOrdinal != sentinel) {
				const SnapshotScenario& scenario = scenarios[selectedScenarioIndex];
				if (selectedEntryOrdinal - 1 < scenario.entryIndices.size()) {
					const uint32_t entryIndex = scenario.entryIndices[selectedEntryOrdinal - 1];
					if (entryIndex < entries.size()) {
						const SnapshotEntry& entry = entries[entryIndex];
                                               SnapshotExecutor executor(canvas, AffineTransformation().scale(scaling), includeDirs, SNAPSHOT_SOURCE_PATH, snapshotPlan, scenario, entry);
                                                Interpreter impd(executor, topVars, formatInfo);
                                                impd.run(StringRange(sourceString));
                                                if (!executor.finished()) {
                                                        throw runtime_error("Snapshot playback did not execute all invocations.");
                                                }
					}
				}
			} else {
				IVGExecutorWithExternalFonts ivgExecutor(canvas, AffineTransformation().scale(scaling));
				Interpreter impd(ivgExecutor, topVars, formatInfo);
				impd.run(StringRange(sourceString));
			}
		}

		SelfContainedRaster<ARGB32>* raster = canvas.accessRaster();
		if (raster == 0) {
				throw runtime_error("IVG image is empty");
		}
		const IntRect bounds = raster->calcBounds();
		if (bounds.width <= 0 || bounds.height <= 0) {
				throw runtime_error("IVG image is empty");
		}
		if (bounds.width > MAX_RASTER_DIMENSION || bounds.height > MAX_RASTER_DIMENSION) {
				std::ostringstream message;
				message << "Rasterization aborted: scaled bounds " << bounds.width << "x" << bounds.height
						<< " exceed the " << MAX_RASTER_DIMENSION << "px dimension cap.";
				throw runtime_error(message.str());
		}
		const long long pixelCount = static_cast<long long>(bounds.width) * static_cast<long long>(bounds.height);
		if (pixelCount > MAX_RASTER_PIXELS) {
				std::ostringstream message;
				message << "Rasterization aborted: " << bounds.width << "x" << bounds.height
						<< " = " << pixelCount << " pixels exceeds the " << MAX_RASTER_PIXELS << " pixel budget.";
				throw runtime_error(message.str());
		}
		const int imageStride = raster->getStride();
		const ARGB32::Pixel* sourcePixels = raster->getPixelPointer() + bounds.top * imageStride + bounds.left;
		const size_t requiredPixelBytes = static_cast<size_t>(bounds.width) * static_cast<size_t>(bounds.height) * 4u;
		const size_t headerUint32Count = 8;
		const size_t headerBytes = headerUint32Count * sizeof(uint32_t);
		const size_t catalogBytes = catalogJson.size() + 1;
		const size_t requiredBytes = headerBytes + requiredPixelBytes + catalogBytes;
#ifdef __EMSCRIPTEN__
		const size_t freeHeapBytes = computeFreeHeapBytes();
		if (freeHeapBytes > 0 && requiredBytes + VECTOR_HEAP_RESERVE_BYTES > freeHeapBytes) {
				std::ostringstream message;
				message << "Rasterization aborted: " << requiredBytes << " bytes required but only "
						<< freeHeapBytes << " bytes free in the WebAssembly heap.";
				throw runtime_error(message.str());
		}
#endif
		pixelsArray = new uint8_t[requiredBytes];
		uint32_t* header = reinterpret_cast<uint32_t*>(pixelsArray);
		header[0] = static_cast<uint32_t>(bounds.left);
		header[1] = static_cast<uint32_t>(bounds.top);
		header[2] = static_cast<uint32_t>(bounds.width);
		header[3] = static_cast<uint32_t>(bounds.height);
		header[4] = static_cast<uint32_t>(requiredPixelBytes);
		header[5] = static_cast<uint32_t>(catalogBytes);
		header[6] = selectedScenarioIndex;
		header[7] = selectedEntryOrdinal;
		uint8_t* dp = pixelsArray + headerBytes;
		for (int y = 0; y < bounds.height; ++y) {
			const ARGB32::Pixel* sp = sourcePixels + y * imageStride;
			for (int x = 0; x < bounds.width; ++x) {
				const uint32_t p = *sp;
				++sp;
				const int a = (p >> 24) & 0xFF;
				int r = (p >> 16) & 0xFF;
				int g = (p >> 8) & 0xFF;
				int b = p & 0xFF;
				if (a != 0xFF && a != 0x00) {
					const int m = 0xFFFF / a;
					r = (r * m) >> 8;
					g = (g * m) >> 8;
					b = (b * m) >> 8;
					assert(0 <= r && r < 0x100);
					assert(0 <= g && g < 0x100);
					assert(0 <= b && b < 0x100);
				}
				dp[0] = r;
				dp[1] = g;
				dp[2] = b;
				dp[3] = a;
				dp += 4;
			}
		}
		assert(dp == pixelsArray + headerBytes + bounds.width * bounds.height * 4);
		std::memcpy(pixelsArray + headerBytes + requiredPixelBytes, catalogJson.c_str(), catalogBytes);
	}
	catch (const IMPD::Exception& x) {
		delete [] pixelsArray;
		cout << x.what() << endl;
		if (x.hasStatement()) {
			cout << "in statement: " << x.getStatement() << endl;
		}
		cout << flush;
		return 0;
	}
	catch (const exception& x) {
		delete [] pixelsArray;
		cout << x.what() << endl << flush;
		return 0;
	}
	catch (...) {
		delete [] pixelsArray;
		cout << "General exception" << endl << flush;
		return 0;
	}
	return pixelsArray;
}

EMSCRIPTEN_KEEPALIVE
void deallocatePixels(uint8_t* pixelsArray) {
	delete [] pixelsArray;
}

EMSCRIPTEN_KEEPALIVE
size_t getFreeHeapBytes() {
	return computeFreeHeapBytes();
}

}

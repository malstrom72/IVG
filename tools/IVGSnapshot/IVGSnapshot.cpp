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

#include <climits>
#include <fstream>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <png.h>
#include <zlib.h>

#include "src/IMPD.h"
#include "src/IVG.h"

namespace IVG {

	/**
		Loads an IVG source once and replays it on demand so snapshot
		playback can reuse parsed scripts without touching the core library.
	**/
	class Document {
	public:
		Document()
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

		void render(IVGExecutor& executor) const
		{
			IMPD::STLMapVariables variables;
			IMPD::FormatInfo formatInfo;
			IMPD::Interpreter interpreter(executor, variables, formatInfo);
			interpreter.run(IMPD::StringRange(source));
		}

	private:
		IMPD::String source;
	};
}

using namespace IMPD;

namespace IVGSnapshotInternal {

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


	class SnapshotPlan {
		public:
		explicit SnapshotPlan(const std::string& ivgPath)
		: baseName(extractBaseName(ivgPath))
		, nextBlockOrdinal(1)
		{
		}

		void addBlock(Interpreter& interpreter, const SnapshotBlock& block)
		{
			if (block.statements.empty()) {
				Interpreter::throwBadSyntax("snapshot meta requires at least one statement block.");
			}

			const uint32_t blockOrdinal = nextBlockOrdinal;
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
		}

		const std::vector<SnapshotScenario>& getScenarios() const { return scenarios; }
		const std::vector<SnapshotEntry>& getEntries() const { return entries; }

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
			plan.addBlock(interpreter, block);
			return true;
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

			const std::map<WideString, IVG::Font>::iterator cached = sharedResources.fonts.find(fontName);
			if (cached != sharedResources.fonts.end()) {
				return std::vector<const IVG::Font*>(1, &cached->second);
			}

			IVG::Font font;
			if (!loadExternalFont(fontName, font)) {
				return std::vector<const IVG::Font*>();
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
			const std::map<std::string, CachedImage>::iterator it = sharedResources.images.find(path);
			if (it != sharedResources.images.end()) {
				return &it->second;
			}

			CachedImage cached;
			if (!loadPngRaster(path, cached.raster)) {
				return 0;
			}
			cached.xResolution = 1.0;
			cached.yResolution = 1.0;

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
		std::cout << "\t--force-update\t\tOverwrite goldens (not yet implemented)." << std::endl;
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



        static bool renderEntry(const CommandLineOptions& options, const std::string& path, const IVG::Document& document, SharedResources& sharedResources, const SnapshotScenario& scenario, const SnapshotEntry& entry)
        {
                IVG::SelfContainedARGB32Canvas canvas;
                SnapshotPlaybackExecutor executor(canvas, scenario, entry, options, path, sharedResources);
                try {
                        document.render(executor);
                } catch (Exception& e) {
                        std::cerr << path << ": scenario " << entry.scenarioName << ": " << e.getError();
                        if (e.hasStatement()) {
                                std::cerr << " near \"" << e.getStatement() << "\"";
			}
			std::cerr << std::endl;
			return false;
		} catch (std::exception& e) {
			std::cerr << path << ": scenario " << entry.scenarioName << ": " << e.what() << std::endl;
			return false;
		}

		if (!executor.finished()) {
			std::cerr << path << ": scenario " << entry.scenarioName << " did not execute all snapshot invocations." << std::endl;
			return false;
		}
		return true;
	}



        static int renderPlan(const CommandLineOptions& options, const std::string& path, const IVG::Document& document, const SnapshotPlan& plan)
        {
                const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
                const std::vector<SnapshotEntry>& entries = plan.getEntries();
                SharedResources sharedResources;
                int exitCode = 0;
                for (size_t i = 0; i < scenarios.size(); ++i) {
                        const SnapshotScenario& scenario = scenarios[i];
                        if (options.verbose) {
                                std::cout << path << ": scenario " << scenario.name << " (validate: " << (scenario.validate ? "yes" : "no") << ")" << std::endl;
                        }
                        for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
                                const SnapshotEntry& entry = entries[scenario.entryIndices[j]];
                                if (options.verbose) {
                                        std::cout << path << ":   entry " << entry.entryOrdinal << " name:" << entry.scenarioName
                                                << " (validate: " << (entry.validate ? "yes" : "no") << ")" << std::endl;
                                }
                                if (!renderEntry(options, path, document, sharedResources, scenario, entry)) {
                                        exitCode = 1;
                                        if (options.exitOnFirstFailure) {
                                                return exitCode;
                                        }
                                }
			}
		}
		return exitCode;
	}
        static int processFile(const CommandLineOptions& options, const std::string& path)
        {
                IVG::Document document;
                if (!document.loadFromFile(path)) {
                        std::cerr << "failed to read IVG file: " << path << std::endl;
                        return 1;
                }

                SnapshotPlan plan(path);
                const String& source = document.getSource();
                SnapshotCollector collector(plan, path, source, options.includeDirs);
		STLMapVariables variables;
		FormatInfo formatInfo;
		Interpreter interpreter(collector, variables, formatInfo);

		try {
			interpreter.run(StringRange(source));
		} catch (Exception& e) {
			std::cerr << path << ": " << e.getError();
			if (e.hasStatement()) {
				std::cerr << " near \"" << e.getStatement() << "\"";
			}
			std::cerr << std::endl;
			return 1;
		} catch (std::exception& e) {
			std::cerr << path << ": " << e.what() << std::endl;
			return 1;
		}

		if (options.listOnly || options.verbose) {
			printPlan(path, plan);
		}

		if (options.listOnly) {
			return 0;
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



} // namespace IVGSnapshotInternal

using namespace IVGSnapshotInternal;

#if !defined(IVG_SNAPSHOT_TESTING)

int main(int argc, char** argv)
{
	CommandLineOptions options;
	if (!parseCommandLine(argc, argv, options)) {
		return 1;
	}

	int exitCode = 0;
	for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
		const int result = processFile(options, options.ivgPaths[i]);
		if (result != 0) {
			exitCode = result;
			if (options.exitOnFirstFailure) {
				break;
			}
		}
	}
	return exitCode;
}

#endif // !defined(IVG_SNAPSHOT_TESTING)

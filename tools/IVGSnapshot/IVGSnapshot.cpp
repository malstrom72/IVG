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
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "src/IMPD.h"

using namespace IMPD;

namespace IVGSnapshotInternal {

	struct SnapshotBlock {
		bool validate;
		String scenario;
		StringVector statements;
		uint32_t sourceLine;
	};

	struct SnapshotEntry {
		uint32_t scenarioIndex;
		uint32_t blockIndex;
		uint32_t entryIndex;
		uint32_t sourceLine;
		bool validate;
		String scenarioName;
		String statements;
	};

	struct SnapshotScenario {
		String name;
		bool validate;
		std::vector<uint32_t> entryIndices;
	};

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

			const bool hasExplicitScenario = !block.scenario.empty();
			for (uint32_t i = 0; i < block.statements.size(); ++i) {
				const uint32_t entryOrdinal = i + 1;
				const String scenarioName = (hasExplicitScenario
				? block.scenario
				: synthesizeScenarioName(block.statements.size(), entryOrdinal));

				const uint32_t scenarioIndex = resolveScenario(interpreter, scenarioName, block.validate);

				SnapshotEntry entry;
				entry.scenarioIndex = scenarioIndex;
				entry.blockIndex = nextBlockOrdinal;
				entry.entryIndex = entryOrdinal;
				entry.sourceLine = block.sourceLine;
				entry.validate = block.validate;
				entry.scenarioName = scenarioName;
				entry.statements = block.statements[i];

				entries.push_back(entry);
				scenarios[scenarioIndex].entryIndices.push_back(static_cast<uint32_t>(entries.size() - 1));
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

		String synthesizeScenarioName(uint32_t blockCount, uint32_t entryOrdinal) const
		{
			String name = baseName;
			name += '-';
			name += Interpreter::toString(static_cast<int32_t>(nextBlockOrdinal));
			if (blockCount > 1) {
				name += '-';
				name += Interpreter::toString(static_cast<int32_t>(entryOrdinal));
			}
			return name;
		}

		uint32_t resolveScenario(Interpreter& interpreter, const String& name, bool validate)
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

			scenarios.push_back(scenario);
			const uint32_t index = static_cast<uint32_t>(scenarios.size() - 1);
			scenarioLookup.insert(std::make_pair(name, index));
			return index;
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
			if (tryReadFile(resolveRelativePath(utf8), contents)) {
				return true;
			}
			for (size_t i = 0; i < includeDirs.size(); ++i) {
				if (tryReadFile(includeDirs[i] + "/" + utf8, contents)) {
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
			block.validate = parseValidate(interpreter, args.fetchOptional("validate"));
			const String* scenarioLabel = args.fetchOptional("scenario");
			if (scenarioLabel != 0) {
				block.scenario = *scenarioLabel;
			}

			const String* rawStatements = args.fetchOptional(0, false);
			if (rawStatements == 0) {
				Interpreter::throwBadSyntax("snapshot meta requires a statement list.");
			}

			block.statements = parseStatements(interpreter, *rawStatements);
			block.sourceLine = locateMetaLine();

			args.throwIfAnyUnfetched();
			plan.addBlock(interpreter, block);
			return true;
		}

		private:
		bool parseValidate(Interpreter& interpreter, const String* value)
		{
			if (value == 0) {
				return true;
			}
			return interpreter.toBool(*value);
		}

		StringVector parseStatements(Interpreter& interpreter, const String& raw)
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

		bool tryReadFile(const std::string& path, String& contents)
		{
			std::ifstream stream(path.c_str(), std::ios::binary);
			if (!stream.good()) {
				return false;
			}
			std::string buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			contents.assign(buffer.begin(), buffer.end());
			return true;
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
			std::cout << "\tScenario " << scenario.name << " (validate: " << (scenario.validate ? "yes" : "no") << ")" << std::endl;
			for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
				const SnapshotEntry& entry = entries[scenario.entryIndices[j]];
				std::cout << "\t\tBlock " << entry.blockIndex << ", entry " << entry.entryIndex << ", line " << entry.sourceLine << std::endl;

				std::istringstream snippet(entry.statements);
				std::string line;
				while (std::getline(snippet, line)) {
					std::cout << "\t\t\t[ " << line << " ]" << std::endl;
				}
			}
		}
	}

	static int processFile(const CommandLineOptions& options, const std::string& path)
	{
		String source;
		if (!readFile(path, source)) {
			std::cerr << "failed to read IVG file: " << path << std::endl;
			return 1;
		}

		SnapshotPlan plan(path);
		SnapshotCollector collector(plan, path, source, options.includeDirs);
		STLMapVariables variables;
		FormatInfo formatInfo;
		Interpreter interpreter(collector, variables, formatInfo);

		try {
			interpreter.run(StringRange(source));
		} catch (Exception& e) {
			std::cerr << path << ": " << e.getError();
			if (e.hasStatement()) {
				std::cerr << " near \"" << String(e.getStatement()) << "\"";
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

		if (!options.listOnly) {
			std::cout << path << ": rendering pipeline not implemented yet." << std::endl;
		}

		return 0;
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

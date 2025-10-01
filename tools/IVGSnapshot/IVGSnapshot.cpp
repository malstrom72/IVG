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
#include <cctype>
#include <climits>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "src/IMPD.h"

using namespace std;
using namespace IMPD;

namespace {
	enum ValidateMode {
		VALIDATE_YES,
		VALIDATE_NO
	};

	struct SnapshotBlock {
		ValidateMode validate;
		std::string scenario;
		std::vector<std::string> statements;
		uint32_t sourceLine;
	};

	struct SnapshotEntry {
		size_t scenarioIndex;
		uint32_t blockIndex;
		uint32_t entryIndex;
		uint32_t sourceLine;
		ValidateMode validate;
		std::string scenarioName;
		std::string statements;
	};

	struct SnapshotScenario {
		std::string name;
		ValidateMode validate;
		std::vector<size_t> entries;
	};

	static const char* toCString(ValidateMode mode) {
		return (mode == VALIDATE_YES ? "yes" : "no");
	}

	static bool isSpace(Char c) {
		switch (c) {
			case ' ': case '\\t': case '\\r': case '\\n': return true;
			default: return false;
		}
	}

	static StringRange trimRange(const String& value) {
		StringIt b = value.begin();
		StringIt e = value.end();
		while (b != e && isSpace(*b)) ++b;
		while (e != b && isSpace(e[-1])) --e;
		return StringRange(b, e);
	}

	static StringRange trimRange(const StringRange& value) {
		StringIt b = value.b;
		StringIt e = value.e;
		while (b != e && isSpace(*b)) ++b;
		while (e != b && isSpace(e[-1])) --e;
		return StringRange(b, e);
	}

	static std::string copyRange(const StringRange& range) {
		return std::string(range.b, range.e);
	}

	class SnapshotPlan {
		public:
			SnapshotPlan(const std::string& ivgPath)
			: baseName(extractBaseName(ivgPath))
			, blockOrdinal(1)
			{
			}

			void addBlock(const SnapshotBlock& block) {
				if (block.statements.empty()) {
					throw std::runtime_error("snapshot block must contain at least one statement list");
				}
				for (size_t i = 0; i < block.statements.size(); ++i) {
					const std::string scenarioName = selectScenarioName(block, static_cast<uint32_t>(i + 1));
					size_t scenarioIndex = resolveScenario(scenarioName, block.validate);
					SnapshotEntry entry;
					entry.scenarioIndex = scenarioIndex;
					entry.blockIndex = blockOrdinal;
					entry.entryIndex = static_cast<uint32_t>(i + 1);
					entry.sourceLine = block.sourceLine;
					entry.validate = block.validate;
					entry.scenarioName = scenarioName;
					entry.statements = block.statements[i];
					scenarios[scenarioIndex].entries.push_back(entries.size());
					entries.push_back(entry);
				}
				++blockOrdinal;
			}

			const std::vector<SnapshotScenario>& getScenarios() const { return scenarios; }
			const std::vector<SnapshotEntry>& getEntries() const { return entries; }

		private:
			std::string extractBaseName(const std::string& path) const {
				size_t slash = path.find_last_of("/\\");
				size_t baseOffset = (slash == std::string::npos ? 0 : slash + 1);
				size_t dot = path.find_last_of('.');
				if (dot == std::string::npos || dot < baseOffset) dot = path.size();
				return path.substr(baseOffset, dot - baseOffset);
			}

			std::string selectScenarioName(const SnapshotBlock& block, uint32_t entryIndex) const {
				if (!block.scenario.empty()) return block.scenario;
				if (block.statements.size() == 1) {
					return baseName + "-" + to_string(blockOrdinal);
				}
				return baseName + "-" + to_string(blockOrdinal) + "-" + to_string(entryIndex);
			}

			size_t resolveScenario(const std::string& name, ValidateMode validate) {
				auto it = scenarioLookup.find(name);
				if (it != scenarioLookup.end()) {
					SnapshotScenario& existing = scenarios[it->second];
					if (existing.validate != validate) {
						throw std::runtime_error("scenario \"" + name + "\" switches validation mode");
					}
					return it->second;
				}
				size_t index = scenarios.size();
				SnapshotScenario scenario;
				scenario.name = name;
				scenario.validate = validate;
				scenarios.push_back(scenario);
				scenarioLookup.insert(std::make_pair(name, index));
				return index;
			}

			std::string baseName;
			std::vector<SnapshotEntry> entries;
			std::vector<SnapshotScenario> scenarios;
			std::map<std::string, size_t> scenarioLookup;
			uint32_t blockOrdinal;
	};

	class SnapshotCollector : public Executor {
		public:
		SnapshotCollector(SnapshotPlan& plan, const std::string& sourcePath, const String& sourceText
					, const std::vector<std::string>& includeDirs)
		: plan(plan)
		, sourcePath(sourcePath)
		, sourceText(sourceText)
		, includeDirs(includeDirs)
		, scanOffset(0)
		{
		}

		virtual bool format(Interpreter& interpreter, const FormatInfo& formatInfo) {
				(void)interpreter;
				(void)formatInfo;
				return true;
		}

		virtual bool execute(Interpreter& interpreter, const String& instruction, const String& arguments) {
				(void)interpreter;
				(void)instruction;
				(void)arguments;
				return true;
		}

		virtual bool progress(Interpreter& interpreter, int maxStatementsLeft) {
				(void)interpreter;
				(void)maxStatementsLeft;
				return true;
		}

		virtual bool load(Interpreter& interpreter, const WideString& filename, String& contents) {
				(void)interpreter;
				const std::string utf8(filename.begin(), filename.end());
				if (tryReadFile(resolveRelativePath(utf8), contents)) return true;
				for (size_t i = 0; i < includeDirs.size(); ++i) {
					if (tryReadFile(includeDirs[i] + "/" + utf8, contents)) return true;
				}
				return false;
		}

		virtual void trace(Interpreter& interpreter, const WideString& s) {
				(void)interpreter;
				(void)s;
		}

		virtual bool meta(Interpreter& interpreter, const String& key, const String& arguments) {
				static const String SNAPSHOT_KEY("snapshot-1");
				if (key != SNAPSHOT_KEY) return false;

				ArgumentsContainer args(ArgumentsContainer::parse(interpreter, StringRange(arguments)));
				SnapshotBlock block;
				block.validate = parseValidate(interpreter, args.fetchOptional("validate"));
				const String* scenarioLabel = args.fetchOptional("scenario");
				if (scenarioLabel != 0) block.scenario.assign(scenarioLabel->begin(), scenarioLabel->end());
				const String* body = args.fetchOptional(0, false);
				if (body == 0) interpreter.throwBadSyntax("snapshot meta requires a statement list.");
				block.statements = parseStatements(interpreter, *body);
				block.sourceLine = locateMetaLine();
				args.throwIfAnyUnfetched();

				try {
					plan.addBlock(block);
				}
				catch (const std::exception& e) {
					Interpreter::throwRunTimeError(String(e.what()));
				}
				return true;
		}

		private:
		ValidateMode parseValidate(Interpreter& interpreter, const String* value) {
				if (value == 0) return VALIDATE_YES;
				String lower = interpreter.toLower(StringRange(value->begin(), value->end()));
				if (lower == Interpreter::YES_STRING) return VALIDATE_YES;
				if (lower == Interpreter::NO_STRING) return VALIDATE_NO;
				interpreter.throwBadSyntax("validate argument must be yes or no.");
				return VALIDATE_YES;
		}

		std::vector<std::string> parseStatements(Interpreter& interpreter, const String& raw) {
				StringRange trimmed = trimRange(raw);
				if (trimmed.b == trimmed.e) interpreter.throwBadSyntax("snapshot meta requires a bracketed statement list.");
				if (*trimmed.b != '[' || trimmed.e[-1] != ']') {
					interpreter.throwBadSyntax("snapshot statements must be surrounded by [ and ].");
				}
				StringRange inner(trimmed.b + 1, trimmed.e - 1);
				StringRange innerTrimmed = trimRange(inner);
				std::vector<std::string> result;
				if (innerTrimmed.b != innerTrimmed.e && *innerTrimmed.b == '[') {
					StringVector elements;
					interpreter.parseList(inner, elements, false, false, 1, INT_MAX);
					result.reserve(elements.size());
					for (size_t i = 0; i < elements.size(); ++i) {
						StringRange elementTrimmed = trimRange(elements[i]);
						result.push_back(extractBlock(interpreter, elementTrimmed));
					}
				} else {
					result.push_back(extractBlock(interpreter, trimmed));
				}
				return result;
		}

		std::string extractBlock(Interpreter& interpreter, const StringRange& block) {
				if (block.e - block.b < 2 || *block.b != '[' || block.e[-1] != ']') {
					interpreter.throwBadSyntax("each snapshot entry must be enclosed in [ ... ].");
				}
				return copyRange(StringRange(block.b + 1, block.e - 1));
		}

		bool tryReadFile(const std::string& path, String& contents) {
				std::ifstream stream(path.c_str(), std::ios::binary);
				if (!stream.good()) return false;
				std::string buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
				contents.assign(buffer.begin(), buffer.end());
				return true;
		}

		std::string resolveRelativePath(const std::string& requested) const {
				size_t slash = sourcePath.find_last_of("/\\");
				if (slash == std::string::npos) return requested;
				return sourcePath.substr(0, slash + 1) + requested;
		}

		uint32_t locateMetaLine() {
				static const String TOKEN("meta snapshot");
				size_t pos = sourceText.find(TOKEN, scanOffset);
				if (pos == String::npos) return 0;
				scanOffset = pos + 1;
				uint32_t line = 1;
				for (size_t i = 0; i < pos; ++i) {
					if (sourceText[i] == '\\n') ++line;
				}
				return line;
		}

		SnapshotPlan& plan;
		std::string sourcePath;
		const String& sourceText;
		std::vector<std::string> includeDirs;
		size_t scanOffset;
	};

	struct CommandLineOptions {
		std::vector<std::string> includeDirs;
		std::vector<std::string> fontDirs;
		std::vector<std::string> imageDirs;
		std::string outputDir;
		bool forceUpdate = false;
		bool listOnly = false;
		bool verbose = false;
		bool exitOnFirstFailure = false;
		uint32_t threads = 0;
		std::vector<std::string> ivgPaths;
	};

	static void printUsage(const char* program) {
		cout << "Usage: " << program << " [options] <ivg> [<ivg> ...]" << endl;
		cout << "Options:" << endl;
		cout << "\t--include-dir <path>\tAdd include search path." << endl;
		cout << "\t--font-dir <path>\tAdd font search path." << endl;
		cout << "\t--image-dir <path>\tAdd image search path." << endl;
		cout << "\t--output-dir <path>\tOverride output directory." << endl;
		cout << "\t--force-update\t\tOverwrite goldens (not yet implemented)." << endl;
		cout << "\t--threads <n>\t\tNumber of worker threads." << endl;
		cout << "\t--list-only\t\tList collected snapshots without rendering." << endl;
		cout << "\t--verbose\t\tPrint verbose diagnostics." << endl;
		cout << "\t--exit-on-first-failure\tAbort after first failure." << endl;
		cout << "\t--help\t\t\tShow this message." << endl;
	}

	static bool parseUnsigned(const std::string& text, uint32_t& value) {
		if (text.empty()) return false;
		uint64_t parsed = 0;
		for (size_t i = 0; i < text.size(); ++i) {
			if (text[i] < '0' || text[i] > '9') return false;
			parsed = parsed * 10 + static_cast<uint32_t>(text[i] - '0');
			if (parsed > UINT32_MAX) return false;
		}
		value = static_cast<uint32_t>(parsed);
		return true;
	}

	static bool parseCommandLine(int argc, char** argv, CommandLineOptions& options) {
		for (int i = 1; i < argc; ++i) {
			const std::string arg(argv[i]);
			if (arg == "--help") {
				printUsage(argv[0]);
				return false;
			} else if (arg == "--include-dir") {
				if (i + 1 >= argc) {
					cerr << "--include-dir requires a path." << endl;
					return false;
				}
				options.includeDirs.push_back(argv[++i]);
			} else if (arg == "--font-dir") {
				if (i + 1 >= argc) {
					cerr << "--font-dir requires a path." << endl;
					return false;
				}
				options.fontDirs.push_back(argv[++i]);
			} else if (arg == "--image-dir") {
				if (i + 1 >= argc) {
					cerr << "--image-dir requires a path." << endl;
					return false;
				}
				options.imageDirs.push_back(argv[++i]);
			} else if (arg == "--output-dir") {
				if (i + 1 >= argc) {
					cerr << "--output-dir requires a path." << endl;
					return false;
				}
				options.outputDir = argv[++i];
			} else if (arg == "--force-update") {
				options.forceUpdate = true;
			} else if (arg == "--threads") {
				if (i + 1 >= argc) {
					cerr << "--threads requires a numeric value." << endl;
					return false;
				}
				uint32_t threads = 0;
				if (!parseUnsigned(argv[i + 1], threads)) {
					cerr << "invalid thread count: " << argv[i + 1] << endl;
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
				cerr << "unrecognized option: " << arg << endl;
				return false;
			} else {
				options.ivgPaths.push_back(arg);
			}
		}
		if (options.ivgPaths.empty()) {
			cerr << "no IVG files specified." << endl;
			return false;
		}
		return true;
	}

	static bool readFile(const std::string& path, String& contents) {
		std::ifstream stream(path.c_str(), std::ios::binary);
		if (!stream.good()) return false;
		std::string buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		contents.assign(buffer.begin(), buffer.end());
		return true;
	}

	static void printPlan(const std::string& path, const SnapshotPlan& plan) {
		cout << path << endl;
		const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
		const std::vector<SnapshotEntry>& entries = plan.getEntries();
		for (size_t i = 0; i < scenarios.size(); ++i) {
			const SnapshotScenario& scenario = scenarios[i];
			cout << "\tScenario " << scenario.name << " (validate: " << toCString(scenario.validate) << ")" << endl;
			for (size_t j = 0; j < scenario.entries.size(); ++j) {
				const SnapshotEntry& entry = entries[scenario.entries[j]];
				cout << "\t\tBlock " << entry.blockIndex << ", entry " << entry.entryIndex
					<< ", line " << entry.sourceLine << endl;
				std::istringstream snippet(entry.statements);
				std::string line;
				while (std::getline(snippet, line)) {
					cout << "\t\t\t[ " << line << " ]" << endl;
				}
			}
		}
	}

	static int processFile(const CommandLineOptions& options, const std::string& path) {
		String source;
		if (!readFile(path, source)) {
			cerr << "failed to read IVG file: " << path << endl;
			return 1;
		}

		SnapshotPlan plan(path);
		SnapshotCollector collector(plan, path, source, options.includeDirs);
		STLMapVariables vars;
		FormatInfo formatInfo;
		Interpreter interpreter(collector, vars, formatInfo);
		try {
			interpreter.run(StringRange(source));
		}
		catch (Exception& e) {
			cerr << path << ": " << e.getError();
			if (e.hasStatement()) {
				cerr << " near \"" << std::string(e.getStatement().begin(), e.getStatement().end()) << "\"";
			}
			cerr << endl;
			return 1;
		}
		catch (std::exception& e) {
			cerr << path << ": " << e.what() << endl;
			return 1;
		}

		if (options.listOnly || options.verbose) {
			printPlan(path, plan);
		}

		if (!options.listOnly) {
			cout << path << ": rendering pipeline not implemented yet." << endl;
		}

		return 0;
	}
}

int main(int argc, char** argv) {
	CommandLineOptions options;
	if (!parseCommandLine(argc, argv, options)) return 1;

	int exitCode = 0;
	for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
		int result = processFile(options, options.ivgPaths[i]);
		if (result != 0) {
			exitCode = result;
			if (options.exitOnFirstFailure) break;
		}
	}
	return exitCode;
}

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
#include <atomic>
#include <cctype>
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
#include <set>
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
#include "tools/IVGSnapshot/BuiltInFonts.h"

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

class SnapshotFormatDetector : public IMPD::Executor {
	public:
		SnapshotFormatDetector()
				: sawFormat(false), supportsSnapshot(false) {}

		bool format(Interpreter &interpreter, const FormatInfo &formatInfo) {
			(void)interpreter;
			sawFormat = true;
			supportsSnapshot =
				(formatInfo.uses.find(String("snapshot-1")) != formatInfo.uses.end());
			throw FormatDetected();
		}

		bool execute(Interpreter &interpreter, const String &instruction,
				const String &arguments) {
			(void)interpreter;
			(void)instruction;
			(void)arguments;
			throw MissingFormat();
		}

		bool progress(Interpreter &interpreter, int maxStatementsLeft) {
			(void)interpreter;
			(void)maxStatementsLeft;
			return true;
		}

		bool load(Interpreter &interpreter, const WideString &filename,
				String &contents) {
			(void)interpreter;
			(void)filename;
			(void)contents;
			throw MissingFormat();
		}

		void trace(Interpreter &interpreter, const WideString &s) {
			(void)interpreter;
			(void)s;
		}

		bool meta(Interpreter &interpreter, const String &key,
				const String &arguments) {
			(void)interpreter;
			(void)key;
			(void)arguments;
			throw MissingFormat();
		}

		struct FormatDetected : public std::exception {
			const char *what() const noexcept override { return "format detected"; }
		};

		struct MissingFormat : public std::exception {
			const char *what() const noexcept override { return "format missing"; }
		};

		bool sawFormat;
		bool supportsSnapshot;
};

struct FormatDetectionResult {
	FormatDetectionResult() : determined(false), supportsSnapshot(false) {}

	bool determined;
	bool supportsSnapshot;
};

static FormatDetectionResult detectSnapshotFormat(const String &source) {
	FormatDetectionResult result;
	SnapshotFormatDetector detector;
	STLMapVariables variables;
	FormatInfo formatInfo;
	Interpreter interpreter(detector, variables, formatInfo);

	try {
		interpreter.run(StringRange(source));
	} catch (const SnapshotFormatDetector::FormatDetected &) {
		result.determined = true;
		result.supportsSnapshot = detector.supportsSnapshot;
		return result;
	} catch (const SnapshotFormatDetector::MissingFormat &) {
		result.determined = true;
		result.supportsSnapshot = false;
		return result;
	} catch (Exception &) {
		return result;
	} catch (std::exception &) {
		return result;
	}

	if (detector.sawFormat) {
		result.determined = true;
		result.supportsSnapshot = detector.supportsSnapshot;
		return result;
	}

	result.determined = true;
	result.supportsSnapshot = false;
	return result;
}

struct SnapshotInvocation {
        uint32_t blockIndex;
        uint32_t sourceLine;
        uint32_t entryOrdinal;
        String statements;
};
struct SnapshotRoundState {
        SnapshotRoundState() { reset(); }

        void reset() {
                hasPinned = false;
                scenario.clear();
                entryOrdinal = 0;
                validate = false;
                blockOrdinalCursor = 0;
                moreRemaining = false;
                explicitScenario = false;
                firstSourceLine = 0;
                invocations.clear();
        }

        void pin(const String &scenarioName, uint32_t ordinal, bool shouldValidate,
                          bool explicitLabel) {
                hasPinned = true;
                scenario = scenarioName;
                entryOrdinal = ordinal;
                validate = shouldValidate;
                explicitScenario = explicitLabel;
                firstSourceLine = 0;
                invocations.clear();
        }

        void advanceBlockCursor() { ++blockOrdinalCursor; }

        bool matchesSelection(const String &scenarioName, uint32_t ordinal) const {
                return hasPinned && scenario == scenarioName && entryOrdinal == ordinal;
        }

        SnapshotInvocation *findInvocation(uint32_t blockIndex,
                                                                  uint32_t entryOrdinal) {
                for (size_t i = 0; i < invocations.size(); ++i) {
                        SnapshotInvocation &invocation = invocations[i];
                        if (invocation.blockIndex == blockIndex &&
                                invocation.entryOrdinal == entryOrdinal) {
                                return &invocation;
                        }
                }
                return 0;
        }

        void recordInvocation(uint32_t blockIndex, uint32_t sourceLine,
                                                      uint32_t entryOrdinal,
                                                      const String &statementBody) {
                SnapshotInvocation invocation;
                invocation.blockIndex = blockIndex;
                invocation.sourceLine = sourceLine;
                invocation.entryOrdinal = entryOrdinal;
                invocation.statements = statementBody;
                invocations.push_back(invocation);
        }

        bool hasPinned;
        String scenario;
        uint32_t entryOrdinal;
        bool validate;
        uint32_t blockOrdinalCursor;
        bool moreRemaining;
        bool explicitScenario;
        uint32_t firstSourceLine;
        std::vector<SnapshotInvocation> invocations;
};

struct ScenarioEntryMetadata {
        ScenarioEntryMetadata() : hasDetails(false), firstSourceLine(0) {}

        bool hasDetails;
        uint32_t firstSourceLine;
        std::vector<SnapshotInvocation> invocations;
};

static bool parseUnsigned(const std::string &text, uint32_t &value);
static std::string stringFromIMPD(const String &value);
static bool isDigit(char ch) { return (ch >= '0' && ch <= '9'); }

static std::string toLowerAscii(const std::string &text) {
	std::string lowered(text);
	for (size_t i = 0; i < lowered.size(); ++i) {
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	}
	return lowered;
}

struct SeenScenario {
        SeenScenario()
                : explicitLabel(false), validate(false), maxOrdinal(0) {}

        void ensureCapacity(uint32_t ordinal) {
                if (ordinal == 0) {
                        return;
                }

                if (ordinal > maxOrdinal) {
                        maxOrdinal = ordinal;
                }

                if (processed.size() < maxOrdinal) {
                        processed.resize(maxOrdinal, false);
                }

                if (entryDetails.size() < maxOrdinal) {
                        entryDetails.resize(maxOrdinal);
                }
        }

        bool isProcessed(uint32_t ordinal) const {
                if (ordinal == 0) {
			return true;
		}

		if (ordinal > processed.size()) {
			return false;
		}

		return processed[ordinal - 1];
	}

	void markProcessed(uint32_t ordinal) {
		ensureCapacity(ordinal);
		if (ordinal == 0) {
			return;
		}

                processed[ordinal - 1] = true;
        }

        ScenarioEntryMetadata &accessEntryMetadata(uint32_t ordinal) {
                ensureCapacity(ordinal);
                if (ordinal == 0) {
                        throw std::runtime_error("entry ordinal must be >= 1");
                }
                return entryDetails[ordinal - 1];
        }

        const ScenarioEntryMetadata *getEntryMetadata(uint32_t ordinal) const {
                if (ordinal == 0) {
                        return 0;
                }
                if (ordinal > entryDetails.size()) {
                        return 0;
                }
                return &entryDetails[ordinal - 1];
        }

        String name;
        bool explicitLabel;
        bool validate;
        uint32_t maxOrdinal;
        std::vector<bool> processed;
        std::vector<ScenarioEntryMetadata> entryDetails;
};

struct ImplicitLabelGroup {
        ImplicitLabelGroup()
                : ordinal(0), totalEntries(0), processedEntries(0), preferredIndex(-1) {}

        uint32_t ordinal;
        uint32_t totalEntries;
        uint32_t processedEntries;
        int32_t preferredIndex;
};

struct ImplicitGroupKey {
        ImplicitGroupKey() : key(), preferredIndex(-1) {}

        std::string key;
        int32_t preferredIndex;
};

static ImplicitGroupKey deriveImplicitGroupKey(const std::string &scenarioName,
                                                                                 uint32_t fallbackIndex) {
        ImplicitGroupKey info;
        if (!scenarioName.empty()) {
                const size_t lastHyphen = scenarioName.find_last_of('-');
                if (lastHyphen != std::string::npos && lastHyphen + 1 < scenarioName.size()) {
                        bool trailingDigits = true;
                        for (size_t i = lastHyphen + 1; i < scenarioName.size(); ++i) {
                                if (!isDigit(scenarioName[i])) {
                                        trailingDigits = false;
                                        break;
                                }
                        }
                        if (trailingDigits) {
                                const std::string prefix = scenarioName.substr(0, lastHyphen);
                                const size_t prefixHyphen = prefix.find_last_of('-');
                                if (prefixHyphen != std::string::npos && prefixHyphen + 1 < prefix.size()) {
                                        bool prefixDigits = true;
                                        for (size_t i = prefixHyphen + 1; i < prefix.size(); ++i) {
                                                if (!isDigit(prefix[i])) {
                                                        prefixDigits = false;
                                                        break;
                                                }
                                        }
                                        if (prefixDigits) {
                                                uint32_t parsed = 0;
                                                if (parseUnsigned(scenarioName.substr(lastHyphen + 1), parsed)) {
                                                        info.preferredIndex =
                                                                (parsed > 0 ? static_cast<int32_t>(parsed - 1) : 0);
                                                }
                                                info.key = prefix;
                                                if (!info.key.empty()) {
                                                        return info;
                                                }
                                        }
                                }
                        }
                }

                info.key = scenarioName;
                return info;
        }

        std::ostringstream fallback;
        fallback << "implicit-" << fallbackIndex;
        info.key = fallback.str();
        return info;
}

class SnapshotProgress {
  public:
	struct Target {
		Target()
			: entryOrdinal(0), validate(false), explicitLabel(false) {}

		String scenario;
		uint32_t entryOrdinal;
		bool validate;
		bool explicitLabel;
	};

	SnapshotProgress() { reset(); }

	void reset() {
		seenScenarios.clear();
		scenarioLookup.clear();
		displayLabels.clear();
		implicitGroups.clear();
		nextImplicitOrdinal = 1;
		hasPendingTarget = false;
		pendingTarget = Target();
}

	bool empty() const { return seenScenarios.empty(); }

        bool observeScenarioEntry(SnapshotRoundState &round,
                const String &scenarioName,
                bool explicitLabel, bool validate,
                uint32_t entryOrdinal) {
                if (entryOrdinal == 0) {
                        throw std::runtime_error("snapshot entry ordinal must be >= 1");
                }

                const uint32_t normalizedOrdinal = (explicitLabel ? entryOrdinal : 1);

                SeenScenario &scenario = upsertScenarioRecord(scenarioName, explicitLabel,
                                validate);
                scenario.ensureCapacity(normalizedOrdinal);

                const bool alreadyProcessed = scenario.isProcessed(normalizedOrdinal);

                if (!round.hasPinned) {
                        if (!alreadyProcessed) {
                                round.pin(scenarioName, normalizedOrdinal, validate,
                                                explicitLabel);
                                return true;
                        }
                        return false;
                }

                if (!round.matchesSelection(scenarioName, normalizedOrdinal)) {
                        if (!alreadyProcessed) {
                                round.moreRemaining = true;
                        }
                        return false;
                }

                if (round.validate != validate) {
                        throw std::runtime_error("validate flag mismatch for scenario");
                }

                return !alreadyProcessed;
        }

	bool hasNextTarget() const {
		if (hasPendingTarget) {
			return true;
		}

		Target target;
		return findNextUnprocessedTarget(target);
	}

	Target makeRound() {
		if (hasPendingTarget) {
			hasPendingTarget = false;
			return pendingTarget;
		}

		Target target;
		if (!findNextUnprocessedTarget(target)) {
			return Target();
		}

		return target;
	}

	void setNextTarget(const String &scenarioName, uint32_t entryOrdinal,
		          bool validate, bool explicitLabel) {
		hasPendingTarget = true;
		pendingTarget.scenario = scenarioName;
		pendingTarget.entryOrdinal = entryOrdinal;
		pendingTarget.validate = validate;
		pendingTarget.explicitLabel = explicitLabel;
	}

        void markProcessed(const String &scenarioName, uint32_t entryOrdinal) {
                const auto lookupIterator = scenarioLookup.find(scenarioName);
                if (lookupIterator == scenarioLookup.end()) {
                        throw std::runtime_error("unknown scenario while marking processed");
                }

                SeenScenario &scenario = seenScenarios[lookupIterator->second];
                scenario.markProcessed(entryOrdinal);
        }

        void recordRoundDetails(const SnapshotRoundState &round) {
                if (!round.hasPinned) {
                        return;
                }

                const auto lookupIterator = scenarioLookup.find(round.scenario);
                if (lookupIterator == scenarioLookup.end()) {
                        throw std::runtime_error(
                                "unknown scenario while recording round details");
                }

                SeenScenario &scenario = seenScenarios[lookupIterator->second];
                ScenarioEntryMetadata &metadata =
                        scenario.accessEntryMetadata(round.entryOrdinal);
                metadata.hasDetails = true;
                metadata.firstSourceLine = round.firstSourceLine;
                metadata.invocations = round.invocations;
        }

        const SeenScenario *findScenarioRecord(const String &scenarioName) const {
                const auto lookupIterator = scenarioLookup.find(scenarioName);
                if (lookupIterator == scenarioLookup.end()) {
                        return 0;
                }
                return &seenScenarios[lookupIterator->second];
        }

        bool hasUnprocessedEntries() const {
                Target target;
                return findNextUnprocessedTarget(target);
        }

        const std::vector<SeenScenario> &getSeenScenarios() const {
                return seenScenarios;
        }

	std::string ensureDisplayLabel(const String &scenarioName, bool explicitLabel,
		const String *explicitScenarioLabel, uint32_t blockOrdinal,
		uint32_t catalogOrdinal, uint32_t normalizedOrdinal,
		uint32_t entryCount, bool firstEntryOfScenario)
	{
		const LabelKey key(stringFromIMPD(scenarioName), normalizedOrdinal);
		const auto existing = displayLabels.find(key);
		if (existing != displayLabels.end()) {
			return existing->second;
		}

		std::string label;
		if (explicitLabel && explicitScenarioLabel != 0 && !explicitScenarioLabel->empty()) {
			label = stringFromIMPD(*explicitScenarioLabel);
			if (entryCount > 1) {
				std::ostringstream stream;
				stream << label << " #" << static_cast<int32_t>(catalogOrdinal - 1);
				label = stream.str();
			}
		} else {
			label = buildImplicitLabel(stringFromIMPD(scenarioName), blockOrdinal,
				catalogOrdinal, entryCount, firstEntryOfScenario);
		}

		displayLabels.insert(std::make_pair(key, label));
		return label;
	}

	const std::string &lookupDisplayLabel(const String &scenarioName,
		uint32_t normalizedOrdinal) const
	{
		static const std::string EMPTY;
		const LabelKey key(stringFromIMPD(scenarioName), normalizedOrdinal);
		const auto it = displayLabels.find(key);
		if (it != displayLabels.end()) {
			return it->second;
		}
		return EMPTY;
	}

private:
	bool findNextUnprocessedTarget(Target &target) const {
		for (size_t i = 0; i < seenScenarios.size(); ++i) {
			const SeenScenario &scenario = seenScenarios[i];
			for (uint32_t ordinal = 1; ordinal <= scenario.maxOrdinal; ++ordinal) {
				if (!scenario.isProcessed(ordinal)) {
					target.scenario = scenario.name;
					target.entryOrdinal = ordinal;
					target.validate = scenario.validate;
					target.explicitLabel = scenario.explicitLabel;
					return true;
				}
			}
		}

		return false;
	}

	SeenScenario &upsertScenarioRecord(const String &scenarioName,
			bool explicitLabel,
			bool validate) {
		const auto lookupIterator = scenarioLookup.find(scenarioName);
		if (lookupIterator == scenarioLookup.end()) {
			const size_t index = seenScenarios.size();
			scenarioLookup.insert(std::make_pair(scenarioName, index));
			seenScenarios.push_back(SeenScenario());
			SeenScenario &scenario = seenScenarios.back();
			scenario.name = scenarioName;
			scenario.explicitLabel = explicitLabel;
			scenario.validate = validate;
			return scenario;
		}

		SeenScenario &scenario = seenScenarios[lookupIterator->second];
		if (scenario.validate != validate) {
			throw std::runtime_error("validate flag mismatch for scenario");
		}

		if (explicitLabel && !scenario.explicitLabel) {
			scenario.explicitLabel = true;
		}

		return scenario;
	}

	struct LabelKey {
		LabelKey(const std::string &nameValue, uint32_t ordinalValue)
			: name(nameValue), ordinal(ordinalValue) {}

		std::string name;
		uint32_t ordinal;

		bool operator<(const LabelKey &other) const
		{
			if (name < other.name) {
				return true;
			}
			if (name > other.name) {
				return false;
			}
			return ordinal < other.ordinal;
		}
	};

	std::string buildImplicitLabel(const std::string &scenarioKey,
		uint32_t blockOrdinal, uint32_t catalogOrdinal,
		uint32_t entryCount, bool firstEntryOfScenario)
	{
		const ImplicitGroupKey keyInfo =
			deriveImplicitGroupKey(scenarioKey, blockOrdinal);

		ImplicitLabelGroup &group = implicitGroups[keyInfo.key];
		if (group.ordinal == 0) {
			group.ordinal = nextImplicitOrdinal++;
			group.totalEntries = 0;
			group.processedEntries = 0;
			group.preferredIndex = keyInfo.preferredIndex;
		}

		if (firstEntryOfScenario) {
			group.totalEntries += entryCount;
		}

		const uint32_t totalEntries =
			(group.totalEntries > 0 ? group.totalEntries : entryCount);

		int32_t listIndex = -1;
		if (totalEntries > 1) {
			if (entryCount == 1 && keyInfo.preferredIndex >= 0) {
				listIndex = keyInfo.preferredIndex;
			} else if (catalogOrdinal > 0) {
				listIndex = static_cast<int32_t>(catalogOrdinal - 1);
			} else {
				listIndex = static_cast<int32_t>(group.processedEntries);
			}
		}

		std::ostringstream stream;
		stream << "unlabeled-" << group.ordinal;
		if (totalEntries > 1) {
			const int32_t normalizedIndex =
				(listIndex >= 0 ? listIndex
				       : static_cast<int32_t>(group.processedEntries));
			stream << " #" << normalizedIndex;
		}

		group.processedEntries += 1;
		return stream.str();
	}

	std::vector<SeenScenario> seenScenarios;
	std::map<String, size_t> scenarioLookup;
	std::map<LabelKey, std::string> displayLabels;
	std::map<std::string, ImplicitLabelGroup> implicitGroups;
	uint32_t nextImplicitOrdinal;
	bool hasPendingTarget;
	Target pendingTarget;
};

class SnapshotRoundCoordinator {
  public:
	SnapshotRoundCoordinator() { reset(); }

	void reset() {
		progress.reset();
		hasActiveTarget = false;
		activeTarget = SnapshotProgress::Target();
	}

	SnapshotRoundState beginRound() {
		SnapshotRoundState round;
		round.reset();
		activeTarget = progress.makeRound();
		hasActiveTarget = (activeTarget.entryOrdinal != 0);
                if (hasActiveTarget) {
                        round.pin(activeTarget.scenario, activeTarget.entryOrdinal,
                                activeTarget.validate, activeTarget.explicitLabel);
                }
		return round;
	}

	void completeRound(const SnapshotRoundState &round) {
		if (!round.hasPinned) {
			return;
		}

		progress.markProcessed(round.scenario, round.entryOrdinal);
		hasActiveTarget = false;
		activeTarget = SnapshotProgress::Target();
	}

	bool needsAnotherRound(const SnapshotRoundState &round) const {
		if (round.moreRemaining) {
			return true;
		}
		return progress.hasNextTarget();
	}

	SnapshotProgress &accessProgress() { return progress; }

	const SnapshotProgress &accessProgress() const { return progress; }

	const SnapshotProgress::Target *getActiveTarget() const {
		return (hasActiveTarget ? &activeTarget : 0);
	}

  private:
	SnapshotProgress progress;
	bool hasActiveTarget;
	SnapshotProgress::Target activeTarget;
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
		  fileFailed(false), ignored(false) {}

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
	bool ignored;
	std::string ignoreReason;
};

struct SnapshotTotals {
	SnapshotTotals()
		: filesProcessed(0), failedFiles(0), totalEntries(0), draftEntries(0),
		  validatedEntries(0), updatedEntries(0), failedEntries(0),
		  diffFailures(0), ignoredFiles(0) {}

	void accumulate(const SnapshotRunResult &run) {
		if (run.ignored) {
			++ignoredFiles;
			return;
		}
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
	uint32_t ignoredFiles;
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
	bool recursive;
	bool goldenAudit;
	uint32_t threads;
	std::vector<std::string> ivgPaths;

	CommandLineOptions()
		: rootDir(NuXFiles::Path::getCurrentDirectoryPath()),
		  forceUpdate(false), listOnly(false), verbose(false),
		  exitOnFirstFailure(false), recursive(false), goldenAudit(true),
		  threads(0) {}
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
std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
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
std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
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

// Returns true if there exists any golden PNG matching
// the source tag for this IVG (regardless of scenario label).
// Pattern: <snapshot-root>/<snapshotBase>__*.png
static bool hasAnyGoldensForSource(const std::string &ivgPath,
										 const std::string &snapshotBase,
										 const CommandLineOptions &options) {
	const std::string root =
		(options.snapshotDir.empty() ? extractDirectory(ivgPath)
									   : options.snapshotDir);

	if (root.empty() || snapshotBase.empty()) {
		return false;
	}

	const std::string wildcard =
		joinPath(root, snapshotBase + std::string("__*.png"));

	try {
		std::vector<NuXFiles::Path> matches;
		NuXFiles::Path::findPaths(matches, pathStringToWide(wildcard));
		return !matches.empty();
	} catch (...) {
		return false;
	}
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
	if (rootDir.makeRelative(withoutExtension, false, relativeWide)) {
		if (!relativeWide.empty()) {
			relative = pathStringFromWide(relativeWide);
			for (size_t i = 0; i < relative.size(); ++i) {
				if (relative[i] == '\\') {
					relative[i] = '/';
				}
			}
			return true;
		}
	}

	const std::wstring rootWide = rootDir.getFullPath();
	const std::wstring targetWide = withoutExtension.getFullPath();

	if (rootWide.empty() || targetWide.empty()) {
		return false;
	}

	std::wstring normalizedRoot = NuXFiles::Path::appendSeparator(rootWide);
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

	relative = pathStringFromWide(remainder);
	for (size_t i = 0; i < relative.size(); ++i) {
		if (relative[i] == '\\') {
			relative[i] = '/';
		}
	}
	return true;
}



static std::string buildSnapshotSourceTag(const std::string &ivgPath,
		const NuXFiles::Path &rootDir) {
	const NuXFiles::Path sourcePath = pathFromNativeString(ivgPath);
	if (!sourcePath.isNull()) {
		NuXFiles::Path withoutExtension(sourcePath);
		if (sourcePath.hasExtension()) {
			withoutExtension = sourcePath.withoutExtension();
		}

		std::string relative;
		if (tryBuildRelativeSnapshotTag(rootDir, withoutExtension, relative)) {
			return sanitizeFileComponent(relative);
		}
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
		const std::string &scenarioLabel,
		uint32_t blockIndex,
		uint32_t entryOrdinal) {
	std::ostringstream stream;
	stream << snapshotBase << '#' << scenarioLabel << '#'
			<< blockIndex << '#' << entryOrdinal;
	return stream.str();
}

static bool fileExists(const std::string &path) {
	if (path.empty()) {
		return false;
	}
	const NuXFiles::Path filePath = pathFromNativeString(path);
	if (filePath.isNull()) {
		return false;
	}
	return (filePath.exists() && filePath.isFile());
}

static bool directoryExists(const std::string &path) {
	if (path.empty()) {
		return false;
	}
	const NuXFiles::Path dirPath = pathFromNativeString(path);
	if (dirPath.isNull()) {
		return false;
	}
	return (dirPath.exists() && dirPath.isDirectory());
}

static bool hasIvgExtension(const std::string &path) {
	const size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) {
		return false;
	}
	const std::string extension = toLowerAscii(path.substr(dot + 1));
	return (extension == "ivg");
}

static bool collectDirectoryIvgFiles(const std::string &directory, bool recursive,
		std::vector<std::string> &files) {
	const NuXFiles::Path dirPath = pathFromNativeString(directory);
	if (dirPath.isNull() || !dirPath.exists() || !dirPath.isDirectory()) {
		std::cerr << "not a directory: " << directory << std::endl;
		return false;
	}

	std::vector<NuXFiles::Path> pending(1, dirPath);
	while (!pending.empty()) {
		const NuXFiles::Path current = pending.back();
		pending.pop_back();

		std::vector<NuXFiles::Path> subPaths;
		current.listSubPaths(subPaths);
		for (size_t i = 0; i < subPaths.size(); ++i) {
			const NuXFiles::Path &entry = subPaths[i];
			if (entry.isDirectory()) {
				if (recursive) {
					pending.push_back(entry);
				}
				continue;
			}
			if (!entry.isFile()) {
				continue;
			}
			const std::wstring widePath = entry.getFullPath();
			const std::string nativePath = pathStringFromWide(widePath);
			if (hasIvgExtension(nativePath)) {
				files.push_back(nativePath);
			}
		}
	}

	if (files.empty()) {
		std::cerr << "no IVG files found under directory: " << directory
				<< std::endl;
		return false;
	}

	std::sort(files.begin(), files.end());
	return true;
}

static bool expandInputPaths(CommandLineOptions &options) {
	std::vector<std::string> expanded;
	expanded.reserve(options.ivgPaths.size());
	for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
		const std::string &input = options.ivgPaths[i];
		if (directoryExists(input)) {
			std::vector<std::string> directoryFiles;
			if (!collectDirectoryIvgFiles(input, options.recursive, directoryFiles)) {
				return false;
			}
			expanded.insert(expanded.end(), directoryFiles.begin(), directoryFiles.end());
		} else {
			expanded.push_back(input);
		}
	}

	options.ivgPaths.swap(expanded);
	return true;
}

static bool ensureDirectoryPath(const NuXFiles::Path &directory) {
	if (directory.isNull()) {
		return true;
	}
	if (directory.exists()) {
		return directory.isDirectory();
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
		return false;
	}

	for (std::vector<NuXFiles::Path>::reverse_iterator it = toCreate.rbegin();
	     it != toCreate.rend(); ++it) {
		if (it->isRoot()) {
			continue;
		}
		if (!it->tryToCreate() && !(it->exists() && it->isDirectory())) {
			return false;
		}
	}

	return directory.exists() && directory.isDirectory();
}

static bool ensureDirectory(const std::string &path) {
	if (path.empty()) {
		return true;
	}
	return ensureDirectoryPath(pathFromNativeString(path));
}

static bool ensureParentDirectory(const std::string &filePath) {
	if (filePath.empty()) {
		return true;
	}
	const NuXFiles::Path target = pathFromNativeString(filePath);
	if (target.isNull() || target.isRoot()) {
		return true;
	}
	return ensureDirectoryPath(target.getParent());
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

static void logFileReport(const std::string &path, const SnapshotRunResult &run,
						  const CommandLineOptions &options) {
	const bool showDetails = (options.verbose || options.listOnly);

	// In concise mode, suppress noise for ignored and fully passing files.
	if (!showDetails) {
		if (run.ignored) {
			return;
		}
		if (!run.fileFailed && run.failedEntries == 0 && run.updatedEntries == 0) {
			return;
		}
	}

	std::cout << "# " << path << std::endl << std::endl;
	if (run.ignored) {
		std::string message("Ignored");
		if (!run.ignoreReason.empty()) {
			message += " (";
			message += run.ignoreReason;
			message += ')';
		}
		std::cout << "Summary" << std::endl;
		printSummaryLine("Status", message);
		std::cout << std::endl;
		return;
	}

    if (options.verbose || options.listOnly) {
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
	} else {
		// Concise mode: only show file-level failures and per-entry failures.
		bool anyPrinted = false;
		if (run.fileFailed && !run.fileError.empty()) {
			std::cout << "Error: " << run.fileError << std::endl;
			anyPrinted = true;
		}
		for (size_t i = 0; i < run.entries.size(); ++i) {
			const SnapshotEntryResult &entry = run.entries[i];
			if (entry.success) {
				continue;
			}
			const std::string scenarioName =
				(entry.scenarioName.empty() ? "(unnamed)" : entry.scenarioName);
			std::cout << "FAIL: scenario \"" << scenarioName << "\" entry "
					  << entry.entryOrdinal << " (block " << entry.blockIndex << ")";
			if (!entry.message.empty()) {
				std::cout << ": " << entry.message;
			}
			std::cout << std::endl;
			anyPrinted = true;
		}
		if (anyPrinted) {
			std::cout << std::endl;
		}
	}

	if (showDetails) {
		std::cout << "Summary" << std::endl;
		std::ostringstream snapshotLine;
		snapshotLine << run.totalEntries << " total (" << run.validatedEntries
				 << " validated)";
		printSummaryLine("Snapshots", snapshotLine.str());
		printSummaryLine("Updated", Interpreter::toString(static_cast<int32_t>(run.updatedEntries)));
		std::ostringstream failedLine;
		failedLine << run.failedEntries << " snapshots (diff failures: "
				 << run.diffFailures << ')';
		printSummaryLine("Failed", failedLine.str());
		std::cout << std::endl;
	}
}

// Backwards-compatible overload used by internal tests and any
// existing callers expecting the verbose, detailed report format.
static void logFileReport(const std::string &path, const SnapshotRunResult &run) {
	CommandLineOptions opts;
	opts.verbose = true;
	logFileReport(path, run, opts);
}

static void logTotalsSummary(const SnapshotTotals &totals) {
	std::cout << "# Overall Summary" << std::endl << std::endl;
	std::ostringstream processedLine;
	processedLine << totals.filesProcessed << " total (" << totals.failedFiles
			<< " files reported failures)";
	printSummaryLine("Processed files", processedLine.str());
	std::ostringstream snapshotLine;
	snapshotLine << totals.totalEntries << " total (" << totals.validatedEntries
				 << " validated)";
	printSummaryLine("Snapshots", snapshotLine.str());
	printSummaryLine("Updated", Interpreter::toString(static_cast<int32_t>(totals.updatedEntries)));
	std::ostringstream failedLine;
	failedLine << totals.failedEntries << " snapshots (diff failures: "
			<< totals.diffFailures << ')';
	printSummaryLine("Failed", failedLine.str());
	printSummaryLine("Ignored files", Interpreter::toString(static_cast<int32_t>(totals.ignoredFiles)));
}
static bool
loadPngRaster(const std::string &path,
			  NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &outRaster);
static bool writeRasterToPng(
	const std::string &path,
	const NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &raster,
	std::string &error);
class SnapshotGolden {
  public:
        SnapshotGolden(const std::string &ivgPath, const std::string &snapshotBase,
                                   const std::string &scenarioLabel,
                                   const CommandLineOptions &options) {
                initializePaths(ivgPath, snapshotBase, scenarioLabel, options);
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
		if (bounds.width <= 0 || bounds.height <= 0) {
			removeFileIfExists(oldPath);
			removeFileIfExists(actualPath);
			removeFileIfExists(diffPath);
			removeFileIfExists(goldenPath);
			return true;
		}

		if (!ensureParentDirectory(oldPath)) {
			result.success = false;
			result.message = std::string("failed to prepare directory for ") +
							 oldPath + ": " + std::strerror(errno);
			return false;
		}

		if (fileExists(goldenPath)) {
			std::string renameError;
			if (!renameFile(goldenPath, oldPath, renameError)) {
				result.success = false;
				result.message = renameError;
				return false;
			}
		}

		std::string error;
		if (!writeRasterToPng(oldPath, raster, error)) {
			result.success = false;
			result.message = error;
			return false;
		}
		removeFileIfExists(actualPath);
		removeFileIfExists(diffPath);
		removeFileIfExists(goldenPath);
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

		std::string error;
		if (!ensureParentDirectory(goldenPath)) {
			result.success = false;
			result.message = std::string("failed to prepare directory for ") +
							 goldenPath + ": " + std::strerror(errno);
			return false;
		}

		const bool goldenExists = fileExists(goldenPath);
		const bool oldExists = fileExists(oldPath);

		if (forceUpdate) {
			const NuXPixels::IntRect bounds = raster.calcBounds();
			if (bounds.width <= 0 || bounds.height <= 0) {
				removeFileIfExists(goldenPath);
				removeFileIfExists(oldPath);
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
			} else if (oldExists) {
				if (!renameFile(oldPath, backupPath, error)) {
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
			removeFileIfExists(oldPath);
			result.updated = true;
			result.success = true;
			return true;
		}

		if (!goldenExists) {
			if (!oldExists) {
				result.success = false;
				result.message = std::string("missing golden: ") + goldenPath +
								 " (no .old fallback present)";
				return false;
			}

			const NuXPixels::IntRect bounds = raster.calcBounds();
			if (bounds.width <= 0 || bounds.height <= 0) {
				removeFileIfExists(goldenPath);
				removeFileIfExists(oldPath);
				removeFileIfExists(actualPath);
				removeFileIfExists(diffPath);
				result.updated = true;
				result.success = true;
				result.message =
					std::string("promoted draft image to golden: ") +
					goldenPath + '.';
				return true;
			}

			if (!writeRasterToPng(goldenPath, raster, error)) {
				result.success = false;
				result.message = error;
				return false;
			}
			removeFileIfExists(oldPath);
			removeFileIfExists(actualPath);
			removeFileIfExists(diffPath);
			result.updated = true;
			result.success = true;
			result.message = std::string("promoted draft image to golden: ") +
							 goldenPath + '.';
			return true;
		}

		NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> goldenRaster;
		if (!loadPngRaster(goldenPath, goldenRaster)) {
			result.success = false;
			result.message =
				std::string("failed to read golden PNG: ") + goldenPath;
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
			removeFileIfExists(actualPath);
			removeFileIfExists(diffPath);
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
			removeFileIfExists(actualPath);
			removeFileIfExists(diffPath);
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
        std::string oldPath;
        std::string actualPath;
        std::string diffPath;
        std::string backupPath;

  private:
        void initializePaths(const std::string &ivgPath,
                                                 const std::string &snapshotBase,
                                                 const std::string &scenarioLabel,
                                                 const CommandLineOptions &options) {
                const std::string root =
                        (options.snapshotDir.empty() ? extractDirectory(ivgPath)
                                                                   : options.snapshotDir);
                const std::string scenarioName = sanitizeFileComponent(scenarioLabel);
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
};

static void PNGAPI snapshotPNGError(png_structp png, png_const_charp message) {
	throw std::runtime_error(std::string("Error reading PNG image: ") +
							 message);
}

static bool isLittleEndian() {
	static const unsigned char bytes[4] = {0x4A, 0x3B, 0x2C, 0x1D};
	return (*reinterpret_cast<const unsigned int *>(bytes) == 0x1D2C3B4A);
}

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

static bool
loadPngRaster(const std::string &path,
			  NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> &outRaster) {
	FILE *file = std::fopen(path.c_str(), "rb");
	if (file == 0) {
		return false;
	}

	png_structp png = 0;
	png_infop info = 0;
	try {
		png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, snapshotPNGError,
									 0);
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
		png_bytep *rows = png_get_rows(png, info);

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

		png_destroy_read_struct(&png, &info, 0);
		std::fclose(file);
		return true;
	} catch (...) {
		png_destroy_read_struct(&png, &info, 0);
		std::fclose(file);
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

	FILE *file = std::fopen(path.c_str(), "wb");
	if (file == 0) {
		error =
			std::string("failed to open ") + path + ": " + std::strerror(errno);
		return false;
	}

	png_structp png = 0;
	png_infop info = 0;

	try {
		png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0,
									  snapshotPNGError, 0);
		if (png == 0) {
			throw std::runtime_error("could not initialize PNG writer");
		}

		info = png_create_info_struct(png);
		if (info == 0) {
			throw std::runtime_error("could not initialize PNG info struct");
		}

		png_init_io(png, file);
		png_set_IHDR(png, info, static_cast<png_uint_32>(width),
					 static_cast<png_uint_32>(height), 8,
					 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
					 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_sRGB_gAMA_and_cHRM(png, info, PNG_sRGB_INTENT_ABSOLUTE);
		png_set_oFFs(png, info, static_cast<png_int_32>(bounds.left),
					 static_cast<png_int_32>(bounds.top), PNG_OFFSET_PIXEL);
		png_set_rows(png, info, &rows[0]);
		png_write_png(png, info, PNG_TRANSFORM_IDENTITY, 0);
		png_destroy_write_struct(&png, &info);
		std::fclose(file);
		return true;
	} catch (const std::exception &e) {
		error = std::string("failed to write PNG: ") + e.what();
	} catch (...) {
		error = "failed to write PNG: unknown error";
	}

	png_destroy_write_struct(&png, &info);
	std::fclose(file);
	return false;
}

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

// NOTE: Do not manually trim or strip brackets here; rely on IMPD APIs.

static bool parseValidateFlag(Interpreter &interpreter, const String *value) {
	if (value == 0) {
		return true;
	}
	return interpreter.toBool(*value);
}

static StringVector parseSnapshotStatements(Interpreter &interpreter,
												ArgumentsContainer &args) {
	// New grammar:
	// - Single body as positional arg #0; keep value verbatim (including brackets if present).
	// - List of bodies via label: list:[ [ ... ] [ ... ] ... ]

	const String *listArg = args.fetchOptional("list", false);
	if (listArg != 0) {
		// Parse labeled list; remove the outer list brackets via expand(),
		// but keep each element exactly as returned by parseList (including brackets).
		const String expandedOuter = interpreter.expand(StringRange(*listArg));
		StringVector elements;
		interpreter.parseList(StringRange(expandedOuter), elements, false, false, 1, INT_MAX);
		return elements;
	}

	// Expect exactly one positional argument (verbatim; may be bracketed or raw)
	return StringVector(1, args.fetchRequired(0, false));
}

static bool readFile(const std::string &path, String &contents);

class SnapshotPlaybackExecutor : public IVG::IVGExecutor {
  public:
	SnapshotPlaybackExecutor(IVG::Canvas &canvas,
                                                         const CommandLineOptions &options,
                                                         const std::string &sourcePath,
                                                         const String &sourceText,
                                                         SharedResources &sharedResources,
                                                         SnapshotRoundState &roundState,
                                                         SnapshotProgress &snapshotProgress)
                : IVG::IVGExecutor(canvas), includeDirs(options.includeDirs),
                  fontDirs(options.fontDirs), imageDirs(options.imageDirs),
                  sourcePath(sourcePath), verbose(options.verbose),
                  sharedResources(sharedResources), round(&roundState),
                  progress(&snapshotProgress), sourceText(sourceText),
                  scanOffset(0) {}

	bool load(Interpreter &interpreter, const WideString &filename,
                          String &contents) {
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

	std::vector<const IVG::Font *>
	lookupFonts(Interpreter &interpreter, const WideString &fontName,
								const UniString &forString) {
		(void)interpreter;
		(void)forString;

		{
			NuXThreads::MutexLock lock(sharedResources.fontMutex);
			const std::map<WideString, IVG::Font>::iterator cached =
				sharedResources.fonts.find(fontName);
			if (cached != sharedResources.fonts.end()) {
				return std::vector<const IVG::Font *>(1, &cached->second);
			}
		}

		IVG::Font font;
		if (!loadExternalFont(fontName, font)) {
			return std::vector<const IVG::Font *>();
		}

		NuXThreads::MutexLock lock(sharedResources.fontMutex);
		const std::map<WideString, IVG::Font>::iterator cached =
			sharedResources.fonts.find(fontName);
		if (cached != sharedResources.fonts.end()) {
			return std::vector<const IVG::Font *>(1, &cached->second);
		}

		const std::map<WideString, IVG::Font>::iterator inserted =
			sharedResources.fonts.insert(std::make_pair(fontName, font)).first;
		return std::vector<const IVG::Font *>(1, &inserted->second);
	}

	IVG::Image loadImage(Interpreter &interpreter,
								const WideString &imageSource,
								const NuXPixels::IntRect *sourceRectangle,
								bool forStretching, double forXSize,
								bool xSizeIsRelative, double forYSize,
								bool ySizeIsRelative) {
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
                          const String &arguments) {
                static const String SNAPSHOT_KEY("snapshot-1");
                if (key != SNAPSHOT_KEY) {
                        return false;
                }

                if (round == 0 || progress == 0) {
                        Interpreter::throwBadSyntax(
                                "snapshot playback executor missing round context.");
                }

                ArgumentsContainer args(
                        ArgumentsContainer::parse(interpreter, StringRange(arguments)));
                return handleRoundMeta(interpreter, args);
        }

        bool finished() const { return true; }

  private:

        bool handleRoundMeta(Interpreter &interpreter, ArgumentsContainer &args) {
                const String *validateFlag = args.fetchOptional("validate");
                const bool blockValidate =
                        parseValidateFlag(interpreter, validateFlag);
                const String *scenarioLabel = args.fetchOptional("scenario");
                const bool explicitLabel = (scenarioLabel != 0);

                const uint32_t blockOrdinal = round->blockOrdinalCursor + 1;
                round->advanceBlockCursor();

                StringVector statements = parseSnapshotStatements(interpreter, args);
                args.throwIfAnyUnfetched();

                const uint32_t sourceLine = locateRoundMetaLine();
                const bool multipleEntries = (statements.size() > 1);
                const uint32_t entryCount = static_cast<uint32_t>(statements.size());

                bool sawPinnedEntry = false;
                bool executedPinnedEntry = false;

                for (uint32_t i = 0; i < statements.size(); ++i) {
                        const uint32_t entryOrdinal = i + 1;
                        const uint32_t scenarioOrdinal =
                                (explicitLabel ? entryOrdinal : 1);
                        const String scenarioName =
                                (explicitLabel
                                         ? *scenarioLabel
                                         : buildImplicitScenarioName(blockOrdinal,
                                                 entryOrdinal, multipleEntries));

			progress->ensureDisplayLabel(scenarioName, explicitLabel,
scenarioLabel, blockOrdinal, entryOrdinal, scenarioOrdinal,
			entryCount, (i == 0));

                        const bool shouldExecute = progress->observeScenarioEntry(
                                *round, scenarioName, explicitLabel, blockValidate,
                                entryOrdinal);

                        const bool matchesPinned = round->matchesSelection(
                                scenarioName, scenarioOrdinal);
                        if (matchesPinned) {
                                sawPinnedEntry = true;
                        }

                        if (!shouldExecute) {
                                continue;
                        }

                        if (!matchesPinned) {
                                Interpreter::throwBadSyntax(
                                        "snapshot round selection changed mid-execution.");
                        }

                        executedPinnedEntry = true;
                        if (round->firstSourceLine == 0) {
                                round->firstSourceLine = sourceLine;
                        }

                        SnapshotInvocation *existing =
                                round->findInvocation(blockOrdinal, entryOrdinal);
                        const String &statementBody = statements[i];
                        if (existing != 0) {
                                if (existing->statements != statementBody) {
                                        Interpreter::throwBadSyntax(
                                                "snapshot statements changed within iterative round.");
                                }
                        } else {
                                round->recordInvocation(blockOrdinal, sourceLine,
                                        entryOrdinal, statementBody);
                        }

                        if (verbose) {
                                std::cout << sourcePath << ": scenario "
                                                  << round->scenario << " entry "
                                                  << round->entryOrdinal << " block "
                                                  << blockOrdinal << " (block entry "
                                                  << entryOrdinal << ")" << std::endl;
                        }

                        IVG::Context invocationContext(
                                currentContext->accessCanvas(), *currentContext);
                        runInNewContext(interpreter, invocationContext, statementBody);
                }

                if (sawPinnedEntry && !executedPinnedEntry) {
                        Interpreter::throwBadSyntax(
                                "selected snapshot entry missing from scenario block.");
                }

                return true;
        }

        String buildImplicitScenarioName(uint32_t blockOrdinal,
                                                                  uint32_t entryOrdinal,
                                                                  bool multipleEntries) const {
                String name("implicit-");
                name += Interpreter::toString(
                        static_cast<int32_t>(blockOrdinal));
                if (multipleEntries) {
                        name += '-';
                        name += Interpreter::toString(
                                static_cast<int32_t>(entryOrdinal));
                }
                return name;
        }

        uint32_t locateRoundMetaLine() {
                if (sourceText.empty()) {
                        return 0;
                }

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

        std::string resolveRelativePath(const std::string &requested) const {
                const size_t slash = sourcePath.find_last_of("/\\");
                if (slash == std::string::npos) {
                        return requested;
                }
		return sourcePath.substr(0, slash + 1) + requested;
	}

        const std::vector<std::string> &includeDirs;
        const std::vector<std::string> &fontDirs;
        const std::vector<std::string> &imageDirs;
        std::string sourcePath;
        bool verbose;
        SharedResources &sharedResources;
        SnapshotRoundState *round;
        SnapshotProgress *progress;
        String sourceText;
        size_t scanOffset;

	bool loadExternalFont(const WideString &fontName, IVG::Font &font) {
		const std::string fontName8(fontName.begin(), fontName.end());
		const std::string fileName = fontName8 + ".ivgfont";

		String contents;
		if (loadBuiltInFont(fontName8, contents)) {
			return parseFont(contents, font);
		}
		if (readFile(resolveRelativePath(fileName), contents) ||
				loadFromDirectories(fontDirs, fileName, contents)) {
			return parseFont(contents, font);
		}
		return false;
	}

	bool loadFromDirectories(const std::vector<std::string> &dirs,
							 const std::string &name, String &contents) const {
		for (size_t i = 0; i < dirs.size(); ++i) {
			if (readFile(dirs[i] + "/" + name, contents)) {
				return true;
			}
		}
		return false;
	}

	bool loadBuiltInFont(const std::string &fontName, String &contents) const {
		const std::string normalized = toLowerAscii(fontName);
		const IVGSnapshotBuiltInFonts::FontEntry *entry =
			IVGSnapshotBuiltInFonts::find(normalized);
		if (entry == 0) {
			return false;
		}
		contents.assign(entry->source, entry->length);
		return true;
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
		const std::string local = resolveRelativePath(requested);
		const CachedImage *cached = loadImageFromPath(local);
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

	const CachedImage *loadImageFromPath(const std::string &path) {
		{
			NuXThreads::MutexLock lock(sharedResources.imageMutex);
			const std::map<std::string, CachedImage>::iterator it =
				sharedResources.images.find(path);
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
		const std::map<std::string, CachedImage>::iterator existing =
			sharedResources.images.find(path);
		if (existing != sharedResources.images.end()) {
			return &existing->second;
		}

		const std::map<std::string, CachedImage>::iterator inserted =
			sharedResources.images.insert(std::make_pair(path, cached)).first;
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
	std::cout << "\t--recursive\t\tScan directories recursively for IVG files."
			  << std::endl;
	std::cout << "\t--force-update\t\tOverwrite goldens." << std::endl;
	std::cout << "\t--threads <n>\t\tNumber of worker threads." << std::endl;
	std::cout << "\t--list-only\t\tList collected snapshots without rendering."
			  << std::endl;
	std::cout << "\t--verbose\t\tPrint verbose diagnostics." << std::endl;
	std::cout << "\t--exit-on-first-failure\tAbort after first failure."
				  << std::endl;
	std::cout << "\t--no-golden-audit\tDisable orphan golden PNG audit."
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

static bool parseCommandLine(int argc, char **argv,
							 CommandLineOptions &options) {
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
		} else if (arg == "--snapshot-dir") {
			if (i + 1 >= argc) {
				std::cerr << "--snapshot-dir requires a path." << std::endl;
				return false;
			}
			options.snapshotDir = argv[++i];
		} else if (arg == "--root-dir") {
			if (i + 1 >= argc) {
				std::cerr << "--root-dir requires a path." << std::endl;
				return false;
			}
			const std::string rootArgument(argv[++i]);
			options.rootDir = pathFromNativeString(rootArgument);
			if (options.rootDir.isNull()) {
				std::cerr << "failed to parse root directory: " << rootArgument
						<< std::endl;
				return false;
			}
		} else if (arg == "--recursive") {
			options.recursive = true;
		} else if (arg == "--force-update") {
			options.forceUpdate = true;
		} else if (arg == "--threads") {
			if (i + 1 >= argc) {
				std::cerr << "--threads requires a numeric value." << std::endl;
				return false;
			}
			uint32_t threads = 0;
			if (!parseUnsigned(argv[i + 1], threads)) {
				std::cerr << "invalid thread count: " << argv[i + 1]
						  << std::endl;
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
			} else if (arg == "--no-golden-audit") {
				options.goldenAudit = false;
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

static void printScenarioListing(const std::string &path,
                                                            const SnapshotProgress &progress) {
        static const char scenarioIndent[] = "  ";
        static const char entryIndent[] = "    ";
        static const char blockIndent[] = "      ";
        static const char snippetIndent[] = "        ";

        std::cout << path << std::endl;
        const std::vector<SeenScenario> &scenarios = progress.getSeenScenarios();
        for (size_t i = 0; i < scenarios.size(); ++i) {
                const SeenScenario &scenario = scenarios[i];
                std::cout << scenarioIndent << "Scenario "
                          << stringFromIMPD(scenario.name)
                          << " (validate: " << (scenario.validate ? "yes" : "no")
                          << ")" << std::endl;
                for (uint32_t ordinal = 1; ordinal <= scenario.maxOrdinal; ++ordinal) {
                        if (!scenario.isProcessed(ordinal)) {
                                continue;
                        }

                        std::cout << entryIndent << "Scenario entry #" << ordinal
                                  << std::endl;
                        const ScenarioEntryMetadata *metadata =
                                scenario.getEntryMetadata(ordinal);
                        if (metadata == 0) {
                                continue;
                        }

                        for (size_t k = 0; k < metadata->invocations.size(); ++k) {
                                const SnapshotInvocation &invocation =
                                        metadata->invocations[k];
                                std::cout << blockIndent << "Snapshot block #"
                                                  << invocation.blockIndex
                                                  << " (block entry #"
                                                  << invocation.entryOrdinal
                                                  << ", source line "
                                                  << invocation.sourceLine << ")"
                                                  << std::endl;

                                std::istringstream snippet(
                                        stringFromIMPD(invocation.statements));
                                std::string line;
                                while (std::getline(snippet, line)) {
                                        std::cout << snippetIndent << line << std::endl;
                                }
                        }
                }
        }
}

static SnapshotRunResult processFileIterative(const CommandLineOptions &options,
											const std::string &path) {
		SnapshotRunResult run;
		CachedDocument document;
    if (!document.loadFromFile(path)) {
        run.fileFailed = true;
        run.exitCode = 1;
        run.fileError = "failed to read IVG file";
        if (options.verbose || options.listOnly) {
            std::cerr << "failed to read IVG file: " << path << std::endl;
        }
        return run;
    }

	const String &source = document.getSource();
		const FormatDetectionResult formatDetection = detectSnapshotFormat(source);
		if (formatDetection.determined && !formatDetection.supportsSnapshot) {
			run.ignored = true;
			run.ignoreReason = "format missing uses:snapshot-1";
			return run;
		}
		const std::string snapshotBase = buildSnapshotSourceTag(path, options.rootDir);
		const bool goldensExist = hasAnyGoldensForSource(path, snapshotBase, options);
		SharedResources sharedResources;
		SnapshotRoundCoordinator coordinator;
		SnapshotProgress &progress = coordinator.accessProgress();

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

	bool stopAfterFailure = false;
	while (!stopAfterFailure) {
		SnapshotRoundState round = coordinator.beginRound();
		IVG::SelfContainedARGB32Canvas canvas;
		SnapshotPlaybackExecutor executor(canvas, options, path, source,
										sharedResources, round, progress);

		bool executionFailed = false;
		std::string executionError;

		try {
			document.render(executor);
		} catch (Exception &e) {
			std::ostringstream message;
			message << e.getError();
			if (e.hasStatement()) {
				message << " near \"" << e.getStatement() << "\"";
			}
			executionFailed = true;
			executionError = message.str();
		} catch (std::exception &e) {
			executionFailed = true;
			executionError = e.what();
		}

            if (!round.hasPinned) {
                // If the format declares snapshot support, or if goldens already exist
                // for this source, then a failure before any snapshot meta executes
                // should be treated as a hard error (not ignored).
                if (formatDetection.supportsSnapshot || goldensExist) {
                    run.fileFailed = true;
                    run.exitCode = 1;
                    if (executionFailed) {
                        run.fileError = executionError;
                        if (options.verbose || options.listOnly) {
                            std::cerr << path << ": " << executionError << std::endl;
                        }
                    } else {
                        run.fileError = "no snapshots executed";
                        if (options.verbose || options.listOnly) {
                            std::cerr << path << ": expected snapshots but none executed." << std::endl;
                        }
                    }
                    coordinator.completeRound(round);
                    break;
                }

                // Otherwise keep current behavior: ignore files with no snapshots executed.
                if (executionFailed && (options.verbose || options.listOnly)) {
                    std::cerr << path << ": " << executionError
                              << " (ignored: no snapshots executed)." << std::endl;
                }
                coordinator.completeRound(round);
                break;
            }

		SnapshotEntryResult result;
		result.ivgPath = path;
		const std::string &registeredLabel =
		        progress.lookupDisplayLabel(round.scenario, round.entryOrdinal);
		const std::string effectiveLabel =
		        (registeredLabel.empty() ? stringFromIMPD(round.scenario)
		                               : registeredLabel);

		result.scenarioName = effectiveLabel;
		result.entryOrdinal = round.entryOrdinal;
		result.validate = round.validate;
		result.planOrdinal = static_cast<uint32_t>(run.entries.size());
		result.blockIndex =
		        (round.invocations.empty() ? 0 : round.invocations[0].blockIndex);
		result.identifier = buildEntryIdentifier(snapshotBase, effectiveLabel,
		        result.blockIndex, round.entryOrdinal);

		if (executionFailed) {
			result.message = executionError;
			result.success = false;
			run.fileFailed = true;
			run.exitCode = 1;
			run.fileError = executionError;
				if (options.verbose || options.listOnly) {
					std::cerr << path << ": scenario " << result.scenarioName
					                  << ": " << result.message << std::endl;
				}
		} else {
			NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> *raster =
					canvas.accessRaster();
			if (!executor.finished()) {
				result.message = "did not execute all snapshot invocations";
				result.success = false;
					if (options.verbose || options.listOnly) {
						std::cerr << path << ": scenario " << result.scenarioName
						                  << " did not execute all snapshot invocations."
						                  << std::endl;
					}
                } else if (raster == 0) {
                    result.message = "rendered image is empty";
                    result.success = false;
                    if (options.verbose || options.listOnly) {
                        std::cerr << path << ": scenario " << result.scenarioName
                                      << " produced no raster output." << std::endl;
                    }
                } else if (options.listOnly) {
				result.rendered = false;
				result.skipped = true;
				result.success = true;
} else {
result.rendered = true;
SnapshotGolden golden(path, snapshotBase, effectiveLabel,
options);
				if (!round.validate) {
                        if (!golden.writeDraft(*raster, result)) {
                            if (result.message.empty()) {
                                result.message = "failed to write draft";
                            }
                            if (options.verbose || options.listOnly) {
                                std::cerr << path << ": scenario "
                                                  << result.scenarioName << ": "
                                                  << result.message << std::endl;
                            }
                        }
                    } else if (!golden.validate(*raster, options.forceUpdate, result)) {
                        if (result.message.empty()) {
                            result.message = "validation failed";
                        }
                        if (options.verbose || options.listOnly) {
                            std::cerr << path << ": scenario " << result.scenarioName
                                              << ": " << result.message << std::endl;
                        }
                    }
                }
		}

		if (!result.success) {
			run.failedEntries++;
			if (result.diffed) {
				run.diffFailures++;
			}
			run.exitCode = 1;
			if (options.exitOnFirstFailure) {
				stopAfterFailure = true;
			}
		}

		if (result.validate) {
			++run.validatedEntries;
		} else {
			++run.draftEntries;
		}
		if (result.updated) {
			++run.updatedEntries;
		}
		++run.totalEntries;

                run.entries.push_back(result);

                progress.recordRoundDetails(round);
                coordinator.completeRound(round);
                if (!stopAfterFailure && !coordinator.needsAnotherRound(round)) {
                        break;
                }
        }

        if (options.listOnly || options.verbose) {
                printScenarioListing(path, progress);
        }

        return run;
}

static SnapshotRunResult processFile(const CommandLineOptions &options,
                                                                         const std::string &path) {
        SnapshotRunResult run = processFileIterative(options, path);
        if (run.exitCode == 0 && run.failedEntries > 0) {
                run.exitCode = 1;
        }
        return run;
}

static uint32_t determineThreadCount(const CommandLineOptions &options,
                                                                        size_t jobCount) {
        if (jobCount == 0) {
                return 1;
        }

        if (options.exitOnFirstFailure) {
                return 1;
        }

        uint32_t threads = options.threads;
        if (threads == 0) {
                const unsigned int hardware = std::thread::hardware_concurrency();
                threads = (hardware > 0 ? hardware : 1);
        }

        if (threads == 0) {
                threads = 1;
        }

        if (threads > jobCount) {
                threads = static_cast<uint32_t>(jobCount);
        }

        return (threads == 0 ? 1 : threads);
}

#if !defined(IVG_SNAPSHOT_TESTING)

int main(int argc, char **argv) {
	CommandLineOptions options;
	if (!parseCommandLine(argc, argv, options)) {
		return 1;
	}

	if (!expandInputPaths(options)) {
		return 1;
	}
	if (options.ivgPaths.empty()) {
		std::cerr << "no IVG files found." << std::endl;
		return 1;
	}

        SnapshotTotals totals;
        int exitCode = 0;
        const size_t fileCount = options.ivgPaths.size();
        const uint32_t threadCount = determineThreadCount(options, fileCount);

	std::vector<SnapshotRunResult> runs(fileCount);
	std::vector<uint8_t> processed(fileCount, 0);
	std::atomic<bool> stop(false);

	struct SnapshotLoop {
		SnapshotLoop(const CommandLineOptions &optionsRef,
								std::vector<SnapshotRunResult> &runsRef,
								std::vector<uint8_t> &processedRef,
								std::atomic<bool> &stopRef)
			: options(optionsRef), runs(runsRef), processed(processedRef),
			  stop(stopRef) {}

		bool operator()(int index, int iterationCount, int threadIndex) const {
			(void)iterationCount;
			(void)threadIndex;

			if (stop.load()) {
				return false;
			}

			const size_t fileIndex = static_cast<size_t>(index);
			if (fileIndex >= options.ivgPaths.size()) {
				return false;
			}

			const std::string &path = options.ivgPaths[fileIndex];
			SnapshotRunResult run;
			bool shouldStop = false;

			try {
				run = processFile(options, path);
				if (options.exitOnFirstFailure &&
						(run.exitCode != 0 || run.fileFailed)) {
					shouldStop = true;
				}
                } catch (Exception &e) {
                    std::ostringstream message;
                    message << path << ": " << e.getError();
                    if (e.hasStatement()) {
                        message << " near \"" << e.getStatement() << "\"";
                    }
                    run.fileFailed = true;
                    run.exitCode = 1;
                    run.fileError = message.str();
                    if (options.verbose || options.listOnly) {
                        std::cerr << message.str() << std::endl;
                    }
                    shouldStop = options.exitOnFirstFailure;
                } catch (std::exception &e) {
                    run.fileFailed = true;
                    run.exitCode = 1;
                    run.fileError = e.what();
                    if (options.verbose || options.listOnly) {
                        std::cerr << path << ": " << e.what() << std::endl;
                    }
                    shouldStop = options.exitOnFirstFailure;
                } catch (...) {
                    run.fileFailed = true;
                    run.exitCode = 1;
                    run.fileError = "unknown exception";
                    if (options.verbose || options.listOnly) {
                        std::cerr << path << ": unknown exception" << std::endl;
                    }
                    shouldStop = options.exitOnFirstFailure;
                }

			runs[fileIndex] = run;
			processed[fileIndex] = 1;

			if (shouldStop) {
				stop.store(true);
				return false;
			}

			return true;
		}

		const CommandLineOptions &options;
		std::vector<SnapshotRunResult> &runs;
		std::vector<uint8_t> &processed;
		std::atomic<bool> &stop;
	};

	SnapshotLoop loop(options, runs, processed, stop);

	if (fileCount > 0) {
		NuXThreads::runLoopInParallel(static_cast<int>(fileCount), loop,
				static_cast<int>(threadCount));
	}

		for (size_t i = 0; i < fileCount; ++i) {
			if (!processed[i]) {
				continue;
			}

		const std::string &path = options.ivgPaths[i];
		SnapshotRunResult &run = runs[i];
		totals.accumulate(run);
			logFileReport(path, run, options);
			if (run.exitCode != 0 || run.fileFailed) {
				if (exitCode == 0) {
					exitCode = (run.exitCode != 0 ? run.exitCode : 1);
				}
				if (options.exitOnFirstFailure) {
					break;
				}
			}
		}

		// Global golden audit: ensure that any golden PNG present under the
		// snapshot directories correspond to at least one IVG source included
		// in this run (by matching the sanitized snapshot base).
		if (!options.listOnly && options.goldenAudit) {
			std::set<std::string> processedBases;
			processedBases.clear();
			for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
				const std::string &ivgPath = options.ivgPaths[i];
				processedBases.insert(buildSnapshotSourceTag(ivgPath, options.rootDir));
			}

			std::set<std::string> auditRoots;
			if (!options.snapshotDir.empty()) {
				auditRoots.insert(options.snapshotDir);
			} else {
				for (size_t i = 0; i < options.ivgPaths.size(); ++i) {
					auditRoots.insert(extractDirectory(options.ivgPaths[i]));
				}
			}

			std::vector<std::string> orphanGoldens;
			for (std::set<std::string>::const_iterator it = auditRoots.begin();
				 it != auditRoots.end(); ++it) {
				const std::string &root = *it;
				if (root.empty()) {
					continue;
				}
				const std::string wildcard = joinPath(root, std::string("*__*.png"));
				try {
					std::vector<NuXFiles::Path> matches;
					NuXFiles::Path::findPaths(matches, pathStringToWide(wildcard));
					for (size_t k = 0; k < matches.size(); ++k) {
						const NuXFiles::Path &p = matches[k];
						const std::wstring name = p.getName(); // without extension
						std::string narrow = pathStringFromWide(name);
						const size_t sep = narrow.find("__");
						if (sep == std::string::npos) {
							continue;
						}
						const std::string base = narrow.substr(0, sep);
						if (processedBases.find(base) == processedBases.end()) {
							orphanGoldens.push_back(pathStringFromWide(p.getFullPath()));
						}
					}
				} catch (...) {
					// Ignore listing errors; best-effort audit only.
				}
			}

			if (!orphanGoldens.empty()) {
				for (size_t i = 0; i < orphanGoldens.size(); ++i) {
					std::cerr << "orphan golden PNG without IVG in run: "
					          << orphanGoldens[i] << std::endl;
				}
				if (exitCode == 0) {
					exitCode = 1;
				}
			}
		}

	        logTotalsSummary(totals);
	        return exitCode;
}

#endif // !defined(IVG_SNAPSHOT_TESTING)

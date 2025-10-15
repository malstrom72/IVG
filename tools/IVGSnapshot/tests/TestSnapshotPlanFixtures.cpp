#define IVG_SNAPSHOT_TESTING 1
#include "../IVGSnapshot.cpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
struct Fixture {
	const char *label;
	const char *path;
	const char *source;
};

class PlanBuilder {
  public:
	PlanBuilder(const Fixture &fixture) : fixture(fixture), plan(fixture.path) {
		build();
	}

	const SnapshotPlan &getPlan() const { return plan; }

  private:
	void build() {
		const String text(fixture.source);
		plan.beginCollection();
		while (true) {
			SnapshotCollector collector(plan, fixture.path, text, includeDirs);
			STLMapVariables variables;
			FormatInfo formatInfo;
			Interpreter interpreter(collector, variables, formatInfo);

			try {
				interpreter.run(StringRange(text));
			} catch (Exception &e) {
				std::ostringstream stream;
				stream << "fixture " << fixture.label
					   << " failed: " << e.getError();
				if (e.hasStatement()) {
					stream << " near '" << e.getStatement() << "'";
				}
				fail(stream.str());
			} catch (std::exception &e) {
				std::ostringstream stream;
				stream << "fixture " << fixture.label
					   << " threw std::exception: " << e.what();
				fail(stream.str());
			}

			plan.completeCollectionPass();
			if (!plan.prepareNextCollectionPass()) {
				break;
			}
		}
	}

	static void fail(const std::string &message) {
		std::cerr << "TestSnapshotPlanFixtures: " << message << std::endl;
		std::exit(1);
	}

	const Fixture &fixture;
	SnapshotPlan plan;
	std::vector<std::string> includeDirs;
};

void appendEscaped(const std::string &value, std::ostream &stream) {
	for (size_t i = 0; i < value.size(); ++i) {
		const char c = value[i];
		switch (c) {
		case '\\':
		case '"':
			stream << '\\' << c;
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
			if (static_cast<unsigned char>(c) < 0x20u) {
				static const char HEX[] = "0123456789abcdef";
				stream << "\\u00" << HEX[(c >> 4) & 0x0F] << HEX[c & 0x0F];
			} else {
				stream << c;
			}
			break;
		}
	}
}

std::string scenarioName(const SnapshotScenario &scenario) {
	return stringFromIMPD(scenario.name);
}

std::string entryName(const SnapshotEntry &entry) {
	return stringFromIMPD(entry.scenarioName);
}

struct ScenarioMetrics {
	std::string name;
	uint32_t entryCount;
	uint32_t invocationCount;
	uint32_t validated;
};

ScenarioMetrics summarizeScenario(const SnapshotScenario &scenario,
								  const std::vector<SnapshotEntry> &entries) {
	ScenarioMetrics metrics;
	metrics.name = scenarioName(scenario);
	metrics.entryCount = static_cast<uint32_t>(scenario.entryIndices.size());
	metrics.invocationCount = 0;
	metrics.validated = 0;
	for (size_t i = 0; i < scenario.entryIndices.size(); ++i) {
		const uint32_t index = scenario.entryIndices[i];
		if (index >= entries.size()) {
			continue;
		}
		const SnapshotEntry &entry = entries[index];
		metrics.invocationCount +=
			static_cast<uint32_t>(entry.invocations.size());
		if (entry.validate) {
			++metrics.validated;
		}
	}
	return metrics;
}

void emitReplayOrder(const SnapshotPlan &plan, std::ostream &stream) {
	const std::vector<SnapshotScenario> &scenarios = plan.getScenarios();
	const std::vector<SnapshotEntry> &entries = plan.getEntries();
	bool first = true;
	stream << "[";
	for (size_t i = 0; i < scenarios.size(); ++i) {
		const SnapshotScenario &scenario = scenarios[i];
		for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
			const uint32_t entryIndex = scenario.entryIndices[j];
			if (entryIndex >= entries.size()) {
				continue;
			}
			const SnapshotEntry &entry = entries[entryIndex];
			if (!first) {
				stream << ',';
			}
			first = false;
			stream << "{\"scenario\":\"";
			appendEscaped(entryName(entry), stream);
			stream << "\",\"ordinal\":";
			stream << entry.entryOrdinal;
			stream << "}";
		}
	}
	stream << "]";
}
} // namespace

int main() {
	const Fixture fixtures[] = {
		{"multi-scenario", "fixtures/primary.ivg",
		 "format snapshot uses:[snapshot-1]\n"
		 "meta snapshot scenario:primary [ [ color=#FF0000 ], [ color=#00FF00 "
		 "] ]\n"
		 "meta snapshot scenario:alternate validate:no [ [ color=#0000FF ], [ "
		 "color=#00FFFF ] ]\n"
		 "meta snapshot [ [ color=#111111 ], [ color=#222222 ], [ "
		 "color=#333333 ] ]\n"
		 "meta snapshot scenario:primary [ [ color=#FF9900 ], [ color=#FF5500 "
		 "] ]\n"
		 "FILL $color\nRECT 0,0,10,10\n"},
		{"implicit-only", "fixtures/implicit.ivg",
		 "format snapshot uses:[snapshot-1]\n"
		 "meta snapshot [ [ set fill red ], [ set stroke blue ] ]\n"
		 "meta snapshot [ highlight=#CCCCCC ]\n"
		 "FILL red\nRECT 0,0,5,5\n"}};

	std::cout << "{\n\t\"fixtures\": [\n";
	for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
		const Fixture &fixture = fixtures[i];
		PlanBuilder builder(fixture);
		const SnapshotPlan &plan = builder.getPlan();
		const std::vector<SnapshotScenario> &scenarios = plan.getScenarios();
		const std::vector<SnapshotEntry> &entries = plan.getEntries();
		std::cout << "\t\t{\n";
		std::cout << "\t\t\t\"label\": \"";
		appendEscaped(fixture.label, std::cout);
		std::cout << "\",\n";
		std::cout << "\t\t\t\"base\": \"";
		appendEscaped(stringFromIMPD(plan.getBaseName()), std::cout);
		std::cout << "\",\n";
		std::cout << "\t\t\t\"scenarioCount\": "
				  << static_cast<uint32_t>(scenarios.size()) << ",\n";
		std::cout << "\t\t\t\"entryCount\": "
				  << static_cast<uint32_t>(entries.size()) << ",\n";
		std::cout << "\t\t\t\"scenarios\": [";
		for (size_t s = 0; s < scenarios.size(); ++s) {
			if (s != 0) {
				std::cout << ',';
			}
			ScenarioMetrics metrics = summarizeScenario(scenarios[s], entries);
			std::cout << "{\"name\":\"";
			appendEscaped(metrics.name, std::cout);
			std::cout << "\",\"entries\":" << metrics.entryCount;
			std::cout << ",\"invocations\":" << metrics.invocationCount;
			std::cout << ",\"validated\":" << metrics.validated << "}";
		}
		std::cout << "],\n";
		std::cout << "\t\t\t\"replayOrder\": ";
		emitReplayOrder(plan, std::cout);
		std::cout << "\n\t\t}";
		if (i + 1 < sizeof(fixtures) / sizeof(fixtures[0])) {
			std::cout << ',';
		}
		std::cout << '\n';
	}
	std::cout << "\t]\n}" << std::endl;
	return 0;
}

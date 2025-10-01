#define IVG_SNAPSHOT_TESTING 1
#include "../IVGSnapshot.cpp"

#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace IVGSnapshotInternal;

namespace {

void Fail(const std::string& message)
{
	std::cerr << "TestSnapshotPlan: " << message << std::endl;
	std::exit(1);
}

void Expect(bool condition, const std::string& message)
{
	if (!condition) {
		Fail(message);
	}
}

void ExpectEqual(uint32_t actual, uint32_t expected, const std::string& label)
{
	if (actual != expected) {
		std::ostringstream stream;
		stream << label << " expected " << expected << " but got " << actual;
		Fail(stream.str());
	}
}

void ExpectEqual(const String& actual, const std::string& expected, const std::string& label)
{
	if (actual != expected) {
		std::ostringstream stream;
		stream << label << " expected '" << expected << "' but got '" << actual << "'";
		Fail(stream.str());
	}
}

SnapshotPlan CollectPlan(const std::string& path, const char* source)
{
	String text(source);
	SnapshotPlan plan(path);
	std::vector<std::string> includeDirs;
	SnapshotCollector collector(plan, path, text, includeDirs);
	STLMapVariables variables;
	FormatInfo formatInfo;
	formatInfo.formatId = "meta";
	formatInfo.uses.insert("snapshot-1");
	Interpreter interpreter(collector, variables, formatInfo);

	try {
		interpreter.run(StringRange(text));
	} catch (Exception& e) {
		std::ostringstream stream;
		stream << "unexpected exception: " << e.getError();
		if (e.hasStatement()) {
			stream << " near '" << e.getStatement() << "'";
		}
		Fail(stream.str());
	}

	return plan;
}

void TestExplicitScenario()
{
	const char* source =
		"meta snapshot scenario:one [ set fill red ]\n";
	SnapshotPlan plan = CollectPlan("explicit.ivg", source);

	const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
	const std::vector<SnapshotEntry>& entries = plan.getEntries();

	ExpectEqual(static_cast<uint32_t>(scenarios.size()), 1, "scenario count");
	ExpectEqual(static_cast<uint32_t>(entries.size()), 1, "entry count");

	const SnapshotScenario& scenario = scenarios[0];
	ExpectEqual(scenario.name, "one", "scenario name");
	Expect(scenario.validate, "scenario validate flag should default to true");
	ExpectEqual(static_cast<uint32_t>(scenario.entryIndices.size()), 1, "scenario entry count");

	const SnapshotEntry& entry = entries[scenario.entryIndices[0]];
	ExpectEqual(entry.blockIndex, 1, "block index");
	ExpectEqual(entry.entryIndex, 1, "entry index");
	ExpectEqual(entry.sourceLine, 1, "source line");
	Expect(entry.validate, "entry validate flag");
	ExpectEqual(entry.scenarioName, "one", "entry scenario name");
	ExpectEqual(entry.statements, " set fill red ", "entry statements preserve whitespace");
}

void TestArrayStatements()
{
	const char* source =
		"meta snapshot scenario:array [ [ do-alpha ], [ do-beta ], [ do-gamma ] ]\n";
	SnapshotPlan plan = CollectPlan("array.ivg", source);

	const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
	const std::vector<SnapshotEntry>& entries = plan.getEntries();

	ExpectEqual(static_cast<uint32_t>(scenarios.size()), 1, "scenario count");
	ExpectEqual(static_cast<uint32_t>(entries.size()), 3, "entry count");

	const SnapshotScenario& scenario = scenarios[0];
	ExpectEqual(static_cast<uint32_t>(scenario.entryIndices.size()), 3, "scenario entry count");

	for (uint32_t i = 0; i < 3; ++i) {
		const SnapshotEntry& entry = entries[scenario.entryIndices[i]];
		ExpectEqual(entry.blockIndex, 1, "array block index");
		ExpectEqual(entry.entryIndex, i + 1, "array entry ordinal");
		ExpectEqual(entry.sourceLine, 1, "array source line");
		ExpectEqual(entry.scenarioName, "array", "array scenario name");
		const std::string expected = (i == 0 ? " do-alpha " : (i == 1 ? " do-beta " : " do-gamma "));
		ExpectEqual(entry.statements, expected, "array entry statements");
	}
}

void TestRepeatedScenario()
{
	const char* source =
		"meta snapshot scenario:smurf [ [ do-stuff ], [ do-other-stuff ] ]\n\n"
		"meta snapshot scenario:smurf [ [ do-more-stuff ], [ do-more-other-stuff ] ]\n";
	SnapshotPlan plan = CollectPlan("repeat.ivg", source);

	const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
	const std::vector<SnapshotEntry>& entries = plan.getEntries();

	ExpectEqual(static_cast<uint32_t>(scenarios.size()), 1, "scenario count");
	ExpectEqual(static_cast<uint32_t>(entries.size()), 4, "entry count");

	const SnapshotScenario& scenario = scenarios[0];
	ExpectEqual(static_cast<uint32_t>(scenario.entryIndices.size()), 4, "scenario entry count");

	ExpectEqual(entries[scenario.entryIndices[0]].blockIndex, 1, "first block index");
	ExpectEqual(entries[scenario.entryIndices[0]].sourceLine, 1, "first block line");
	ExpectEqual(entries[scenario.entryIndices[2]].blockIndex, 2, "second block index");
	ExpectEqual(entries[scenario.entryIndices[2]].sourceLine, 3, "second block line");
}

void TestDefaultScenarioNames()
{
	const char* source =
		"meta snapshot [ [ first ], [ second ] ]\n"
		"meta snapshot [ third ]\n";
	SnapshotPlan plan = CollectPlan("implicit.ivg", source);

	const std::vector<SnapshotScenario>& scenarios = plan.getScenarios();
	const std::vector<SnapshotEntry>& entries = plan.getEntries();

	ExpectEqual(static_cast<uint32_t>(scenarios.size()), 3, "scenario count");
	ExpectEqual(static_cast<uint32_t>(entries.size()), 3, "entry count");

	ExpectEqual(scenarios[0].name, "implicit-1-1", "first implicit scenario name");
	ExpectEqual(scenarios[1].name, "implicit-1-2", "second implicit scenario name");
	ExpectEqual(scenarios[2].name, "implicit-2", "third implicit scenario name");

	ExpectEqual(entries[scenarios[0].entryIndices[0]].entryIndex, 1, "first implicit entry index");
	ExpectEqual(entries[scenarios[1].entryIndices[0]].entryIndex, 2, "second implicit entry index");
	ExpectEqual(entries[scenarios[2].entryIndices[0]].entryIndex, 1, "third implicit entry index");
}

void TestValidateMismatch()
{
	const char* source =
		"meta snapshot scenario:toggle validate:no [ [ draft ] ]\n"
		"meta snapshot scenario:toggle [ [ validate ] ]\n";

	String text(source);
	SnapshotPlan plan("mismatch.ivg");
	std::vector<std::string> includeDirs;
	SnapshotCollector collector(plan, "mismatch.ivg", text, includeDirs);
	STLMapVariables variables;
	FormatInfo formatInfo;
	formatInfo.formatId = "meta";
	formatInfo.uses.insert("snapshot-1");
	Interpreter interpreter(collector, variables, formatInfo);

	bool caught = false;
	try {
		interpreter.run(StringRange(text));
	} catch (Exception& e) {
		caught = true;
		Expect(e.getError() == "scenario switches between validate yes/no.", "validate mismatch error message");
	}
	Expect(caught, "validate mismatch should throw");
}

} // namespace

int main()
{
	TestExplicitScenario();
	TestArrayStatements();
	TestRepeatedScenario();
	TestDefaultScenarioNames();
	TestValidateMismatch();
	return 0;
}

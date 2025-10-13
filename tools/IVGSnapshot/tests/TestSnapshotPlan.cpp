#define IVG_SNAPSHOT_TESTING 1
#include "../IVGSnapshot.cpp"

#include <cstdlib>
#include <iostream>
#include <sstream>

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
		ExpectEqual(entry.entryOrdinal, 1, "entry ordinal");
		Expect(entry.validate, "entry validate flag");
		ExpectEqual(entry.scenarioName, "one", "entry scenario name");
		ExpectEqual(static_cast<uint32_t>(entry.invocations.size()), 1, "entry invocation count");
		const SnapshotInvocation& invocation = entry.invocations[0];
		ExpectEqual(invocation.blockIndex, 1, "invocation block index");
		ExpectEqual(invocation.statementOrdinal, 1, "invocation statement ordinal");
		ExpectEqual(invocation.sourceLine, 1, "invocation source line");
		ExpectEqual(invocation.statements, " set fill red ", "entry statements preserve whitespace");
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
			ExpectEqual(entry.entryOrdinal, i + 1, "array entry ordinal");
			ExpectEqual(static_cast<uint32_t>(entry.invocations.size()), 1, "array invocation count");
			ExpectEqual(entry.scenarioName, "array", "array scenario name");
			const SnapshotInvocation& invocation = entry.invocations[0];
			ExpectEqual(invocation.blockIndex, 1, "array block index");
			ExpectEqual(invocation.statementOrdinal, i + 1, "array statement ordinal");
			ExpectEqual(invocation.sourceLine, 1, "array source line");
			const std::string expected = (i == 0 ? " do-alpha " : (i == 1 ? " do-beta " : " do-gamma "));
			ExpectEqual(invocation.statements, expected, "array entry statements");
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
		ExpectEqual(static_cast<uint32_t>(entries.size()), 2, "entry count");

		const SnapshotScenario& scenario = scenarios[0];
		ExpectEqual(static_cast<uint32_t>(scenario.entryIndices.size()), 2, "scenario entry count");

		const SnapshotEntry& entryOne = entries[scenario.entryIndices[0]];
		const SnapshotEntry& entryTwo = entries[scenario.entryIndices[1]];
		ExpectEqual(static_cast<uint32_t>(entryOne.invocations.size()), 2, "entry one invocation count");
		ExpectEqual(static_cast<uint32_t>(entryTwo.invocations.size()), 2, "entry two invocation count");
		ExpectEqual(entryOne.invocations[0].blockIndex, 1, "entry one first block index");
		ExpectEqual(entryOne.invocations[0].sourceLine, 1, "entry one first block line");
		ExpectEqual(entryOne.invocations[1].blockIndex, 2, "entry one second block index");
		ExpectEqual(entryOne.invocations[1].sourceLine, 3, "entry one second block line");
		ExpectEqual(entryTwo.invocations[0].blockIndex, 1, "entry two first block index");
		ExpectEqual(entryTwo.invocations[1].blockIndex, 2, "entry two second block index");
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

	ExpectEqual(entries[scenarios[0].entryIndices[0]].entryOrdinal, 1, "first implicit entry ordinal");
	ExpectEqual(entries[scenarios[1].entryIndices[0]].entryOrdinal, 1, "second implicit entry ordinal");
	ExpectEqual(entries[scenarios[2].entryIndices[0]].entryOrdinal, 1, "third implicit entry ordinal");
}



void TestSnapshotSourceTags()
{
	const NuXFiles::Path root = NuXFiles::Path::getCurrentDirectoryPath();
	std::wstring rootWide;
	try {
		rootWide = root.getFullPath();
	} catch (const std::exception&) {
		Fail("failed to read current directory path");
	}
	std::wstring ivgWide = NuXFiles::Path::appendSeparator(rootWide);
	ivgWide += L"alpha_beta";
	ivgWide = NuXFiles::Path::appendSeparator(ivgWide);
	ivgWide += L"gamma_delta.ivg";
	const std::string ivgPath = pathStringFromWide(ivgWide);
	const std::string relativeTag = buildSnapshotSourceTag(ivgPath, root);
	Expect(relativeTag == "alpha__beta_gamma__delta", "root relative snapshot tag should escape underscores");

	NuXFiles::Path parent;
	if (root.isRoot()) {
		Fail("cannot run absolute tag test from filesystem root");
	}
	try {
		parent = root.getParent();
	} catch (const std::exception&) {
		Fail("failed to resolve parent directory for absolute tag test");
	}
	std::wstring outsideWide = NuXFiles::Path::appendSeparator(parent.getFullPath());
	outsideWide += L"absolute_example.ivg";
	const std::string outsidePath = pathStringFromWide(outsideWide);
	const std::string absoluteTag = buildSnapshotSourceTag(outsidePath, root);
	std::string normalized = outsidePath;
	for (size_t i = 0; i < normalized.size(); ++i) {
		if (normalized[i] == '\\') {
			normalized[i] = '/';
		}
	}
	const size_t dot = normalized.find_last_of('.');
	if (dot != std::string::npos) {
		normalized.resize(dot);
	}
	Expect(absoluteTag == sanitizeFileComponent(normalized), "outside root should sanitize absolute path");
}



void TestValidateMismatch()
{
	const char* source =
		"meta snapshot scenario:toggle validate:no [ [ draft ] ]\n"
		"meta snapshot scenario:toggle [ [ validate ] ]\n";

	String textSource(source);
	SnapshotPlan plan("mismatch.ivg");
	std::vector<std::string> includeDirs;
	SnapshotCollector collector(plan, "mismatch.ivg", textSource, includeDirs);
	STLMapVariables variables;
	FormatInfo formatInfo;
	formatInfo.formatId = "meta";
	formatInfo.uses.insert("snapshot-1");
	Interpreter interpreter(collector, variables, formatInfo);

	bool caught = false;
	try {
		interpreter.run(StringRange(textSource));
	} catch (Exception& e) {
		caught = true;
		Expect(e.getError() == "scenario switches between validate yes/no.", "validate mismatch error message");
	}
	Expect(caught, "validate mismatch should throw");
}

void TestDraftValidateWorkflow()
{
	const char* tempEnv = std::getenv("TMPDIR");
	const std::string tempRoot = (tempEnv != 0 && tempEnv[0] != '\0') ? std::string(tempEnv) : std::string("/tmp");

	CommandLineOptions options;
	options.snapshotDir = joinPath(tempRoot, "IVGSnapshotWorkflowTest");
	options.forceUpdate = false;

	SnapshotScenario scenario;
	scenario.name = "workflow";
	scenario.validate = true;
	scenario.entryIndices.push_back(0);

	SnapshotEntry entry;
	entry.scenarioIndex = 0;
	entry.entryOrdinal = 1;
	entry.validate = true;
	entry.scenarioName = "workflow";

	SnapshotGolden golden("workflow.ivg", "workflow", scenario, entry, options);

	SnapshotEntryResult paths;
	golden.populateResult(paths);
	Expect(ensureDirectory(extractDirectory(paths.goldenPath)), "create workflow directory");
	removeFileIfExists(paths.goldenPath);
	removeFileIfExists(paths.oldPath);
	removeFileIfExists(paths.actualPath);
	removeFileIfExists(paths.diffPath);
	removeFileIfExists(paths.backupPath);

	NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> raster(NuXPixels::IntRect(0, 0, 1, 1));
	raster.getPixelPointer()[0] = 0xFFFFFFFFu;

	SnapshotEntryResult draftResult;
	Expect(golden.writeDraft(raster, draftResult), "draft write should succeed");
	Expect(draftResult.success, "draft result success flag");
	Expect(draftResult.skipped, "draft result skipped flag");
	Expect(fileExists(draftResult.oldPath), "old draft file should exist");
	Expect(!fileExists(draftResult.goldenPath), "golden should not exist after draft");

	SnapshotEntryResult validateResult;
	Expect(golden.validate(raster, false, validateResult), "validate should promote draft");
	Expect(validateResult.success, "validate result success flag");
	Expect(validateResult.updated, "validate should mark updated");
	Expect(!validateResult.skipped, "validate should not be skipped");
	Expect(fileExists(validateResult.goldenPath), "golden should exist after validate");
	Expect(!fileExists(validateResult.oldPath), "old draft file should be removed");
	Expect(validateResult.message.find("promoted draft image") != std::string::npos, "validate should report promotion");

	SnapshotEntryResult secondResult;
	Expect(golden.validate(raster, false, secondResult), "second validate should compare against golden");
	Expect(secondResult.success, "second validate success flag");
	Expect(!secondResult.updated, "second validate should not mark updated");
	Expect(!secondResult.diffed, "second validate should not diff");
	Expect(secondResult.message.empty(), "second validate should not report message");

	removeFileIfExists(secondResult.goldenPath);
	removeFileIfExists(secondResult.actualPath);
	removeFileIfExists(secondResult.diffPath);
	removeFileIfExists(secondResult.backupPath);
	removeFileIfExists(paths.oldPath);
}

} // namespace

int main()
	{
	TestExplicitScenario();
	TestArrayStatements();
	TestRepeatedScenario();
	TestDefaultScenarioNames();
	TestSnapshotSourceTags();
	TestValidateMismatch();
	TestDraftValidateWorkflow();
	return 0;
}

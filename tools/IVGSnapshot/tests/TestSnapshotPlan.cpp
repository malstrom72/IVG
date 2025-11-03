#define IVG_SNAPSHOT_TESTING 1
#include "../IVGSnapshot.cpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

void Fail(const std::string &message)
{
        std::cerr << "TestSnapshotPlan: " << message << std::endl;
        std::exit(1);
}

void Expect(bool condition, const std::string &message)
{
        if (!condition) {
                Fail(message);
        }
}

void ExpectEqual(uint32_t actual, uint32_t expected, const std::string &label)
{
        if (actual != expected) {
                std::ostringstream stream;
                stream << label << " expected " << expected << " but got " << actual;
                Fail(stream.str());
        }
}

void ExpectEqual(const std::string &actual, const std::string &expected, const std::string &label)
{
        if (actual != expected) {
                std::ostringstream stream;
                stream << label << " expected:\n" << expected << "\nbut got:\n" << actual;
                Fail(stream.str());
        }
}

std::string ReadFile(const std::string &path)
{
        std::ifstream input(path.c_str(), std::ios::binary);
        if (!input.good()) {
                Fail(std::string("failed to read file: ") + path);
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
}

struct CapturedIO {
        std::string out;
        std::string err;
};

CapturedIO RunListOnlyTool(const std::string &path, SnapshotRunResult &outRun)
{
        CommandLineOptions options;
        options.listOnly = true;

        SnapshotTotals totals;
        std::ostringstream outBuffer;
        std::ostringstream errBuffer;

        std::streambuf *oldOut = std::cout.rdbuf(outBuffer.rdbuf());
        std::streambuf *oldErr = std::cerr.rdbuf(errBuffer.rdbuf());

        outRun = processFile(options, path);
        totals.accumulate(outRun);
        logFileReport(path, outRun);
        logTotalsSummary(totals);

        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);

        CapturedIO captured;
        captured.out = outBuffer.str();
        captured.err = errBuffer.str();
        return captured;
}

void TestListOnlySample()
{
        const std::string ivgPath = "tools/IVGSnapshot/tests/ListOnlySample.ivg";
        SnapshotRunResult run;
        CapturedIO io = RunListOnlyTool(ivgPath, run);

        Expect(run.exitCode == 0, "ListOnlySample run should succeed");
        Expect(io.err.empty(), "ListOnlySample run should not print to stderr");
        const std::string expected = ReadFile("tools/IVGSnapshot/tests/ListOnlySample.txt");
        ExpectEqual(io.out, expected, "ListOnlySample list-only output");
        ExpectEqual(run.totalEntries, static_cast<uint32_t>(3), "ListOnlySample entry count");
        ExpectEqual(run.validatedEntries, static_cast<uint32_t>(3), "ListOnlySample validated count");
}

void TestListScenarioVariants()
{
        const std::string ivgPath = "tools/IVGSnapshot/tests/ListScenarioVariants.ivg";
        SnapshotRunResult run;
        CapturedIO io = RunListOnlyTool(ivgPath, run);

        Expect(run.exitCode == 0, "ListScenarioVariants run should succeed");
        Expect(io.err.empty(), "ListScenarioVariants run should not print to stderr");
        const std::string expected = ReadFile("tools/IVGSnapshot/tests/ListScenarioVariants.txt");
        ExpectEqual(io.out, expected, "ListScenarioVariants list-only output");
        ExpectEqual(run.totalEntries, static_cast<uint32_t>(7), "ListScenarioVariants entry count");
}

void TestListVariableExpansion()
{
        const std::string ivgPath = "tools/IVGSnapshot/tests/ListVariableExpansion.ivg";
        SnapshotRunResult run;
        CapturedIO io = RunListOnlyTool(ivgPath, run);

        Expect(run.exitCode == 0, "ListVariableExpansion run should succeed");
        Expect(io.err.empty(), "ListVariableExpansion run should not print to stderr");
        const std::string expected = ReadFile("tools/IVGSnapshot/tests/ListVariableExpansion.txt");
        ExpectEqual(io.out, expected, "ListVariableExpansion list-only output");
        ExpectEqual(run.totalEntries, static_cast<uint32_t>(4), "ListVariableExpansion entry count");
}

void TestCommonBlockListOnly()
{
        const std::string ivgPath = "tools/IVGSnapshot/tests/CommonBlock.ivg";
        SnapshotRunResult run;
        CapturedIO io = RunListOnlyTool(ivgPath, run);

        Expect(run.exitCode == 0, "CommonBlock run should succeed");
        Expect(io.err.empty(), "CommonBlock run should not print to stderr");
        const std::string expected = ReadFile("tools/IVGSnapshot/tests/CommonBlock.txt");
        ExpectEqual(io.out, expected, "CommonBlock list-only output");
        ExpectEqual(run.totalEntries, static_cast<uint32_t>(3), "CommonBlock entry count");
}

void TestCommonBlockMismatchDetection()
{
        SnapshotRoundState round;
        round.reset();

        SnapshotBodies first;
        first.common = String(" pen blue width:4 ");
        first.statements.push_back(String("fill #123456"));
        testRecordSnapshotBodies(round, 1, first);

        bool threw = false;
        try {
                SnapshotBodies second;
                second.common = String(" pen blue width:6 ");
                second.statements = first.statements;
                testRecordSnapshotBodies(round, 1, second);
        } catch (const std::runtime_error &error) {
                threw = (std::string(error.what()) ==
                        "snapshot common block changed within iterative round.");
        }
        Expect(threw, "Common block mismatch should raise runtime error");
}

void TestScenarioMismatchDetection()
{
        SnapshotRoundState round;
        round.reset();

        SnapshotBodies first;
        first.common = String(" pen red width:2 ");
        first.statements.push_back(String("fill #abcdef"));
        testRecordSnapshotBodies(round, 2, first);

        bool threw = false;
        try {
                SnapshotBodies second;
                second.common = first.common;
                second.statements.push_back(String("fill #fedcba"));
                testRecordSnapshotBodies(round, 2, second);
        } catch (const std::runtime_error &error) {
                threw = (std::string(error.what()) ==
                        "snapshot statements changed within iterative round.");
        }
        Expect(threw, "Scenario statement mismatch should raise runtime error");
}

std::string WriteTemporaryIVG(const std::string &contents)
{
        char tempName[L_tmpnam];
        if (std::tmpnam(tempName) == 0) {
                Fail("failed to allocate temporary name");
        }
        std::string path(tempName);
        path += ".ivg";
        std::ofstream file(path.c_str(), std::ios::binary);
        if (!file.good()) {
                Fail(std::string("failed to open temporary file: ") + path);
        }
        file << contents;
        file.close();
        return path;
}

void TestImplicitSnapshotRendering()
{
        const char *source =
                "format ivg-3 uses:snapshot-1\n"
                "bounds 0,0,32,32\n"
                "color=#00FF00\n"
                "FILL $color\n"
                "ELLIPSE 16,16,12\n";

        const std::string path = WriteTemporaryIVG(source);

        CommandLineOptions options;
        options.snapshotDir = extractDirectory(path);
        options.forceUpdate = true;

        SnapshotRunResult run = processFile(options, path);
        Expect(run.exitCode == 0, "implicit snapshot run should succeed");
        ExpectEqual(run.totalEntries, static_cast<uint32_t>(1),
                "implicit snapshot entry count");
        Expect(!run.entries.empty(), "implicit snapshot should record entry");

        const SnapshotEntryResult &entry = run.entries.front();
        Expect(entry.success, "implicit snapshot entry should succeed");
        Expect(entry.rendered, "implicit snapshot should render image");
        Expect(entry.updated, "implicit snapshot force update should mark updated");
        Expect(entry.validate, "implicit snapshot defaults to validation");
        Expect(fileExists(entry.goldenPath), "implicit snapshot should write golden");

        removeFileIfExists(entry.goldenPath);
        removeFileIfExists(entry.oldPath);
        removeFileIfExists(entry.actualPath);
        removeFileIfExists(entry.diffPath);
        removeFileIfExists(entry.backupPath);
        std::remove(path.c_str());
}

void TestValidateMismatch()
{
        const char *source =
                "format ivg-3 uses:snapshot-1\n"
                "bounds 0,0,16,16\n"
                "meta snapshot scenario:toggle validate:no [ fill red ]\n"
                "meta snapshot scenario:toggle validate:yes [ fill blue ]\n";

        const std::string path = WriteTemporaryIVG(source);

        CommandLineOptions options;
        options.listOnly = true;

        std::ostringstream outBuffer;
        std::ostringstream errBuffer;
        std::streambuf *oldOut = std::cout.rdbuf(outBuffer.rdbuf());
        std::streambuf *oldErr = std::cerr.rdbuf(errBuffer.rdbuf());
        SnapshotRunResult run = processFile(options, path);
        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);

        std::remove(path.c_str());

        Expect(run.fileFailed, "validate mismatch should mark file as failed");
        Expect(run.exitCode == 1, "validate mismatch should set exit code");
        Expect(run.fileError == "validate flag mismatch for scenario",
                "validate mismatch should report correct error");
        Expect(errBuffer.str().find("validate flag mismatch for scenario") != std::string::npos,
                "validate mismatch should log error to stderr");
}

void TestSnapshotSourceTags()
{
        const NuXFiles::Path root = NuXFiles::Path::getCurrentDirectoryPath();
        std::wstring rootWide;
        try {
                rootWide = root.getFullPath();
        } catch (const std::exception &) {
                Fail("failed to read current directory path");
        }
        std::wstring ivgWide = NuXFiles::Path::appendSeparator(rootWide);
        ivgWide += L"alpha_beta";
        ivgWide = NuXFiles::Path::appendSeparator(ivgWide);
        ivgWide += L"gamma_delta.ivg";
        const std::string ivgPath = pathStringFromWide(ivgWide);
        const std::string relativeTag = buildSnapshotSourceTag(ivgPath, root);
        Expect(relativeTag == "alpha__beta_gamma__delta",
                "root relative snapshot tag should escape underscores");

        NuXFiles::Path parent;
        if (root.isRoot()) {
                Fail("cannot run absolute tag test from filesystem root");
        }
        try {
                parent = root.getParent();
        } catch (const std::exception &) {
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
        Expect(absoluteTag == sanitizeFileComponent(normalized),
                "outside root should sanitize absolute path");
}

void TestGoldenAuditWithSanitizedBases()
{
	char tempName[L_tmpnam];
	if (std::tmpnam(tempName) == 0) {
		Fail("failed to allocate temporary audit root");
	}
	std::string root(tempName);
	root += "_ivgsnapshot_audit";
	Expect(ensureDirectory(root), "create audit root");
	const std::string alpha = joinPath(root, "alpha");
	Expect(ensureDirectory(alpha), "create alpha directory");
	const std::string beta = joinPath(alpha, "beta");
	Expect(ensureDirectory(beta), "create beta directory");

	const std::string ivgPath = joinPath(beta, "sample.ivg");
	{
		std::ofstream file(ivgPath.c_str(), std::ios::binary);
		if (!file.good()) {
			Fail(std::string("failed to write temporary IVG: ") + ivgPath);
		}
		file << "format ivg-3 uses:snapshot-1\n";
		file << "bounds 0,0,1,1\n";
		file << "meta snapshot scenario:document [ fill red ]\n";
	}

	const NuXFiles::Path rootPath = pathFromNativeString(root);
	Expect(!rootPath.isNull(), "audit root path should be valid");
	const std::string snapshotBase = buildSnapshotSourceTag(ivgPath, rootPath);
	Expect(!snapshotBase.empty(), "snapshot base should not be empty");

	const std::string goldenPath = joinPath(beta, snapshotBase + "__document.png");
	{
		std::ofstream golden(goldenPath.c_str(), std::ios::binary);
		if (!golden.good()) {
			Fail(std::string("failed to write temporary golden: ") + goldenPath);
		}
		golden.put('\0');
	}

	std::set<std::string> processedBases;
	processedBases.insert(snapshotBase);

	std::set<std::string> auditRoots;
	auditRoots.insert(beta);

	std::vector<std::string> orphanGoldens;
	collectOrphanGoldens(processedBases, auditRoots, orphanGoldens);
	Expect(orphanGoldens.empty(), "sanitized golden should not be orphan");

	removeFileIfExists(ivgPath);
	removeFileIfExists(goldenPath);
}

void TestDraftValidateWorkflow()
{
        const char *tempEnv = std::getenv("TMPDIR");
        const std::string tempRoot =
                (tempEnv != 0 && tempEnv[0] != '\0') ? std::string(tempEnv) : std::string("/tmp");

        CommandLineOptions options;
        options.snapshotDir = joinPath(tempRoot, "IVGSnapshotWorkflowTest");
        options.forceUpdate = false;

        const String scenarioName("workflow");
        SnapshotGolden golden("workflow.ivg", "workflow",
                stringFromIMPD(scenarioName), options);

        SnapshotEntryResult paths;
        golden.populateResult(paths);
        Expect(ensureDirectory(extractDirectory(paths.goldenPath)),
                "create workflow directory");
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
        Expect(!fileExists(draftResult.goldenPath),
                "golden should not exist after draft");

        SnapshotEntryResult validateResult;
        Expect(golden.validate(raster, false, validateResult),
                "validate should promote draft");
        Expect(validateResult.success, "validate result success flag");
        Expect(validateResult.updated, "validate should mark updated");
        Expect(!validateResult.skipped, "validate should not be skipped");
        Expect(fileExists(validateResult.goldenPath),
                "golden should exist after validate");
        Expect(!fileExists(validateResult.oldPath),
                "old draft file should be removed");
        Expect(validateResult.message.find("promoted draft image") != std::string::npos,
                "validate should report promotion");

        SnapshotEntryResult secondResult;
        Expect(golden.validate(raster, false, secondResult),
                "second validate should compare against golden");
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
	TestListOnlySample();
	TestListScenarioVariants();
	TestListVariableExpansion();
	TestCommonBlockListOnly();
	TestCommonBlockMismatchDetection();
	TestScenarioMismatchDetection();
	TestImplicitSnapshotRendering();
	TestValidateMismatch();
	TestSnapshotSourceTags();
	TestGoldenAuditWithSanitizedBases();
	TestDraftValidateWorkflow();
	return 0;
}

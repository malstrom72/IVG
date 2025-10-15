#define IVG_SNAPSHOT_TESTING 1
#include "../IVGSnapshot.cpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {
class TempDir {
  public:
	TempDir(const char *tag) { create(tag); }
	~TempDir() { cleanup(); }

	const std::string &path() const { return directory; }

  private:
	void create(const char *tag) {
		std::wstring rootWide;
		try {
			rootWide = NuXFiles::Path::getCurrentDirectoryPath().getFullPath();
		} catch (const std::exception &) {
			fail("unable to query current directory");
		}
		const std::string root = pathStringFromWide(rootWide);
		const std::string base = joinPath(root, "output/snapshot-tests");
		NuXFiles::Path basePath = pathFromNativeString(base);
		if (!basePath.isNull()) {
			try {
				if (!basePath.exists()) {
					basePath.tryToCreate();
				}
			} catch (const std::exception &) {
				fail("unable to create snapshot test base directory");
			}
		}
		unsigned long pid = 0;
#if defined(_WIN32)
		pid = static_cast<unsigned long>(_getpid());
#else
		pid = static_cast<unsigned long>(getpid());
#endif
		static unsigned long counter = 0;
		++counter;
		std::ostringstream stream;
		stream << "sched_" << tag << '_' << pid << '_' << counter;
		directory = joinPath(base, stream.str());
		NuXFiles::Path dirPath = pathFromNativeString(directory);
		if (dirPath.isNull()) {
			fail("failed to create temporary path");
		}
		if (!dirPath.tryToCreate()) {
			fail("failed to create temporary directory");
		}
	}

	void cleanup() {
		if (directory.empty()) {
			return;
		}
		try {
			NuXFiles::Path dirPath = pathFromNativeString(directory);
			removeRecursively(dirPath);
		} catch (const std::exception &) {
		}
	}

	static void removeRecursively(const NuXFiles::Path &path) {
		if (path.isNull()) {
			return;
		}
		try {
			if (!path.exists()) {
				return;
			}
			if (path.isDirectory()) {
				std::vector<NuXFiles::Path> children;
				path.listSubPaths(children);
				for (size_t i = 0; i < children.size(); ++i) {
					removeRecursively(children[i]);
				}
			}
			path.tryToErase();
		} catch (const std::exception &) {
		}
	}

	static void fail(const std::string &message) {
		std::cerr << "TestSnapshotScheduler: " << message << std::endl;
		std::exit(1);
	}

	std::string directory;
};

void Expect(bool condition, const std::string &message) {
	if (!condition) {
		std::cerr << "TestSnapshotScheduler: " << message << std::endl;
		std::exit(1);
	}
}

void BuildPlan(SnapshotPlan &plan, const std::string &path,
			   const char *source) {
	const String text(source);
	plan.beginCollection();
	while (true) {
		std::vector<std::string> includeDirs;
		SnapshotCollector collector(plan, path, text, includeDirs);
		STLMapVariables variables;
		FormatInfo formatInfo;
		Interpreter interpreter(collector, variables, formatInfo);
		try {
			interpreter.run(StringRange(text));
		} catch (Exception &e) {
			std::ostringstream stream;
			stream << "plan build failed: " << e.getError();
			if (e.hasStatement()) {
				stream << " near '" << e.getStatement() << "'";
			}
			Expect(false, stream.str());
		} catch (std::exception &e) {
			Expect(false,
				   std::string("plan build threw std::exception: ") + e.what());
		}
		plan.completeCollectionPass();
		if (!plan.prepareNextCollectionPass()) {
			break;
		}
	}
}

struct SchedulerContext {
	SchedulerContext(const std::string &ivgPath, const char *script)
		: path(ivgPath), plan(ivgPath) {
		document.setSource(String(script));
		BuildPlan(plan, path, script);
		snapshotBase = stringFromIMPD(plan.getBaseName());
	}

	std::string path;
	SnapshotPlan plan;
	CachedDocument document;
	CommandLineOptions options;
	SharedResources shared;
	std::string snapshotBase;
	SnapshotRunResult run;
	std::vector<SnapshotEntryResult> ordered;
	std::vector<bool> ready;
	size_t nextLogIndex;
};

void InitializeOptions(SchedulerContext &context,
					   const std::string &snapshotDir, bool forceUpdate,
					   bool exitOnFirstFailure, const std::string &rootDir) {
	context.options.forceUpdate = forceUpdate;
	context.options.exitOnFirstFailure = exitOnFirstFailure;
	context.options.snapshotDir = snapshotDir;
	context.options.listOnly = false;
	context.options.verbose = false;
	context.options.threads = 0;
	context.options.rootDir = pathFromNativeString(rootDir);
	context.options.ivgPaths.clear();
	context.options.ivgPaths.push_back(context.path);
	context.run = SnapshotRunResult();
	context.ordered.clear();
	context.ready.clear();
	context.nextLogIndex = 0;
}

void DrainScheduler(SnapshotScheduler &scheduler, SchedulerContext &context,
					bool wait) {
	flushSchedulerResults(scheduler, wait, context.ordered, context.ready,
						  context.nextLogIndex, context.run);
}

void EnqueuePlan(SchedulerContext &context, SnapshotScheduler &scheduler,
				 bool &schedulingStopped) {
	const std::vector<SnapshotScenario> &scenarios =
		context.plan.getScenarios();
	const std::vector<SnapshotEntry> &entries = context.plan.getEntries();
	schedulingStopped = false;
	for (size_t i = 0; i < scenarios.size() && !schedulingStopped; ++i) {
		const SnapshotScenario &scenario = scenarios[i];
		for (size_t j = 0; j < scenario.entryIndices.size(); ++j) {
			if (context.options.exitOnFirstFailure &&
				scheduler.shouldStopScheduling()) {
				schedulingStopped = true;
				break;
			}
			const uint32_t entryIndex = scenario.entryIndices[j];
			if (entryIndex >= entries.size()) {
				continue;
			}
			const SnapshotEntry &entry = entries[entryIndex];
			SnapshotJob job;
			job.options = &context.options;
			job.ivgPath = &context.path;
			job.snapshotBase = &context.snapshotBase;
			job.document = &context.document;
			job.sharedResources = &context.shared;
			job.scenario = &scenario;
			job.entry = &entry;
			job.planOrdinal = static_cast<uint32_t>(context.ordered.size());
			Expect(scheduler.enqueue(job), "scheduler rejected job enqueue");
			context.ordered.push_back(SnapshotEntryResult());
			context.ready.push_back(false);
			DrainScheduler(scheduler, context, false);
		}
		DrainScheduler(scheduler, context, false);
	}
	DrainScheduler(scheduler, context, false);
}

void FinalizeScheduler(SnapshotScheduler &scheduler,
					   SchedulerContext &context) {
	DrainScheduler(scheduler, context, false);
	while (context.nextLogIndex < context.ordered.size()) {
		DrainScheduler(scheduler, context, true);
	}
	scheduler.finalize();
	DrainScheduler(scheduler, context, true);
}

void TestOrdering() {
	TempDir temp("ordering");
	const std::string ivgPath = joinPath(temp.path(), "ordering.ivg");
	const char *script =
		"format ivg-3 uses:snapshot-1\n"
		"bounds 0,0,16,16\n"
		"meta snapshot scenario:alpha [ [ color=#FF0000 ], [ color=#00FF00 ] "
		"]\n"
		"meta snapshot scenario:beta validate:no [ color=#0000FF ]\n"
		"meta snapshot [ color=#FFFF00 ]\n"
		"FILL $color\n"
		"RECT 0,0,16,16\n";
	SchedulerContext context(ivgPath, script);
	const std::string snapshotDir = joinPath(temp.path(), "snapshots");
	Expect(ensureDirectory(snapshotDir),
		   "failed to create snapshot output directory");
	InitializeOptions(context, snapshotDir, true, false, temp.path());
	SnapshotScheduler scheduler(2, context.options.exitOnFirstFailure);
	scheduler.start();
	bool schedulingStopped = false;
	EnqueuePlan(context, scheduler, schedulingStopped);
	FinalizeScheduler(scheduler, context);
	Expect(!schedulingStopped,
		   "scheduling unexpectedly stopped during ordering test");
	const std::vector<SnapshotEntry> &entries = context.plan.getEntries();
	Expect(context.run.entries.size() == entries.size(),
		   "unexpected entry count in ordering results");
	Expect(context.run.failedEntries == 0,
		   "ordering run should not report failures");
	Expect(context.run.exitCode == 0, "ordering run exit code mismatch");
	for (size_t i = 0; i < context.run.entries.size(); ++i) {
		const SnapshotEntryResult &result = context.run.entries[i];
		Expect(result.planOrdinal == i, "plan ordinal mismatch");
		Expect(result.rendered, "entry did not render successfully");
		Expect(result.success, "entry did not report success");
	}
	Expect(context.run.entries.size() == 4,
		   "expected four entries across scenarios");
	Expect(context.run.entries[0].scenarioName == "alpha",
		   "first entry should belong to alpha");
	Expect(context.run.entries[1].scenarioName == "alpha",
		   "second entry should belong to alpha");
	Expect(context.run.entries[2].scenarioName == "beta",
		   "third entry should belong to beta");
	Expect(context.run.entries[3].scenarioName == "ordering-3",
		   "implicit scenario did not use synthesized name");
}

void TestExitOnFailure() {
	TempDir temp("failure");
	const std::string ivgPath = joinPath(temp.path(), "failing.ivg");
	const char *script = "format ivg-3 uses:snapshot-1\n"
						 "bounds 0,0,16,16\n"
						 "meta snapshot scenario:fatal validate:yes [ [ "
						 "color=#101010 ], [ color=#202020 ] ]\n"
						 "FILL $color\n"
						 "RECT 0,0,16,16\n";
	SchedulerContext context(ivgPath, script);
	const std::string snapshotDir = joinPath(temp.path(), "snapshots");
	Expect(ensureDirectory(snapshotDir),
		   "failed to create snapshot output directory");
	InitializeOptions(context, snapshotDir, false, true, temp.path());
	SnapshotScheduler scheduler(1, context.options.exitOnFirstFailure);
	scheduler.start();
	bool schedulingStopped = false;
	EnqueuePlan(context, scheduler, schedulingStopped);
	FinalizeScheduler(scheduler, context);
	Expect(!context.run.entries.empty(),
		   "failure run should record at least one result");
	Expect(context.run.failedEntries >= 1,
		   "failure run should report at least one failed entry");
	Expect(context.run.exitCode == 0, "failure run exit code mismatch");
	bool sawFailure = false;
	for (size_t i = 0; i < context.run.entries.size(); ++i) {
		const SnapshotEntryResult &result = context.run.entries[i];
		if (!result.success) {
			sawFailure = true;
			Expect(result.message.find("missing golden") != std::string::npos,
				   "failure should mention missing golden");
			break;
		}
	}
	Expect(sawFailure, "failure run should include an unsuccessful entry");
}
} // namespace

int main() {
	TestOrdering();
	TestExitOnFailure();
	std::cout << "scheduler tests passed" << std::endl;
	return 0;
}

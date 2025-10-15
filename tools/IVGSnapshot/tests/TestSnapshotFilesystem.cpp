#define IVG_SNAPSHOT_TESTING 1
#include "../IVGSnapshot.cpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
#if !defined(_WIN32)
bool RunningAsRoot() { return (::geteuid() == 0); }
#endif

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
		stream << "snapshot_" << tag << '_' << pid << '_' << counter;
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
		std::cerr << "TestSnapshotFilesystem: " << message << std::endl;
		std::exit(1);
	}

	std::string directory;
};

void Expect(bool condition, const std::string &message) {
	if (!condition) {
		std::cerr << "TestSnapshotFilesystem: " << message << std::endl;
		std::exit(1);
	}
}

bool PathIsDirectory(const std::string &path) {
	try {
		const NuXFiles::Path native = pathFromNativeString(path);
		return (!native.isNull() && native.exists() && native.isDirectory());
	} catch (const std::exception &) {
		return false;
	}
}

bool PathIsFile(const std::string &path) {
	try {
		const NuXFiles::Path native = pathFromNativeString(path);
		return (!native.isNull() && native.exists() && native.isFile());
	} catch (const std::exception &) {
		return false;
	}
}

void WriteFile(const std::string &path) {
	std::ofstream stream(path.c_str(), std::ios::binary);
	stream << "guard";
}
} // namespace

int main() {
	TempDir root("fs");
	const std::string base = joinPath(root.path(), "stage");
	Expect(ensureDirectory(base), "ensureDirectory should create stage root");

	const std::string nested = joinPath(base, "alpha/beta");
	Expect(ensureDirectory(nested),
		   "ensureDirectory should build nested directories");
	Expect(PathIsDirectory(nested),
		   "nested directory missing after ensureDirectory");

	const std::string filePath = joinPath(base, "gamma/output.dat");
	Expect(ensureParentDirectory(filePath),
		   "ensureParentDirectory should create parent directories");
	Expect(PathIsDirectory(joinPath(base, "gamma")),
		   "parent directory missing after ensureParentDirectory");

	const std::string blocker = joinPath(base, "blocked");
	WriteFile(blocker);
	Expect(PathIsFile(blocker), "failed to create blocking file");
	Expect(!ensureDirectory(joinPath(blocker, "child")),
		   "ensureDirectory should fail when a file blocks the path");
	Expect(PathIsFile(blocker), "blocking file should remain after failure");

	const std::string blockedFile = joinPath(blocker, "child/file.bin");
	Expect(!ensureParentDirectory(blockedFile),
		   "ensureParentDirectory should fail when a file blocks the path");
	Expect(!PathIsDirectory(joinPath(blocker, "child")),
		   "ensureParentDirectory should not create child under blocking file");

#if !defined(_WIN32)
	if (RunningAsRoot()) {
		std::cout << "Skipping permission failure test: running as root"
				  << std::endl;
	} else {
		const std::string locked = joinPath(base, "locked");
		Expect(ensureDirectory(locked),
			   "failed to create directory for permission test");
		Expect(::chmod(locked.c_str(), 0555) == 0,
			   "chmod failed for permission test");
		Expect(!ensureDirectory(joinPath(locked, "inner")),
			   "ensureDirectory should fail inside read-only directory");
		Expect(!PathIsDirectory(joinPath(locked, "inner")),
			   "permission test should not create inner directory");
		const std::string lockedFile = joinPath(locked, "inner/file.bin");
		Expect(!ensureParentDirectory(lockedFile),
			   "ensureParentDirectory should fail for read-only directory");
		Expect(::chmod(locked.c_str(), 0755) == 0, "chmod restore failed");
	}
#endif

	std::cout << "filesystem guard tests passed" << std::endl;
	return 0;
}

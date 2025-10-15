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
		stream << "pngtest_" << tag << '_' << pid << '_' << counter;
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
		std::cerr << "TestSnapshotPngIO: " << message << std::endl;
		std::exit(1);
	}

	std::string directory;
};

void Expect(bool condition, const std::string &message) {
	if (!condition) {
		std::cerr << "TestSnapshotPngIO: " << message << std::endl;
		std::exit(1);
	}
}

void WriteCorruptFile(const std::string &path) {
	std::ofstream stream(path.c_str(), std::ios::binary);
	stream << "not a png";
}

NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> MakeRaster(int width,
															 int height) {
	NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> raster(
		NuXPixels::IntRect(0, 0, width, height));
	NuXPixels::ARGB32::Pixel *pixels = raster.getPixelPointer();
	const int stride = raster.getStride();
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const unsigned int alpha = 0xFFu;
			const unsigned int red = static_cast<unsigned int>(x * 40 + y * 10);
			const unsigned int green =
				static_cast<unsigned int>(y * 40 + x * 10);
			const unsigned int blue = static_cast<unsigned int>((x + y) * 20);
			pixels[y * stride + x] = (alpha << 24) | ((red & 0xFFu) << 16) |
									 ((green & 0xFFu) << 8) | (blue & 0xFFu);
		}
	}
	return raster;
}
} // namespace

int main() {
	TempDir root("io");
	const std::string corrupt = joinPath(root.path(), "broken.png");
	WriteCorruptFile(corrupt);
	NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> raster;
	Expect(!loadPngRaster(corrupt, raster),
		   "loadPngRaster should fail for corrupt data");

	NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> source = MakeRaster(4, 3);
	const std::string valid = joinPath(root.path(), "valid.png");
	std::string error;
	Expect(writeRasterToPng(valid, source, error),
		   "writeRasterToPng should succeed for valid raster");
	Expect(error.empty(),
		   "writeRasterToPng should not report error on success");

	NuXPixels::SelfContainedRaster<NuXPixels::ARGB32> reloaded;
	Expect(loadPngRaster(valid, reloaded),
		   "loadPngRaster should read back written PNG");
	const NuXPixels::IntRect bounds = reloaded.calcBounds();
	Expect(bounds.width == 4 && bounds.height == 3,
		   "reloaded raster should match original dimensions");

	const std::string blocker = joinPath(root.path(), "occupied");
	std::ofstream(blocker.c_str(), std::ios::binary) << "block";
	Expect(fileExists(blocker), "failed to create blocking file for PNG test");
	const std::string blockedTarget = joinPath(blocker, "child.png");
	std::string blockedError;
	Expect(!writeRasterToPng(blockedTarget, source, blockedError),
		   "writeRasterToPng should fail when parent is a file");
	Expect(!blockedError.empty(), "failure should provide an error message");
	Expect(!fileExists(blockedTarget), "blocked target should not be created");

	std::string retryError;
	const std::string retry = joinPath(root.path(), "retry.png");
	Expect(writeRasterToPng(retry, source, retryError),
		   "writeRasterToPng should recover after failure");
	Expect(loadPngRaster(retry, reloaded),
		   "loadPngRaster should succeed after previous failure");

	std::cout << "png IO tests passed" << std::endl;
	return 0;
}

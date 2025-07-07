#include "NuXDebug.h"
#include "assert_h_replacement.h"

namespace NuXDebug {

#if (NUX_DEBUG_INCLUDE_TESTS)

struct TestFunction {
	const char* name;
	bool (*function)();
};

const int MAX_TEST_FUNCTIONS = 256;

static int registeredTestsCount = 0;
static TestFunction registeredTests[MAX_TEST_FUNCTIONS];
static bool hasRunTests = false;

bool registerTest(const char* name, bool (*function)()) {
	assert(!hasRunTests);
	for (int i = 0; i < registeredTestsCount; ++i) {
		const TestFunction& test = registeredTests[i];
		if (test.function == function) {
			assert(strcmp(test.name, name) == 0);
			return (strcmp(test.name, name) == 0);
		} else {
			assert(strcmp(test.name, name) != 0);
			if (strcmp(test.name, name) == 0) {
				return false;
			}
		}
	}
	assert(registeredTestsCount < MAX_TEST_FUNCTIONS);
	if (registeredTestsCount < MAX_TEST_FUNCTIONS) {
		registeredTests[registeredTestsCount].name = name;
		registeredTests[registeredTestsCount].function = function;
		++registeredTestsCount;
		return true;
	} else {
		return false;
	}
}

bool runTests() {
	bool allSuccess = true;
	for (int i = 0; i < registeredTestsCount; ++i) {
		const TestFunction& test = registeredTests[i];
		try {
			trace(std::string("Running test ") + test.name);
			if (!test.function()) {
				error(std::string("Test ") + test.name + " failed!");
				allSuccess = false;
			}
		}
		catch (std::exception& x) {
			error(std::string("Test ") + test.name + " failed with exception: " + x.what());
			allSuccess = false;
		}
		catch (...) {
			error(std::string("Test ") + test.name + " failed with unknown exception");
			allSuccess = false;
		}
	}
	if (allSuccess) {
		trace("All tests ran successfully");
	} else {
		warning("Some tests failed");
	}
	return allSuccess;
}

#endif	// (NUX_DEBUG_INCLUDE_TESTS)

} /* namespace NuXDebug */

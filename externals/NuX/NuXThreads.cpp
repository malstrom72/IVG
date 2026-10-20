/*
BSD 2-Clause License

Copyright (c) 2005-2025, Magnus Lidström

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "NuXThreads.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>

namespace NuXThreads {

void Thread::run()
{
	assert(0); // You should override this or create the thread with another 'Runnable'.
}

Runnable::~Runnable()
{
}

template<typename T, typename U> T lossless_cast(U x)
{
	assert(static_cast<T>(x) == x);
	return static_cast<T>(x);
}

class LoopParallelizer : public NuXThreads::Thread {
	public:
		LoopParallelizer(NuXThreads::AtomicInt& jobCounter, int iterationCount, ParallelLoopBody& body, int threadIndex)
				: jobCounter(jobCounter), iterationCount(iterationCount), threadIndex(threadIndex), body(&body) {
		}

		virtual void run() {
			try {
				int index;
				do {
					index = jobCounter++;
					if (index < iterationCount) {
						if (!body->run(index, iterationCount, threadIndex)) {
							return;
						}
					}
				} while (index < iterationCount);
			}
			catch (const std::exception& x) {
				std::cout << "Exception in parallel loop: " << x.what() << std::endl;
				assert(0);
			}
			catch (...) {
				std::cout << "Unknown exception in parallel loop" << std::endl;
				assert(0);
			}
		}

	protected:
		NuXThreads::AtomicInt& jobCounter;
		const int iterationCount;
		const int threadIndex;
		ParallelLoopBody* const body;
};

static int suggestThreadCount() {
#ifdef NDEBUG
	return std::max(queryCPUCount(), 1);
#else
	return 1;
#endif
}

struct FunctionLoopBodyAdapter : public ParallelLoopBody {
	FunctionLoopBodyAdapter(ParallelLoopFunction function, void* context)
			: function(function), context(context) {
		assert(function != 0);
	}

	virtual bool run(int index, int iterationCount, int threadIndex) {
		return function(index, iterationCount, threadIndex, context);
	}

	ParallelLoopFunction function;
	void* context;
};

void runLoopInParallel(int count, ParallelLoopBody& body, int threadCount) {
	NuXThreads::AtomicInt jobCounter = 0;
	const int workerCount = (threadCount == 0 ? suggestThreadCount() : threadCount);
	std::vector<LoopParallelizer*> workers;
	workers.reserve(workerCount);

	try {
		for (int i = 0; i < workerCount; ++i) {
			LoopParallelizer* worker = new LoopParallelizer(jobCounter, count, body, i);
			try {
				worker->start();
			}
			catch (...) {
				delete worker;
				throw;
			}
			workers.push_back(worker);
		}
		for (size_t i = 0; i < workers.size(); ++i) {
			workers[i]->join();
		}
	}
	catch (...) {
		for (size_t i = 0; i < workers.size(); ++i) {
			try {
				workers[i]->join();
			}
			catch (...) {
				assert(0);
			}
		}
		for (size_t i = 0; i < workers.size(); ++i) {
			delete workers[i];
		}
		throw;
	}

	for (size_t i = 0; i < workers.size(); ++i) {
		delete workers[i];
	}
}

void runLoopInParallel(int count, ParallelLoopFunction function, void* context, int threadCount) {
	FunctionLoopBodyAdapter adapter(function, context);
	runLoopInParallel(count, static_cast<ParallelLoopBody&>(adapter), threadCount);
}

} /* namespace NuX */

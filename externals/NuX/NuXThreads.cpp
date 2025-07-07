#include "NuXThreads.h"

namespace NuXThreads {

void Thread::run()
{
	assert(0); // You should override this or create the thread with another 'Runnable'.
}

Runnable::~Runnable()
{
}

} /* namespace NuX */

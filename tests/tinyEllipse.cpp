#include <iostream>
#include <math.h>
#include "externals/NuX/NuXPixels.h"

using namespace NuXPixels;

static bool checkTiny(double rx, double ry) {
	Path p;
	p.moveTo(rx, 0.0);
	p.arcSweep(0.0, 0.0, PI2, rx, ry, 1.0);
	return p.size() > 4;
}

int main() {
	if (!checkTiny(1e-12, 1.0)) {
		std::cerr << "tiny rx produced a line" << std::endl;
		return 1;
	}
	if (!checkTiny(1.0, 1e-12)) {
		std::cerr << "tiny ry produced a line" << std::endl;
		return 1;
	}
	return 0;
}

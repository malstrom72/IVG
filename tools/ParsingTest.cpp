/**
	IVG is released under the BSD 2-Clause License.

	Copyright (c) 2013-2025, Magnus Lidström

	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
	following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
	disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
	disclaimer in the documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

/*
	Constructed tests for number parsing and for IVG::buildPathFromSVG, against exact oracles rather than random
	inputs:
	- numbers parsed by IMPD stay within fixed distances of strtod, which is correctly rounded;
	- every command, absolute and relative, gives the same vertices as its absolute equivalent;
	- implicit repetition of commands and the compact number syntax;
	- arcs: the endpoint is reached by the arc itself, every vertex lies on the ellipse that the SVG specification's
	  endpoint-to-center conversion (implementation notes F.6.5, with radii scaled up per F.6.6) gives, and the arc
	  sweeps that ellipse in the right direction and through the right half;
	- the degenerate arcs that the specification turns into lines or skips, and malformed path data.
*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "src/IVG.h"

using namespace IVG;
using namespace NuXPixels;

static int failures = 0;

static void fail(const std::string& what, const std::string& svg) {
	if (++failures <= 40) {
		printf("FAIL %s: %s\n", what.c_str(), svg.c_str());
	}
}

static bool build(const std::string& svg, Path& path) {
	const char* error = 0;
	path.clear();
	return buildPathFromSVG(svg, 1.0, path, error);
}

static std::string num(double d) {
	char buffer[64];
	snprintf(buffer, sizeof (buffer), "%.17g", d);
	return buffer;
}

static bool sameVertices(const Path& a, const Path& b, double tolerance) {
	if (a.size() != b.size()) {
		return false;
	}
	for (Path::const_iterator i = a.begin(), j = b.begin(); i != a.end(); ++i, ++j) {
		if (i->first != j->first || fabs(i->second.x - j->second.x) > tolerance
				|| fabs(i->second.y - j->second.y) > tolerance) {
			return false;
		}
	}
	return true;
}

static void expectSame(const std::string& svg, const std::string& reference) {
	Path a;
	Path b;
	if (!build(svg, a) || !build(reference, b)) {
		fail("did not parse", svg + " / " + reference);
	} else if (!sameVertices(a, b, 1e-9)) {
		fail("differs from " + reference, svg);
	}
}

static void expectError(const std::string& svg) {
	Path path;
	if (build(svg, path)) {
		fail("accepted malformed data", svg);
	}
}

// A deterministic generator, so that failures reproduce.
static unsigned int seed = 12345;
static double random(double low, double high) {
	seed = seed * 1103515245u + 12345u;
	return low + (high - low) * ((seed >> 8) & 0xFFFFFF) / double(0x1000000);
}

static void testCommandsAndSyntax() {
	expectSame("M10 20 L30 40", "M10,20L30,40");
	expectSame("M10 20 30 40 50 60", "M10 20 L30 40 L50 60");
	expectSame("m10 20 30 40", "M10 20 L40 60");
	expectSame("M10 20 L30 40 50 60", "M10 20 L30 40 L50 60");
	expectSame("M10 20 l5 5 5 5", "M10 20 L15 25 L20 30");
	expectSame("M10 20 H50 V60 h-10 v-10", "M10 20 L50 20 L50 60 L40 60 L40 50");
	expectSame("M10 20 H50 60", "M10 20 L50 20 L60 20");
	expectSame("M1.5.5 L-.5-1e1", "M1.5 0.5 L-0.5 -10");
	expectSame("M0 0 L1e2,2E1", "M0 0 L100 20");
	expectSame("M0,0 c10,0 20,10 20,20", "M0 0 C10 0 20 10 20 20");
	expectSame("M5 5 c1 2 3 4 5 6 1 2 3 4 5 6", "M5 5 C6 7 8 9 10 11 C11 13 13 15 15 17");
	expectSame("M0 0 C10 0 20 10 20 20 S30 40 40 40", "M0 0 C10 0 20 10 20 20 C20 30 30 40 40 40");
	expectSame("M0 0 L20 20 S30 40 40 40", "M0 0 L20 20 C20 20 30 40 40 40");
	expectSame("M0 0 C10 0 20 10 20 20 s10 20 20 20", "M0 0 C10 0 20 10 20 20 C20 30 30 40 40 40");
	expectSame("M0 0 q10 0 10 10", "M0 0 Q10 0 10 10");
	expectSame("M0 0 Q10 0 10 10 T10 30", "M0 0 Q10 0 10 10 Q10 20 10 30");
	expectSame("M0 0 Q10 0 10 10 t0 20", "M0 0 Q10 0 10 10 Q10 20 10 30");
	expectSame("M0 0 L10 10 T20 20", "M0 0 L10 10 Q10 10 20 20");
	expectSame("M0 0 Q10 0 10 10 T10 30 20 40", "M0 0 Q10 0 10 10 Q10 20 10 30 Q10 40 20 40");
	expectSame("M0 0 a10 10 0 0 1 20 0", "M0 0 A10 10 0 0 1 20 0");
	expectSame("M0 0 A10 10 0 0120 0", "M0 0 A10 10 0 0 1 20 0");
	expectSame("M0 0 A10 10 0 1,0 20,0", "M0 0 A10 10 0 1 0 20 0");
	expectSame("M0 0 A10 10 0 0 1 20 0 10 10 0 0 1 40 0", "M0 0 A10 10 0 0 1 20 0 A10 10 0 0 1 40 0");
	expectSame("M10 10 L20 10 L20 20 Z l5 5", "M10 10 L20 10 L20 20 Z L15 15");
	expectSame("M10 10 L20 10 Z m5 5 l5 0", "M10 10 L20 10 Z M15 15 L20 15");
	expectSame("  M0 0  ", "M0 0");
	expectSame("", "");
	expectError("L10 10");
	expectError("M");
	expectError("M10");
	expectError("M10 10 X20 20");
	expectError("M10 10 L20");
	expectError("M10 10 A10 10 0 2 0 20 20");
	expectError("M10 10 A10 10 0 0 0 20");
	expectError("M10 10 L1e400 0");
	expectError("M10 10 Lnan 0");

	// Random paths: every relative command matches its absolute equivalent.
	for (int round = 0; round < 2000; ++round) {
		double x = random(-100, 100);
		double y = random(-100, 100);
		std::string absolute = "M" + num(x) + " " + num(y);
		std::string relative = absolute;
		for (int i = 0; i < 6; ++i) {
			const int command = static_cast<int>(random(0, 7));
			const double x1 = random(-100, 100);
			const double y1 = random(-100, 100);
			const double x2 = random(-100, 100);
			const double y2 = random(-100, 100);
			const double x3 = random(-100, 100);
			const double y3 = random(-100, 100);
			switch (command) {
				case 0: {
					absolute += " L" + num(x1) + " " + num(y1);
					relative += " l" + num(x1 - x) + " " + num(y1 - y);
					x = x1;
					y = y1;
					break;
				}
				case 1: {
					absolute += " H" + num(x1);
					relative += " h" + num(x1 - x);
					x = x1;
					break;
				}
				case 2: {
					absolute += " V" + num(y1);
					relative += " v" + num(y1 - y);
					y = y1;
					break;
				}
				case 3: {
					absolute += " C" + num(x1) + " " + num(y1) + " " + num(x2) + " " + num(y2) + " " + num(x3) + " "
							+ num(y3);
					relative += " c" + num(x1 - x) + " " + num(y1 - y) + " " + num(x2 - x) + " " + num(y2 - y) + " "
							+ num(x3 - x) + " " + num(y3 - y);
					x = x3;
					y = y3;
					break;
				}
				case 4: {
					absolute += " S" + num(x2) + " " + num(y2) + " " + num(x3) + " " + num(y3);
					relative += " s" + num(x2 - x) + " " + num(y2 - y) + " " + num(x3 - x) + " " + num(y3 - y);
					x = x3;
					y = y3;
					break;
				}
				case 5: {
					absolute += " Q" + num(x1) + " " + num(y1) + " " + num(x3) + " " + num(y3);
					relative += " q" + num(x1 - x) + " " + num(y1 - y) + " " + num(x3 - x) + " " + num(y3 - y);
					x = x3;
					y = y3;
					break;
				}
				default: {
					const std::string arc = num(random(1, 80)) + " " + num(random(1, 80)) + " " + num(random(-180, 180))
							+ " " + (random(0, 1) < 0.5 ? "0 " : "1 ") + (random(0, 1) < 0.5 ? "0 " : "1 ");
					absolute += " A" + arc + num(x3) + " " + num(y3);
					relative += " a" + arc + num(x3 - x) + " " + num(y3 - y);
					x = x3;
					y = y3;
					break;
				}
			}
		}
		Path a;
		Path b;
		if (!build(absolute, a) || !build(relative, b)) {
			fail("did not parse", absolute);
		} else if (!sameVertices(a, b, 1e-6)) {
			fail("relative differs from absolute", relative);
		}
	}
}

// The SVG specification's endpoint-to-center conversion (F.6.5), with out-of-range radii scaled up (F.6.6).
struct SpecArc {
	double cx, cy, rx, ry, phi, theta1, deltaTheta;
};

static SpecArc specArc(double x1, double y1, double rx, double ry, double phiDegrees, bool largeArc, bool sweep, double x2
		, double y2) {
	SpecArc arc;
	arc.phi = phiDegrees * 3.14159265358979323846 / 180.0;
	const double c = cos(arc.phi);
	const double s = sin(arc.phi);
	const double hx = (x1 - x2) / 2.0;
	const double hy = (y1 - y2) / 2.0;
	const double x1p = c * hx + s * hy;
	const double y1p = -s * hx + c * hy;
	rx = fabs(rx);
	ry = fabs(ry);
	const double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
	if (lambda > 1.0) {
		rx *= sqrt(lambda);
		ry *= sqrt(lambda);
	}
	const double numerator = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
	const double denominator = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
	const double coefficient = (largeArc != sweep ? 1.0 : -1.0) * sqrt(numerator > 0.0 ? numerator / denominator : 0.0);
	const double cxp = coefficient * rx * y1p / ry;
	const double cyp = -coefficient * ry * x1p / rx;
	arc.cx = c * cxp - s * cyp + (x1 + x2) / 2.0;
	arc.cy = s * cxp + c * cyp + (y1 + y2) / 2.0;
	arc.rx = rx;
	arc.ry = ry;
	const double ux = (x1p - cxp) / rx;
	const double uy = (y1p - cyp) / ry;
	const double vx = (-x1p - cxp) / rx;
	const double vy = (-y1p - cyp) / ry;
	arc.theta1 = atan2(uy, ux);
	double delta = atan2(ux * vy - uy * vx, ux * vx + uy * vy);
	if (!sweep && delta > 0.0) {
		delta -= 2.0 * 3.14159265358979323846;
	} else if (sweep && delta < 0.0) {
		delta += 2.0 * 3.14159265358979323846;
	}
	arc.deltaTheta = delta;
	return arc;
}

// The angle of a point on the arc's ellipse, and how far the point is from the ellipse (0 on it).
static void ellipseFrame(const SpecArc& arc, double x, double y, double& angle, double& offEllipse) {
	const double dx = x - arc.cx;
	const double dy = y - arc.cy;
	const double xp = (cos(arc.phi) * dx + sin(arc.phi) * dy) / arc.rx;
	const double yp = (-sin(arc.phi) * dx + cos(arc.phi) * dy) / arc.ry;
	angle = atan2(yp, xp);
	offEllipse = fabs(sqrt(xp * xp + yp * yp) - 1.0);
}

static void testArc(double x1, double y1, double rx, double ry, double phi, bool largeArc, bool sweep, double x2, double y2) {
	const std::string svg = "M" + num(x1) + " " + num(y1) + " A" + num(rx) + " " + num(ry) + " " + num(phi) + " "
			+ (largeArc ? "1 " : "0 ") + (sweep ? "1 " : "0 ") + num(x2) + " " + num(y2);
	Path path;
	if (!build(svg, path)) {
		fail("arc did not parse", svg);
		return;
	}
	std::vector<Vertex> vertices;
	for (Path::const_iterator it = path.begin(); it != path.end(); ++it) {
		vertices.push_back(it->second);
	}
	// MOVE to the start, the arc's vertices, then an explicit LINE to the endpoint.
	if (vertices.size() < 3) {
		fail("arc produced no vertices", svg);
		return;
	}
	const SpecArc arc = specArc(x1, y1, rx, ry, phi, largeArc, sweep, x2, y2);
	const double scale = std::max(arc.rx, arc.ry);
	const Vertex last = vertices[vertices.size() - 2];
	if (hypot(last.x - x2, last.y - y2) > 1e-6 * scale) {
		fail("arc stops " + num(hypot(last.x - x2, last.y - y2)) + " short of its endpoint", svg);
	}
	double previous = arc.theta1;
	double swept = 0.0;
	for (size_t i = 1; i + 1 < vertices.size(); ++i) {
		double angle;
		double offEllipse;
		ellipseFrame(arc, vertices[i].x, vertices[i].y, angle, offEllipse);
		if (offEllipse > 1e-5) {
			fail("arc vertex " + num(offEllipse) + " off the specified ellipse", svg);
			return;
		}
		double step = angle - previous;
		step -= 2.0 * 3.14159265358979323846 * floor(step / (2.0 * 3.14159265358979323846) + 0.5);
		if (step * arc.deltaTheta < -1e-9) {
			fail("arc sweeps the wrong way", svg);
			return;
		}
		swept += step;
		previous = angle;
	}
	if (fabs(swept - arc.deltaTheta) > 1e-6) {
		fail("arc sweeps " + num(swept) + " radians instead of " + num(arc.deltaTheta), svg);
	}
}

static void testArcs() {
	const bool flags[2] = { false, true };
	for (int largeArc = 0; largeArc < 2; ++largeArc) {
		for (int sweep = 0; sweep < 2; ++sweep) {
			testArc(0, 0, 10, 10, 0, flags[largeArc], flags[sweep], 20, 0);
			testArc(0, 0, 10, 10, 0, flags[largeArc], flags[sweep], 10, 10);
			testArc(0, 0, 25, 10, 30, flags[largeArc], flags[sweep], 20, 15);
			testArc(0, 0, 1, 1, 0, flags[largeArc], flags[sweep], 100, 0);
			testArc(0, 0, 1, 50, 45, flags[largeArc], flags[sweep], 40, -30);
			testArc(5, 5, 1000, 1000, 0, flags[largeArc], flags[sweep], 6, 5);
			testArc(-50, 20, 30, 20, -120, flags[largeArc], flags[sweep], 10, -40);
		}
	}
	for (int round = 0; round < 3000; ++round) {
		const double x1 = random(-200, 200);
		const double y1 = random(-200, 200);
		const double x2 = random(-200, 200);
		const double y2 = random(-200, 200);
		const double rx = random(0.5, 300);
		const double ry = rx * random(0.05, 20);
		testArc(x1, y1, rx, ry, random(-360, 360), random(0, 1) < 0.5, random(0, 1) < 0.5, x2, y2);
	}

	// Degenerate arcs: a zero radius makes a straight line, and an arc to its own start is skipped.
	expectSame("M10 10 A0 5 0 0 1 30 40", "M10 10 L30 40");
	expectSame("M10 10 A5 0 0 1 0 30 40", "M10 10 L30 40");
	expectSame("M10 10 A5 5 0 1 1 10 10", "M10 10 L10 10");
	expectSame("M10 10 A-5 -5 0 0 1 20 10", "M10 10 A5 5 0 0 1 20 10");
}

// How many units in the last place a and b differ by.
static long long ulpDistance(double a, double b) {
	if (a == b) {
		return 0;
	}
	if (a != a || b != b || (a < 0.0) != (b < 0.0)) {
		return 1LL << 60;
	}
	long long ia;
	long long ib;
	memcpy(&ia, &a, sizeof (double));
	memcpy(&ib, &b, sizeof (double));
	return (ia > ib ? ia - ib : ib - ia);
}

/*
	IMPD::Interpreter::parseDouble accumulates digits in double arithmetic, so it is not correctly rounded: "0.3" gives
	0.30000000000000004 where strtod gives 0.29999999999999999. These bounds pin how far from strtod it is today, so a
	change to the parser shows up here and in the rendering tests rather than unnoticed.
*/
static void expectParsed(const std::string& s, long long maxUlps) {
	const IMPD::String string(s);
	double v;
	if (IMPD::Interpreter::parseDouble(string.begin(), string.end(), v) != string.end()) {
		fail("number not fully parsed", s);
		return;
	}
	const long long ulps = ulpDistance(v, strtod(s.c_str(), 0));
	if (ulps > maxUlps) {
		fail(num(static_cast<double>(ulps)) + " ulps from strtod, more than " + num(static_cast<double>(maxUlps)), s);
	}
}

static void testNumbers() {
	char buffer[64];
	for (int i = 0; i < 1000000; i += 7) {
		snprintf(buffer, sizeof (buffer), "%d", i);
		expectParsed(buffer, 0);
		snprintf(buffer, sizeof (buffer), "%d.5", i);
		expectParsed(buffer, 0);
		snprintf(buffer, sizeof (buffer), "%d.25", i);
		expectParsed(buffer, 0);
	}
	expectParsed("9007199254740992", 0);
	for (int i = 0; i < 100000; ++i) {
		snprintf(buffer, sizeof (buffer), "%d.%02d", i / 100, i % 100);
		expectParsed(buffer, 2);
		snprintf(buffer, sizeof (buffer), "-%d.%05d", i / 7, (i * 7919) % 100000);
		expectParsed(buffer, 2);
	}
	for (int round = 0; round < 100000; ++round) {
		snprintf(buffer, sizeof (buffer), "%.17g", ldexp(random(0, 1), static_cast<int>(random(-20, 20))));
		expectParsed(buffer, 8);
	}
	for (int e = -300; e <= 300; e += 7) {
		for (int m = 1; m < 100; m += 3) {
			snprintf(buffer, sizeof (buffer), "%de%d", m, e);
			expectParsed(buffer, 1);
		}
	}
	expectParsed("0e400", 0);
	expectParsed("-0.000e99999999999", 0);
	expectSame("M0.30000000000000000 0 L1e1 0", "M.3 0 L10 0");
	expectError("M1e309 0");
	expectError("M0 0 L1.0e99999999999 0");
}

int main() {
	testCommandsAndSyntax();
	testArcs();
	testNumbers();
	if (failures != 0) {
		printf("ParsingTest: %d failures\n", failures);
		return 1;
	}
	printf("ParsingTest passed\n");
	return 0;
}

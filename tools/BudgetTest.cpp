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
	Tests IVG::IVGBudget: what each kind of work is charged, that a budget exactly as large as a document's work
	renders it like the default budget, that one unit less fails with the budget error, and that work which would be
	large is refused before it is done.
*/

#include <cstdio>
#include <string>
#include <vector>
#include "src/IVG.h"

using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

static int failures = 0;

static void fail(const std::string& what, const std::string& source) {
	++failures;
	printf("FAIL %s: %s\n", what.c_str(), source.c_str());
}

struct QuietExecutor : public IVGExecutor {
	QuietExecutor(Canvas& canvas, IVGBudget& budget) : IVGExecutor(canvas, budget) { }
	virtual void trace(Interpreter& interpreter, const WideString& s) { (void)interpreter; (void)s; }
};

// Renders source, charging `budget`, and returns the error, or "" with the pixels.
static std::string render(const std::string& source, IVGBudget& budget, std::vector<ARGB32::Pixel>& pixels) {
	pixels.clear();
	SelfContainedARGB32Canvas canvas;
	try {
		{
			STLMapVariables topVars;
			QuietExecutor executor(canvas, budget);
			FormatInfo formatInfo;
			Interpreter impd(executor, topVars, formatInfo, budget);
			impd.run(source);
		}
		SelfContainedRaster<ARGB32>* raster = canvas.accessRaster();
		const IntRect bounds = raster->calcBounds();
		for (int y = 0; y < bounds.height; ++y) {
			const ARGB32::Pixel* row = raster->getPixelPointer() + y * raster->getStride();
			pixels.insert(pixels.end(), row, row + bounds.width);
		}
	}
	catch (const IMPD::Exception& x) {
		return x.getError();
	}
	return "";
}

// Measures what source costs, then checks that exactly that much renders it unchanged and one unit less fails.
static void testExact(const std::string& source, double expectedSegments, double expectedPixels) {
	std::vector<ARGB32::Pixel> defaultPixels;
	IVGBudget defaults;
	if (!render(source, defaults, defaultPixels).empty()) {
		fail("does not render with the default budget", source);
		return;
	}
	std::vector<ARGB32::Pixel> pixels;
	IVGBudget measure(1e12, 1e12);
	render(source, measure, pixels);
	const double usedSegments = 1e12 - measure.pathSegmentsLeft;
	const double usedPixels = 1e12 - measure.pixelsLeft;
	if (expectedSegments >= 0.0 && usedSegments != expectedSegments) {
		char buffer[128];
		snprintf(buffer, sizeof (buffer), "charged %.0f path segments, not %.0f", usedSegments, expectedSegments);
		fail(buffer, source);
	}
	if (expectedPixels >= 0.0 && usedPixels != expectedPixels) {
		char buffer[128];
		snprintf(buffer, sizeof (buffer), "charged %.0f pixels, not %.0f", usedPixels, expectedPixels);
		fail(buffer, source);
	}
	IVGBudget exact(usedSegments, usedPixels);
	if (!render(source, exact, pixels).empty() || pixels != defaultPixels) {
		fail("does not render the same with an exact budget", source);
	}
	if (exact.pathSegmentsLeft != 0.0 || exact.pixelsLeft != 0.0) {
		fail("charges differently on a second run", source);
	}
	if (usedSegments > 0.0) {
		IVGBudget lessSegments(usedSegments - 1.0, usedPixels);
		if (render(source, lessSegments, pixels).find("Path segment budget exceeded") == std::string::npos) {
			fail("one path segment less does not fail", source);
		}
	}
	if (usedPixels > 0.0) {
		IVGBudget lessPixels(usedSegments, usedPixels - 1.0);
		if (render(source, lessPixels, pixels).find("Pixel budget exceeded") == std::string::npos) {
			fail("one pixel less does not fail", source);
		}
	}
}

static void expectError(const std::string& source, IVGBudget budget, const std::string& error) {
	std::vector<ARGB32::Pixel> pixels;
	if (render(source, budget, pixels).find(error) == std::string::npos) {
		fail("does not fail with \"" + error + "\"", source);
	}
}

int main() {
	const std::string header = "format IVG-2 requires:IMPD-1\n";

	// What each kind of work is charged.
	testExact(header + "bounds 0,0,100,50", 0, 5000);
	testExact(header + "bounds 0,0,100,50\nwipe red", 0, 10000);
	testExact(header + "bounds 0,0,100,100\nfill red\nrect 10,10,20,30", 5, 10000 + 600);
	testExact(header + "bounds 0,0,100,100\nfill red\nrect -50,-50,100,100", 5, 10000 + 2500);
	testExact(header + "bounds 0,0,100,100\nfill none\npen blue width:2\nrect 10,10,20,30", 5 + 10, 10000 + 704);
	testExact(header + "bounds 0,0,100,100\npen blue width:2 dash:3,2\nellipse 50,50,30", 401, 10000 + 3844);
	// A wipe through a mask is charged the mask raster's bounds, which are the canvas's, as that is what it covers.
	testExact(header + "bounds 0,0,100,100\nmask [ rect 0,0,50,50 ]\nwipe red", 5, 10000 + 10000 + 2500 + 10000);
	testExact(header + "bounds 0,0,100,100\nfill pattern:[bounds 0,0,4,4; wipe red]\nrect 0,0,10,10", 5
			, 10000 + 16 + 16 + 100);
	// The image is charged its texture's output bounds, which include a margin around the 10 x 10 pixels.
	testExact(header + "bounds 0,0,100,100\ndefine image dot [bounds 0,0,10,10; wipe red] resolution:2\n"
			"image 20,20 dot", 0, 10000 + 400 + 400 + 196);
	// A fractional resolution is charged the whole pixels it allocates: 4.5 x 4.5 rounds out to 5 x 5.
	testExact(header + "bounds 0,0,100,100\ndefine image d [bounds 0,0,3,3] resolution:1.5", 0, 10000 + 25);
	testExact(header + "bounds 0,0,100,100\nfill red\npath svg:[M10 10 C20 0 30 20 40 10 Z]\nstar 50,50,7,20,10", 22
			, 10000 + 1584);
	testExact(header + "bounds 0,0,100,100\ndefine font f [ metrics upm:1000 ascent:800 descent:-200 linegap:0; "
			"glyph a 500 [M0,0 L500,0 L500,-500 Z] ]\nfont f size:20\ntext at:10,50 aaa", 3 * 4, 10000 + 3 * 100);

	// Large work is refused before it is done.
	expectError(header + "bounds 0,0,32767,32767", IVGBudget(1e8, 1e8)
			, "Pixel budget exceeded");
	// The defaults keep one document under about 2 GB: no 32767 x 32767 canvas (4 GB) and no 1e8 segment dash (5 GB).
	expectError(header + "bounds 0,0,32767,32767", IVGBudget(), "Pixel budget exceeded");
	expectError(header + "bounds 0,0,100,100\npen black width:1 dash:2\nfill none\nrect 0,0,1e90,190", IVGBudget()
			, "Path segment budget exceeded");
	std::string longPath = "M0 0";
	for (int i = 0; i < 200000; ++i) {
		longPath += " L1 1";
	}
	expectError(header + "bounds 0,0,100,100\nfill red\npath svg:[" + longPath + "]"
			, IVGBudget(1000.0, 1e10), "Path segment budget exceeded");
	expectError(header + "bounds 0,0,100,100\nfill none\npath svg:[" + longPath + "]"
			, IVGBudget(1000.0, 1e10), "Path segment budget exceeded");
	expectError(header + "bounds 0,0,100,100\npen blue dash:0.01,0.01\nrect 0,0,100000,100000"
			, IVGBudget(10000.0, 1e10), "Path segment budget exceeded");
	expectError(header + "bounds 0,0,100,100\npen blue width:1000 caps:round dash:0.01,0.01\nrect 0,0,100,100"
			, IVGBudget(1e6, 1e10), "Path segment budget exceeded");
	std::string zigzag = "M0 0";
	for (int i = 0; i < 1000; ++i) {
		zigzag += (i % 2 == 0 ? " L1 1" : " L2 0");
	}
	expectError(header + "bounds 0,0,100,100\nfill none\npen blue width:1000 joints:curve\npath svg:[" + zigzag + "]"
			, IVGBudget(10000.0, 1e10), "Path segment budget exceeded");
	expectError(header + "bounds 0,0,100,100\ndefine font f [ metrics upm:1000 ascent:800 descent:-200 linegap:0; "
			"glyph a 500 [M0,0 L500,0 L500,-500 Z] ]\nfont f size:20 color:none\ntext at:10,50 "
			+ std::string(10000, 'a'), IVGBudget(1000.0, 1e10)
			, "Path segment budget exceeded");
	expectError(header + "bounds 0,0,100,100\nrepeat 1000 [ fill red; rect 0,0,100,100 ]"
			, IVGBudget(1e8, 1e6), "Pixel budget exceeded");

	// Path::stroke stops near its limit instead of building the whole outline first.
	Path dashed;
	dashed.addRect(0.0, 0.0, 100.0, 100.0);
	dashed.dash(0.01, 0.01, 0.0);
	dashed.stroke(1000.0, Path::ROUND, Path::BEVEL, 2.0, 1.0, 1000);
	if (dashed.size() > 1000 + 500) {
		fail("stroke overshoots its limit", "Path::stroke with limit 1000");
	}

	// Pattern bounds are range-checked once scaled, in double, so a huge left cannot wrap into range.
	expectError(header + "bounds 0,0,100,100\nscale 3\nfill pattern:[bounds 1431655766,0,10,10; wipe red]\n"
			"rect 0,0,10,10", IVGBudget(), "rescaled bounds left out of range");

	// Negative bounds fail without refunding pixels first.
	{
		const std::string source = header + "bounds 0,0,-30000,30000";
		IVGBudget budget(1e8, 1e10);
		std::vector<ARGB32::Pixel> pixels;
		if (render(source, budget, pixels).empty()) {
			fail("negative bounds do not fail", source);
		}
		if (budget.pixelsLeft != 1e10) {
			fail("negative bounds change the pixel budget", source);
		}
	}

	// The interpreter charges its characters to the same budget, so one object limits the whole render.
	expectError(header + "o = x\nREPEAT 40 [ o = {$o $o} ]", IVGBudget(), "Characters limit reached");
	expectError(header + "bounds 0,0,10,10\nwipe red", IVGBudget(1e7, 5e8, 10.0), "Characters limit reached");

	if (failures != 0) {
		printf("BudgetTest: %d failures\n", failures);
		return 1;
	}
	printf("BudgetTest passed\n");
	return 0;
}

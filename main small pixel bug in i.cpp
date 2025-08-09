//
//  main.cpp
//  SVGTest
//
//  Created by Magnus Lidström on 2012-12-27.
//  Copyright (c) 2012 Magnus Lidström. All rights reserved.
//

/*
	FIX:
	
	    // Pass key events on to the menu to handle cmd-keys
    if([[NSApp menu] performKeyEquivalent:event])
        return;

	To the top of the QZ_DoKey() function in video/quartz/SDL_QuartzEvents.m. It gives the menu a chance to process and swallow the event before it's passed to SDL. Cmd-q will "just work" and other things like Cmd-h or Cmd-m will work and the keypresses that trigger them wont make it to the game to screw things up.
*/

#include <cmath>
#include <cctype>
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <stdexcept>
#include "externals/NuX/NuXPixels.h"
#include "SDL/SDL.h"

using namespace NuXPixels;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 250;

namespace NuXXML {

	const char* CDATA_TYPE = "![CDATA[";
	const char* COMMENT_TYPE = "!--";

	class Element;
	
	typedef std::string String;
	typedef String::const_iterator StringIt;
	typedef std::map<String, String> AttributesMap;
	typedef std::vector<Element> ElementsVector;
	typedef char Char;
	
	class BadXMLException : public std::exception {
		public:		virtual const char* what() const throw() { return "Bad XML format"; }
	};

	class Element {
		public:		static String convertStandardEntities(const String& s);												///< Returns a new string where any occurence of the five standard entities (&amp, &lt, &gt, &apos, &quot) or character references are converted to their ASCII equivalent. Exception will be thrown if any entity is encountered that is not one of the five standard entities. Text containing such entities has to be converted with another routine.
		public:		Element(const String& contents);																	///< Use this constructor to create the root element or a text only element.
		public:		Element(const String& tag, const String& contents);													///< Use this constructor to create a tag with optional content (\p content can be empty). \p tag should not include the leading '<' and the trailing '>'.
		public:		String getType() const;																				///< Empty string returned means text element (including entities / char references). "![CDATA[" means raw character data (no entities / char references). "!--" is comment. Rest are ARBITRARY XML types. \p doConvertEntities will convert the five standard entities in attribute values (throws on any other entity).
		public:		String getTag() const;																				///< Returns the raw tag (start tag, empty tag, comment etc), excluding the enclosing '<' and '>'. Will return empty string for text elements. Normally you use getType() to determine the element type, not this function.
		public:		String getContents(bool doConvertEntities) const;													///< Gets contents (the text between start tag and end tag) without parsing child tags. Normally you use this only for text and "![CDATA[" elements. \p doConvertEntities should be true only for text elements and will convert only the 5 standard entities (throws on any other entity).
		public:		void parseAttributes(AttributesMap& attributes, bool doConvertEntities) const;						///< Parses attributes of tag. Will not work with elements of type text, "![CDATA[" or comment.
		public:		void parseContents(ElementsVector& children) const;													///< Parses contents of tag. Will not work with elements of type text, "![CDATA[" or comment.
		protected:	String tag;
		protected:	String contents;
		protected:	static bool isToken(StringIt p, const StringIt& e, const Char* token);
		protected:	static StringIt eatSpace(StringIt p, const StringIt& e);
		protected:	static StringIt eatName(StringIt p, const StringIt& e);
		protected:	static StringIt eatType(StringIt p, const StringIt& e);
		protected:	static StringIt eatString(StringIt p, const StringIt& e);
		protected:	static StringIt parseElement(const StringIt& b, const StringIt& e, StringIt& tagEnd, StringIt& contentEnd);
		protected:	static void badFormat() { throw BadXMLException(); }
	};

	String Element::convertStandardEntities(const String& s) {
		String out;
		StringIt p = s.begin();
		StringIt e = s.end();
		while (p != e) {
			StringIt b = p;
			while (p != e && *p != '&') ++p;
			out.append(b, p);
			if (p != e) {
				assert(*p == '&');
				if (p + 1 != e && *(p + 1) == '#') {
					Char c;
					if (p + 2 != e && *(p + 2) == 'x') {
						p += 3;
						while (p != e && (*p >= '0' && *p <= '9' || *p >= 'A' && *p <= 'F' || *p >= 'a' && *p <= 'f')) {
							c = (c << 4) + (*p <= '9' ? *p - '0' : (*p & ~0x20) - ('A' - 10));
							++p;
						}
					} else {
						p += 2;
						while (p != e && *p >= '0' && *p <= '9') {
							c = c * 10 + (*p - '0');
							++p;
						}
					}
					if (p == e || *p != ';') badFormat();
					++p;
					out += c;
				} else {
					static const Char* STANDARD_ENTITIES[5] = { "amp;", "lt;", "gt;", "apos;", "quot;" };
					static const Char STANDARD_ENTITIES_CHAR[5] = { '&', '<', '>', '\'', '\"' };
					int i = 0;
					const Char* t;
					StringIt q;
					do {
						t = STANDARD_ENTITIES[i];
						q = p + 1;
						while (*t != 0 && q != e && *q == *t) { ++q; ++t; }
					} while (*t != 0 && ++i < 5);
					if (i >= 5) badFormat();
					out += STANDARD_ENTITIES_CHAR[i];
					p = q;
				}
			}
		}
		return out;
	}

	inline Element::Element(const String& contents) : contents(contents) { }
	inline Element::Element(const String& tag, const String& contents) : tag(tag), contents(contents) { }
	inline String Element::getTag() const { return tag; }
	inline String Element::getType() const { return String(tag.begin(), eatType(tag.begin(), tag.end())); }
	inline String Element::getContents(bool doConvertEntities) const { return (doConvertEntities ? convertStandardEntities(contents) : contents); }

	bool Element::isToken(StringIt p, const StringIt& e, const Char* token) {
		while (*token != 0 && p != e && *p == *token) { ++p; ++token; }
		return (*token == 0);
	}

	StringIt Element::eatSpace(StringIt p, const StringIt& e) {
		while (p != e && isspace(*p)) ++p;
		return p;
	}

	StringIt Element::eatName(StringIt p, const StringIt& e) {
		while (p != e && (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == ':'
				|| static_cast<unsigned char>(*p) >= 0x80))
			++p;
		return p;
	}

	StringIt Element::eatType(StringIt p, const StringIt& e) {
		if (p == e) return p;
		else if (isToken(p, e, "!--")) return p + 3;
		else if (isToken(p, e, CDATA_TYPE)) return p + 8;
		else return eatName(((*p == '!' || *p == '?') ? p + 1 : p), e);
	}

	StringIt Element::eatString(StringIt p, const StringIt& e) {
		if (p != e && (*p == '"' || *p == '\'')) {
			StringIt q = p;
			Char c = *q++;
			while (q != e && *q != c) ++q;
			if (q != e) p = q + 1;
		}
		return p;
	}

	void Element::parseAttributes(AttributesMap& attributes, bool doConvertEntities) const {
		attributes.clear();
		StringIt e = tag.end();
		StringIt p = eatType(tag.begin(), e);
		while (p != e && isspace(*p)) {
			StringIt b = eatSpace(p + 1, e);
			p = eatName(b, e);
			if (p == b) break;
			String name(b, p);
			b = eatSpace(p, e);
			if (b == e || *b != '=') badFormat();
			b = eatSpace(b + 1, e);
			p = eatString(b, e);
			if (p == b) badFormat();
			std::string value(b + 1, p - 1);
			if (doConvertEntities) value = convertStandardEntities(value);
			attributes.insert(std::pair<String, String>(name, value));
		}
		if (p != e && *p != '/') badFormat();
	}

	StringIt Element::parseElement(const StringIt& b, const StringIt& e, StringIt& tagEnd, StringIt& contentEnd) {
		if (b == e) badFormat();
		if (isToken(b, e, "!--")) {
			StringIt p = b + 3;
			while (p != e && !isToken(p, e, "-->")) ++p;
			if (p == e) badFormat();
			tagEnd = p + 3;
			contentEnd = tagEnd;
			return tagEnd;
		} else {
			StringIt nameBegin = b;
			Char c = *nameBegin;
			if (c == '!' || c == '?') ++nameBegin;
			StringIt nameEnd = eatName(nameBegin, e);
			if (nameEnd == nameBegin) badFormat();
			StringIt p = nameEnd;
			while (p != e && (*p != '>' || (c == '?' && *(p - 1) != '?'))) {
				if (*p == '"' || *p == '\'') p = eatString(p, e);
				else if (*p != '[') ++p;
				else {
					p = eatSpace(p + 1, e);
					while (p != e && *p != ']') {
						if (*p == '%') {
							p = eatName(p + 1, e);
							if (p == e || *p != ';') badFormat();
							++p;
						} else if (*p == '<') {
							StringIt dummyTagEnd;
							StringIt dummyContentEnd;
							p = parseElement(p + 1, e, dummyTagEnd, dummyContentEnd);
						} else badFormat();
						p = eatSpace(p, e);
					}
					if (p == e) badFormat();
					++p;
				}
			}
			if (p == e) badFormat();
			tagEnd = ++p;
			contentEnd = tagEnd;
			if (c == '!' || c == '?' || *(p - 2) == '/') return tagEnd;
			while (p != e) {
				if (*p != '<') ++p;
				else if (p + 1 != e && *(p + 1) == '/') {
					StringIt q = p + 2;
					while (q != e && nameBegin != nameEnd && *q == *nameBegin) { ++q; ++nameBegin; }
					if (nameBegin != nameEnd) badFormat();
					q = eatSpace(q, e);
					if (q == e || *q != '>') badFormat();
					contentEnd = p;
					return q + 1;
				} else if (isToken(p + 1, e, CDATA_TYPE)) {
					p += 1 + 8;
					while (p != e && !isToken(p, e, "]]>")) ++p;
					if (p == e) badFormat();
					p += 3;
				} else {
					StringIt dummyTagEnd;
					StringIt dummyContentEnd;
					p = parseElement(p + 1, e, dummyTagEnd, dummyContentEnd);
				}
			}
			if (p == e) badFormat();
			return p;
		}
	}

	void Element::parseContents(ElementsVector& children) const {
		children.clear();
		StringIt p = contents.begin();
		StringIt e = contents.end();
		p = eatSpace(p, e);
		while (p != e) {
			StringIt q = p;
			while (q != e && *q != '<') ++q;
			if (p != q) {
				children.push_back(Element(String(p, q)));
				p = q;
			}
			if (p != e && *p == '<') {
				if (isToken(p + 1, e, CDATA_TYPE)) {
					StringIt cdataBegin = p + 1 + 8;
					StringIt cdataEnd = cdataBegin;
					while (cdataEnd != e && !isToken(cdataEnd, e, "]]>")) ++cdataEnd;
					if (cdataEnd == e) badFormat();
					children.push_back(Element(CDATA_TYPE, String(cdataBegin, cdataEnd)));
					p = cdataEnd + 3;
				} else {
					StringIt tagEnd;
					StringIt contentEnd;
					StringIt elementEnd = parseElement(p + 1, e, tagEnd, contentEnd);
					children.push_back(Element(String(p + 1, tagEnd - 1), String(tagEnd, contentEnd)));
					p = elementEnd;
				}
			}
		}
	}
}

template<typename T> std::string toString(const T& value) {
	std::stringstream ss;
	ss << value;
	return ss.str();
}

template<typename T> T fromString(const std::string& str, const T& defaultValue) {
	std::stringstream ss(str);
	T result;
	return ss >> result ? result : defaultValue;
}

std::string coalesce(const std::string& a, const std::string& b) { return (a.empty() ? b : a); }

RLERaster<ARGB32> renderSVG(const std::string& svgSource, const NuXPixels::GammaTable* gammaTable = 0) {	
	using namespace NuXXML;
	
	IntRect bounds(0, 0, 1000, 1000);
	RLERaster<ARGB32> output(bounds); // FIX : size?!
	Element xmlRootElement(svgSource);
	ElementsVector outermostChildren;
	xmlRootElement.parseContents(outermostChildren);
	ElementsVector::const_iterator it = outermostChildren.begin();
	while (it != outermostChildren.end() && it->getType() != "svg") ++it;
	if (it == outermostChildren.end()) throw std::exception(); // FIX
	
	ElementsVector svgChildren;
	it->parseContents(svgChildren);
	for (it = svgChildren.begin(); it != svgChildren.end(); ++it) {
		if (it->getType() == "rect") {
			AttributesMap attributes;
			it->parseAttributes(attributes, false);
			double rectX = fromString<double>(attributes["x"], 0);
			double rectY = fromString<double>(attributes["y"], 0);
			double rectWidth = fromString<double>(attributes["width"], 0);
			double rectHeight = fromString<double>(attributes["height"], 0);
			std::string rectFill = coalesce(attributes["fill"], "black");
			std::string rectStroke = coalesce(attributes["stroke"], "none");
			Path path;
			path.addRect(rectX, rectY, rectWidth, rectHeight);
			output |= Solid<ARGB32>(0xFF000000) * PolygonMask(path, bounds);
//							  <rect x="1" y="1" width="398" height="398" fill="none" stroke="blue" />

		}
	}
	
	return output;
}

typedef std::string::const_iterator StringIt;

StringIt eatSpace(StringIt p, const StringIt& e) {
	while (p != e && isspace(*p)) ++p;
	return p;
}

StringIt eatSpaceAndComma(StringIt p, const StringIt& e) {
	p = eatSpace(p, e);
	if (p != e && *p == ',') p = eatSpace(p + 1, e);
	return p;
}

Vertex toAbsoluteVertex(const Path& path, bool sourceIsRelative, const Vertex& sourceVertex) {
	if (!sourceIsRelative) return sourceVertex;
	else {
		Vertex pos(path.getPosition());
		return Vertex(pos.x + sourceVertex.x, pos.y + sourceVertex.y);
	}
}

bool parseInt(StringIt& p, const StringIt& e, int& v) {
	assert(p <= e);
	StringIt q = p;
	bool negative = (e - q > 1 && (*q == '+' || *q == '-') ? (*q++ == '-') : false);
	int i = 0;
	if (q == e || *q < '0' || *q > '9') return false;
	else {
		p = q;
		for (; p != e && *p >= '0' && *p <= '9'; ++p) i = i * 10 + (*p - '0');
		v = negative ? -i : i;
		return true;
	}
}

bool parseDouble(StringIt& p, const StringIt& e, double& v) {
	assert(p <= e);
	double d = 0;
	StringIt q = p;
	double sign = (e - q > 1 && (*q == '+' || *q == '-') ? (*q++ == '-' ? -1.0 : 1.0) : 1.0);
	if (q == e || (*q != '.' && (*q < '0' || *q > '9'))) return false;
	else {
		p = q;
		while (p != e && *p >= '0' && *p <= '9') d = d * 10.0 + (*p++ - '0');
		if (p != e && *p == '.') {
			double f = 1.0;
			while (++p != e && *p >= '0' && *p <= '9') d += (*p - '0') * (f *= 0.1);
		}
		if (e - p > 1 && (*p == 'E' || *p == 'e')) {
			int i;
			StringIt q = p + 1;
			if (parseInt(q, e, i)) {
				d *= pow(10, static_cast<double>(i));
				p = q;
			}
		}
		v = d * sign;
		return true;
	}
}

bool parseCoordinatePair(StringIt& p, const StringIt& e, Vertex& vertex, bool acceptLeadingComma) {
	StringIt q = (acceptLeadingComma ? eatSpaceAndComma(p, e) : eatSpace(p, e));
	if (!parseDouble(q, e, vertex.x)) return false;
	q = eatSpaceAndComma(q, e);
	if (!parseDouble(q, e, vertex.y)) return false;
	p = q;
	return true;
}

Path svgPath(const std::string& svgSource, double curveQuality = 1.0) {
	Path path;
	
	StringIt p = svgSource.begin();
	StringIt e = svgSource.end();
	Vertex quadraticReflectionPoint;
	Vertex cubicReflectionPoint;
	
	while (p != e) {
		p = eatSpace(p, e);
		if (p != e) {
			char c = *p++;
			bool isRelative = (islower(c) != 0);
			c = toupper(c);
			if (c != 'T') quadraticReflectionPoint = Vertex(0.0, 0.0);
			if (c != 'S') cubicReflectionPoint = Vertex(0.0, 0.0);
			bool first = true;
			switch (c) {
				case 'M': {
					Vertex v;
					if (!parseCoordinatePair(p, e, v, false)) throw std::exception();
					v = toAbsoluteVertex(path, isRelative, v);
					path.moveTo(v.x, v.y);
					while (parseCoordinatePair(p, e, v, true)) {
						v = toAbsoluteVertex(path, isRelative, v);
						path.lineTo(v.x, v.y);
					}
					break;
				}
				
				case 'L': {
					Vertex v;
					if (!parseCoordinatePair(p, e, v, false)) throw std::exception();
					do {
						v = toAbsoluteVertex(path, isRelative, v);
						path.lineTo(v.x, v.y);
					} while (parseCoordinatePair(p, e, v, true));
					break;
				}

				case 'H':
				case 'V': {
					Vertex pos(path.getPosition());
					double v;
					StringIt q = eatSpace(p, e);
					while (parseDouble(q, e, v)) {
						p = q;
						if (c == 'H') {
							if (isRelative) pos.x += v;
							else pos.x = v;
						} else {
							if (isRelative) pos.y += v;
							else pos.y = v;
						}
						path.lineTo(pos.x, pos.y);
						q = eatSpaceAndComma(p, e);
					}
					break;
				}

				case 'C': {
					Vertex bcp;
					Vertex ecp;
					Vertex v;
					StringIt q = p;
					while (parseCoordinatePair(q, e, bcp, first)
							&& parseCoordinatePair(q, e, ecp, true)
							&& parseCoordinatePair(q, e, v, true)) {
						first = false;
						p = q;
						bcp = toAbsoluteVertex(path, isRelative, bcp);
						ecp = toAbsoluteVertex(path, isRelative, ecp);
						v = toAbsoluteVertex(path, isRelative, v);
						cubicReflectionPoint = Vertex(v.x - ecp.x, v.y - ecp.y);
						path.cubicTo(bcp.x, bcp.y, ecp.x, ecp.y, v.x, v.y, curveQuality);
					}
					break;
				}

				case 'S': {
					Vertex ecp;
					Vertex v;
					StringIt q = p;
					while (parseCoordinatePair(q, e, ecp, first)
							&& parseCoordinatePair(q, e, v, true)) {
						first = false;
						p = q;
						Vertex pos(path.getPosition());
						Vertex bcp(pos.x + cubicReflectionPoint.x, pos.y + cubicReflectionPoint.y);
						ecp = toAbsoluteVertex(path, isRelative, ecp);
						v = toAbsoluteVertex(path, isRelative, v);
						cubicReflectionPoint = Vertex(v.x - ecp.x, v.y - ecp.y);
						path.cubicTo(bcp.x, bcp.y, ecp.x, ecp.y, v.x, v.y, curveQuality);
					}
					break;
				}

				case 'Q': {
					Vertex cp;
					Vertex v;
					StringIt q = p;
					while (parseCoordinatePair(q, e, cp, first)
							&& parseCoordinatePair(q, e, v, true)) {
						first = false;
						p = q;
						cp = toAbsoluteVertex(path, isRelative, cp);
						v = toAbsoluteVertex(path, isRelative, v);
						quadraticReflectionPoint = Vertex(v.x - cp.x, v.y - cp.y);
						path.quadraticTo(cp.x, cp.y, v.x, v.y, curveQuality);
					}
					break;
				}

				case 'T': {
					Vertex v;
					StringIt q = p;
					while (parseCoordinatePair(q, e, v, first)) {
						first = false;
						p = q;
						Vertex pos(path.getPosition());
						Vertex cp(pos.x + quadraticReflectionPoint.x, pos.y + quadraticReflectionPoint.y);
						v = toAbsoluteVertex(path, isRelative, v);
						quadraticReflectionPoint = Vertex(v.x - cp.x, v.y - cp.y);
						path.quadraticTo(cp.x, cp.y, v.x, v.y, curveQuality);
					}
					break;
				}
				
				case 'A': {
					Vertex radii;
					double xAxisRotation;
					int largeArcFlag;
					int sweepFlag;
					Vertex v;
					StringIt q = p;
					while (parseCoordinatePair(q, e, radii, first)
							&& (q = eatSpaceAndComma(q, e), parseDouble(q, e, xAxisRotation))
							&& (q = eatSpaceAndComma(q, e), parseInt(q, e, largeArcFlag))
							&& (q = eatSpaceAndComma(q, e), parseInt(q, e, sweepFlag))
							&& parseCoordinatePair(q, e, v, false)) {
						first = false;
						p = q;
						v = toAbsoluteVertex(path, isRelative, v);
						radii.x = fabs(radii.x);
						radii.y = fabs(radii.y);
						if (radii.x >= EPSILON && radii.y >= EPSILON) {
							Vertex startPos(path.getPosition());
							Vertex endPos(v);
							AffineTransformation affineReverse;
							if (xAxisRotation != 0.0) {
								affineReverse = AffineTransformation().rotate(xAxisRotation * (PI2 / 360.0));
								AffineTransformation affineForward = affineReverse;
								bool success = affineForward.invert();
								assert(success);
								startPos = affineForward.transform(startPos);
								endPos = affineForward.transform(endPos);
							}
							double largeArcSign = (largeArcFlag != 0 ? 1.0 : -1.0);
							double sweepSign = (sweepFlag != 0 ? largeArcSign : -largeArcSign);
							double dx = endPos.x - startPos.x;
							double dy = endPos.y - startPos.y;
							double aspectRatio = radii.x / radii.y;
							double l = dx * dx + (aspectRatio * dy) * (aspectRatio * dy);
							double b = std::max(4.0 * radii.x * radii.x / l - 1.0, EPSILON);
							double a = sweepSign * sqrt(b * 0.25);
							double centerX = startPos.x + dx * 0.5 + a * dy * aspectRatio;
							double centerY = startPos.y + dy * 0.5 - a * dx / aspectRatio;
							double sweepRadians = sweepSign * (largeArcSign * PI + PI - acos((b - 1.0) / (1.0 + b)));
							if (xAxisRotation != 0.0) {
								Path tempPath;
								tempPath.lineTo(startPos.x, startPos.y);
								tempPath.arcSweep(centerX, centerY, sweepRadians, aspectRatio, curveQuality);
								tempPath.transform(affineReverse);
								path.append(tempPath);
							} else {
								path.arcSweep(centerX, centerY, sweepRadians, aspectRatio, curveQuality);
							}
						}
						path.lineTo(v.x, v.y);
					}
					break;
				}
				
				case 'Z': path.close(); break;
				
				default: throw std::exception();
			}
		}
	}
	return path;
}

int main(int argc, char** argv)
{
	try {
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
			throw std::exception(); // FIX
		}
	
		SDL_WM_SetCaption("NuXPixels Tests", NULL);

		SDL_Surface* screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_SWSURFACE);
		if (screen == 0) {
			throw std::exception(); // FIX
		}
	
		if (screen->format->BitsPerPixel != 32) throw std::exception(); // FIX
		if (screen->format->BytesPerPixel != 4) throw std::exception(); // FIX
		if ((screen->format->Rmask >> screen->format->Rshift) != 0xFF) throw std::exception(); // FIX
		if ((screen->format->Gmask >> screen->format->Gshift) != 0xFF) throw std::exception(); // FIX
		if ((screen->format->Bmask >> screen->format->Bshift) != 0xFF) throw std::exception(); // FIX
	
		static GammaTable myGamma(1.41);

		std::ifstream instream("test.svg");
		if (!instream.good()) throw std::exception();
		std::string chars;
		while (!instream.eof()) {
			if (!instream.good()) throw std::exception();
			char buffer[4096];
			instream.read(buffer, 4096);
			chars += std::string(buffer, static_cast<std::string::size_type>(instream.gcount()));
		}
		
		{
			if (SDL_MUSTLOCK(screen)) {
				if (SDL_LockSurface(screen) < 0) {
					throw std::exception(); // FIX
				}
			}
			{
				IntRect bounds(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
				ARGB32::Pixel* pixels = reinterpret_cast< ARGB32::Pixel* >(screen->pixels);
				Raster<ARGB32> raster32(pixels, screen->pitch / 4, bounds, true);
				RLERaster<ARGB32> rleRaster(bounds);
				//Path p(svgPath("M6701 645c-247,109 -512,183 -790,216 284,-170 502,-440 604,-761 -266,158 -560,272 -873,334 -251,-267 -608,-434 -1004,-434 -759,0 -1375,616 -1375,1375 0,108 12,213 36,313 -1143,-57 -2156,-605 -2834,-1437 -118,203 -186,439 -186,691 0,477 243,898 612,1144 -225,-7 -437,-69 -623,-172 0,6 0,11 0,17 0,666 474,1222 1103,1348 -115,31 -237,48 -362,48 -89,0 -175,-9 -259,-25 175,546 683,944 1284,955 -471,369 -1063,589 -1708,589 -111,0 -220,-7 -328,-19 608,390 1331,618 2108,618 2529,0 3912,-2095 3912,-3912 0,-60 -1,-119 -4,-178 269,-194 502,-436 686,-712z"));
				//p.transform(AffineTransformation().scale(0.03).translate(20, 20));
				Path p;
				/*
				p.addRoundedRect(250, 280, 140, 120, 10.0, 10.0);
				p.moveTo(400, 400);
				p.arcSweep(300, 300, -1.0, 1.0);
				p.arcSweep(500, 300, 1.0, 1.0);
				p.moveTo(200,500);
				p.arcSweep(150, 450, PI2 * 0.99, 1.0);
				*/
				/*
				p.append(svgPath("M200,300 Q400,50 600,300 T1000,300"));
				p.append(svgPath("M  210 130      C  145 130     110  80     110  80       S  75  25      10  25          m    0 105      c   65   0      100 -50     100 -50       s   35 -55     100 -55   "));
				p.append(svgPath("   M 240  90      c 0  30    7  50    50  0       c 43  -50    50  -30    50  0       c 0  83    -68  -34    -90  -30       C 240  60    240  90    240  90   z  "));
				p.append(svgPath("M80 170   C100 170 160 170 180 170Z"));
				p.append(svgPath("M5 260 C40 260 60 175  55 160 c  -5  15 15 100 50 100Z"));
				p.append(svgPath("m 200 260      c  50 -40     50 -100     25 -100       s -25  60     25  100  "));
				p.append(svgPath("   M 360 100   C 420 90 460 140 450 190"));
				p.append(svgPath("M360 210      c   0  20    -16  36    -36  36       s -36 -16    -36 -36       s  16 -36     36 -36    s  36  16     36  36   z  "));
				p.append(svgPath("m 360  325  c -40  -60     95 -100     80    0      z  "));*/
	/*			p.append(svgPath("M300,200 h-150 a150,150 0 1,0 150,-150 z"));
				p.append(svgPath("M275,175 v-150 a150,150 0 0,0 -150,150 z"));
				p.append(svgPath("M600,350 l 50,-25  a25,25 -30 0,1 50,-25 l 50,-25      a25,50 -30 0,1 50,-25 l 50,-25       a25,75 -30 0,1 50,-25 l 50,-25      a25,100 -30 0,1 50,-25 l 50,-25"));*/

const double curveQuality = 1.0;
Path tPath(svgPath("M2633.875,228.125c-1.625,3.5-12.25,2.625-15.75,6.125c3.098,30.593,4.516,45.713,4.5,48.75c-0.02,3.036-1.678,13.533-4.625,32.125c-9.25-2.5-34.846-18.893-41.375-23c2.203-4.868,11.879-29.292,12.125-30.875c1.375-7.25,2.314-15.627,1.75-28.125c-5.125-1.375-21.455-5.773-26.75-9l1.375-5.375c0.936,0.326,15.75,6.625,24.625,5.25c-1.963-5.016-4.648-8.692-10.375-15.5c3.65-2.486,10.604-5.272,22.32-9.362c8.555,6.362,10.555,16.737,16.68,26.862C2618.375,226,2630.75,227.625,2633.875,228.125z", curveQuality));
Path rPath(svgPath("M2719.125,303.875c-2.125,4.125-38.748,9.741-40.25,9.375c0.844-3.464,2.125-13.25,2.5-16.375c7.625-19.625,9.375-26.625,18.5-42.25c-8.375-2.25-20.361-3.172-34.447-4.2l-3.303,1.075l11.375,43.875l-2.625,5.875c-11.348,1.382-21.221,2.701-27.75,2.75c-1.5-24-21-79.5-27.199-94.854c0.699-1.521,1.449-3.271,1.449-3.271c5.986,0.399,10.313-0.575,14.125-3.125c0.721-1.178,3.375-4.125,4-4.875c0.838-0.471,14-3.5,19.5-4c-8.25,11.25,17.375,15.875,38.5,26c14.25,6.5,12.75,21.625-8.625,25.875c0.436,0.591,44,4.625,47.875,4.875C2721.375,263.375,2718.75,298.75,2719.125,303.875zM2673.125,236.75c5.75-10.25-13-13.75-25.625-22.75c1.314,3.507,10.936,25.366,12.25,28.875C2663.52,241.673,2672.125,239,2673.125,236.75z", curveQuality));
Path aPath(svgPath("M2581.25,307.125c-4.875-0.25-31.25,3.125-42.875,2.875c-3.5-9.125-3.875-13.375-9.875-35.625l-34,3.75c-2.512,2.572-8.094,8.438-13.5,18.125c-14.25,4.125-30.625,8.125-37.125,7.75c23.682-25.209,69.25-61.625,83.25-97c8.457-0.251,16.25-1.625,20.125-3.375c3.375,6.875,9.125,6.25,12,6.375C2553.375,260.75,2568.625,282.75,2581.25,307.125zM2525.5,263.375c-0.486-5.719,0.514-11.344,0.25-22.125c-3.959,4.344-11.34,11.428-20.25,25.375C2515.893,266.173,2522.275,264.712,2525.5,263.375z", curveQuality));
Path ePath(svgPath("M2493.125,292.75c-13.5-4.375-42,4.625-51.625,21.375c-12.299-5.999-36.375-17.25-40.375-18.875c25.75-47,17.25-61.875,0.75-94.75c-1.125-2.875,4.875-5.875,5.75-5.75c2.125,0.625,1.5,0.25,5.5,1.125c21.75,13.625,51.375,18.375,57.625,18.125c2.375,5.75,3.875,9.25,8.875,19.125c-4.502,0.639-6.553,1.068-8.875,0.5c-2.871-0.701-11.129-3.011-31.875-11.75L2436.25,223c1.592,4.153,1.055,7.006,2.625,11.25c3.121,5.591,8.5,9.125,18,10.125c-2.244,1.479-7.25,5.375-9.375,8.75c-2.082,2.292-0.625,14.875,1.25,34.375c5.875-5.625,18.375-8.5,25.75-8.375l0.32-3.978c-0.1-0.796-7.541-12.012-8.07-13.397c2.705-0.4,7.379-0.564,14.75-0.5C2484.375,268.125,2488,277.25,2493.125,292.75z", curveQuality));
Path bPath(svgPath("M2409.75,271.125c-1.633,4.913-10.158,12.166-16.5,17.375c-6.568,5.542-14,8.211-21.834,10.643c-4.75,1.062-9.428,2.14-14.041,3.232c-6.797,1.721-11,3.625-10.125,6.375c-2.477-0.733-5.5-1.5-7.5-1.875c11.25-46,3.25-75.292-1.75-90.875c2.833-2.417,7.917-4.5,10.167-6c1.166-1.417,2.416-2.333,1.833-5.833c0.667-2.583,6-4.083,10.5-4.917c1.125,0.25,2.5,0.625,3.625,0.875c-0.042,0.875,0.5,2.625,1.5,3.75c5.375,8.5,29,19.25,27.75,26.375c-0.125,3.25-9.125,10.125-13.375,11.875c16.617,6.667,20.924,8.733,24.625,11.375C2412.135,258.908,2411.375,264.375,2409.75,271.125zM2360.248,218.04c0.762,2.215,4.342,16.713,5.127,18.835C2369,239,2388,228.625,2360.248,218.04zM2381.625,255.75c-2.25-2.25-10.25-3.625-11.875-4c-0.75,1-1,1.5-1.75,3c3.125,4.375-1.75,24.5-2.125,28.875c1.625-0.5,2.213-0.274,3.625-0.125C2391.375,270.625,2383.625,258.625,2381.625,255.75z", curveQuality));
Path kPath(svgPath("M2948.125,305.875c-4.043,1.137-35.205,10.363-39.25,11.5c-3.25-5.75-9.625-17.5-5.25-64.375c-4.188-0.056-22.031-6.973-25.625-6.5c3.625,7.375,18.125,33.375,19.25,49.375c-0.625,2.375-18.5,13.75-27.375,17c-17.125-50.375-23.25-76.125-51.766-114.5c5.635-1.133,14.203-2.708,25.705-4.728l6.561,1.603c8.125,12.375,17.625,30.25,21.25,39.75c5.209,0.02,9.469-1.529,14-5.25L2865.75,198c5.254-0.165,14.426-0.699,25.875-0.125l17.875,32.25l-0.25,3c-3.553-0.289-21.787,5.644-24.375,6.75c9.605,0.996,33.662,10.888,46.625,10C2935.25,270.125,2944.75,294,2948.125,305.875z", curveQuality));
Path cPath(svgPath("M2837,301.75c-2.875,1.625-7.875,4.75-26.25,8.125l-7.125-1.625c-27.875-31.875-47-69.875-39.875-100.125c8.791-1.618,20.744-5.697,37.75-11.875c2.75,11.25,8.25,22.125,15.125,30c-4.182,1.104-9.467,2.656-16.375,4.25c-3-1.375-10.25-7.625-12.25-9.875c-1.5,0.75-1.5,3.875-1.5,8.25c4.875,28,22.75,53.875,28,58.25c3.5-5.625,1.375-16.25-2.25-25.875c3.475-0.213,11.176-1.813,21.75-4.25l5.875,1.125C2841.875,266.375,2837.375,291.25,2837,301.75z", curveQuality));
Path iPath(svgPath("M2752.875,216.875c0,0-23.057,0.7-23.875,0.5c-3.875-1.25-18.625-15-22.75-20.125c8.25-1.625,28.234-3.402,28.234-3.402l6.391,1.402L2752.875,216.875zM2779.875,306.125c-1.75,3.625-2.5,6.5-5.625,11.375c-3.311-1.872-31.078-17.036-34.25-18.875c13.25-31.625,1.625-62.375-1.5-70.125c7.865-0.78,15.861-1.458,23.625-0.625C2778.75,245.5,2783.25,286.5,2779.875,306.125z", curveQuality));
Path beatrickPath(iPath);
beatrickPath.append(cPath);
beatrickPath.append(kPath);
beatrickPath.append(bPath);
beatrickPath.append(ePath);
beatrickPath.append(aPath);
beatrickPath.append(rPath);
beatrickPath.append(tPath);
Path redPath(svgPath("M2369.891,195.781c2.36,2.052,19.443,14.469,19.443,14.469s-0.166,2.583-0.166,3.333c2.416,5.083,19,14.417,27.5,22.417c6.932,5.745,24.665-6.583,24.665-6.583l33-20.917l12.25,13.5l10.5,21.833l68.25-39l4.084,10.917l5.666,1.167l-3.833-9.5l27.416,1.75l67.917-21.75l-0.416,14.333l-0.917,1.167l42.667,31.583l-2.25,8.75l23.083,3.583l-5.75-9.667c0,0,0.167-0.667,0.25-1.083c-5,0.917-18.167-12.833-31.333-27.333c0.25-0.75,4.5-15.083,4.5-15.083l57.333,18.417l5.5-8.667l39.833,1.75l9.5-10.417l42.25,2.667l5.25-4.583l98.833,116.917c0,0-69.083,20.75-70.083,21c-1.917-2.583-5.917-15.917-5.917-15.917l-5.5,2.5l-24.999,9.833l-7.667-21.333c0,0-20.667,11.5-22.5,12.333s-19.834,4.917-19.834,4.917l-9.916-2.167l-6.083-7.083l-6.251,11l-9.499,6.917c0,0-38.417-21.083-41.417-22.583c12-23.083,7.082-43.25,6.916-44.167c1.595-0.766-4.75,2.833-4.75,2.833s-5.416,32.083-2.75,37.25c-4.166,7.917-6.834,11.667-11.5,13.5c-5.166,2.083-17.166,4.583-29.666,6.75c-3,0.667-5.584,0.75-7.417,0.5s-6-1.333-6-1.333l2.25-9.75c0,0-22.75,2.75-33.25,2.583c-0.792-6.067-0.265-2.662-1.166-7.75c-0.584-15.917-7.126-35.001-9.5-40.75c0.167,1.583,2.083,18.083,1.916,23.583c-0.167,1.75-4.333,34.083-5.333,37.333c-16.333-4.75-31.917-15.167-31.917-15.167l3.334,5.583l-10.5,0.083c0,0-31.917,2.75-42.917,2.333c-0.25-0.25-8.083-6.75-8.333-7.083c-3.334-6.25-7.5-25.083-7.5-25.083l-18.5,2.167l-0.501,0.583l5.25,16.417l-13.916-4.25l-2.417,3.833c0,0-25,6.083-28.75,6.833c-2.167,1.083-5.25,5.083-5.75,6.417c-0.333,0.75-2.666,6.75-2.666,6.75s-50.815-24.344-50.834-24.333c-3.109,1.861-10.677,5.822-15.925,7.176c-0.654,0.197-16.814,4.272-16.814,4.272l3.24,7.636L2357,318.125l-23.953-6.906 M2569.5,241.917c0.083,5.667,0.083,21.25,7.416,39.167c9.167-21.333,9.167-26.417,9.834-33.083C2582.5,247.583,2573.584,244.167,2569.5,241.917z M2670.333,267.333l7.5,26.25l10.5-24.667L2670.333,267.333z M2796.417,247.167l5.583,12l2.583-2.917l19.333-1.083l-7.249-15l-2.75,2.417L2796.417,247.167z"));
				 p = redPath;
				 p.transform(AffineTransformation().translate(-2200, -150));
				/*
	p.lineTo(150, 50);
	p.moveTo(900, 100);
	p.moveTo(900, 100);
	p.moveTo(900, 100);
	p.moveTo(100, 100);
	Path p2;
	p2.lineTo(200, 200);
	p2.lineTo(200, 100);
	p2.lineTo(100, 100);
	p.append(p2);
	p.close();
	p.close();
	p.close();
	p.close();
	p.lineTo(50, 120);
	p.lineTo(60, 150);
	p.moveTo(300, 400);
	p.cubicTo(400, 180, 70, 210, 300, 460);
	p.lineTo(500, 450);
	p.lineTo(350, 490);
	p.addRect(700, 520, 150, 130);
	p2.clear();
	p2.addCircle(400, 520, 150);
	p2.addEllipse(50, 520, 150, 250);
	p.append(p2);
	p.close();
//	p.addArc(550, 230, 150, 70, 0.5, 2 * PI);
	p.addRect(600, 420, 150, 130);
	p.addLine(500, 25, 600, 40);
	p.addLine(600, 40, 650, 10);
//	p.transform(AffineTransformation().scale(4).translate(-400,-400));*/

	/*
	p.lineTo(200, 560);
//	p.moveTo(250, 590);
//	p.lineTo(100, 100);
	p.close();
	p.lineTo(50, 140);*/

			//	p.dash(8.0, 16.0);
		//		p.stroke(2.0);
		//		p.stroke(1.0, Path::BUTT, Path::CURVE);
				AffineTransformation xlate(AffineTransformation().translate(-2200, -150));
				Vertex sp = xlate.transform(Vertex(2742.1494, 196.7764));
				Vertex ep = xlate.transform(Vertex(2743.7817, 316.4407));
				LinearAscend myRamp(sp.x, sp.y, ep.x, ep.y);
				Gradient<ARGB32>::Stop myGradientStops[5] = { 0.0, 0xFF78CCCB, 0.1012, 0xFF74C1C8, 0.2942, 0xFF6AA8C2, 0.5562, 0xFF5C82B5, 0.8182, 0xFF4F5DAA };
				Gradient<ARGB32> myGradient(5, myGradientStops);
			int startTicks = SDL_GetTicks();
			int stopTicks = startTicks + 2000;
			int sdlTicksNow;
			int iterations = 0;
			do {
				raster32 = Solid<ARGB32>(0xFFFFFFFF)
					| Solid<ARGB32>(0xFF6C0D0E) * myGamma[PolygonMask(Path(redPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFED1C24) * myGamma[PolygonMask(Path(beatrickPath).stroke(11, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(iPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(iPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(cPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(cPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(kPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(kPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(bPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(bPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(ePath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(ePath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(aPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(aPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(rPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(rPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)]
					| myGradient[myRamp] * myGamma[PolygonMask(Path(tPath).transform(xlate).closeAll(), bounds)]
					| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(tPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)];
				//		| Solid<ARGB32>(0xFFFFFF00) * myGamma[PolygonMask(outline.stroke(5.0, Path::ROUND, Path::CURVE).stroke(1), bounds)]
					//	| Solid<ARGB32>(0xFFFFFF00) * myGamma[~PolygonMask(Path(p).stroke(4.0, Path::ROUND, Path::CURVE), bounds) * PolygonMask(Path(p).stroke(6.0, Path::ROUND, Path::CURVE), bounds)]
				//		| SolidRect<ARGB32>(0xFFFF3030, IntRect(150-3,130-3,5,5))
						;
				//raster32 = rleRaster;
				++iterations;
				sdlTicksNow = SDL_GetTicks();
			} while ((sdlTicksNow - stopTicks) < 0);
			std::cout << "FPS: " << (1000.0 * static_cast<double>(iterations) / (sdlTicksNow - startTicks)) << std::endl;
			
				for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
					pixels[i] = (((pixels[i] >> 16) & 0xFF) << screen->format->Rshift)
							| (((pixels[i] >> 8) & 0xFF) << screen->format->Gshift)
							| ((pixels[i] & 0xFF) << screen->format->Bshift);
				}
			}
			
			if (SDL_MUSTLOCK(screen)) {
				SDL_UnlockSurface(screen);
			}
			SDL_UpdateRect(screen, 0, 0, 0, 0);
		}
			
		bool doQuit = false;
		do {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
					case SDL_QUIT: doQuit = true; break;
					case SDL_MOUSEBUTTONDOWN: doQuit = true; break;
				}
			}
		} while (!doQuit);
	
		SDL_Quit();
		return 0;
	}
	catch (const std::exception& x) {
		std::cout << "Exception" << x.what() << std::endl;
		return 1;
	}
	catch (...) {
		std::cout << "General exception" << std::endl;
		return 1;
	}
}


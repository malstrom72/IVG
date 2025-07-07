#include "NuXAngelCode.h"

namespace NuXAngelCode {

static std::string getOptional(const StringMap& map, const std::string& key)
{
	StringMap::const_iterator it = map.find(key);
	if (it == map.end()) {
		return std::string();
	} else {
		return it->second;
	}
}

static std::string getRequired(const StringMap& map, const std::string& key)
{
	StringMap::const_iterator it = map.find(key);
	if (it == map.end()) {
		throw Exception(std::string("Missing '" + key + "' value in AngelCode Font File"));
	} else {
		return it->second;
	}
}

static int toInt(const std::string& s)
{
	return atoi(s.c_str());
}

BMCharacter::BMCharacter()
	: x(0)
	, y(0)
	, width(0)
	, height(0)
	, offsetX(0)
	, offsetY(0)
	, advance(0)
	, page(0)
	, channel(0)
{
}

BMCharacter::BMCharacter(const StringMap& params)
{
	x = toInt(getRequired(params, "x"));
	y = toInt(getRequired(params, "y"));
	width = toInt(getRequired(params, "width"));
	height = toInt(getRequired(params, "height"));
	offsetX = toInt(getRequired(params, "xoffset"));
	offsetY = toInt(getRequired(params, "yoffset"));
	advance = toInt(getRequired(params, "xadvance"));
	page = toInt(getRequired(params, "page"));
	channel = toInt(getOptional(params, "channel"));
}

BMFont::BMFont()
	: size(0)
	, bold(false)
	, italic(false)
	, unicode(false)
	, stretchHeight(0)
	, smoothing(false)
	, aaFactor(0)
	, paddingTop(0)
	, paddingRight(0)
	, paddingBottom(0)
	, paddingLeft(0)
	, spacingHorizontal(0)
	, spacingVertical(0)
	, lineHeight(0)
	, baseOffset(0)
	, textureWidth(0)
	, textureHeight(0)
	, packed(false)
{
}

static const char* parseLine(const char* b, const char* e, std::string& tag, StringMap& params)
{
	// Find end-of-line.
	
	const char* l = b;
	while (l != e && *l != '\r' && *l != '\n') {
		++l;
	}
	
	// Read tag.
	
	const char* p = b;
	while (p != l && *p != ' ') {
		++p;
	}
	tag = std::string(b, p);
	
	while (p != l && *p == ' ') {
		++p;
	}
	
	// Read params.
	
	while (p != l) {
		b = p;
		while (p != l && *p != '=') {
			++p;
		}
		std::string key = std::string(b, p);
		if (p != l) {
			++p;
			std::string value;
			if (*p == '"') {
				++p;
				b = p;
				while (p != l && *p != '"') {
					++p;
				}
				value = std::string(b, p);
				if (p != l) {
					++p;
				}
			} else {
				b = p;
				while (p != l && *p != ' ') {
					++p;
				}
				value = std::string(b, p);
			}
			while (p != l && *p == ' ') {
				++p;
			}
			params.insert(std::pair<std::string, std::string>(key, value));
		}
	}
	
	// Find start of next non-empty line.
		
	while (l != e && (*l == '\r' || *l == '\n')) {
		++l;
	}
	
	return l;
}

BMFont::BMFont(const char* b, const char* e)
	: size(0)
	, bold(false)
	, italic(false)
	, unicode(false)
	, stretchHeight(0)
	, smoothing(false)
	, aaFactor(0)
	, paddingTop(0)
	, paddingRight(0)
	, paddingBottom(0)
	, paddingLeft(0)
	, spacingHorizontal(0)
	, spacingVertical(0)
	, lineHeight(0)
	, baseOffset(0)
	, textureWidth(0)
	, textureHeight(0)
	, packed(false)
{
	bool didCommon = false;
	
	while (b != e) {
		std::string tag;
		StringMap params;
		b = parseLine(b, e, tag, params);
		
		if (tag == "info") {
			
			faceName = getOptional(params, "face");
			size = toInt(getOptional(params, "size"));
			bold = (toInt(getOptional(params, "bold")) != 0);
			italic = (toInt(getOptional(params, "italic")) != 0);
			charSet = getOptional(params, "charset");
			unicode = (toInt(getOptional(params, "unicode")) != 0);
			stretchHeight = toInt(getOptional(params, "stretchH"));
			smoothing = (toInt(getOptional(params, "smooth")) != 0);
			aaFactor = toInt(getOptional(params, "aa"));
			sscanf(getOptional(params, "padding").c_str(), "%d,%d,%d,%d", &paddingTop, &paddingRight, &paddingBottom, &paddingLeft);
			sscanf(getOptional(params, "spacing").c_str(), "%d,%d", &spacingHorizontal, &spacingVertical);
		
		} else if (tag == "common") {
		
			lineHeight = toInt(getRequired(params, "lineHeight"));
			baseOffset = toInt(getRequired(params, "base"));
			textureWidth = toInt(getOptional(params, "scaleW"));
			textureHeight = toInt(getOptional(params, "scaleH"));
			packed = (toInt(getOptional(params, "packed")) != 0);
			didCommon = true;
		
		} else if (tag == "page") {
		
			int id = toInt(getRequired(params, "id"));
			std::map<int, std::string>::iterator it = pages.find(id);
			if (it != pages.end()) {
				throw Exception("Duplicate page id found in AngelCode Font File");
			}
			pages.insert(it, std::pair<int, std::string>(id, getOptional(params, "file")));
		
		} else if (tag == "char") {
			
			int id = toInt(getRequired(params, "id"));
			std::map<int, BMCharacter>::iterator it = characters.find(id);
			if (it != characters.end()) {
				throw Exception("Duplicate character id found in AngelCode Font File");
			}
			characters.insert(it, std::pair<int, BMCharacter>(id, BMCharacter(params)));
		
		} else if (tag == "kerning") {
		
			std::pair<int, int> kerningPair(toInt(getRequired(params, "first")), toInt(getRequired(params, "second")));
			std::map< std::pair<int, int>, int>::iterator it = kernings.find(kerningPair);
			int amountInt = toInt(getRequired(params, "amount"));
			if (it != kernings.end() && amountInt != it->second) {
				throw Exception("Duplicate kerning pair (with different amount) found in AngelCode Font File");
			} else {
				kernings.insert(it, std::pair< std::pair<int, int>, int>(kerningPair, amountInt));
			}

		}
	}
	
	if (!didCommon) {
		throw Exception("Missing common tag in AngelCode Font File");
	}
}

} /* namespace NuXAngelCode */

#ifndef NuXAngelCode_h
#define NuXAngelCode_h

#include <string>
#include <map>

namespace NuXAngelCode {
	
typedef std::map<std::string, std::string> StringMap;

class Exception : public std::exception {
	public:		Exception(const std::string& errorString) : errorString(errorString) { }
	public:		virtual const char *what() const throw() { return errorString.c_str(); }
	public:		std::string getErrorString() const { return errorString; }
	public:		virtual ~Exception() throw() { }
	private:	std::string errorString;
};

class BMCharacter {
	public:		BMCharacter();									///< Default constructor, just initializes all fields to 0.
	public:		BMCharacter(const StringMap& params);			///< Construct character info from a map of strings.
	public:		int x;											///< The left position of the character image in the texture.
	public:		int y;											///< The top position of the character image in the texture.
	public:		int width;										///< The width of the character image in the texture.
	public:		int height;										///< The height of the character image in the texture. 
	public:		int offsetX;									///< How much the current position should be offset when copying the image from the texture to the screen.
	public:		int offsetY;									///< How much the current position should be offset when copying the image from the texture to the screen.
	public:		int advance;									///< How much the current position should be advanced after drawing the character.
	public:		int page;										///< The texture page where the character image is found.
	public:		int channel;									///< The texture channel where the character image is found (1 = blue, 2 = green, 4 = red, 8 = alpha).
};

class BMFont {
	public:		BMFont();										///< Default constructor, just initializes all fields to 0.
	public:		BMFont(const char* b, const char* e);			///< Construct entire font from string. \p b should point to string beginning, \p e should point to ending (i.e. one element after last character).
	public:		std::string faceName;							///< This is the name of the true type font.
	public:		int size;										///< The size of the true type font.
	public:		bool bold;										///< The font is bold.
	public:		bool italic;									///< The font is italic.
	public:		std::string charSet;							///< The name of the charset used (when not unicode).
	public:		bool unicode;									///< Set to true if it is the unicode charset.
	public:		int stretchHeight;								///< The font height stretch in percentage. 100% means no stretch.
	public:		bool smoothing;									///< Set to true if smoothing was turned on.
	public:		int aaFactor;									///< The supersampling level used. 1 means no supersampling was used.
	public:		int paddingTop;									///< The padding for each character (top).
	public:		int paddingRight;								///< The padding for each character (right).
	public:		int paddingBottom;								///< The padding for each character (bottom).
	public:		int paddingLeft;								///< The padding for each character (left).
	public:		int spacingHorizontal;							///< The spacing for each character (horizontal).
	public:		int spacingVertical;							///< The spacing for each character (vertical).
	public:		int lineHeight;									///< This is the distance in pixels between each line of text.
	public:		int baseOffset;									///< The number of pixels from the absolute top of the line to the base of the characters.
	public:		int textureWidth;								///< The width of the texture, normally used to scale the x pos of the character image.
	public:		int textureHeight;								///< The height of the texture, normally used to scale the y pos of the character image.
	public:		bool packed;									///< Set to true if the monochrome characters have been packed into each of the texture channels.
	public:		std::map<int, std::string> pages;				///< Names of a texture files. There is one for each page in the font. (Key is page id, value is texture name.)
	public:		std::map<int, BMCharacter> characters;			///< Characters in the font. There is one for each included character in the font. (Key is character id, value is character class.)
	public:		std::map< std::pair<int, int>, int> kernings;	///< The kerning information is used to adjust the distance between certain characters, e.g. some characters should be placed closer to each other than others. (Key is pair of first and second character id's, value is horizontal adjustment for this pair.)
};
	
} /* namespace NuXAngelCode */

#endif

# Throwing Sites

This document lists all lines in the IVG project that contain `throw` statements or exception specifications.

## IMPD

```
src/IMPD.cpp:154:			if (!labeled.insert(kv).second) interpreter.throwBadSyntax(String("Duplicate label: ") + kv.first);
src/IMPD.cpp:192:		interpreter.throwBadSyntax(String("Missing indexed argument ") + Interpreter::toString(index + 1));
src/IMPD.cpp:205:	if (it == labeled.end()) interpreter.throwBadSyntax(String("Missing argument: ") + label);
src/IMPD.cpp:209:void ArgumentsContainer::throwIfNoneFetched() {
src/IMPD.cpp:210:	if (unfetchedCount == lossless_cast<int>(arguments.size())) interpreter.throwBadSyntax("Missing argument(s)");
src/IMPD.cpp:213:void ArgumentsContainer::throwIfAnyUnfetched() {
src/IMPD.cpp:214:	if (unfetchedCount != 0) interpreter.throwBadSyntax("Unrecognized labels or too many arguments");
src/IMPD.cpp:228:		Interpreter::throwRunTimeError("Math error (log of 0 or less)");
src/IMPD.cpp:235:		Interpreter::throwRunTimeError("Math error (log10 of 0 or less)");
src/IMPD.cpp:242:		Interpreter::throwRunTimeError("Math error (sqrt of negative)");
src/IMPD.cpp:343:void Interpreter::throwBadSyntax(const String& how) { throw SyntaxException(how); }
src/IMPD.cpp:344:void Interpreter::throwRunTimeError(const String& how) { throw RunTimeException(how); }
src/IMPD.cpp:345:void Interpreter::throwBadSyntax(const char* how) { throwBadSyntax(String(how)); }
src/IMPD.cpp:346:void Interpreter::throwRunTimeError(const char* how) { throwRunTimeError(String(how)); }
src/IMPD.cpp:360:		if (p == e) throwBadSyntax("Missing */");
src/IMPD.cpp:422:	if (p == e) throwBadSyntax(c == '[' ? "Missing ]" : "Missing }");
src/IMPD.cpp:437:	if (p == e) throwBadSyntax("Missing \""); 
src/IMPD.cpp:470:			if (lastRange.b == lastRange.e) throwBadSyntax("Label cannot be empty");
src/IMPD.cpp:481:		if (p == q && p != r.e) throwBadSyntax("Syntax error");
src/IMPD.cpp:491:				throwRunTimeError(String("Could not set variable ") + name);
src/IMPD.cpp:502:	if (f == 0) throwRunTimeError(String("Variable ") + name + " does not exist");
src/IMPD.cpp:538:		throwBadSyntax(String("Too many list elements (got " + toString(lossless_cast<int>(elements.size()))
src/IMPD.cpp:542:		throwBadSyntax(String("Too few list elements (got " + toString(lossless_cast<int>(elements.size()))
src/IMPD.cpp:576:		if (p == r.b) throwBadSyntax("Invalid instruction");
src/IMPD.cpp:585:	if (recursionLimit == 0) throwRunTimeError("Recursion limit reached");
src/IMPD.cpp:596:			if (rootFrame.statementsLimit == 0) throwRunTimeError("Statements limit reached");
src/IMPD.cpp:597:			if (!executor.progress(*this, rootFrame.statementsLimit)) throw AbortedException("Aborted");
src/IMPD.cpp:608:		throw;
src/IMPD.cpp:612:		throw;
src/IMPD.cpp:727:	if (!isFinite(v)) throwRunTimeError("Number overflow");
src/IMPD.cpp:734:	if (p == r.b || p != r.e) throwRunTimeError(String("Invalid integer: ") + String(r.b, r.e));
src/IMPD.cpp:741:	if (p == r.b || p != r.e) throwRunTimeError(String("Invalid number: ") + String(r.b, r.e));
src/IMPD.cpp:747:	else if (s != NO_STRING) throwRunTimeError(String("Invalid boolean (should be 'yes' or 'no'): ") + s);
src/IMPD.cpp:766:		if (q == p) throwBadSyntax("Syntax error");
src/IMPD.cpp:774:				case '/': if (r == 0.0) throwRunTimeError("Division by zero"); else l /= r; break;
src/IMPD.cpp:775:				case '^': { errno = 0; l = pow(l, r); if (errno != 0) throwRunTimeError("Math error"); break; }
src/IMPD.cpp:778:			if (!isFinite(l)) throwRunTimeError("Number overflow");
src/IMPD.cpp:798:				if (r == 0.0) throwRunTimeError("Modulo by zero");
src/IMPD.cpp:828:			if (q == p) throwBadSyntax("Syntax error");
src/IMPD.cpp:860:		if ((op0 == '!' || op0 == '=') && op1 == 0) throwBadSyntax("Syntax error");
src/IMPD.cpp:862:		if (q == p) throwBadSyntax("Syntax error");
src/IMPD.cpp:902:		if (q == e || *q != ':') throwBadSyntax("Expected :");
src/IMPD.cpp:926:				throwBadSyntax("Syntax error");
src/IMPD.cpp:929:			throwBadSyntax("Syntax error");
src/IMPD.cpp:932:		if (q == e || *q != '}') throwBadSyntax("Missing }");
src/IMPD.cpp:988:	if (p == e) throwBadSyntax("Unexpected end");
src/IMPD.cpp:1015:			if (p == e || *p != ')') throwBadSyntax("Missing )");
src/IMPD.cpp:1025:					throwBadSyntax("Invalid character escape code inside { } expression");
src/IMPD.cpp:1057:					if (errno != 0) throwRunTimeError("Math error");
src/IMPD.cpp:1058:					if (!isFinite(v)) throwRunTimeError("Number overflow");
src/IMPD.cpp:1176:					if (q == p) throwBadSyntax("Syntax error");
src/IMPD.cpp:1182:					if (p == e || *p != '}') throwBadSyntax("Syntax error");
src/IMPD.cpp:1233:		else throwBadSyntax(String("Unrecognized instruction: ") + instructionString);
src/IMPD.cpp:1242:		case STOP_INSTRUCTION: throw AbortedException("Encountered STOP instruction");
src/IMPD.cpp:1254:			args.throwIfAnyUnfetched();
src/IMPD.cpp:1261:				throw FormatException("Unsupported data format");
src/IMPD.cpp:1267:			if (argumentsRange.b == argumentsRange.e) throwBadSyntax("Missing variable name");
src/IMPD.cpp:1269:			if (p == argumentsRange.b) throwBadSyntax("Invalid variable name");
src/IMPD.cpp:1274:				if (*q != '=') throwBadSyntax("Expected =");
src/IMPD.cpp:1279:				if (callingFrame == 0) throwRunTimeError("Cannot return in global frame");
src/IMPD.cpp:1281:			} else if (!vars.declare(varName, varValue)) throwRunTimeError(String("Variable ") + varName + " already declared");
src/IMPD.cpp:1290:			args.throwIfAnyUnfetched();
src/IMPD.cpp:1302:			args.throwIfAnyUnfetched();
src/IMPD.cpp:1307:				if ((*condition)[0] != '[') throwBadSyntax("'while:' condition has to be enclosed in [ ]");
src/IMPD.cpp:1326:				args.throwIfAnyUnfetched();
src/IMPD.cpp:1339:				args.throwIfAnyUnfetched();
src/IMPD.cpp:1353:			if (allArguments.size() < 1) throwBadSyntax("Missing argument(s)");
src/IMPD.cpp:1365:					throwRunTimeError(String("Could not include file: ") + String(file.begin(), file.end()));
src/IMPD.h:84:	public:		std::string getError() const throw() { return error; }
src/IMPD.h:85:	public:		bool hasStatement() const throw() { return !statement.empty(); }
src/IMPD.h:86:	public:		String getStatement() const throw() { return statement; }
src/IMPD.h:87:	public:		virtual const char* what() const throw() { return error.c_str(); }
src/IMPD.h:88:	public:		virtual ~Exception() throw() { }
src/IMPD.h:93:struct SyntaxException : public Exception { SyntaxException(const std::string& error) : Exception(error) { } };			///< SyntaxException is thrown when file data cannot be parsed properly.
src/IMPD.h:95:struct AbortedException : public Exception { AbortedException(const std::string& error) : Exception(error) { } };		///< AbortedException is thrown by a 'stop' intruction or when an Executor returns false from progress()
src/IMPD.h:96:struct FormatException : public Exception { FormatException(const std::string& error) : Exception(error) { } };			///< FormatException is thrown when the 'format' instruction indicates that the data format is unsupported.
src/IMPD.h:118: public:		void throwIfNoneFetched();
src/IMPD.h:119: public:		void throwIfAnyUnfetched();
src/IMPD.h:161:						, const StringVector& requires) = 0;															///< Return false to throw FormatException if "identifier" is not correct or any element in "requires" is unknown / not supported. Empty requirements and requirements of 'IMPD-1' etc are removed from the list before this call. All strings are passed in lower case.
src/IMPD.h:162: public:		virtual bool execute(Interpreter& interpreter, const String& instruction, const String& arguments) = 0; ///< Return false to throw SyntaxException if instruction is unrecognized. \p instruction is passed in lower case.
src/IMPD.h:163: public:		virtual bool progress(Interpreter& interpreter, int maxStatementsLeft) = 0;								///< Called before every statement is executed. Return false to stop processing and throw AbortedException.
src/IMPD.h:164: public:		virtual bool load(Interpreter& interpreter, const WideString& filename, String& contents) = 0;			///< Called by the INCLUDE instruction. Load contents of file into \p contents. Return false to throw a RunTimeException.
src/IMPD.h:177: public:		static void throwBadSyntax(const String& how);												///< Bad syntax should be thrown when file data cannot be parsed properly. E.g. missing arguments etc.
src/IMPD.h:178: public:		static void throwBadSyntax(const char* how);
src/IMPD.h:179: public:		static void throwRunTimeError(const String& how);											///< Run-time error should be thrown when dynamic processing fails. E.g. variable contents is of wrong type for operation.
src/IMPD.h:180: public:		static void throwRunTimeError(const char* how);
```

## IVG

```
src/IVG.h:260:	public:		virtual NuXPixels::IntRect getBounds() const = 0;							///< Returns the outer boundaries of the canvas. A canvas may throw if bounds has not been set yet.
src/IVG.h:515:					if (image.get() == 0) IMPD::Interpreter::throwRunTimeError("Undeclared bounds");
src/IVG.h:519:					if (image.get() == 0) IMPD::Interpreter::throwRunTimeError("Undeclared bounds");
src/IVG.h:548:					if (image.get() != 0) IMPD::Interpreter::throwRunTimeError("Multiple bounds declarations");
src/IVG.h:550:						IMPD::Interpreter::throwRunTimeError(IMPD::String("bounds width out of range [1..32767]: ")
src/IVG.h:554:						IMPD::Interpreter::throwRunTimeError(IMPD::String("bounds height out of range [1..32767]: ")
src/IVG.h:561:					if (image.get() == 0) IMPD::Interpreter::throwRunTimeError("Undeclared bounds");
src/IVG.cpp:126:		if (p - (r.b + 1) != 2) impd.throwBadSyntax(String("Invalid opacity: ") + String(r.b + 1, r.e));
src/IVG.cpp:129:		if (d < 0.0 || d > 1.0) impd.throwRunTimeError(String("opacity out of range [0..1]: ") + impd.toString(d));
src/IVG.cpp:371:	Interpreter::throwRunTimeError("Bounds cannot be declared for mask");
src/IVG.cpp:444:					impd.throwRunTimeError(String("hsv value number ") + impd.toString(i + 1)
src/IVG.cpp:469:			default: impd.throwBadSyntax(String("Invalid color: ") + String(r.b + 1, r.e));
src/IVG.cpp:473:					impd.throwBadSyntax(String("Invalid pre-multiplied alpha color: ") + String(r.b + 1, r.e));
src/IVG.cpp:490:			impd.throwBadSyntax(String("Invalid color name: ") + String(r.b, r.e));
src/IVG.cpp:612:					args.throwIfAnyUnfetched();
src/IVG.cpp:659:							args.throwIfAnyUnfetched();
src/IVG.cpp:674:							args.throwIfAnyUnfetched();
src/IVG.cpp:694:							args.throwIfAnyUnfetched();
src/IVG.cpp:714:								else impd.throwBadSyntax(String("Invalid sweep for arc-to: ") + *sweep);
src/IVG.cpp:721:								else impd.throwBadSyntax(String("Invalid large for arc-to: ") + *large);
src/IVG.cpp:725:							args.throwIfAnyUnfetched();
src/IVG.cpp:738:							args.throwIfAnyUnfetched();
src/IVG.cpp:769:							args.throwIfAnyUnfetched();
src/IVG.cpp:788:						Interpreter::throwRunTimeError("Invalid first path instruction: " + instruction);
src/IVG.cpp:830:		impd.throwBadSyntax(String("Unrecognized gradient type: ") + gradientType);
src/IVG.cpp:838:			impd.throwBadSyntax("IVG-1 and IVG-2 require comma-separated gradient coordinates");
src/IVG.cpp:843:			impd.throwBadSyntax(String("Invalid linear gradient coordinates: ") + *arg2);
src/IVG.cpp:850:			impd.throwBadSyntax("IVG-3 requires space-separated gradient coordinates");
src/IVG.cpp:858:		impd.throwRunTimeError(String("Negative radial gradient radius: ") + impd.toString(coords[coords[2] < 0.0 ? 2 : 3]));
src/IVG.cpp:866:			impd.throwBadSyntax(String("Invalid stops for gradient (odd number of elements): ") + *s);
src/IVG.cpp:875:				impd.throwBadSyntax(String("Invalid stops for gradient (invalid position: ") + impd.toString(position) + ")");
src/IVG.cpp:894:	gradientArgs.throwIfAnyUnfetched();
src/IVG.cpp:911:	if (image.get() == 0) Interpreter::throwRunTimeError("Undeclared bounds");
src/IVG.cpp:1071:		if (d < 0.0) impd.throwRunTimeError(String("Negative stroke width: ") + impd.toString(d));
src/IVG.cpp:1079:		else impd.throwBadSyntax(String("Unrecognized stroke caps: ") + *s);
src/IVG.cpp:1086:		else impd.throwBadSyntax(String("Unrecognized stroke joints: ") + *s);
src/IVG.cpp:1090:		if (d < 1.0) impd.throwRunTimeError(String("miter-limit out of range [1..inf): ") + impd.toString(d));
src/IVG.cpp:1102:				impd.throwRunTimeError(String("Negative dash value: ") + impd.toString(dash));
src/IVG.cpp:1106:				impd.throwRunTimeError(String("Negative gap value: ") + impd.toString(gap));
src/IVG.cpp:1114:	args.throwIfNoneFetched();
src/IVG.cpp:1115:	args.throwIfAnyUnfetched();
src/IVG.cpp:1127:		throw;
src/IVG.cpp:1183:		impd.throwBadSyntax(String("Instruction requires ") + requiredString + ": " + instruction);
src/IVG.cpp:1194:		args.throwIfAnyUnfetched();
src/IVG.cpp:1197:			Interpreter::throwRunTimeError(String("Duplicate font definition: ") + String(name.begin(), name.end()));
src/IVG.cpp:1211:			impd.throwRunTimeError(String("resolution out of range [0.0001..inf): ") + impd.toString(resolution));
src/IVG.cpp:1213:		args.throwIfAnyUnfetched();
src/IVG.cpp:1216:			Interpreter::throwRunTimeError(String("Duplicate image definition: ") + String(name.begin(), name.end()));
src/IVG.cpp:1229:		args.throwIfAnyUnfetched();
src/IVG.cpp:1232:			Interpreter::throwRunTimeError(String("Duplicate path definition: ") + String(name.begin(), name.end()));
src/IVG.cpp:1239:		pathArgs.throwIfAnyUnfetched();
src/IVG.cpp:1242:		Interpreter::throwBadSyntax(String("Invalid define instruction type: ") + type);
src/IVG.cpp:1276:					   impd.throwBadSyntax(errorString);
src/IVG.cpp:1322:				impd.throwBadSyntax(String("Unrecognized alignment: ") + alignment);
src/IVG.cpp:1325:					impd.throwBadSyntax(String("Duplicate horizontal alignment: " + *s));
src/IVG.cpp:1332:					impd.throwBadSyntax(String("Duplicate vertical alignment: " + *s));
src/IVG.cpp:1362:			impd.throwRunTimeError(String("Negative clip width: ") + impd.toString(numbers[2]));
src/IVG.cpp:1365:			impd.throwRunTimeError(String("Negative clip height: ") + impd.toString(numbers[2]));
src/IVG.cpp:1373:	args.throwIfAnyUnfetched();
src/IVG.cpp:1390:			Interpreter::throwRunTimeError(String("Missing image: ") + String(imageName.begin(), imageName.end()));
src/IVG.cpp:1480:	args.throwIfAnyUnfetched();
src/IVG.cpp:1488:	args.throwIfAnyUnfetched();
src/IVG.cpp:1490:		impd.throwRunTimeError(String("Negative rectangle width: ") + impd.toString(numbers[2]));
src/IVG.cpp:1493:		impd.throwRunTimeError(String("Negative rectangle height: ") + impd.toString(numbers[3]));
src/IVG.cpp:1503:			impd.throwRunTimeError(String("Negative rounded corner radius: ")
src/IVG.cpp:1515:		if (secondArg == 0) impd.throwBadSyntax("IVG-3 requires space-separated ellipse syntax");
src/IVG.cpp:1517:		if (secondArg != 0) impd.throwBadSyntax("IVG-1 and IVG-2 require comma-separated ellipse syntax");
src/IVG.cpp:1537:	args.throwIfAnyUnfetched();
src/IVG.cpp:1539:		impd.throwRunTimeError(String("Negative ellipse radius: ") + impd.toString(rx < 0.0 ? rx : ry));
src/IVG.cpp:1557:		if (arg2 == 0) impd.throwBadSyntax("IVG-3 requires space-separated star syntax");
src/IVG.cpp:1559:		if (arg2 != 0) impd.throwBadSyntax("IVG-1 and IVG-2 require comma-separated star syntax");
src/IVG.cpp:1582:	args.throwIfAnyUnfetched();
src/IVG.cpp:1584:		impd.throwRunTimeError(String("star points out of range [1..10000]: ") + impd.toString(points));
src/IVG.cpp:1587:		impd.throwRunTimeError(String("Negative star radius: ") + impd.toString(r1 < 0.0 ? r1 : r2));
src/IVG.cpp:1612:			impd.throwBadSyntax(String("Unrecognized anchor: ") + *s);
src/IVG.cpp:1629:		Interpreter::throwRunTimeError("Need to set font before writing");
src/IVG.cpp:1633:		Interpreter::throwRunTimeError(String("Missing font: ")
src/IVG.cpp:1677:				else impd.throwBadSyntax(String("Unrecognized fill rule: ") + *s);
src/IVG.cpp:1679:			args.throwIfNoneFetched();
src/IVG.cpp:1680:			args.throwIfAnyUnfetched();
src/IVG.cpp:1693:					Interpreter::throwRunTimeError(String("Undefined path: ") + String(name.begin(), name.end()));
src/IVG.cpp:1706:			args.throwIfAnyUnfetched();
src/IVG.cpp:1718:			args.throwIfAnyUnfetched();
src/IVG.cpp:1726:			args.throwIfAnyUnfetched();
src/IVG.cpp:1735:			args.throwIfNoneFetched();
src/IVG.cpp:1736:			args.throwIfAnyUnfetched();
src/IVG.cpp:1738:				impd.throwRunTimeError("Relative paint is not allowed with wipe");
src/IVG.cpp:1754:					impd.throwRunTimeError(String("aa-gamma out of range (0..100): ") + impd.toString(d));
src/IVG.cpp:1761:					impd.throwRunTimeError(String("curve-quality out of range (0..100): ") + impd.toString(d));
src/IVG.cpp:1768:					impd.throwRunTimeError(String("pattern-resolution out of range (0..100): ") + impd.toString(d));
src/IVG.cpp:1772:			args.throwIfNoneFetched();
src/IVG.cpp:1773:			args.throwIfAnyUnfetched();
src/IVG.cpp:1778:			args.throwIfAnyUnfetched();
src/IVG.cpp:1801:			args.throwIfAnyUnfetched();
src/IVG.cpp:1820:			args.throwIfAnyUnfetched();
src/IVG.cpp:1839:					Interpreter::throwRunTimeError("Invalid font name");
src/IVG.cpp:1842:					Interpreter::throwRunTimeError(String("Missing font: ")
src/IVG.cpp:1862:					impd.throwRunTimeError(String("font size out of range (0..inf): ") + impd.toString(d));
src/IVG.cpp:1869:			args.throwIfNoneFetched();
src/IVG.cpp:1870:			args.throwIfAnyUnfetched();
src/IVG.cpp:1885:			args.throwIfAnyUnfetched();
src/IVG.cpp:1954:	if (raster.get() == 0) Interpreter::throwRunTimeError("Undeclared bounds");
src/IVG.cpp:1963:	if (raster.get() != 0) Interpreter::throwRunTimeError("Multiple bounds declarations");
src/IVG.cpp:1965:		Interpreter::throwRunTimeError(String("bounds width out of range [1..32767]: ")
src/IVG.cpp:1969:		Interpreter::throwRunTimeError(String("bounds height out of range [1..32767]: ")
src/IVG.cpp:2124:				impd.throwBadSyntax("Duplicate metrics instruction in font definition");
src/IVG.cpp:2134:			args.throwIfAnyUnfetched();
src/IVG.cpp:2137:				impd.throwBadSyntax("Invalid metrics instruction in font definition");
src/IVG.cpp:2146:				impd.throwBadSyntax(String("Invalid glyph character (length is not 1): ") + String(ws.begin(), ws.end()));
src/IVG.cpp:2151:			args.throwIfAnyUnfetched();
src/IVG.cpp:2154:				impd.throwBadSyntax("Missing metrics before glyph instruction in font definition");
src/IVG.cpp:2157:				impd.throwBadSyntax(String("Negative glyph advance in font definition: ")
src/IVG.cpp:2161:				impd.throwBadSyntax(String("Duplicate glyph definition in font definition (unicode: ")
src/IVG.cpp:2177:							impd.throwBadSyntax(String("Duplicate kerning pair in font definition: ")
src/IVG.cpp:2184:			args.throwIfAnyUnfetched();
```

## NuXPixels

```
externals/NuX/NuXPixels.cpp:122:inline Fixed32_32 divide(Int32 v1, Int32 v2) throw()
externals/NuX/NuXPixels.h:165:inline Fixed32_32 toFixed32_32(Int32 high, UInt32 low) throw() { return (Fixed32_32(high) << 32) | low; }
externals/NuX/NuXPixels.h:166:inline Fixed32_32 toFixed32_32(double d) throw() { return Fixed32_32(floor(d * 4294967296.0 + 0.5)); }
externals/NuX/NuXPixels.h:167:inline Fixed32_32 add(Fixed32_32 v1, Fixed32_32 v2) throw() { return v1 + v2; }
externals/NuX/NuXPixels.h:168:inline Int32 addCarry(Fixed32_32& v1, Fixed32_32 v2) throw() { Int32 carry = Int32((Fixed32_32((UInt32)(v1)) + (UInt32)(v2)) >> 32); v1 += v2; return carry; }
externals/NuX/NuXPixels.h:169:inline Fixed32_32 shiftLeft(Fixed32_32 v, Int32 s) throw() { return v << s; }
externals/NuX/NuXPixels.h:170:inline Fixed32_32 shiftRight(Fixed32_32 v, Int32 s) throw() { return v >> s; }
externals/NuX/NuXPixels.h:171:inline Int32 high32(Fixed32_32 v) throw() { return static_cast<Int32>(v >> 32); }
externals/NuX/NuXPixels.h:172:inline UInt32 low32(Fixed32_32 v) throw() { return static_cast<UInt32>(v); }
externals/NuX/NuXPixels.h:173:inline Fixed32_32 divide(Int32 v1, Int32 v2) throw() { return (Fixed32_32(v1) << 32) / v2; }
externals/NuX/NuXPixels.h:174:inline Fixed32_32 multiply(Int32 v1, Fixed32_32 v2) throw() { return v1 * v2; }
externals/NuX/NuXPixels.h:192:Fixed32_32 divide(Int32 v1, Int32 v2) throw();
externals/NuX/NuXPixels.h:194:inline Fixed32_32 toFixed32_32(Int32 high, UInt32 low) throw() { return Fixed32_32(high, low); }
externals/NuX/NuXPixels.h:195:inline Fixed32_32 toFixed32_32(double d) throw() { d += (0.5 / 4294967296.0); return Fixed32_32(Int32(floor(d)), (UInt32)((d - floor(d)) * 4294967296.0)); }
externals/NuX/NuXPixels.h:196:inline Fixed32_32 add(Fixed32_32 v1, Fixed32_32 v2) throw() { return Fixed32_32(v1.high + v2.high + (v1.low + v2.low < v1.low), v1.low + v2.low); }
externals/NuX/NuXPixels.h:197:inline Int32 addCarry(Fixed32_32& v1, Fixed32_32 v2) throw() { Int32 carry = (v1.low + v2.low < v1.low); v1 = Fixed32_32(v1.high + v2.high + carry, v1.low + v2.low); return carry; }
externals/NuX/NuXPixels.h:198:inline Fixed32_32 shiftLeft(Fixed32_32 v, Int32 s) throw() { Int32 h = (v.high << s) | (v.low >> (32 - s) & -(s != 0)); UInt32 l = v.low << s; return Fixed32_32(h, l); }
externals/NuX/NuXPixels.h:199:inline Fixed32_32 shiftRight(Fixed32_32 v, Int32 s) throw() { Int32 h = v.high >> s; UInt32 l = (v.high << (32 - s)) | (v.low >> s & -(s != 0)); return Fixed32_32(h, l); }
externals/NuX/NuXPixels.h:200:inline Int32 high32(Fixed32_32 v) throw() { return v.high; }
externals/NuX/NuXPixels.h:201:inline UInt32 low32(Fixed32_32 v) throw() { return v.low; }
externals/NuX/NuXPixels.h:202:inline Fixed32_32 negate(Fixed32_32 v) throw() { return Fixed32_32(~v.high + (v.low == 0), ~v.low + 1); }
externals/NuX/NuXPixels.h:204:inline Fixed32_32 multiply(UInt32 v1, Fixed32_32 v2) throw() {
externals/NuX/NuXPixels.h:215:inline Fixed32_32 multiply(UInt16 v1, Fixed32_32 v2) throw() {
```

/*
 *  Bassembler6502.h
 *  6502assembler
 *
 *  Created by Zoltán Majoros on 2011.11.21.
 *  Copyright 2011-2012 Zoltán Majoros. All rights reserved.
 *
 */

#include <iostream>
#include <vector>
#include <map>
#include "types.h"
#include <pcrecpp.h>

using namespace std; // mainly for 'string'

#define PETSCII     0
#define ASCII       1
#define SCREENSCII  2

class MemChunk; // fw. dec.

/*
 * Opcode class
 *
 * Container for a single opcode.
 * Contains the name and the instruction code for each addressing mode.
 * The 'opcodeMap' (map<string, Opcode>) member variable will hold a map
 * of these opcodes.
 */
class Opcode
{
public:
    string name;
    byte codes[11];

    // OK, this might look ridiculous, but if you take a look at BASSembler6502::initOpcodeTable(),
    // it will immediately make sense why it is implemented this way.
    Opcode(string n, byte a, byte b, byte c, byte d, byte e, byte f, byte g, byte h, byte i, byte j, byte k)
    {
        name = n;
        codes[0] = a;
        codes[1] = b;
        codes[2] = c;
        codes[3] = d;
        codes[4] = e;
        codes[5] = f;
        codes[6] = g;
        codes[7] = h;
        codes[8] = i;
        codes[9] = j;
        codes[10] = k;
    }

    Opcode(){};
    ~Opcode(){};
};

/*
 * AssemblyError
 * Serves as a container for a single error
 */
struct AssemblyError
{
	string lineContent;
	string errorString;
	string errorStringVerbose;
	unsigned int errorLineNumber;
};

struct UnresolvedAddress
{
    word address;
    MemChunk *memChunk;
    bool isBranch;
    bool isOneByteAddr;
    bool isLowPart;
};

struct UnresolvedLabel
{
    vector<UnresolvedAddress> addresses;
    word realAddress;
    unsigned int line;
};

/*
 * BASSembler6502
 *
 * The whole assembler functionality is encompassed in this class.
 * Most member variable names are self-descriptive
 */
class BASSembler6502
{
	vector<string> lines;
	word actAddress;
	vector<MemChunk> chunks; // see MemChunk for info
	MemChunk *actChunk; // current chunk we assemble into. must be added to the 'chunks' vector and reset when we change the .pc
	int charset;
	string petsciiChars;
	string screenChars;
    map<string, word> labels;
    map<string, Opcode> opcodeMap;
    string singleByteInstructions;
    string singleByteInstructionsSpecial; // some instructions have a one byte version as well, they are listed here
    map<string, UnresolvedLabel> unresolvedLabels;
	
	int assembleLine(string line, unsigned int lineNumber);
	int checkDirectives(string &line);
    int detectLabelDefinition(string line);
	
    // utility functions
    int countChars(string text, char c);
	int findChar(string text, char c);
    int convertIntoDecimal(string valueStr);
    
    void initOpcodeTable(void);
    
    // declarations of regular expressions used across the assembler
	pcrecpp::RE *remove_comments; //("\\s*;.*");
	pcrecpp::RE *remove_leading_space; //("^\\s+(.+)");
	pcrecpp::RE *remove_trailing_space; //("\\s+$");
    pcrecpp::RE *searchDirective; //("\\s*\\..*"); // create regex "\s*\..*": looks for '.' in line, leading white space is allowed
	pcrecpp::RE *extractKeyword; //("\\s*\\.(\\w+).*"); // extract keyword
    pcrecpp::RE *extractMemoryAddress; //("\\s*.pc\\s*=\\s*\\$([a-f0-9]+)"); // extract memory address (without '$' character)
    pcrecpp::RE *getDataElements; //("\\s*\\.\\w+\\s+(.*)\\s*"); // get whole line after directive excluding optional white space
    pcrecpp::RE *getSingleElement; //("\\s*((%[0|1]+)|(\\$[0-9a-f]+)|([0-9]+))\\s*,\\s*");
    pcrecpp::RE *getLastElement; //("\\s*((%[0|1]+)|(\\$[0-9a-f]+)|([0-9]+))\\s*"); // notice the absence of ','
    pcrecpp::RE *getDataElements2; //("\\s*\\.\\w+\\s+\"(.*)\"$");
    pcrecpp::RE *isEmptyLine; //("^\\s*$"); // if line is empty, return
    pcrecpp::RE *detectLabelDef; //("^(\\S*):\\s*(.*)\\s*$"); // basically looks for some text followed by a colon (:)
    pcrecpp::RE *detectLabelDefCorrectness; //("^([A-Z]+[A-Z0-9_!]*):.*");  // check if there is a legal label definition
    pcrecpp::RE *removeLabelDefinition; //("^[A-Z]+[A-Z0-9_!]*:\\s*(.*)\\s*");
    pcrecpp::RE *getInstructionElements; //("\\s*([a-zA-Z]{3})\\s*(.*)"); // $1 = opcode, $2 = operand
    pcrecpp::RE *checkImmediateAddr; //("^#(.*)");
    pcrecpp::RE *checkZPorAbsolute; //("^\\$[0-9A-F]+");
    pcrecpp::RE *checkZPXorAbsoluteX; //("(\\$[0-9A-F]+)\\s*,\\s*X");
    pcrecpp::RE *checkZPYorAbsoluteY; //("(\\$[0-9A-F]+)\\s*,\\s*Y");
    pcrecpp::RE *checkIndirect; //("\\((\\$[0-9A-F]+)\\)");
    pcrecpp::RE *checkIndexedIndirect; //("\\(\\s*(\\$[0-9A-F]+)\\s*,\\s*X\\s*\\)");
    pcrecpp::RE *checkIndirectIndexed; //("\\(\\s*(\\$[0-9A-F]+)\\s*\\)\\s*,\\s*Y");
    pcrecpp::RE *checkIfBin; //("^%([0-1]{8})");
    pcrecpp::RE *checkIfHex; //("^\\$([0-9A-Z]+)");
    pcrecpp::RE *checkIfDec; //("^([0-9]+)");
    pcrecpp::RE *checkSimpleLabelReference;
    pcrecpp::RE *checkXIndexedLabelReference;
    pcrecpp::RE *checkYIndexedLabelReference;
    pcrecpp::RE *checkIndirectLabelReference;
	pcrecpp::RE *detectAsteriskExpression;

public:
	AssemblyError asmError; // the caller can fetch the error message here in case assemble() returns with an error

	BASSembler6502()
    {
        remove_comments = new pcrecpp::RE("\\s*;.*");
        remove_leading_space = new pcrecpp::RE("^\\s+(.+)");
        remove_trailing_space = new pcrecpp::RE("\\s+$");
        searchDirective = new pcrecpp::RE("\\s*\\..*");
        extractKeyword = new pcrecpp::RE("\\s*\\.(\\w+).*");
        extractMemoryAddress = new pcrecpp::RE("\\s*.pc\\s*=\\s*\\$([a-f0-9]+)");
        getDataElements = new pcrecpp::RE("\\s*\\.\\w+\\s+(.*)\\s*");
        getSingleElement = new pcrecpp::RE("\\s*((%[0|1]+)|(\\$[0-9a-f]+)|([0-9]+))\\s*,\\s*");
        getLastElement = new pcrecpp::RE("\\s*((%[0|1]+)|(\\$[0-9a-f]+)|([0-9]+))\\s*");
        getDataElements2 = new pcrecpp::RE("\\s*\\.\\w+\\s+\"(.*)\"$");
        isEmptyLine = new pcrecpp::RE("^\\s*$");
        detectLabelDef = new pcrecpp::RE("^(\\S*):\\s*(.*)\\s*");
        detectLabelDefCorrectness = new pcrecpp::RE("^([A-Z]+[A-Z0-9_!]*):.*");
        removeLabelDefinition = new pcrecpp::RE("^[A-Z]+[A-Z0-9_!]*:\\s*(.*)\\s*");
        getInstructionElements = new pcrecpp::RE("\\s*([a-zA-Z]{3})\\s*(.*)");
        checkImmediateAddr = new pcrecpp::RE("^#[<>]?(.*)");
        checkZPorAbsolute = new pcrecpp::RE("^\\$?[0-9A-F]+");
        checkZPXorAbsoluteX = new pcrecpp::RE("(\\$[0-9A-F]+)\\s*,\\s*X");
        checkZPYorAbsoluteY = new pcrecpp::RE("(\\$[0-9A-F]+)\\s*,\\s*Y");
        checkIndirect = new pcrecpp::RE("\\((\\$[0-9A-F]+)\\)");
        checkIndexedIndirect = new pcrecpp::RE("\\(\\s*(\\$[0-9A-F]+)\\s*,\\s*X\\s*\\)");
        checkIndirectIndexed = new pcrecpp::RE("\\(\\s*(\\$[0-9A-F]+)\\s*\\)\\s*,\\s*Y");
        checkIfBin = new pcrecpp::RE("^%([0-1]+)");
        checkIfHex = new pcrecpp::RE("^\\$([0-9A-Z]+)");
        checkIfDec = new pcrecpp::RE("^([0-9]+)");
        checkSimpleLabelReference = new pcrecpp::RE("#?([<>]?[A-Z]+[A-Z0-9_!]*)");
        checkXIndexedLabelReference = new pcrecpp::RE("([A-Z]+[A-Z0-9_!]*)\\s*,\\s*X");
        checkYIndexedLabelReference = new pcrecpp::RE("([A-Z]+[A-Z0-9_!]*)\\s*,\\s*Y");
        checkIndirectLabelReference = new pcrecpp::RE("\\(\\s*([A-Z]+[A-Z0-9_!]*)\\s*\\)");
        detectAsteriskExpression = new pcrecpp::RE("\\*\\s*([\\-|\\+])\\s*([0-9]+)");
		actChunk = NULL;
		actAddress = 0;
		charset = ASCII;
		
		petsciiChars = "                                 !\"#$%&'()*+,-./0123456789:;<=>?@abcdefghijklmno"
					   "pqrstuvwxyz[\\]^_`ABCDEFGHIJKLMNOPQRSTUVWXYZ   ~                                 "
					   "                          ✓     `ABCDEFGHIJKLMNOPQRSTUVWXYZ   ~                "
					   "               ";
		screenChars =  "@abcdefghijklmnopqrstuvwxyz[\\]^_ !\"#$%&'()*+,-./0123456789:;<=>?@abcdefghijklmno"
					   "pqrstuvwxyz[\\]^_`ABCDEFGHIJKLMNOPQRSTUVWXYZ{\\}~ "
					   "                                                                                "
					   "                                                ";

        initOpcodeTable();
	};
	
	~BASSembler6502(){};
	
	int assemble(char *source, vector<MemChunk> *&chunks);
};

/*
 * MemChunk
 *
 * Assembling works in a way that several independent memory chunks
 * might be produced along the way. One such "chunk" is defined with
 * this class. One byte is given to the chunk a time, just like a stack.
 * The size of the data buffer is dynamically expanded when needed.
 *
 * The invoker of the assembler class must handle the produced collection
 * of chunks since these are the ultimate results of the assembly.
 */
class MemChunk
{
	word bufferSize;
public:
	word startAddress;
	word length;
	byte *data;
	
	MemChunk()
	{
		length = bufferSize = 0;
		data = NULL;
	}

	~MemChunk()
	{
	}
	
	void addByte(byte newByte)
	{
		if(data==NULL) // if this is called for the first time
		{
			data = new byte[256]; // then create initial data buffer
			bufferSize = 256; // and set the length
		}
		
		if(length==bufferSize) // if our buffer is full
		{
			if((bufferSize*2) > 65536) // cannot have a chunk larger than 64K
			{
				cout << "Internal error (2)" << endl;
				exit(-1);
			}
			// 256, 512, 1024, 2048, 4086, 8192, 16384, 32768, 65536
			byte *tmpBuffer = new byte[bufferSize*2]; // then create new, twice as ama.. big buffer
			memcpy(tmpBuffer, data, bufferSize); // copy the content of the old one to the new one
			delete [] data; // free up the old one
			data = tmpBuffer; // redirect the pointer to the new buffer
			// ps.: i know that there's such a thing called realloc(), but anyway. :)
		}
		
		data[length++] = newByte;
		//printf("addByte: [%.2d] new byte added %.2X\n", length, newByte);
	}
	
	void addWord(word newWord)
	{
		addByte(newWord & 0xff);
		addByte((newWord & 0xff00)>>8);
	}
	
	void finalize()
	{
		if(length==bufferSize)	// if the buffer happens to be exactly full
			return;				// then we don't need to do anything
		
		byte *tmpBuffer = new byte[length]; // otherwise we tailor the buffer size exactly
		memcpy(tmpBuffer, data, length);	// to the amount of data we're storing
		delete [] data;
		data = tmpBuffer;
	}

    void rewriteByteAtAddress(byte newData, word destAddress)
    {
        word offset = destAddress - startAddress;
        data[offset] = newData;
    }
    
    void rewriteWordAtAddress(word newData, word destAddress)
    {
        word offset = destAddress - startAddress;
        data[offset] = (byte)(newData & 0xff);
        data[offset+1] = (byte)((newData & 0xff00) >> 8);
    }
};

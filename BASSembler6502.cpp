/*
 *  BASSembler6502.cpp
 *  6502assembler
 *
 *  Created by Zoltán Majoros on 2011.11.21-12.13., 2012.04.30-05.03.
 *  Copyright 2011-2012 Zoltán Majoros. All rights reserved.
 *
 */

#include "BASSembler6502.h"
#include <sstream> // istringstream
#include <locale> // toupper()

/*
 * assemble()
 *
 * The one and only public method of the class.
 *
 * Input: char *source: the 6502 assembly source code in C string format
 * Output: vector<MemChunk> *&chunks: an array of MemChunks
 *
 * Description: a MemChunk contains a block of machine code for at a given memory address.
 */
int BASSembler6502::assemble(char *source, vector<MemChunk> *&chunks) // source = input, chunks = output
{
	asmError.lineContent = asmError.errorString = asmError.errorStringVerbose = "";
	asmError.errorLineNumber = 0;
	
	istringstream src(source); // we slice up the source with this line by line
	string line;

	unsigned int actLine = 1;
	while(getline(src,line)) // step through the source code and process each line
	{
		remove_leading_space->GlobalReplace("\\1", &line);
		remove_trailing_space->GlobalReplace("", &line);
		
		lines.push_back( line );

        int dirResult = checkDirectives(line);
        int labResult = detectLabelDefinition(line);
        int asmResult = assembleLine(line, actLine);

		if((dirResult==1) && (asmResult==1) && (labResult==1)) // return value of 1 means no related content detected
        {
            asmError.errorString = "Syntax error";
            asmError.errorLineNumber = actLine;
            asmError.lineContent = line;
            return -1;
        }
        
		if((dirResult==-1) || (asmResult==-1) || (labResult==-1)) // return value of -1 means error during assembly
		{
			asmError.errorLineNumber = actLine;
			asmError.lineContent = line;
			return -1;
		}

		actLine++;
	}

	actChunk->finalize(); // close last chunk
	this->chunks.push_back(*actChunk);
    
    // handle unresolved labels
    map<string, UnresolvedLabel>::iterator iter;    
    for(iter = unresolvedLabels.begin(); iter != unresolvedLabels.end(); iter++)
    {
        // now loop through of all occurrences of the unknown label references
        string addressStr; // variables to hold extracted data
        UnresolvedLabel uLabel;
        word resolvedAddress;
        int size = (int)iter->second.addresses.size();
        for(int i=0; i<size; i++)
        {
            addressStr = iter->first;
            uLabel = iter->second;
            resolvedAddress = labels[addressStr];
                        
            if(uLabel.addresses[i].isOneByteAddr) // LDA #<LABEL or LDA #>LABEL
            {
                if(uLabel.addresses[i].isLowPart)
                    uLabel.addresses[i].memChunk->rewriteByteAtAddress((byte)(resolvedAddress&0xff), uLabel.addresses[i].address);
                else
                    uLabel.addresses[i].memChunk->rewriteByteAtAddress((byte)((resolvedAddress&0xff00)>>8), uLabel.addresses[i].address);                
            }
            else
            {
                if(uLabel.addresses[i].isBranch == true) // branching values are handled differently
                    uLabel.addresses[i].memChunk->rewriteByteAtAddress((byte)(resolvedAddress-uLabel.addresses[i].address-1), uLabel.addresses[i].address);
                else // normal 16bit addresses are simply overwritten with the resoloved addresses
                    uLabel.addresses[i].memChunk->rewriteWordAtAddress(labels[addressStr], uLabel.addresses[i].address);
            }
        }
        
        if( labels[iter->first] == 0 ) // if searched label is not found...
        {
            asmError.errorString = "Unresolved label definition '" + iter->first + "'";
            asmError.errorLineNumber = iter->second.line;
            asmError.lineContent = lines[iter->second.line-1];
            return -1;
        }
    }
 
    // assembly's done, preparing to return the binary data in the correct form.
	// we make a new vector of chunks with the copy of the local vector
	chunks = new vector<MemChunk>(this->chunks);
	return 0;
}

/*
 * As the name implies, this method checks if the line begins with a .keyword
 * and executes the command associated for the given directive.
 *
 */
int BASSembler6502::checkDirectives(string &line)
{
	if(!searchDirective->FullMatch(line)) // check string against our regex
		return 1; // no directive found
	
	string keyword;
	if(!extractKeyword->FullMatch(line, &keyword))
	{
		asmError.errorString = "Syntax error";
		asmError.errorStringVerbose = "'.' must be followed by a valid keyword.\n"
									  "Valid keywords are: .pc, .byte, .word, .text, .ascii, .petscii, .screen";
		return -1;
	}
	
    std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);
    
    // ----------------------------------------------------------------------------
    // .TEXT found
    // ----------------------------------------------------------------------------
	if(keyword=="text") // what about a '.text0'? actually it can be easily simulated...
	{
        // get whole line after directive excluding optional white space and quotation marks
		string dataString;
		if(!getDataElements2->FullMatch(line, &dataString))
		{
			asmError.errorString = "Syntax error";
			asmError.errorStringVerbose = "Valid syntax for .text directive: .text \"your text here\"\n"
            "Quotation marks must be escaped out with \\\" format.\n"
            "Note: comments are not allowed after a .text directive.";
			return -1;
		}
		
		string cleanString;
		int size = (int)dataString.length();
		for(int i=0; i<size; i++)
		{
			if(dataString[i]=='\\') // we found a '\'
			{
				if(dataString[i+1]=='"') // it's escaping a quote mark
				{
					cleanString += '"'; // insert quote mark
					i++;
					continue;
				}
				if(dataString[i+1]=='\\') // it's escaping itself
				{
					cleanString += '\\'; // insert backslash
					i++;
					continue;
				}
				asmError.errorString = "Syntax error";
				asmError.errorStringVerbose = "Unrecognized use of backslash character.";
				return -1;
			}
			if(dataString[i]=='"')
			{
				asmError.errorString = "Syntax error";
				asmError.errorStringVerbose = "Only one string per line is allowed. Additional quotation marks must be escaped "
                "with a backslash character.";
				return -1;
			}
			cleanString += dataString[i];
		}
		
        if(actChunk==NULL)
        {
            asmError.errorString = "Instruction reached without address specification";
            asmError.errorStringVerbose = "Specify a starting address with the .pc directive.";
            return -1;
        }
        
		size = (int)cleanString.size();
		for(int s=0; s<size; s++)
		{
			if( charset == SCREENSCII )
				actChunk->addByte((byte)findChar(screenChars, cleanString[s]));
			else if( charset == PETSCII )
				actChunk->addByte((byte)findChar(petsciiChars, cleanString[s]));
			else if( charset == ASCII )
				actChunk->addByte(cleanString[s]);
			actAddress++;
		}
		
		return 0;
	}
// ----------------------------------------------------------------------------
	if(keyword=="ascii")
	{
		//cout << "Switching to ASCII" << endl;
		charset = ASCII;
        
		return 0;		
	}
// ----------------------------------------------------------------------------
	if(keyword=="petscii")
	{
		//cout << "Switching to PETSCII" << endl;
		charset = PETSCII;
		
		return 0;		
	}
// ----------------------------------------------------------------------------
	if(keyword=="screen")
	{
		//cout << "Switching to PETSCII" << endl;
		charset = SCREENSCII;
		
		return 0;
	}

    remove_comments->GlobalReplace("", &line); // at this point it's safe to remove comments.
// ----------------------------------------------------------------------------
// .PC found
// ----------------------------------------------------------------------------
	if(keyword == "pc")
	{
		string address;
		if(!extractMemoryAddress->FullMatch(line, &address))
		{
			asmError.errorString = "Syntax error";
			asmError.errorStringVerbose =	"correct .pc format: .pc = ${Addr}, "
											"where {Addr} is a hexadecimal number between 0 and FFFF. "
											"White space is allowed before '.pc', and around the equal sign.";
			return -1;
		}
		
		unsigned int addressNum;
        // TODO: convert regex to class member variable!
		pcrecpp::RE("(.+)").FullMatch("0x"+address, pcrecpp::Hex(&addressNum)); // convert hex to dec
		
		if(addressNum>65535) // check if a valid (<64K) address was specified
		{
			asmError.errorString = "Address out of range: $" + address;
			asmError.errorStringVerbose = "Address must be in range $0-$FFFF.";
			return -1;
		}
		
		// at this point the directive syntax is processed, executing action
		actAddress = (word)addressNum;

		// in case this is the first time we set .pc and the actChunk is still non-existent,
		// then we don't need to push it into the chunks vector.
		if(actChunk==NULL) // begin new chunk
		{
            //cout << "first chunk created..." << endl;
			actChunk = new MemChunk();
			actChunk->startAddress = actAddress;
			return 0;
		}

		// finish old chunk
		actChunk->finalize(); // it's important not to forget this
		chunks.push_back(*actChunk); // TODO: i hope that the vector keeps a COPY of the pointer to this object...
		actChunk = new MemChunk();
		actChunk->startAddress = actAddress;
        //cout << "new chunk created..." << endl;
		
		return 0;
	}
	
// ----------------------------------------------------------------------------
// .BYTE or .WORD found
// ----------------------------------------------------------------------------
	if((keyword == "byte") || (keyword == "word"))
	{
		string dataString;
		getDataElements->FullMatch(line, &dataString);
		
		pcrecpp::StringPiece input(dataString);
		vector<string> values;
		string actValue;
		while(getSingleElement->Consume(&input, &actValue)) // loop through all values in row
			values.push_back(actValue);

		// this is not a mistake. it's here to check the _last_ element, after which a ',' is not accepted
		if(getLastElement->Consume(&input, &actValue))
			values.push_back(actValue);

		// error check: see if the number of commas+1 is equal to the number of extracted data elements.
		if((countChars(line, ',')+1) != (int)values.size()) // if not, there was a processing error
		{
			asmError.errorString = "Invalid number format";
			asmError.errorStringVerbose = "Data must be in one of the following three formats:\n";

			if(keyword=="byte")
			{
				asmError.errorStringVerbose += "- decimal: {X}, {X}=>[0,255] (no leading character before numerical characters)\n";
				asmError.errorStringVerbose += "- hexadecimal: ${Y}, Y=>[0,FF] (leading '$')\n";			
				asmError.errorStringVerbose += "- binary: %{ZZZZZZZZ}, {Z}=>[0,1], value in decimal must be in [0,255], (leading '%')";
			}
			if(keyword=="word")
			{
				asmError.errorStringVerbose += "- decimal: {X}, {X}=>[0,65535] (no extra leading character before numerical characters)\n";
				asmError.errorStringVerbose += "- hexadecimal: ${Y}, Y=>[0,FFFF] (leading '$')\n";			
				asmError.errorStringVerbose += "- binary: %{ZZZZZZZZZZZZZZZZ}, {Z}=>[0,1], value in decimal must be in [0,65535], "
												"(leading '%')";
			}
			
			asmError.errorStringVerbose += "Different data formats are allowed on a single line. "
											"Comma as delimiting character is obligatory.";
			return -1;
		}

        if(actChunk==NULL)
        {
            asmError.errorString = "Instruction reached without address specification";
            asmError.errorStringVerbose = "Specify a starting address with the .pc directive.";
            return -1;
        }

		// at this point we have all the data elements stored in string format in a <vector>.
		// we need to convert them into decimal format (if needed) and store them into "memory"
		int size = (int)values.size();
		int valueInDecimal;
		string tempValue = "";
		
		for(int i=0; i<size; i++) // TODO: you SHOULD convert these RegExps into member variables
		{   
			if(pcrecpp::RE("\\d+\\d*").FullMatch(values[i])) // check if decimal
			{
				valueInDecimal = atoi(values[i].c_str());
			}
			else if( pcrecpp::RE( "\\$([0-9a-f]+)" ).FullMatch(values[i], &tempValue) ) // check if hexa
			{
				valueInDecimal = (int)strtol(tempValue.c_str(), NULL, 16);
			}
			else if ( pcrecpp::RE( "%([0|1]+)" ).FullMatch(values[i], &tempValue) ) // check if binary
			{
				valueInDecimal = (int)strtol(tempValue.c_str(), NULL, 2);
			}
			else // should not get here ever, but to be sure, here's an error message
			{
				asmError.errorString = "Internal error (1)";
				return -1;
			}

			// range checks
			if(keyword=="byte")
			{
				if((valueInDecimal > 255) || (valueInDecimal < 0)) // check range
				{
					asmError.errorString = "Value out of range: " + values[i];
					asmError.errorStringVerbose = "Value must fit into 8 bits. $0-$FF or 0-255 or %0-%11111111.";
					return -1;
				}
				actChunk->addByte((byte)valueInDecimal);
				actAddress++;
			}
			if(keyword=="word")
			{
				if((valueInDecimal > 65535) || (valueInDecimal < 0)) // check range
				{
					asmError.errorString = "Value out of range: " + values[i];
					asmError.errorStringVerbose = "Value must fit into 16 bits. $0-$FFFF or 0-65535 or %0-%1111111111111111.";
					return -1;
				}
				actChunk->addWord((byte)valueInDecimal);
				actAddress+=2;
			}
		}		

		return 0;
	}
	
// ----------------------------------------------------------------------------
	asmError.errorString = "Unrecognized directive '." + keyword + "'";
	asmError.errorStringVerbose = "Recognized keywords: .pc, .byte, .word, .text, .ascii, .petscii, .screen";
	return -1;
}

// ----------------------------------------------------------------------------
int BASSembler6502::detectLabelDefinition(string line) // 7815772, 821250366 <- kathrin's numbers
{
    remove_comments->GlobalReplace("", &line);
    
    if(isEmptyLine->FullMatch(line))  // without any processing
        return 0;
    
    // convert whole line into upper case
    std::transform(line.begin(), line.end(), line.begin(), ::toupper);
    
    // handle label definition
    string labelCandidate;
    string textAfterLabel;
	if (detectLabelDef->FullMatch(line, &labelCandidate, &textAfterLabel)) // check if there's a : in the line.
    {                                                    // that indicates a label definition
        string label;
        if (detectLabelDefCorrectness->FullMatch(line, &label))             // if yes, register it
        {                                                
            //cout << "label detected: " << label << endl;
            if( labels[label] != 0 )
            {
                asmError.errorString = "Label already defined: " + label;
                return -1;
            }
            labels[label] = actAddress;
            if(textAfterLabel=="")
            {
                return 0;
            }
            else
                return 1; // return value of 1 means no related content detected
        }
        else // label definition was found, but label was defined incorrectly 
        {
            asmError.errorString = "Incorrect label definition: " + labelCandidate;
            asmError.errorStringVerbose = "Labels must start with an alphanumeric character. "
            "See documentation for detailed rules.";
            return -1;
        }
    }
    
    return 1; // return value of 1 means no related content detected
}

// ----------------------------------------------------------------------------
int BASSembler6502::assembleLine(string line, unsigned int lineNumber) // 7815772, 821250366 <- kathrin's numbers
{
    remove_comments->GlobalReplace("", &line);
    
    if(isEmptyLine->FullMatch(line))  // without any processing
        return 0;
    
    // convert whole line into upper case
    std::transform(line.begin(), line.end(), line.begin(), ::toupper);

    removeLabelDefinition->GlobalReplace("\\1", &line);
        
    // now we can process the instruction
    string opcodeStr, operandStr;
    if(!getInstructionElements->FullMatch(line, &opcodeStr, &operandStr))
        return 1;

    if(actChunk==NULL)
    {
        asmError.errorString = "Instruction reached without address specification";
        asmError.errorStringVerbose = "Specify a starting address with the .pc directive.";
        return -1;
    }
    
    Opcode opcode = opcodeMap[opcodeStr];
    if(opcode.name == "")
    {
        asmError.errorString = "Unknown instruction " + opcodeStr;
        return -1;
    }
    
    if(singleByteInstructions.find(opcodeStr)!=string::npos) // if we have a one byte instruction
    {
        if(operandStr!="")
        {
            asmError.errorString = "Unknown instruction";
            asmError.errorStringVerbose = "This instruction is not supposed to have an operand.";
            return -1;
        }
        actChunk->addByte(opcode.codes[9]); // then we're done: add it's ML code from the 'implicit' column
        actAddress++;
        return 0;
    }
    else // we need to decode the addressing mode
    {   // first look for a label in the operand
        int imm = 0, indx = 0, indy = 0, indi = 0;		
		 
        string rawLabel;
        
        // handle label references
		 if((imm=checkSimpleLabelReference->FullMatch(operandStr, &rawLabel)) || (indx=checkXIndexedLabelReference->FullMatch(operandStr, &rawLabel)) || (indy=checkYIndexedLabelReference->FullMatch(operandStr, &rawLabel)) || (indi=checkIndirectLabelReference->FullMatch(operandStr, &rawLabel)))
        {
            if(labels[operandStr] == 0) // if label is unknown yet, then...
            {
                UnresolvedAddress unresolvedAddress;
                unresolvedAddress.address = actAddress + 1;
                unresolvedAddress.memChunk = actChunk;

                bool low, high;
                if((low=(operandStr.find("#<")!=string::npos)) || (high=(operandStr.find("#>")!=string::npos)))
                {
                    unresolvedAddress.isOneByteAddr = true;
                    rawLabel = operandStr.substr(2);
                }
                else
                    unresolvedAddress.isOneByteAddr = false;
                
                if(low)
                {
                    unresolvedAddress.isLowPart = true;
                }
                if(high)
                {
                    unresolvedAddress.isLowPart = false;
                }
                
                if((opcodeStr=="BCC")||(opcodeStr=="BCS")||(opcodeStr=="BEQ")||(opcodeStr=="BMI")||(opcodeStr=="BNE")||(opcodeStr=="BPL")||(opcodeStr=="BVC")||(opcodeStr=="BVS"))
                    unresolvedAddress.isBranch = true;
                else
                    unresolvedAddress.isBranch = false;

                unresolvedLabels[rawLabel].addresses.push_back(unresolvedAddress);
                unresolvedLabels[rawLabel].line = lineNumber;
                stringstream ss; // ...and create a fake temporary address to be able to compile this line
                if(imm)
                {
                    if(unresolvedAddress.isOneByteAddr == true)
                    {
                        if(unresolvedAddress.isLowPart == true)
                            ss << "#<$" << hex << actAddress;
                        else
                            ss << "#>$" << hex << actAddress;                            
                    }
                    else
                        ss << "$" << hex << actAddress;
                }
                if(indx)
                    ss << "$" << hex << actAddress << ",X";
                if(indy)
                    ss << "$" << hex << actAddress << ",Y";
                if(indi)
                    ss << "($" << hex << actAddress << ")";
                operandStr = ss.str();
            }
            else // label is known
            {
                stringstream ss; // convert address to hex string and assign it to operandStr
                ss << "$" << hex << labels[operandStr];
                operandStr = ss.str();
            }
            std::transform(operandStr.begin(), operandStr.end(), operandStr.begin(), ::toupper);
        }

        if(operandStr == "*")
        {
            stringstream ss;
            ss << "$" << hex << actAddress;
            operandStr = ss.str();
            std::transform(operandStr.begin(), operandStr.end(), operandStr.begin(), ::toupper);
        }
        
        // check for '*' in operand
        string operatorStr; string valueStr; int value;
        if(detectAsteriskExpression->FullMatch(operandStr, &operatorStr, &valueStr))
        {
            value = convertIntoDecimal(valueStr);
            if(value > 127)
            {
                asmError.errorString = "Branch out of range";
                asmError.errorStringVerbose = "You can only jump +/-127 bytes with a branch instruction.";
                return -1;
            }
            word tmpAddress;
            
            if(operatorStr=="+")
            {
                tmpAddress = actAddress + (word)value;
            }
            else
            {
                if(operatorStr=="-")
                {
                    tmpAddress = actAddress - (word)value;
                }
                else
                {
                    asmError.errorString = "Invalid operator";
                    return  -1;
                }
            }
            
            stringstream ss;
            ss << "$" << hex << tmpAddress;
            operandStr = ss.str();
            std::transform(operandStr.begin(), operandStr.end(), operandStr.begin(), ::toupper);
            
//            cout << opcodeStr << " " << operandStr << endl;
        }

        string operandValue;

        // check immediate: LDA #0, LDA #$12, LDA #%10010011, LDA #<$3322
        if(checkImmediateAddr->FullMatch(operandStr, &operandValue))
        {
            int value = convertIntoDecimal(operandValue);
            if(value==-1)
            {
                asmError.errorString = "Invalid number type: " + operandStr;
                return -1;
            }
            
            if(operandStr.find("#<")!=string::npos)
                value = value & 0xff;
            if(operandStr.find("#>")!=string::npos)
                value = (value & 0xff00)>>8;
            
            if((value<0) || (value>255))
            {
                stringstream ss;
                ss << "Value out of range (" << value << "/$" << hex << value << "): " << operandStr;
                asmError.errorString = ss.str();
                asmError.errorStringVerbose = "Value value must fall between 0 and 255/$ff.";
                return -1;
            }
            
           if(opcode.codes[0])
            {
                actChunk->addByte(opcode.codes[0]);
                actChunk->addByte((byte)value);
                actAddress += 2;
                return 0;
            }
        }
        
        if(checkZPorAbsolute->FullMatch(operandStr))
        {
            int address = convertIntoDecimal(operandStr);
            if(address==-1)
            {
                asmError.errorString = "Invalid number type: " + operandStr;
                return -1;
            }
            if((address<0) || (address>0xffff))
            {
                stringstream ss;
                ss << "Value out of range (" << address << "/$" << hex << address << "): " << operandStr;
                asmError.errorString = ss.str();
                asmError.errorStringVerbose = "Address value must fall between 0 and 65535/$ffff.";
                return -1;
            }
            
            // check if branch...
            if(opcode.codes[10]) // TODO: verify if it is correct
            {
                int diff = address - actAddress - 2; 
                if(abs(diff) > 127)
                {
                    asmError.errorString = "Branch out of range";
                    asmError.errorStringVerbose = "You can only jump +/-127 bytes with a branch instruction.";
                    return -1;
                }
                actChunk->addByte(opcode.codes[10]);
                actChunk->addByte(diff&0xff);
                actAddress += 2;
                return 0;
            }

            if(address<0x100) // ZeroPage: lda $10
            {
                // TODO: make exception for JSR $10, JMP $10 etc
                if(opcode.codes[1])
                {
                    actChunk->addByte(opcode.codes[1]);
                    actChunk->addByte((byte)address);
                    actAddress += 2;
                    return 0;
                }
            }
            else // Absolute: lda $1001
            {
                if(opcode.codes[4])
                {
                    actChunk->addByte(opcode.codes[4]);
                    actChunk->addWord((word)address);
                    actAddress += 3;
                    return 0;
                }
            }
        }
        
        if(checkZPXorAbsoluteX->FullMatch(operandStr, &operandValue))
        {
            int address = convertIntoDecimal(operandValue);
            //cout << "ABS,X: " << operandStr << ", address = " << hex << address << endl;
            if((address<0) || (address>0xffff)) // TODO: more precise error messages! (like before) handle the case of -1
            {
                asmError.errorString = "Address out of range or invalid syntax" + operandStr;
                return -1;
            }
            
            // note: originally the delimiter was an OR (||)
            if((opcode.codes[2]==0) && (opcode.codes[5])==0) // if neither of these are available, then quit with error.
            {
                asmError.errorString = "Unknown instruction";
                return -1;
            }
            
            if(address<0x100) // ZeroPage: lda $10,X
            {
                actChunk->addByte(opcode.codes[2]);
                actChunk->addByte((byte)address);
                actAddress += 2;
            }
            else // Absolute: lda $1001,X
            {
                actChunk->addByte(opcode.codes[5]);
                actChunk->addWord((word)address);
                actAddress += 3;
            }
            return 0;
        }

        if(checkZPYorAbsoluteY->FullMatch(operandStr, &operandValue))
        {
            int address = convertIntoDecimal(operandValue);
//            cout << "opcode = " << hex << opcode << endl;
            if((address<0) || (address>0xffff)) // TODO: more precise error messages! (like before)
            {
                asmError.errorString = "Address out of range or invalid syntax" + operandStr;
                return -1;
            }
            
            if((opcode.codes[3]==0) && (opcode.codes[6]==0))
            {
                asmError.errorString = "Unknown instruction";
                return -1;
            }
            
            if(address<0x100) // ZeroPage: ldx $10,Y
            {
                actChunk->addByte(opcode.codes[3]);
                actChunk->addByte((byte)address);
                actAddress += 2;
            }
            else // Absolute: lda $1001,Y
            {
                actChunk->addByte(opcode.codes[6]);
                actChunk->addWord((word)address);
                actAddress += 3;
            }
            return 0;
        }
        
        if(checkIndirect->FullMatch(operandStr, &operandValue))
        {
            int address = convertIntoDecimal(operandValue);
            if((address<0) || (address>0xffff)) // TODO: more precise error messages! (like before)
            {
                asmError.errorString = "Address out of range or invalid syntax" + operandStr;
                return -1;
            }
            actChunk->addByte(0x6c);
            actChunk->addWord((word)address);
            actAddress += 3;
            return 0;
        }
        
        if(checkIndexedIndirect->FullMatch(operandStr, &operandValue))
        {
            int address = convertIntoDecimal(operandValue);
            
            if((address<0) || (address > 0xff)) // TODO: more precise error messages! (like before)
            {
                asmError.errorString = "Address out of range or invalid syntax" + operandStr;
                asmError.errorStringVerbose = "Address must fall between $0 and $FF.";
                return -1;
            }
            
            actChunk->addByte(opcode.codes[7]);
            actChunk->addByte((byte)address);
            actAddress += 2;            
            return 0;
        }

        if(checkIndirectIndexed->FullMatch(operandStr, &operandValue))
        {
            int address = convertIntoDecimal(operandValue);
            
            if((address<0) || (address > 0xff)) // TODO: more precise error messages! (like before)
            {
                asmError.errorString = "Address out of range or invalid syntax" + operandStr;
                asmError.errorStringVerbose = "Address must fall between $0 and $FF.";
                return -1;
            }
            
            actChunk->addByte(opcode.codes[8]);
            actChunk->addByte((byte)address);
            actAddress += 2;
            return 0;
        }
        
        // here we check for implied addr. mode for ror, rol, asl, lsr
        if((singleByteInstructionsSpecial.find(opcodeStr)!=string::npos) && (operandStr==""))
        {
            actChunk->addByte(opcode.codes[9]);
            actAddress++;
            return 0;
        }
        
        asmError.errorString = "Unknown instruction";
    }
    
	return -1;
}

// ----------------------------------------------------------------------------
/*
 * convertIntoDecimal
 *
 * Helper function to convert a number in string format into an integer.
 * Input: a string containing the numerical value
 *        -if the string contains a leading $, it will be treated as a hexa number
 *        -if the string contains a leading %, it will be treates as a bin number
 *        -if the string begins with a numberical character, it will be treates as a decimal number
 */
// ----------------------------------------------------------------------------
int BASSembler6502::convertIntoDecimal(string valueStr)
{
    string table = "0123456789ABCDEF";
    string tmpStr;
    int i, result = 0;
    char actChar;
    int length;
    int baseValue;
    
    if(checkIfBin->FullMatch(valueStr, &tmpStr))
    {
        length = (int)tmpStr.length();
        baseValue = 1;
        for(i=length-1; i>=0; i--)
        {
            actChar = tmpStr[i];
            if(actChar=='1')
                result += baseValue;
            baseValue *= 2;
        }
        
        return result;
    }
    
    if(checkIfHex->FullMatch(valueStr, &tmpStr))
    {
        length = (int)tmpStr.length();
        baseValue = 1;
        for(i=length-1; i>=0; i--)
        {
            actChar = tmpStr[i];
            result += findChar(table, actChar) * baseValue;
            baseValue *= 16;
        }
        
        return result;
    }
    
    if(checkIfDec->FullMatch(valueStr, &tmpStr))
    {
        length = (int)tmpStr.length();
        baseValue = 1;
        for(i=length-1; i>=0; i--)
        {
            actChar = tmpStr[i];
            result += findChar(table, actChar) * baseValue;
            baseValue *= 10;
        }
        
        return result;
    }
    
    return -1; // invalid number format
}

// ----------------------------------------------------------------------------
int BASSembler6502::countChars(string text, char c)
{
	int count_ = 0;
	int size = (int)text.size();
	
	for(int i=0; i<size; i++)
		if(text[i]==c)
			count_++;

	return count_;
}
// ----------------------------------------------------------------------------
int BASSembler6502::findChar(string text, char c)
{
	int size = (int)text.size();
	
	for(int i=0; i<size; i++)
		if(text[i]==c)
			return i;
	
	return 0;
}
// ----------------------------------------------------------------------------
void BASSembler6502::initOpcodeTable()
{
    singleByteInstructions = "CLC SEC CLI SEI CLV CLD SED TAX TXA DEX INX TAY TYA DEY INY RTI RTS TXS TSX PHA PLA PHP PLP NOP";
    singleByteInstructionsSpecial = "ROL ROR ASL LSR";
    
//                     Imm,  ZP,   ZPX,  ZPY,  ABS,  ABSX, ABSY, INDX, INDY, IMPL, BRA
    Opcode _ADC("ADC", 0x69, 0x65, 0x75, 0x00, 0x6d, 0x7d, 0x79, 0x61, 0x71, 0x00, 0x00);
    Opcode _AND("AND", 0x29, 0x25, 0x35, 0x00, 0x2d, 0x3d, 0x39, 0x21, 0x31, 0x00, 0x00);
    Opcode _ASL("ASL", 0x00, 0x06, 0x16, 0x00, 0x0e, 0x1e, 0x00, 0x00, 0x00, 0x0a, 0x00);
    Opcode _BIT("BIT", 0x00, 0x24, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _BPL("BPL", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10);
    Opcode _BMI("BMI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30);
    Opcode _BVC("BVC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50);
    Opcode _BVS("BVS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70);
    Opcode _BCC("BCC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90);
    Opcode _BCS("BCS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0);
    Opcode _BNE("BNE", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0);
    Opcode _BEQ("BEQ", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0);
    Opcode _CMP("CMP", 0xc9, 0xc5, 0xd5, 0x00, 0xcd, 0xdd, 0xd9, 0xc1, 0xd1, 0x00, 0x00);
    Opcode _CPX("CPX", 0xe0, 0xe4, 0x00, 0x00, 0xec, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _CPY("CPY", 0xc0, 0xc4, 0x00, 0x00, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _DEC("DEC", 0x00, 0xc6, 0xd6, 0x00, 0xce, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _EOR("EOR", 0x49, 0x45, 0x55, 0x00, 0x4d, 0x5d, 0x59, 0x41, 0x51, 0x00, 0x00);
    Opcode _CLC("CLC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00); //
    Opcode _SEC("SEC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00); //
    Opcode _CLI("CLI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00); // 
    Opcode _SEI("SEI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00); //
    Opcode _CLV("CLV", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x00); //
    Opcode _CLD("CLD", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00); //
    Opcode _SED("SED", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00); //
    Opcode _INC("INC", 0x00, 0xe6, 0xf6, 0x00, 0xee, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _JMP("JMP", 0x00, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _JSR("JSR", 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _LDA("LDA", 0xa9, 0xa5, 0xb5, 0x00, 0xad, 0xbd, 0xb9, 0xa1, 0xb1, 0x00, 0x00);
    Opcode _LDX("LDX", 0xa2, 0xa6, 0x00, 0xb6, 0xae, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00);
    Opcode _LDY("LDY", 0xa0, 0xa4, 0xb4, 0x00, 0xac, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _LSR("LSR", 0x00, 0x46, 0x56, 0x00, 0x4e, 0x5e, 0x00, 0x00, 0x00, 0x4a, 0x00);
    Opcode _NOP("NOP", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x00); //
    Opcode _ORA("ORA", 0x09, 0x05, 0x15, 0x00, 0x0d, 0x1d, 0x19, 0x01, 0x11, 0x00, 0x00);
    Opcode _TAX("TAX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00); //
    Opcode _TXA("TXA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x00); //
    Opcode _DEX("DEX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca, 0x00); //
    Opcode _INX("INX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00); //
    Opcode _TAY("TAY", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa8, 0x00); //
    Opcode _TYA("TYA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x00); //
    Opcode _DEY("DEY", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00); //
    Opcode _INY("INY", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00); //
    Opcode _ROR("ROR", 0x00, 0x66, 0x76, 0x00, 0x6e, 0x7e, 0x00, 0x00, 0x00, 0x6a, 0x00);
    Opcode _ROL("ROL", 0x00, 0x26, 0x36, 0x00, 0x2e, 0x3e, 0x00, 0x00, 0x00, 0x2a, 0x00); //<- temporarily corrupted at xxx,y
    Opcode _RTI("RTI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00); //
    Opcode _RTS("RTS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00); //
    Opcode _SBC("SBC", 0xe9, 0xe5, 0xf5, 0x00, 0xed, 0xfd, 0xf9, 0xe1, 0xf1, 0x00, 0x00);
    Opcode _STA("STA", 0x00, 0x85, 0x95, 0x00, 0x8d, 0x9d, 0x99, 0x81, 0x91, 0x00, 0x00);
    Opcode _TXS("TXS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x00); //
    Opcode _TSX("TSX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xba, 0x00); //
    Opcode _PHA("PHA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00); //
    Opcode _PLA("PLA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00); //
    Opcode _PHP("PHP", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00); //
    Opcode _PLP("PLP", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00); //
    Opcode _STX("STX", 0x00, 0x86, 0x00, 0x96, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    Opcode _STY("STY", 0x00, 0x84, 0x94, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    opcodeMap["ADC"] = _ADC;
    opcodeMap["AND"] = _AND;
    opcodeMap["ASL"] = _ASL;
    opcodeMap["BIT"] = _BIT;
    opcodeMap["BPL"] = _BPL;
    opcodeMap["BMI"] = _BMI;
    opcodeMap["BVC"] = _BVC;
    opcodeMap["BVS"] = _BVS;
    opcodeMap["BCC"] = _BCC;
    opcodeMap["BCS"] = _BCS;
    opcodeMap["BNE"] = _BNE;
    opcodeMap["BEQ"] = _BEQ;
    opcodeMap["CMP"] = _CMP;
    opcodeMap["CPX"] = _CPX;
    opcodeMap["CPY"] = _CPY;
    opcodeMap["DEC"] = _DEC;
    opcodeMap["EOR"] = _EOR;
    opcodeMap["CLC"] = _CLC;
    opcodeMap["SEC"] = _SEC;
    opcodeMap["CLI"] = _CLI;
    opcodeMap["SEI"] = _SEI;
    opcodeMap["CLV"] = _CLV;
    opcodeMap["CLD"] = _CLD;
    opcodeMap["SED"] = _SED;
    opcodeMap["INC"] = _INC;
    opcodeMap["JMP"] = _JMP;
    opcodeMap["JSR"] = _JSR;
    opcodeMap["LDA"] = _LDA;
    opcodeMap["LDX"] = _LDX;
    opcodeMap["LDY"] = _LDY;
    opcodeMap["LSR"] = _LSR;
    opcodeMap["NOP"] = _NOP;
    opcodeMap["ORA"] = _ORA;
    opcodeMap["TAX"] = _TAX;
    opcodeMap["TXA"] = _TXA;
    opcodeMap["DEX"] = _DEX;
    opcodeMap["INX"] = _INX;
    opcodeMap["TAY"] = _TAY;
    opcodeMap["TYA"] = _TYA;
    opcodeMap["DEY"] = _DEY;
    opcodeMap["INY"] = _INY;
    opcodeMap["ROR"] = _ROR;
    opcodeMap["ROL"] = _ROL;
    opcodeMap["RTI"] = _RTI;
    opcodeMap["RTS"] = _RTS;
    opcodeMap["SBC"] = _SBC;
    opcodeMap["STA"] = _STA;
    opcodeMap["TXS"] = _TXS;
    opcodeMap["TSX"] = _TSX;
    opcodeMap["PHA"] = _PHA;
    opcodeMap["PLA"] = _PLA;
    opcodeMap["PHP"] = _PHP;
    opcodeMap["PLP"] = _PLP;
    opcodeMap["STX"] = _STX;
    opcodeMap["STY"] = _STY;
}

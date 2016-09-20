#include <iostream>
#include <string>
#include "ACFile.hpp"
#include "Bassembler6502.h"
#include <sstream> // istringstream

using namespace std;

int main (int argc, char * const argv[])
{
	BASSembler6502 asm6502;
	vector<MemChunk> *chunks;

    cout << "BASSembler6502 v0.17beta (12.06.2012) -- 6502 cross-assembler\nWritten (c) 2011-2012 by Zoltán Majoros (zoltan@arcanelab.com)" << endl << endl;
    
    if(argc<2)
    {
        cout << "Please specify a file name." << endl;
        return 0;
    }
    
	char *buffer;
	ACFile file(argv[1], buffer);
	
	if(asm6502.assemble(buffer, chunks)) // if compliation is unsuccessful...
	{
		cout << "Error: " << asm6502.asmError.errorString << " in line " << dec << asm6502.asmError.errorLineNumber << endl;
		cout << "\"" << asm6502.asmError.lineContent << "\"" << endl;
		if(asm6502.asmError.errorStringVerbose!="")
			cout << "\nHint: " << asm6502.asmError.errorStringVerbose << endl;
		
		return -1;
	}
    
	for(int i=0; i<(int)chunks->size(); i++)
	{
		MemChunk chunk = (*chunks)[i];
		cout << "block #" << i+1 << ":" << endl;
		cout << "address = $" << hex << chunk.startAddress << endl;		
		cout << "length = $" << chunk.length << endl;
		
        if(chunk.length==0)
        {
            cout << endl;
            continue;
        }
        
        // composing filename for binary
        stringstream ss;
        ss << "block-" << hex << chunk.startAddress << ".prg";
        cout << "filename: " << ss.str() << endl << endl;;
        string fileName = ss.str();
/*
        printf("byte prg[] = {");
        for(int j=0; j<chunk.length; j++)
		{
            if(j==0)
                printf("0x%.2X", chunk.data[j]);
            else
                printf(", 0x%.2X", chunk.data[j]);
			if((j%16)==15) cout << endl;
		}
        printf("};");
*/
        
		for(int j=0; j<chunk.length; j++)
		{
			//cout << hex << (unsigned int)chunk.data[j];
			printf("%.2X ", chunk.data[j]);
			if((j%16)==15) cout << endl;
		}
        
		cout << endl << endl;

        // creating buffer for binary
        char *buffer = new char[chunk.length+2];
        buffer[0] = chunk.startAddress & 0xff; // adding startaddress at beginning
        buffer[1] = (chunk.startAddress & 0xff00)>>8;
        memcpy(buffer+2, chunk.data, chunk.length);
        ACFile file;
        file.save((const string)fileName, buffer, (unsigned int)chunk.length+2); // writing out data
        delete [] buffer;
	}
	
    return 0;
}

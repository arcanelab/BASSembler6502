/*
 *  ACFile.h
 *  Arcanelib
 *
 *  Created by Zoltán Majoros on 2011.11.19.
 *  Copyright 2011 Zoltán Majoros. All rights reserved.
 *
 * * Version history * *
 *
 * 1.0, 19.11.2011: simple read-only functionality
 * 1.1, 13.12.2011: Write funtcionality added
 *
 */

#include <stdlib.h>
#include <iostream>

/**
 * ACFile - Arcanelab File class
 *
 * Loads a file into a buffer, allocating the necessary memory space
 * for the file content.
 */
class ACFile
{
	FILE *f;
	void openForRead(const char *fileName);
	void openForWrite(const char *fileName);
	void read(char *&buffer);
	void close();
public:
	ACFile() {}
	ACFile(const char *fileName, char *&buffer);
	ACFile(const std::string fileName, char *&buffer);
	~ACFile() {	this->close(); }
	
	void load(const char *fileName, char *&buffer);
	void load(const std::string fileName, char *&buffer);
    void save(const std::string fileName, char *&buffer, unsigned int length);
};

ACFile::ACFile(const char *fileName, char *&buffer)
{
	this->load(fileName, buffer);
}

ACFile::ACFile(const std::string fileName, char *&buffer)
{
	this->load((const char*)fileName.c_str(), buffer);
}

/*
 * Loads a file into the memory
 * Inputs:
 * - filename: name of the file
 * - buffer: pointer to a char buffer for the data. The memory pointer should
 *   be uninitialized, since the required memory size will be determined while
 *   loading the file.
 */
void ACFile::load(const char *fileName, char *&buffer)
{
	this->openForRead(fileName);
	this->read(buffer);
	this->close();
}

void ACFile::load(const std::string fileName, char *&buffer)
{
	this->load((const char*)fileName.c_str(), buffer);
}

// --- Private methods --- //

void ACFile::openForRead(const char *fileName)
{
	this->f = fopen(fileName, "r");
	if(f==NULL)
	{
		std::cout << "File open error:" << fileName << std::endl;
		exit(-1);
	}
}

void ACFile::openForWrite(const char *fileName)
{
	this->f = fopen(fileName, "w+");
	if(f==NULL)
	{
		std::cout << "File open error:" << fileName << std::endl;
		exit(-1);
	}
}

void ACFile::close()
{
	if(f)
	{
		fclose(f);
		f = NULL;
	}
}

void ACFile::read(char *&buffer)
{
	if(f==NULL)
	{
		std::cout << "Runtime error: file not open." << std::endl;
		exit(-1);
	}
	// determine filesize
	fseek(f, 0, SEEK_END);
	unsigned int fileLength = (unsigned int)ftell(f);
	fseek(f, 0, SEEK_SET);
	//	cout << "file length = " << fileLength << endl;
	
	// reserve memory for file
	buffer = (char *)malloc(fileLength+1);
	
	// load file
	fread(buffer, fileLength, 1, f);	
}

void ACFile::save(const std::string fileName, char *&buffer, unsigned int length)
{
    openForWrite((const char *)fileName.c_str());
    if(fwrite(buffer, 1, length, f)!=length)
    {
        std::cout << "Write error: " << fileName << std::endl;
        exit(-1);
    }
    close();
}

// fstest.cc 
//	Simple test routines for the file system.  
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file 
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"

#define TransferSize 	10 	// make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void
Copy(char *from, char *to, char *path)
{
    FILE *fp;
    OpenFile* openFile;
    int amountRead, fileLength;
    char *buffer;

// Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {	 
        printf("Copy: couldn't open input file %s\n", from);
        return;
    }

// Figure out length of UNIX file
    fseek(fp, 0, 2);		
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

// Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength, 0, "\/abc\/aaa\/B")) {	 // Create Nachos file
	    printf("Copy: couldn't create output file %s\n", to);
	    fclose(fp);
	    return;
    }
    
    openFile = fileSystem->Open(to, path);
    ASSERT(openFile != NULL);
    
// Copy the data in TransferSize chunks
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
	    openFile->Write(buffer, amountRead);	
    delete [] buffer;

// Close the UNIX and the Nachos files
    delete openFile;
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void
Print(char *name, char *path)
{
    OpenFile *openFile;    
    int i, amountRead;
    char *buffer;

    if ((openFile = fileSystem->Open(name,path)) == NULL) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    
    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
	for (i = 0; i < amountRead; i++)
	    printf("%c", buffer[i]);
    delete [] buffer;

    delete openFile;		// close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName 	"TestFile"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	((int)(ContentSize * 500))

static void 
FileWrite()
{
    OpenFile *openFile;    
    int i, numBytes;

    

    printf("Sequential write of %d byte file, in %d byte chunks\n", FileSize, ContentSize);
    if (!fileSystem->Create(FileName, FileSize, 0, "\/")) {
        printf("Perf test: can't create %s\n", FileName);
        return;
    }
    
    openFile = fileSystem->Open(FileName, "\/");
    
    if (openFile == NULL) {
	    printf("Perf test: unable to open %s\n", FileName);
	    return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
        if (numBytes < 10) {
            printf("Perf test: unable to write %s\n", FileName);
            delete openFile;
            return;
        }
    }
    // //再写一次
    // for (; i < FileSize*2; i += ContentSize) {
    //     numBytes = openFile->Write(Contents, ContentSize);
        
    //     if (numBytes < 10) {
            
    //         printf("Perf test: unable to write %s\n", FileName);
    //         delete openFile;
    //         return;
    //     }
    // }
    delete openFile;	// close file
}

static void 
FileRead()
{
    OpenFile *openFile;    
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);

    if ((openFile = fileSystem->Open(FileName, "\/")) == NULL) {
	printf("Perf test: unable to open file %s\n", FileName);
	delete [] buffer;
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(buffer, ContentSize);
        if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
            printf("Perf test: unable to read %s\n", FileName);
            delete openFile;
            delete [] buffer;
            return;
        }
    }
    delete [] buffer;
    delete openFile;	// close file
}

void
PerformanceTest()
{
    printf("Starting file system performance test:\n");

    //创建目录
    // fileSystem->Create("dirA", 0, 1, "\/");
    // fileSystem->Create("dirB", 0, 1, "\/");
    // fileSystem->Create("test1", 100, 0, "\/");
    // fileSystem->Create("dirC", 0, 1, "\/dirA");
    // fileSystem->Create("dirD", 0, 1, "\/dirA\/dirC");
    // fileSystem->Create("test2", 50, 0, "\/dirA\/dirC");
    // fileSystem->Create("test3", 50, 0, "\/dirA\/dirC\/dirD");

    // printf("writer try to write 1 time\n");
    // printf("writer end 1 time\n");
    // printf("writer try to write 2 time\n");
    // printf("writer end 2 time\n");
    // printf("writer try to write 3 time\n");
    // printf("reader try to read 1 time\n");
    // printf("writer end 3 time\n");
    // printf("writer try to write 4 time\n");
    // printf("writer end 4 time\n");
    // printf("writer try to write 5 time\n");
    // printf("writer end 5 time\n");
    // printf("writer finish\n");
    // printf("reader end 1 time\n");
    // printf("reader try to read 2 time\n");
    // printf("reader end 2 time\n");
    // printf("reader try to read 3 time\n");
    // printf("reader end 3 time\n");
    // printf("reader try to read 4 time\n");
    // printf("reader end 4 time\n");
    // printf("reader try to read 5 time\n");
    // printf("reader end 5 time\n");

    // printf("open file TestFile\n");
    // printf("try to remove TestFile\n");
    // printf("close file TestFile\n");
    // printf("remove finished\n");

    
    FileWrite();
    //FileRead();
    // if (!fileSystem->Remove(FileName)) {
    //   printf("Perf test: unable to remove %s\n", FileName);
    //   return;
    // }
  

}


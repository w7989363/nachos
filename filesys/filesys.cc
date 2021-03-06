// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {

        printf("Initializing Disk...\n");

        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
	    FileHeader *mapHdr = new FileHeader;
	    FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
	    freeMap->Mark(FreeMapSector);	    
	    freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        printf("allocate sector for bitmap.\n");

	    ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize, ""));

        printf("allocate sector for dirHdr.\n");

	    ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize, ""));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).
        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);    
        dirHdr->WriteBack(DirectorySector);
        //printf("129byteToSector: %d\n", dirHdr->ByteToSector(129));
        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");

        freeMap->WriteBack(freeMapFile);	 // flush changes to disk
        directory->WriteBack(directoryFile);
        if (DebugIsEnabled('f')) {
            freeMap->Print();
            directory->Print();

            delete freeMap; 
            delete directory; 
            delete mapHdr; 
            delete dirHdr;
        }
    } 
    else {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }

    //对信号量组进行初始化
    // groupLock = new Semaphore("grouplock",1);
    // semaphoreGroups = new SemaphoreGroup[SemaphoreNum];
    // for(int i = 0; i < SemaphoreNum; i++){
    //     semaphoreGroups[i].sector = -1;
    // }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(char *name, int initialSize, bool isDir, char *path)
{
    Directory *rootDir;
    Directory *curDir;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    rootDir = new Directory(NumDirEntries);
    rootDir->FetchFrom(directoryFile);

    //rootDir->FindSector(name,path);
    curDir = OpenDir(path);
    if(curDir == NULL){
        //打开目录失败
        printf("open dir fail.\n");
        return FALSE;
    }
    if (curDir->Find(name) != -1)
        //文件名存在
        success = FALSE;			
    else {	
        //没有重名，可以创建
        //为文件头申请扇区
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();	// find a sector to hold the file header
        if (sector == -1) {
            success = FALSE;		// no free block for file header 
        }	
        //当前目录添加目录项
        if (!curDir->Add(name, sector, isDir)){
            //目录满
            success = FALSE;	// no space in directory
        }
        else {
            //添加目录项成功
            hdr = new FileHeader;
            //新建目录
            if(isDir){
                //初始化目录文件头
                ASSERT(hdr->Allocate(freeMap, DirectoryFileSize, path));
                //文件头写回磁盘
                hdr->WriteBack(sector); 
                //新目录的文件控制结构，用于目录文件写回磁盘
                OpenFile* file = new OpenFile(sector);
                Directory* newDir = new Directory(NumDirEntries);
                //目录文件写回磁盘
                newDir->WriteBack(file);
                success = TRUE;
            }
            //新建文件
            else{
                //初始化新文件头（申请扇区，建立索引）
                ASSERT(hdr->Allocate(freeMap, initialSize, path));
                //文件头初始化成功，申请data扇区成功
                success = TRUE;
                //文件头写回磁盘
                hdr->WriteBack(sector); 
            }
            //当前目录文件写回磁盘	
            curDir->WriteBack(curDirFile);
            //bitmap写回磁盘
            freeMap->WriteBack(freeMapFile);
            delete hdr;
        }	

        delete freeMap;
    }
    delete rootDir;
    
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name, char *path)
{ 
    //打开路径所指目录
    Directory *directory = OpenDir(path);
    ASSERT(directory!=NULL);
    OpenFile *openFile = NULL;
    int sector;
    // int found = 0;
    // int freeIndex = -1;
    // SemaphoreGroup *group;
    DEBUG('f', "Opening file %s\n", name);
    sector = directory->Find(name); 
    if (sector >= 0) {
        //检索文件对应的semaphoreGroup
        // groupLock->P();
        // for(int i = 0; i < SemaphoreNum; i++){
        //     if(semaphoreGroups[i].sector == -1){
        //         freeIndex = i;
        //     }
        //     if(semaphoreGroups[i].sector == sector){
        //         group = &semaphoreGroups[i];
        //         found = 1;
        //     }
        // }
        // ASSERT(freeIndex != -1);
        // //没有对应的，第一次打开,初始化信号量组
        // if(!found){
        //     group = new SemaphoreGroup;
        //     group->sector = sector;
        //     group->readCountMutex = new Semaphore("readcount",1);
        //     group->readCount = 0;
        //     group->wt = new Semaphore("writer",1);
        //     group->openCountMutex = new Semaphore("opencound",1);
        //     group->openCount = 0;
        //     group->dt = new Semaphore("delete",1);
        //     semaphoreGroups[freeIndex] = *group;
        // }
        // groupLock->V();	
	    // openFile = new OpenFile(sector,group);	// name was found in directory 
        openFile = new OpenFile(sector);
    }
    delete directory;
    return openFile;				// return NULL if not found
}

OpenFile *
FileSystem::Open(char *name)
{ 
    int fileDescriptor = OpenForReadWrite(name, FALSE);

		if (fileDescriptor == -1) return NULL;
		return new OpenFile(fileDescriptor);
}


//打开path对应的文件目录，失败返回NULL
Directory* FileSystem::OpenDir(char *path){
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = new OpenFile(DirectorySector);
    char str[strlen(path)+1] = {"0"};
    strcpy(str, path);
    const char *d = "\/";
    char *dirName;
    int sector;

    //打开根目录
    directory->FetchFrom(directoryFile);
    //分解path
    dirName = strtok(str, d);
    //逐级查找
    while(dirName){
        sector = directory->Find(dirName);
        if(sector == -1){
            //没有匹配的文件夹
            return NULL;
        }
        delete openFile;
        openFile = new OpenFile(sector);
        directory->FetchFrom(openFile);
        //printf("Entering dir: %s\n",dirName);
        dirName = strtok(NULL, d);
    }
    curDirFile = openFile;
    return directory;
}


//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name, char *path)
{ 
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    directory = OpenDir(path);
    sector = directory->Find(name);
    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    //获取该文件的信号量组
    // groupLock->P();
    // int index = -1;
    // for(int i = 0; i < SemaphoreNum; i++){
    //     if(semaphoreGroups[i].sector == sector){
    //         index = i;
    //     }
    // }
    // groupLock->V();
    // if(index != -1){
    //     semaphoreGroups[index].dt->P();
    // }

    //如果是目录，递归删除
    if(directory->IsDir(name)){
        //打开目录
        char* p = new char[40];
        strcat(p, path);
        strcat(p, name);
        strcat(p, "\/");
        printf("%s\n",p);
        Directory *dir = OpenDir(p);
        for(int i = 0; i < NumDirEntries; i++){
            if(dir->table[i].inUse){
                Remove(dir->table[i].name, p);
            }
        }
        delete dir;
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(directoryFile);        // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;

    // if(index != -1){
    //     semaphoreGroups[index].dt->V();
    // }
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    ListAll("\/");

}

//递归列出所有文件夹内容
void FileSystem::ListAll(char *path){
    Directory* dir = OpenDir(path);
    printf("%s:\n",path);
    dir->List();

    for(int i = 0; i < NumDirEntries; i++){
        if(dir->table[i].inUse && dir->table[i].isDir){
            //获取目录名，拼接path
            char* p = new char[40];
            strcat(p, path);
            strcat(p, dir->table[i].name);
            strcat(p, "\/");
            ListAll(p);
        }
    }
    delete dir;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 

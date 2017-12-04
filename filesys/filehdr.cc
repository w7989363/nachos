// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include "time.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize, char *p)
{ 
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    printf("file total sector: %d\n",numSectors);
    if (freeMap->NumClear() < numSectors)
	    return FALSE;		// not enough space
    
    //创建时间
    time_t timep;
    time(&timep);
    strftime(createTime, sizeof(createTime), "%Y-%m-%d %H:%M:%S", localtime(&timep));
    printf("createTime: %s\n",createTime);
    //路径
    strncpy(path, p, strlen(p)); 
    printf("file path:%s\n",path);


    //构建索引表
    //初始化
    for(int i = 0; i < NumDirect; i++){
        dataSectors[i] = 0;
    }
    for(int i = 0; i < NumIndirect; i++){
        secondSectors[i] = 0;
    }
    //只需要一级索引
    if(numSectors <= NumDirect){
        for (int i = 0; i < numSectors; i++){
            dataSectors[i] = freeMap->Find();
            //printf("allocate sector for data :%d\n",dataSectors[i]);
        }
    }
    //需要二级索引
    else{
        //首先为一级索引分配扇区
        for (int i = 0; i < NumDirect; i++){
            dataSectors[i] = freeMap->Find();
            //printf("allocate sector for data :%d\n",dataSectors[i]);
        }
        //计算还需要多少个扇区 （直接用常量做减法会出错）
        int leftSectors = -(NumDirect-numSectors);
        //一个扇区能存放多少一级索引
        int numFirstIndexInaSector = SectorSize / sizeof(int);
        //计算需要多少个二级索引
        int numSecondIndex = divRoundUp(leftSectors, numFirstIndexInaSector);
        //为每个二级索引分配扇区
        for(int i = 0; i < numSecondIndex; i++){
            //二级索引指向的扇区
            secondSectors[i] = freeMap->Find();
            //printf("allocate sector for secondIndex :%d\n",secondSectors[i]);
            //二级索引指向的扇区存放的一级索引表
            int dataSecondSectors[numFirstIndexInaSector] = {0};
            for(int j = 0; j < numFirstIndexInaSector; j++){
                if(leftSectors == 0){
                    //如果没有剩余扇区未分配，则跳出循环
                    break;
                }
                //为一级索引表分配扇区
                dataSecondSectors[j] = freeMap->Find();
                //printf("allocate sector for data :%d\n",dataSecondSectors[j]);
                //剩余扇区递减
                leftSectors--;
            }
            //将一级索引表写入扇区
            synchDisk->WriteSector(secondSectors[i],(char*)dataSecondSectors);
        }

    }
    
    return TRUE;
}

//增加n个数据扇区，更新索引表和numSectors
//失败返回false
bool FileHeader::AppendSectors(int n){
    printf("Append %d sectors\n",n);
    BitMap* freeMap = new BitMap(NumSectors);
    OpenFile* freeMapFile = new OpenFile(0);
    freeMap->FetchFrom(freeMapFile);
    //磁盘没有足够空间
    if(freeMap->NumClear() < n){
        return FALSE;
    }
    //只需要一级索引
    if(numSectors + n <= NumDirect){
        for (int i = numSectors; i < (numSectors+n); i++){
            dataSectors[i] = freeMap->Find();
        }
    }
    //二级索引
    else{
        //首先为一级索引分配扇区
        for (int i = 0; i < NumDirect; i++){
            //该项未分配
            if(dataSectors[i] == 0){
                dataSectors[i] = freeMap->Find();
            }
        }
        //计算还需要多少个扇区 （直接用常量做减法会出错）
        int leftSectors = -(NumDirect-numSectors-n);
        //一个扇区能存放多少一级索引
        int numFirstIndexInaSector = SectorSize / sizeof(int);
        //计算需要多少个二级索引
        int numSecondIndex = divRoundUp(leftSectors, numFirstIndexInaSector);
        //为每个二级索引分配扇区
        for(int i = 0; i < numSecondIndex; i++){
            //新建一级索引表
            int dataSecondSectors[numFirstIndexInaSector] = {0};
            //二级索引指向的扇区未分配
            if(secondSectors[i] == 0){
                secondSectors[i] = freeMap->Find();
            }
            //已经分配，则读出一级索引表
            else{
                synchDisk->ReadSector(secondSectors[i],(char*)dataSecondSectors);
            }
            //遍历一级索引表
            for(int j = 0; j < numFirstIndexInaSector; j++){
                //如果没有剩余扇区未分配，则跳出循环
                if(leftSectors == 0){
                    break;
                }
                //该项索引未分配
                if(dataSecondSectors[j] == 0){
                    //为一级索引表分配扇区
                    dataSecondSectors[j] = freeMap->Find();
                }
                //剩余扇区递减
                leftSectors--;
            }
            //将一级索引表写入扇区
            synchDisk->WriteSector(secondSectors[i],(char*)dataSecondSectors);
        }
    }
    //更新扇区总数
    numSectors += n;
    //freeMap写回磁盘
    freeMap->WriteBack(freeMapFile);
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    //只用到了一级索引
    if(numSectors <= NumDirect){
        for (int i = 0; i < numSectors; i++) {
	        ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	        freeMap->Clear((int) dataSectors[i]);
            //printf("Deallocate sector for data :%d\n",dataSectors[i]);
        }
    }
    //有二级索引
    else{
        //释放一级索引
        for (int i = 0; i < NumDirect; i++){
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	        freeMap->Clear((int) dataSectors[i]);
            //printf("Deallocate sector for data :%d\n",dataSectors[i]);
        }
        //计算还需要释放多少个扇区
        int leftSectors = -(NumDirect-numSectors);
        //一个扇区能存放多少一级索引
        int numFirstIndexInaSector = SectorSize / sizeof(int);
        //计算需要释放多少个二级索引
        int numSecondIndex = divRoundUp(leftSectors, numFirstIndexInaSector);
        for(int i = 0; i < numSecondIndex; i++){
            //一级索引表
            int dataSecondSectors[numFirstIndexInaSector];
            //从二级索引指向的扇区读出一级索引表
            synchDisk->ReadSector(secondSectors[i], (char*)dataSecondSectors);  
            for(int j = 0; j < numFirstIndexInaSector; j++){
                if(leftSectors == 0){
                    //如果没有剩余扇区，则跳出循环
                    break;
                }
                //释放
                ASSERT(freeMap->Test((int) dataSecondSectors[j]));  // ought to be marked!
	            freeMap->Clear((int) dataSecondSectors[j]);
                //printf("Deallocate sector for data :%d\n",dataSecondSectors[j]);
                //剩余扇区递减
                leftSectors--;
            }
            //释放二级索引表占用的扇区
            ASSERT(freeMap->Test((int) secondSectors[i]));  // ought to be marked!
	        freeMap->Clear((int) secondSectors[i]);
            //printf("Deallocate sector for secondIndex :%d\n",secondSectors[i]);
        }
    }
    
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int num = divRoundDown(offset, SectorSize) + 1;
    //内容在一级索引
    if(num <= NumDirect){
        return(dataSectors[offset / SectorSize]);
    }
    //内容在二级索引
    else{
        int numDirect = (int)NumDirect;
        //一个扇区能存放多少一级索引
        int numFirstIndexInaSector = SectorSize / sizeof(int);
        //二级索引
        int secIndex = (num - numDirect - 1) / numFirstIndexInaSector;
        //一级索引
        int firIndex = (num - numDirect - 1) % numFirstIndexInaSector;
        int dataSecondSectors[numFirstIndexInaSector];
        //读取一级索引表所在扇区
        synchDisk->ReadSector(secondSectors[secIndex],(char*)dataSecondSectors);
        return dataSecondSectors[firIndex];
        
    }
    
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j;
    int numFirstIndexInaSector = SectorSize / sizeof(int);
    printf("FileHeader contents.  File size: %d.  File blocks: %d \n", numBytes, numSectors);
    //输出一级索引
    printf("FirstIndex:\n");
    for (i = 0; i < NumDirect; i++){
        printf("%d ", dataSectors[i]);
    }
	printf("\n");
    //输出二级索引
    for(i = 0; i < NumIndirect; i++){
        if(secondSectors[i] == 0){
            break;
        }
        printf("SecondIndex sector:%d\n",secondSectors[i]);
        int dataSecondSectors[numFirstIndexInaSector] = {0};
        //读取一级索引表
        synchDisk->ReadSector(secondSectors[i],(char*)dataSecondSectors);
        for(j = 0; j < numFirstIndexInaSector; j++){
            if(dataSecondSectors[j] == 0){
                break;
            }
            printf("%d ",dataSecondSectors[j]);
        }
        printf("\n");
    }
}

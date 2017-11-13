// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
                    numPages, size);
                    
    //每个用户空间占用连续的一块虚存，
    //设置用户空间的虚拟页号偏移量
    vpnoffset = machine->swapoffset;
    //设置已使用虚存的页偏移量
    machine->swapoffset += numPages;
    ASSERT(machine->swapoffset <= NumPhysPages);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = vpnoffset + i;	// for now, virtual page # = phys page #
        // //由bitmap控制物理内存分配
        // pageTable[i].physicalPage = machine->bitmap->Find();
        // //物理内存满
        // ASSERT(pageTable[i].physicalPage != -1);
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
                        // a separate page, we could set its 
                        // pages to be read-only
    }
    
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    //bzero(machine->mainMemory, size);

// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        //将代码段写入虚存数组
        /*
        int pos = noffH.code.inFileAddr;
        for(i = 0; i < noffH.code.size; i++){
            int vpn = (noffH.code.virtualAddr + i) / PageSize;
            int offset = (noffH.code.virtualAddr + i) % PageSize;
            //int phyaddr = pageTable[vpn].physicalPage * PageSize + offset;
            ASSERT((vpn+vpnoffset)*PageSize + offset < MemorySize);
            executable->ReadAt(&(machine->swapspace[(vpn+vpnoffset)*PageSize + offset]), 1, pos++);
        }
        */
        //executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),noffH.code.size, noffH.code.inFileAddr);
        //将代码段逐个字节写入内存
        int pos = noffH.code.inFileAddr;
        int ppn = -1;
        int lastvpn = noffH.code.virtualAddr / PageSize -1;
        for(i = 0; i < noffH.code.size; i++){
            int vpn = (noffH.code.virtualAddr + i) / PageSize;
            printf("lvpn:%d, vpn:%d\n",lastvpn,vpn);
            if(lastvpn != vpn){
                ppn = machine->bitmap->Find();
            }
            ASSERT(ppn != -1);
            int offset = (noffH.code.virtualAddr + i) % PageSize;
            int phyaddr = ppn * PageSize + offset;
            executable->ReadAt(&(machine->mainMemory[phyaddr]), 1, pos++);
            ASSERT(phyaddr/PageSize < NumPhysPages);
            machine->rPageTable[ppn].virtualPage = vpn;
            machine->rPageTable[ppn].valid = TRUE;
            machine->rPageTable[ppn].tid = currentThread->getTid();
            lastvpn = vpn;
        }
        printf("1\n");
        //printf("codesize:%d,infileaddr:%d,vaddr:%d\n",noffH.code.size,noffH.code.inFileAddr,noffH.code.virtualAddr);
        //printf("dataszie:%d,infileaddr:%d,vaddr:%d\n",noffH.initData.size,noffH.initData.inFileAddr,noffH.initData.virtualAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        //将数据段写入虚存数组
        int pos = noffH.initData.inFileAddr;
        for(i = 0; i < noffH.initData.size; i++){
            int vpn = (noffH.initData.virtualAddr + i) / PageSize;
            int offset = (noffH.initData.virtualAddr + i) % PageSize;
            //int phyaddr = pageTable[vpn].physicalPage * PageSize + offset;
            executable->ReadAt(&(machine->swapspace[(vpn+vpnoffset)*PageSize + offset]), 1, pos++);
        }

        //executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),noffH.initData.size, noffH.initData.inFileAddr);
        //将数据段逐个字节写入内存
        /*
        int pos = noffH.initData.inFileAddr;
        for(i = 0; i < noffH.initData.size; i++){
            int vpn = (noffH.initData.virtualAddr + i) / PageSize;
            int offset = (noffH.initData.virtualAddr + i) % PageSize;
            int phyaddr = pageTable[vpn].physicalPage * PageSize + offset;
            executable->ReadAt(&(machine->mainMemory[phyaddr]), 1, pos++);
        }
        */
    }
    printf("2\n");
    //输出内存占用量
    //machine->bitmap->PrintUsage();
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    for(int i = 0; i < numPages; i++){
        machine->bitmap->Clear(pageTable[i].physicalPage);
    }
    delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

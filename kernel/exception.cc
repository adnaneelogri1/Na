/*! \file exception.cc
//  \brief Entry point into the Nachos kernel .
//
//    There are two kinds of things that can cause control to
//    transfer back to here:
//
//    syscall -- The user code explicitly requests to call a Nachos
//    system call
//
//    exceptions -- The user code does something that the CPU can't handle.
//    For instance, accessing memory that doesn't exist, arithmetic errors,
//    etc.
//
//    Interrupts (which can also cause control to transfer from user
//    code into the Nachos kernel) are handled elsewhere.
 * -----------------------------------------------------
* This file is part of the Nachos-RiscV distribution
* Copyright (c) 2022 University of Rennes 1.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, version 3.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details
* (see see <http://www.gnu.org/licenses/>).
* -----------------------------------------------------
*/
#include "drivers/drvACIA.h"
#include "drivers/drvConsole.h"
#include "filesys/oftable.h"
#include "kernel/msgerror.h"
#include "kernel/synch.h"
#include "kernel/system.h"
#include "machine/machine.h"
#include "userlib/syscall.h"
#include "vm/pagefaultmanager.h"

//----------------------------------------------------------------------
// GetLengthParam
/*! Returns the length of a string stored in the machine memory,
//    including the '\0' terminal
//
// \param addr is the memory address of the string */
//----------------------------------------------------------------------
static int
GetLengthParam(int addr) {
  int i = 0;
  uint64_t c = 1;   // Non nul value to start scanning the string

  // Scan the string until the null character is found
  while (c != 0) {
    g_machine->mmu->ReadMem(addr++, 1, &c);
    i++;
  }
  return i + 1;
}

//----------------------------------------------------------------------
// GetStringParam
/*!	Copies a string from the machine memory
//
//	\param addr is the memory address of the string
//	\param dest is where the string is going to be copied
//      \param maxlen maximum length of the string to copy in dest,
//        including the trailing '\0'
*/
//----------------------------------------------------------------------
static void
GetStringParam(uint64_t addr, char *dest, int maxlen) {
  int i = 0;
  uint64_t c = 1;   // Non nul value to start scanning the string

  while ((c != 0) && (i < maxlen)) {
    // Read a character from the machine memory
    g_machine->mmu->ReadMem(addr++, 1, &c);
    // Put it in the kernel memory
    dest[i++] = (char) c;
  }
  // Force a \0 at the end
  dest[maxlen - 1] = '\0';
}

//----------------------------------------------------------------------
// ExceptionHandler
/*!   Entry point into the Nachos kernel.  Called when a user program
//    is executing, and either does a syscall, or generates an addressing
//    or arithmetic exception.
//
//    For system calls, the calling convention is the following:
//
//    - system call identifier -- r17 (REG_NO_SYSCALL)
//    - arg1 -- r10 (REG_SYSCAL_PARAM_1)
//    - arg2 -- r11 (REG_SYSCAL_PARAM_2)
//    - arg3 -- r12 (REG_SYSCAL_PARAM_3)
//    - arg4 -- r13 (REG_SYSCAL_PARAM_4)
//
//    The result of the system call, if any, must be put back into register r10
(REG_RET_SYSCALL)
//
//    \param exceptiontype is the kind of exception.
//           The list of possible exception are defined in machine.h.
//    \param vaddr is the address that causes the exception to occur
//           (when used)
*/
//----------------------------------------------------------------------
void
ExceptionHandler(ExceptionType exceptiontype, int vaddr) {

  // Get the content of register 17 (system call number in case
  // of a system call
  int64_t no_syscall = g_machine->ReadIntRegister(REG_NO_SYSCALL);

  switch (exceptiontype) {
  case NO_EXCEPTION:
    printf("Nachos internal error, a NoException exception is raised ...\n");
    g_machine->interrupt->Halt(NO_ERROR);
    break;

  case SYSCALL_EXCEPTION: {

    // System calls
    // -------------
    switch (no_syscall) {
      char msg[MAXSTRLEN];   // Argument for the PError system call

    case SC_HALT:
      // The halt system call. Stops Nachos.
      DEBUG('e', (char *) "Shutdown, initiated by user program.\n");
      g_machine->interrupt->Halt(NO_ERROR);
      g_syscall_error->SetMsg((char *) "", NO_ERROR);
      return;

    case SC_SYS_TIME: {
      // The systime system call. Gets the system time
      DEBUG('e', (char *) "Systime call, initiated by user program.\n");
      uint64_t addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      uint64_t tick = g_stats->getTotalTicks();
      uint32_t seconds =
          (uint32_t) cycle_to_sec(tick, g_cfg->ProcessorFrequency);
      uint32_t nanos =
          (uint32_t) cycle_to_nano(tick, g_cfg->ProcessorFrequency);
      g_machine->mmu->WriteMem(addr, sizeof(uint32_t), seconds);
      g_machine->mmu->WriteMem(addr + 4, sizeof(uint32_t), nanos);
      g_syscall_error->SetMsg((char *) "", NO_ERROR);
      break;
    }

    case SC_EXIT: {
      // The exit system call
      // Ends the calling thread
      DEBUG('e', (char *) "Thread 0x%x %s exit call.\n", g_current_thread,
            g_current_thread->GetName());
      ASSERT(g_current_thread->type == THREAD_TYPE);
      g_current_thread->Finish();
      break;
    }

    case SC_EXEC: {
      // The exec system call
      // Creates a new process (thread+address space)
      DEBUG('e', (char *) "Process: Exec call.\n");
      uint64_t addr;
      uint64_t size;
      char name[MAXSTRLEN];
      uint64_t error = NO_ERROR;

      // Get the process name
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      size = GetLengthParam(addr);
      char ch[size];
      GetStringParam(addr, ch, size);
      sprintf(name, "master thread of process %s", ch);
      Process *p = new Process(ch, &error);
      if (error != NO_ERROR) {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        if (error == OUT_OF_MEMORY)
          g_syscall_error->SetMsg((char *) "", error);
        else
          g_syscall_error->SetMsg(ch, error);
        break;
      }
      Thread *ptThread = new Thread(name);
      int32_t tid = g_object_addrs->AddObject(ptThread);
      error = ptThread->Start(p, p->addrspace->getCodeStartAddress64(), -1);
      if (error != NO_ERROR) {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        if (error == OUT_OF_MEMORY)
          g_syscall_error->SetMsg((char *) "", error);
        else
          g_syscall_error->SetMsg(name, error);
        break;
      }
      g_syscall_error->SetMsg((char *) "", NO_ERROR);
      g_machine->WriteIntRegister(REG_RET_SYSCALL, tid);
      break;
    }

    case SC_NEW_THREAD: {
      // The newThread system call
      // Create a new thread in the same address space
      DEBUG('e', (char *) "Multithread: NewThread call.\n");
      Thread *ptThread;
      uint64_t name_addr;
      int64_t fun;
      uint64_t arg;
      uint64_t err = NO_ERROR;
      // Get the address of the string for the name of the thread
      name_addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      // Get the pointer to the function to be executed by the new thread
      fun = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);
      // Get the function parameters
      arg = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_3);
      // Build the name of the thread
      int size = GetLengthParam(name_addr);
      char thr_name[size];
      GetStringParam(name_addr, thr_name, size);
      // char *proc_name = g_current_thread->getProcessOwner()->getName();
      //  Finally start it
      ptThread = new Thread(thr_name);
      int32_t tid = g_object_addrs->AddObject(ptThread);
      err = ptThread->Start(g_current_thread->GetProcessOwner(), fun, arg);
      if (err != NO_ERROR) {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg((char *) "", err);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, tid);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      }
      break;
    }

    case SC_JOIN: {
      // The join system call
      // Wait for the thread idThread to finish
      DEBUG('e', (char *) "Process or thread: Join call.\n");
      int64_t tid;
      Thread *ptThread;
      tid = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      ptThread = (Thread *) g_object_addrs->SearchObject(tid);
      if (ptThread && ptThread->type == THREAD_TYPE) {
        g_current_thread->Join(ptThread);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
      } else
      // Thread already terminated (type set to INVALID_TYPE) or call on an
      // object that is not a thread Exit with no error code since we cannot
      // separate the two cases
      {
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
      }
      DEBUG('e', (char *) "Fin Join");
      break;
    }

    case SC_YIELD: {
      DEBUG('e', (char *) "Process or thread: Yield call.\n");
      if (g_current_thread->type == THREAD_TYPE) {
        g_current_thread->Yield();
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
      } else {
        g_syscall_error->SetMsg((char *) "", INVALID_SEMAPHORE_ID);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
      }
      break;
    }

    case SC_PERROR: {
      // the PError system call
      // print the last error message
      DEBUG('e', (char *) "Debug: Perror call.\n");
      uint64_t size;
      int addr;
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      size = GetLengthParam(addr);
      char ch[size];
      GetStringParam(addr, ch, size);
      g_syscall_error->PrintLastMsg(g_console_driver, ch);
      break;
    }

    case SC_CREATE: {
      // The create system call
      // Create a new file in nachos file system
      DEBUG('e', (char *) "Filesystem: Create call.\n");
      uint64_t addr;
      int size;
      uint64_t ret;
      int sizep;
      // Get the name and initial size of the new file
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      size = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);
      sizep = GetLengthParam(addr);
      char ch[sizep];
      GetStringParam(addr, ch, sizep);
      // Try to create it
      int err = g_file_system->Create(ch, size);
      if (err == NO_ERROR) {
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
        ret = NO_ERROR;
      } else {
        ret = ERROR;
        if (err == OUT_OF_DISK)
          g_syscall_error->SetMsg((char *) "", err);
        else
          g_syscall_error->SetMsg(ch, err);
      }
      g_machine->WriteIntRegister(REG_RET_SYSCALL, ret);
      break;
    }

    case SC_OPEN: {
      // The open system call
      // Opens a file and returns an openfile identifier
      DEBUG('e', (char *) "Filesystem: Open call.\n");
      uint64_t addr;
      int sizep;
      int ret = 0;
      // Get the file name
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      sizep = GetLengthParam(addr);
      char ch[sizep];
      GetStringParam(addr, ch, sizep);
      // Try to open the file
      OpenFile *file = g_open_file_table->Open(ch);
      if (file == NULL) {
        g_syscall_error->SetMsg(ch, OPENFILE_ERROR);
      } else {
        ret = g_object_addrs->AddObject(file);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      }
      g_machine->WriteIntRegister(REG_RET_SYSCALL, ret);
      break;
    }

    case SC_READ: {
      // The read system call
      // Read in a file or the console
      DEBUG('e', (char *) "Filesystem: Read call.\n");
      uint64_t addr;
      int size;
      int64_t f;
      int numread;
      // Get the buffer address in the machine memory
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      // Get the requested size
      size = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);
      // Get the openfile number or 0 (console)
      f = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_3);
      char buffer[size];

      // Read in a file
      if (f != CONSOLE_INPUT) {
        int64_t fid = f;
        OpenFile *file = (OpenFile *) g_object_addrs->SearchObject(fid);
        if (file && file->type == FILE_TYPE) {
          numread = file->Read(buffer, size);
          g_syscall_error->SetMsg((char *) "", NO_ERROR);
        } else {
          numread = ERROR;
          sprintf(msg, "%" PRId64 "", f);
          g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
        }
      }
      // Read on the console
      else {
        g_console_driver->GetString(buffer, size);
        DEBUG('e', (char *) "Console read. We have %s of size %d\n", buffer,
              size);
        numread = size;
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      }
      for (int i = 0; i < numread;
           i++) {   // copy the buffer into the emulator memory
        g_machine->mmu->WriteMem(addr++, 1, buffer[i]);
      }
      g_machine->WriteIntRegister(REG_RET_SYSCALL, numread);
      break;
    }

    case SC_WRITE: {
      // The write system call
      // Write in a file or at the console
      DEBUG('e', (char *) "Filesystem: Write call.\n");
      uint64_t addr;
      int size;
      uint64_t f;
      uint64_t c;
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      size = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);
      // f is the openfileid or 1 (console)
      f = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_3);
      char buffer[size];
      for (int i = 0; i < size; i++) {
        g_machine->mmu->ReadMem(addr++, 1, &c);
        buffer[i] = c;
      }
      int numwrite;
      // Write in a file
      if (f > CONSOLE_OUTPUT) {
        int64_t fid = f;
        OpenFile *file = (OpenFile *) g_object_addrs->SearchObject(fid);
        if (file && file->type == FILE_TYPE) {
          // write in file
          numwrite = file->Write(buffer, size);
          g_syscall_error->SetMsg((char *) "", NO_ERROR);
        } else {
          numwrite = ERROR;
          sprintf(msg, "%" PRId64 "", f);
          g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
        }
      }
      // write at the console
      else {
        if (f == CONSOLE_OUTPUT) {
          g_console_driver->PutString(buffer, size);
          numwrite = size;
          g_syscall_error->SetMsg((char *) "", NO_ERROR);
        } else {
          numwrite = ERROR;
          sprintf(msg, "%" PRId64 "", f);
          g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
        }
      }
      g_machine->WriteIntRegister(REG_RET_SYSCALL, numwrite);
      break;
    }

    case SC_SEEK: {
      // Seek to a given position in an opened file
      DEBUG('e', (char *) "Filesystem: Seek call.\n");
      int offset;
      int64_t f;
      uint64_t error = NO_ERROR;

      // Get the offset into the file
      offset = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      // Get the openfile number or 1 (console)
      f = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);

      // Seek into a file
      if (f > CONSOLE_OUTPUT) {
        int64_t fid = f;
        OpenFile *file = (OpenFile *) g_object_addrs->SearchObject(fid);
        if (file && file->type == FILE_TYPE) {
          file->Seek(offset);
          g_syscall_error->SetMsg((char *) "", NO_ERROR);
        } else {
          error = ERROR;
          sprintf(msg, "%" PRId64 "", f);
          g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
        }
        g_machine->WriteIntRegister(REG_RET_SYSCALL, error);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        sprintf(msg, "%" PRId64 "", f);
        g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
      }
      break;
    }

    case SC_CLOSE: {
      // The close system call
      // Close a file
      DEBUG('e', (char *) "Filesystem: Close call.\n");
      // Get the openfile number
      int64_t fid = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      OpenFile *file = (OpenFile *) g_object_addrs->SearchObject(fid);
      if (file && file->type == FILE_TYPE) {
        g_open_file_table->Close(file->GetName());
        g_object_addrs->RemoveObject(fid);
        delete file;
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        sprintf(msg, "%" PRId64 "", fid);
        g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
      }
      break;
    }

    case SC_REMOVE: {
      // The Remove system call
      // Remove a file from the file system
      DEBUG('e', (char *) "Filesystem: Remove call.\n");
      uint64_t ret;
      uint64_t addr;
      int sizep;
      // Get the name of the file to be removes
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      sizep = GetLengthParam(addr);
      char *ch = new char[sizep];
      GetStringParam(addr, ch, sizep);
      // Actually remove it
      int err = g_open_file_table->Remove(ch);
      if (err == NO_ERROR) {
        ret = 0;
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      } else {
        ret = ERROR;
        g_syscall_error->SetMsg(ch, err);
      }
      g_machine->WriteIntRegister(REG_RET_SYSCALL, ret);
      break;
    }

    case SC_MKDIR: {
      // the Mkdir system call
      // make a new directory in the file system
      DEBUG('e', (char *) "Filesystem: Mkdir call.\n");
      uint64_t addr;
      int sizep;
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      sizep = GetLengthParam(addr);
      char name[sizep];
      GetStringParam(addr, name, sizep);
      // name is the name of the new directory
      uint64_t good = g_file_system->Mkdir(name);
      if (good != NO_ERROR) {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        if (good == OUT_OF_DISK)
          g_syscall_error->SetMsg((char *) "", good);
        else
          g_syscall_error->SetMsg(name, good);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, (good));
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      }
      break;
    }

    case SC_RMDIR: {
      // the Rmdir system call
      // remove a directory from the file system
      DEBUG('e', (char *) "Filesystem: Rmdir call.\n");
      uint64_t addr;
      int sizep;
      addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      sizep = GetLengthParam(addr);
      char name[sizep];
      GetStringParam(addr, name, sizep);
      uint64_t good = g_file_system->Rmdir(name);
      if (good != NO_ERROR) {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg(name, good);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, good);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      }
      break;
    }

    case SC_FSLIST: {
      // The FSList system call
      // Lists all the file and directories in the filesystem
      g_file_system->List();
      g_syscall_error->SetMsg((char *) "", NO_ERROR);
      break;
    }

    case SC_TTY_SEND: {
      // the TtySend system call
      // Sends some char by the serial line emulated
      DEBUG('e', (char *) "ACIA: Send call.\n");
      if (g_cfg->ACIA != ACIA_NONE) {
        uint64_t result;
        uint64_t c;
        uint64_t addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
        char buff[MAXSTRLEN];
        for (int i = 0;; i++) {
          g_machine->mmu->ReadMem(addr + i, 1, &c);
          buff[i] = (char) c;
          if (buff[i] == '\0')
            break;
        }
        result = g_acia_driver->TtySend(buff);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, result);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg((char *) "", NO_ACIA);
      }
      break;
    }

    case SC_TTY_RECEIVE: {
      // the TtyReceive system call
      // read some char on the serial line
      DEBUG('e', (char *) "ACIA: Receive call.\n");
      if (g_cfg->ACIA != ACIA_NONE) {
        uint64_t result;
        uint64_t addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
        int length = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);
        char buff[length + 1];
        result = g_acia_driver->TtyReceive(buff, length);
        int i = 0;
        while ((i <= length)) {
          g_machine->mmu->WriteMem(addr, 1, buff[i]);
          addr++;
          i++;
        }
        g_machine->mmu->WriteMem(addr, 1, 0);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, result);
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg((char *) "", NO_ACIA);
      }
      break;
    }

    case SC_MMAP: {
      // Map a file in memory
      DEBUG('e', (char *) "Filesystem: Mmap call.\n");
      uint64_t fid = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
      OpenFile *file = (OpenFile *) g_object_addrs->SearchObject(fid);
      if (file) {
        int size = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2);
        AddrSpace *ap = g_current_thread->GetProcessOwner()->addrspace;
        int addr = ap->Mmap(file, size);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ((int) addr));
        g_syscall_error->SetMsg((char *) "", NO_ERROR);
      } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        sprintf(msg, "%p", file);
        g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
      }
      break;
    }

    case SC_DEBUG: {
      // Map a file in memory
      DEBUG('e', (char *) "Nachos: debug system call.\n");
      printf("Debug system call: parameter %" PRIu64 "\n",
             g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1));
      break;
    }

    default:
      printf("Invalid system call number : %" PRIu64 "\n", no_syscall);
      exit(ERROR);
      break;
    }

    break;
  }

  // Other exceptions
  // ----------------
  case READONLY_EXCEPTION:
    printf("FATAL USER EXCEPTION (Thread %s, PC=0x%" PRIx64 "):\n",
           g_current_thread->GetName(), g_machine->pc);
    printf("\t*** Write to virtual address 0x%x on read-only page ***\n",
           vaddr);
    g_machine->interrupt->Halt(ERROR);
    break;

  case BUSERROR_EXCEPTION:
    printf("FATAL USER EXCEPTION (Thread %s, PC=0x%" PRIx64 "):\n",
           g_current_thread->GetName(), g_machine->pc);
    printf("\t*** Bus error on access to virtual address 0x%x ***\n", vaddr);
    g_machine->interrupt->Halt(ERROR);
    break;

  case ADDRESSERROR_EXCEPTION:
    printf("FATAL USER EXCEPTION (Thread %s, PC=0x%" PRIx64 "):\n",
           g_current_thread->GetName(), g_machine->pc);
    printf("\t*** Access to invalid or unmapped virtual address 0x%x ***\n",
           vaddr);
    g_machine->interrupt->Halt(ERROR);
    break;

  case OVERFLOW_EXCEPTION:
    printf("FATAL USER EXCEPTION (Thread %s, PC=0x%" PRIx64 "):\n",
           g_current_thread->GetName(), g_machine->pc);
    printf("\t*** Overflow exception at address 0x%x ***\n", vaddr);
    g_machine->interrupt->Halt(ERROR);
    break;

  case ILLEGALINSTR_EXCEPTION:
    printf("FATAL USER EXCEPTION (Thread %s, PC=0x%" PRIx64 "):\n",
           g_current_thread->GetName(), g_machine->pc);
    printf("\t*** Illegal instruction at virtual address 0x%x ***\n", vaddr);
    g_machine->interrupt->Halt(ERROR);
    break;

  case PAGEFAULT_EXCEPTION:
    ExceptionType e;
    e = g_page_fault_manager->PageFault(vaddr / g_cfg->PageSize);
    if (e != NO_EXCEPTION) {
      printf("\t*** Page fault handling failed, ... exiting\n");
      g_machine->interrupt->Halt(ERROR);
    }
    break;
  #ifdef ETUDIANTS_TP
  case SC_SEM_CREATE: {
    // Pour créer un sémaphore
    DEBUG('e', (char*)"Semaphore: Create call.\n");
    
    // 1. Récupérer les paramètres
    uint64_t addr = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1); // adresse du nom 
    int initval = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_2); // valeur initiale
    
    // 2. Récupérer le nom du sémaphore
    int size = GetLengthParam(addr);
    char name[size];
    GetStringParam(addr, name, size);

    // 3. Créer le sémaphore
    Semaphore *sem = new Semaphore(name, initval);
    
    // 4. Ajouter le sémaphore à la table des objets et récupérer son ID
    int id = g_object_addrs->AddObject(sem);
    
    // 5. Renvoyer l'ID ou ERROR
    if (id != ERROR) {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, id);
        g_syscall_error->SetMsg((char*)"", NO_ERROR);
    } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg((char*)"", OUT_OF_MEMORY); 
    }
    break;
}

case SC_SEM_DESTROY: {
    // Pour détruire un sémaphore
    DEBUG('e', (char*)"Semaphore: Destroy call.\n");
    
    // 1. Récupérer l'ID du sémaphore
    int id = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
    
    // 2. Récupérer le sémaphore depuis son ID
    Semaphore *sem = (Semaphore*)g_object_addrs->SearchObject(id);
    
    // 3. Vérifier que c'est bien un sémaphore valide
    if (sem && sem->type == SEMAPHORE_TYPE) {
        delete sem;
        g_object_addrs->RemoveObject(id);
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
        g_syscall_error->SetMsg((char*)"", NO_ERROR);
    } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR); 
        g_syscall_error->SetMsg((char*)"", INVALID_SEMAPHORE_ID);
    }
    break;
}

case SC_P: {
    // Pour faire un P() sur un sémaphore
    DEBUG('e', (char*)"Semaphore: P call.\n");
    
    // 1. Récupérer l'ID du sémaphore
    int id = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
    
    // 2. Récupérer le sémaphore 
    Semaphore *sem = (Semaphore*)g_object_addrs->SearchObject(id);
    
    // 3. Vérifier et faire le P()
    if (sem && sem->type == SEMAPHORE_TYPE) {
        sem->P();
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
        g_syscall_error->SetMsg((char*)"", NO_ERROR);
    } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg((char*)"", INVALID_SEMAPHORE_ID);
    }
    break;
}

case SC_V: {
    // Pour faire un V() sur un sémaphore  
    DEBUG('e', (char*)"Semaphore: V call.\n");
    
    // 1. Récupérer l'ID du sémaphore
    int id = g_machine->ReadIntRegister(REG_SYSCALL_PARAM_1);
    
    // 2. Récupérer le sémaphore
    Semaphore *sem = (Semaphore*)g_object_addrs->SearchObject(id);
    
    // 3. Vérifier et faire le V()
    if (sem && sem->type == SEMAPHORE_TYPE) {
        sem->V();
        g_machine->WriteIntRegister(REG_RET_SYSCALL, NO_ERROR);
        g_syscall_error->SetMsg((char*)"", NO_ERROR);
    } else {
        g_machine->WriteIntRegister(REG_RET_SYSCALL, ERROR);
        g_syscall_error->SetMsg((char*)"", INVALID_SEMAPHORE_ID);
    }
    break;
}
#endif ETUDIANTS_TP
  default:
    printf("Unknown exception %d\n", exceptiontype);
    g_machine->interrupt->Halt(ERROR);
    break;
  }
}

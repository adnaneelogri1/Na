/*! \file system.h
    \brief Global variables used in Nachos

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

#ifndef SYSTEM_H
#define SYSTEM_H

#include <map>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

#include "utility/list.h"
#include "utility/objaddr.h"

/*! Each syscall makes sure that the object that the user passes to it
 * are of the expected type, by checking the typeId field against
 * these identifiers
 */
typedef enum {
  SEMAPHORE_TYPE = 0xdeefeaea,
  LOCK_TYPE = 0xdeefcccc,
  CONDITION_TYPE = 0xdeefcdcd,
  FILE_TYPE = 0xdeadbeef,
  THREAD_TYPE = 0xbadcafe,
  INVALID_TYPE = 0xf0f0f0f
} ObjectType;

// Forward declarations (ie in other files)
class Config;
class Statistics;
class SyscallError;
class Thread;
class Scheduler;
class PageFaultManager;
class PhysicalMemManager;
class SwapManager;
class FileSystem;
class OpenFileTable;
class DriverDisk;
class DriverConsole;
class DriverACIA;
class Machine;

// Initialization and cleanup routines
extern void Initialize(int argc,
                       char **argv);   //!< Initialization,
                                       //!< called before anything else
extern void Cleanup();                 //!< Cleanup, called when
                                       //!< Nachos is done.
// Global variables per type
// By convention, all globals are in lower case and start by g_
// ------------------------------------------------------------

// Hardware components
extern Machine *g_machine;   //!< Machine (includes CPU and peripherals)

// Thread management
extern Thread *g_current_thread;           //!< The thread holding the CPU
extern Thread *g_thread_to_be_destroyed;   //!< The thread that just finished
extern ListThread *g_alive;                //!< List of existing threads
extern Scheduler *g_scheduler;             //!< Thread scheduler

// Device drivers
extern DriverDisk *g_disk_driver;         //!< Disk driver
extern DriverDisk *g_swap_disk_driver;    //!< Swap disk driver
extern DriverConsole *g_console_driver;   //!< Console driver
extern DriverACIA *g_acia_driver;         //!< Serial line driver

// Other Nachos components
extern FileSystem *g_file_system;          //!< File system
extern OpenFileTable *g_open_file_table;   //!< Open File Table
extern SwapManager *g_swap_manager;        //!< Management of swap area
extern PageFaultManager
    *g_page_fault_manager;   //!< Page fault handler (used in VMM)
extern PhysicalMemManager
    *g_physical_mem_manager;            //!< Physical memory manager
extern SyscallError *g_syscall_error;   //!< Error management
extern Config *g_cfg;                   //!< Configuration of Nachos
extern Statistics *g_stats;             //!< performance metrics
extern ObjAddr *g_object_addrs;         //!< addresses of kernel objets

// Endianess of data in ELF file and host endianess
//
// Nachos supports both little and big-mips compilers, variable
// mips_endianess is set when scanning the ELF header to detect the
// endianess.
// Variable host_endianess indicates the host endianess, detected
// automatically when starting the MIPS simulator
extern char risc_endianess;
extern char host_endianess;
#define IS_BIG_ENDIAN    0
#define IS_LITTLE_ENDIAN 1

// Name of files used to emulate the Nachos disk
#define DISK_FILE_NAME (char *) "DISK"
#define DISK_SWAP_NAME (char *) "SWAPDISK"

#endif   // SYSTEM_H

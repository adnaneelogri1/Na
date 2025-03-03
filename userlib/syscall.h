/*! \file syscall.h
    \brief Nachos system call interface.

   These are Nachos kernel operations
   that can be invoked from user programs, by trapping to the kernel
   via the "syscall" instruction.

   This file is included by user programs and by the Nachos kernel.

   Each of these is invoked by a user program by simply calling the
   procedure; an assembly language stub stuffs the system call code
   into a register, and traps to the kernel.  The kernel procedures
   are then invoked in the Nachos kernel, after appropriate error checking,
   from the system call entry point in exception.cc.

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

#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "kernel/copyright.h"

/* system call codes -- used by the stubs to tell the kernel which system call
 * is being asked for
 */
#define SC_HALT           0
#define SC_EXIT           1
#define SC_EXEC           2
#define SC_JOIN           3
#define SC_CREATE         4
#define SC_OPEN           5
#define SC_READ           6
#define SC_WRITE          7
#define SC_SEEK           8
#define SC_CLOSE          9
#define SC_NEW_THREAD     10
#define SC_YIELD          11
#define SC_PERROR         12
#define SC_P              13
#define SC_V              14
#define SC_SEM_CREATE     15
#define SC_SEM_DESTROY    16
#define SC_LOCK_CREATE    17
#define SC_LOCK_DESTROY   18
#define SC_LOCK_ACQUIRE   19
#define SC_LOCK_RELEASE   20
#define SC_COND_CREATE    21
#define SC_COND_DESTROY   22
#define SC_COND_WAIT      23
#define SC_COND_SIGNAL    24
#define SC_COND_BROADCAST 25
#define SC_TTY_SEND       26
#define SC_TTY_RECEIVE    27
#define SC_MKDIR          28
#define SC_RMDIR          29
#define SC_REMOVE         30
#define SC_FSLIST         31
#define SC_SYS_TIME       32
#define SC_MMAP           33
#define SC_DEBUG          34

#ifndef IN_ASM

/* The system call interface.  These are the operations the Nachos
 * kernel needs to support, to be able to run user programs.
 *
 */

typedef int t_error;

/* Stop Nachos, and print out performance stats */
void Halt();

/* Return the time spent running Nachos */

/*! \brief Defines the Nachos basic time unit */
typedef struct {
  long seconds;
  long nanos;
} Nachos_Time;
void SysTime(Nachos_Time *t);

/* Address space control operations: Exit, Exec, and Join */

/* This user program is done (status = 0 means exited normally). */
void Exit(int status);

/* A unique identifier for a thread executed within a user program */
/* Here, type "long" is used, so far unable to mix stdint and asm code */
typedef long ThreadId;

/* Run the executable, stored in the Nachos file "name", and return the
 * master thread identifier
 */
ThreadId Exec(char *name);

/* Create a new thread in the current process
 * Return thread identifier
 */
ThreadId newThread(char *debug_name, int func, int arg);

/* Only return once the the thread "id" has finished.
 */
t_error Join(ThreadId id);

/* Yield the CPU to another runnable thread, whether in this address space
 * or not.
 */
void Yield();

/*! Print the last error message with the personalized one "mess" */
void PError(char *mess);

/* File system operations: Create, Open, Read, Write, Seek, Close
 * These functions are patterned after UNIX -- files represent
 * both files *and* hardware I/O devices.
 *
 * If this assignment is done before doing the file system assignment,
 * note that the Nachos file system has a stub implementation, which
 * will work for the purposes of testing out these routines.
 */

/* A unique identifier for an open Nachos file. */
typedef unsigned long OpenFileId;

/* when an address space starts up, it has two open files, representing
 * keyboard input and display output (in UNIX terms, stdin and stdout).
 * Read and Write can be used directly on these, without first opening
 * the console device.
 */
#define CONSOLE_INPUT  0
#define CONSOLE_OUTPUT 1

/* Create a Nachos file, with "name" */
t_error Create(char *name, int size);

/* Open the Nachos file "name", and return an "OpenFileId" that can
 * be used to read and write to the file.
 */
OpenFileId Open(char *name);

/* Write "size" bytes from "buffer" to the open file. */
t_error Write(char *buffer, int size, OpenFileId id);

/* Read "size" bytes from the open file into "buffer".
 * Return the number of bytes actually read -- if the open file isn't
 * long enough, or if it is an I/O device, and there aren't enough
 * characters to read, return whatever is available (for I/O devices,
 * you should always wait until you can return at least one character).
 */
t_error Read(char *buffer, int size, OpenFileId id);

/* Seek to a specified offset into an opened file */
t_error Seek(int offset, OpenFileId id);

#ifndef SYSDEP_H
/* Close the file, we're done reading and writing to it. */
t_error Close(OpenFileId id);
#endif   // SYSDEP_H

/* Remove the file */
t_error Remove(char *name);

/******************************************************************/
/* system calls concerning directory management */

/* Create a new repertory
   Return a negative number if an error ocurred.
*/
t_error Mkdir(char *name);

/* Destroy a repertory, which must be empty.
   Return a negative number if an error ocurred.
*/
t_error Rmdir(char *name);

/* List the content of NachOS FileSystem */
t_error FSList();

/******************************************************************/
/* User-level synchronization operations :  */

/* System calls concerning semaphores management */

typedef unsigned long SemId;

/* Create a semaphore, initialising it at count.
   Return a Semid, which will enable to do operations on this
   semaphore                                                      */
SemId SemCreate(char *debug_name, int count);

/* Destroy a semaphore identified by sema.
   Return a negative number if an error occured during the destruction */
t_error SemDestroy(SemId sema);

/* Do the operation P() on the semaphore sema */
t_error P(SemId sema);

/* Do the operation V() on the semaphore sema */
t_error V(SemId sema);

/* System calls concerning locks management */
typedef unsigned long LockId;

/* Create a lock.
 Return an identifier */
LockId LockCreate(char *debug_name);

/* Destroy a lock.
   Return a negative number if an error ocurred
   during the destruction. */
t_error LockDestroy(LockId id);

/* Do the operation Acquire on the lock id.
   Return a negative number if an error ocurred. */
t_error LockAcquire(LockId id);

/*  Do the operation Release  on the lock id.
   Return a negative number if an error ocurred.
*/
t_error LockRelease(LockId id);

/* System calls concerning conditions variables. */
typedef unsigned long CondId;

/* Create a new condition variable */
CondId CondCreate(char *debug_name);

/* Destroy a condition variable.
   Return a negative number if an error ocurred.
*/
t_error CondDestroy(CondId id);

/* Do the operation Wait on a condition variable.
   Returns a negative number if an error ocurred.
*/
t_error CondWait(CondId cond);

/* Do the operation Signal on a condition variable (wake up only one thread).
   Return a negative number if an error ocurred.
*/
t_error CondSignal(CondId cond);

/* Do the operation Signal on a condition variable (wake up all threads).
   Return a negative number if an error ocurred.
*/
t_error CondBroadcast(CondId cond);

/******************************************************************/
/* System calls concerning serial port and console */

/* Send the message on the serial communication link.
   Returns the number of bytes successfully sent.
*/
int TtySend(char *mess);

/* Wait for a message comming from the serial communication link.
   The length of the buffer where the bytes will be copied is given as a
   parameter. Returns the number of characters actually received.
*/
int TtyReceive(char *mess, int length);

/* Map an opened file in memory. Size is the size to be mapped in bytes.
 */
void *Mmap(OpenFileId f, int size);

/* For debug purpose
 */
void Debug(int param);

#endif   // IN_ASM
#endif   // SYSCALL_H

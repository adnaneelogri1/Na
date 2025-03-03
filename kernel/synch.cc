/*! \file synch.cc
//  \brief Routines for synchronizing threads.
//
//      Three kinds of synchronization routines are defined here:
//      semaphores, locks and condition variables.
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation. We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts. While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
 * -----------------------------------------------------
 * This file is part of the Nachos-RiscV distribution
 * Copyright (c) 2022 University of Rennes.
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

#include "kernel/synch.h"
#include "kernel/msgerror.h"
#include "kernel/scheduler.h"
#include "kernel/system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
/*! 	Initializes a semaphore, so that it can be used for synchronization.
//
// \param debugName is an arbitrary name, useful for debugging only.
// \param initialValue is the initial value of the semaphore.
*/
//----------------------------------------------------------------------
Semaphore::Semaphore(char *debugName, uint32_t initialCount) {
  semaphore_name = new char[strlen(debugName) + 1];
  strcpy(semaphore_name, debugName);
  count = initialCount;
  wait_queue = new ListThread;
  type = SEMAPHORE_TYPE;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
/*! 	De-allocates a semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
*/
//----------------------------------------------------------------------
Semaphore::~Semaphore() {
  type = INVALID_TYPE;
  if (!wait_queue->IsEmpty()) {
    DEBUG('s',
          (char *) "Destructor of semaphore \"%s\", queue is not empty!!\n",
          semaphore_name);
    Thread *t = (Thread *) wait_queue->Remove();
    DEBUG('s', (char *) "Queue contents %s\n", t->GetName());
    wait_queue->Append((void *) t);
  }
  ASSERT(wait_queue->IsEmpty());
  delete[] semaphore_name;
  delete wait_queue;
}

//----------------------------------------------------------------------
// Semaphore::P
/*!
// Implementation of P (counter decrementation+blocking if required).
// Checking the
// value and decrementing must be done atomically, so we
// need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
*/
//----------------------------------------------------------------------
void
Semaphore::P() {
  
  printf("**** Warning: method Semaphore::P is not implemented yet\n");
    Interrupt *interrupt = g_machine->interrupt;
    Thread *currentThread = g_current_thread;
    IntStatus oldLevel = interrupt->SetStatus(INTERRUPTS_OFF); // disable interrupts
    while(count == 0) { // semaphore not available
        wait_queue->Append((void *)currentThread); // so go to sleep
        currentThread->Sleep();
    }
    count--; // semaphore available, consume its value
    (void) interrupt->SetStatus(oldLevel); // re-enable interrupts
    // disable interrupts
  exit(ERROR);
}

//----------------------------------------------------------------------
// Semaphore::V
/*! 	Implementation of V (counter incrementation + waking up a waiting thread
//  if required).
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that interrupts
//	are disabled when it is called.
*/
//----------------------------------------------------------------------
void
Semaphore::V() {
  printf("**** Warning: method Semaphore::V is not implemented yet\n");
    Interrupt *interrupt = g_machine->interrupt;
    IntStatus oldst= interrupt->SetStatus(INTERRUPTS_OFF); // disable interrupts
    Thread *thread = (Thread *)wait_queue->Remove();
    if (thread != NULL) // make thread ready
        g_scheduler->ReadyToRun(thread);
    count++;
    (void) interrupt->SetStatus(oldst); // re-enable interrupts
  exit(ERROR);
}

//----------------------------------------------------------------------
// Lock::Lock
/*! 	Initialize a Lock, so that it can be used for synchronization.
//      The lock is initialy free
//  \param "debugName" is an arbitrary name, useful for debugging.
*/
//----------------------------------------------------------------------

Lock::Lock(char *debugName) {
  lock_name = new char[strlen(debugName) + 1];
  strcpy(lock_name, debugName);
  wait_queue = new ListThread;
  is_free = true;
  owner = NULL;
  type = LOCK_TYPE;
}

//----------------------------------------------------------------------
// Lock::~Lock
/*! 	De-allocate lock, when no longer needed. Assumes that no thread
//      is waiting on the lock.
*/
//----------------------------------------------------------------------
Lock::~Lock() {
  type = INVALID_TYPE;
  ASSERT(wait_queue->IsEmpty());
  delete[] lock_name;
  delete wait_queue;
}

//----------------------------------------------------------------------
// Lock::Acquire
/*! 	Wait until the lock become free.  Checking the
//	state of the lock (free or busy) and modify it must be done
//	atomically, so we need to disable interrupts before checking
//	the value of is_free.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
*/
//----------------------------------------------------------------------
void
Lock::Acquire() {
  printf("**** Warning: method Lock::Acquire is not implemented yet\n");
  Interrupt *interrupt = g_machine->interrupt;
  Thread *currentThread = g_current_thread;
  IntStatus oldLevel = interrupt->SetStatus(INTERRUPTS_OFF); // disable interrupts

  while (!is_free) { // lock is not free
    wait_queue->Append((void *)currentThread); // so go to sleep
    currentThread->Sleep();
  }

  is_free = false; // lock is now acquired
  owner = currentThread; // current thread is the owner of the lock
  (void) interrupt->SetStatus(oldLevel); // re-enable interrupts
  exit(ERROR);
}

//----------------------------------------------------------------------
// Lock::Release
/*! 	Wake up a waiter if necessary, or release it if no thread is waiting.
//      We check that the lock is held by the g_current_thread.
//	As with Acquire, this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
*/
//----------------------------------------------------------------------
#ifdef ETUDIANTS_TP
void
Lock::Release() {
  printf("**** Warning: method Lock::Release is not implemented yet\n");
  Interrupt *interrupt = g_machine->interrupt;
  IntStatus oldLevel = interrupt->SetStatus(INTERRUPTS_OFF); // disable interrupts

  ASSERT(isHeldByCurrentThread()); // check if the current thread holds the lock

  if (!wait_queue->IsEmpty()) {
    Thread *thread = (Thread *)wait_queue->Remove();
    g_scheduler->ReadyToRun(thread); // make thread ready
  } else {
    is_free = true; // no thread is waiting, release the lock
    owner = NULL; // no owner since the lock is free
  }

  (void) interrupt->SetStatus(oldLevel); // re-enable interrupts
  exit(ERROR);
}
#endif ETUDIANTS_TP

//----------------------------------------------------------------------
// Lock::isHeldByCurrentThread
/*! To check if current thread hold the lock
 */
//----------------------------------------------------------------------
bool
Lock::isHeldByCurrentThread() {
  return (g_current_thread == owner);
}

//----------------------------------------------------------------------
// Condition::Condition
/*! 	Initializes a Condition, so that it can be used for synchronization.
//
//    \param  "debugName" is an arbitrary name, useful for debugging.
*/
//----------------------------------------------------------------------
Condition::Condition(char *debugName) {
  condition_name = new char[strlen(debugName) + 1];
  strcpy(condition_name, debugName);
  wait_queue = new ListThread;
  type = CONDITION_TYPE;
}

//----------------------------------------------------------------------
// Condition::~Condition
/*! 	De-allocate condition, when no longer needed.
//      Assumes that nobody is waiting on the condition.
*/
//----------------------------------------------------------------------
Condition::~Condition() {
  type = INVALID_TYPE;
  ASSERT(wait_queue->IsEmpty());
  delete[] condition_name;
  delete wait_queue;
}

//----------------------------------------------------------------------
// Condition::Wait
/*! Block the calling thread (put it in the wait queue).
//  This operation must be atomic, so we need to disable interrupts.
*/
//----------------------------------------------------------------------
void
Condition::Wait() {
  printf("**** Warning: method Condition::Wait is not implemented yet\n");
  exit(ERROR);
}

//----------------------------------------------------------------------
// Condition::Signal
/*! Wake up the first thread of the wait queue (if any).
// This operation must be atomic, so we need to disable interrupts.
*/
//----------------------------------------------------------------------
void
Condition::Signal() {
  printf("**** Warning: method Condition::Signal is not implemented yet\n");
  exit(ERROR);
}

//----------------------------------------------------------------------
// Condition::Broadcast
/*! Wake up all threads waiting in the waitqueue of the condition
// This operation must be atomic, so we need to disable interrupts.
*/
//----------------------------------------------------------------------
void
Condition::Broadcast() {
  printf("**** Warning: method Condition::Broadcast is not implemented yet\n");
  exit(ERROR);
}

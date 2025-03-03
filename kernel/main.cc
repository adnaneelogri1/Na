/*! \file  main.cc
//  \brief Bootstrap code to initialize the operating system kernel.
//
// Usage: nachos -d <debugflags>
//		-s -x <nachos file>
//              -z -f <configfile>
//
//    -d causes certain debugging messages to be printed (cf. utility.h)
//    -s causes user programs to be executed in single-step mode
//    -z prints the copyright message
//    -f <configfile> gives the name of a configuration file for Nachos
//    -x runs a user program
//
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

#define MAIN
#include "kernel/copyright.h"
#undef MAIN

#include "kernel/msgerror.h"
#include "kernel/system.h"
#include "kernel/thread.h"
#include "utility/config.h"
#include "utility/utility.h"

// External functions used by this file
extern void Copy(char *unixFile, char *nachosFile);
extern void Print(char *file);
extern void StartProcess(char *file);

//----------------------------------------------------------------------
// main
/*! 	Bootstrap the operating system kernel.
//
//	Check command line arguments
//	Initialize data structures
//	(optionally) Call test procedure
//
//	\param argc is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3
//	\param argv is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
*/
//----------------------------------------------------------------------

int
main(int argc, char **argv) {
  int argCount;   // Number of arguments for a particular command
  uint64_t err;   // Error code

  // Init Nachos data structures
  Initialize(argc, argv);
  char *startfilename = g_cfg->ProgramToRun;

  // Process command line arguments
  for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
    argCount = 1;

    if (!strcmp(*argv, "-z")) {   // print copyright
      printf("%s", copyright);
      exit(0);
    }

    if (!strcmp(*argv, "-h")) {   // give commande line arguments
      printf("Usage: %s [opts]\n", argv[0]);
      printf("   -d <debugflags> : turn on debug flags specified in "
             "<debugflags>\n");
      printf("   -s              : single step mode\n");
      printf("   -x <elf_file>   : execute MIPS binary file <elf_file>\n");
      printf("   -z              : print copyright information\n");
      printf("   -f <cfg_file>   : use <cfg_file> instead of default "
             "configuration file\n");
      printf("   -h              : list command line arguments\n");
      exit(0);
    }
    if (!strcmp(*argv, "-x")) {   // run a user program
      ASSERT(argc > 1);
      argCount = 2;
      startfilename = argv[1];
    }
  }

  if (g_cfg->Remove) {   // remove Nachos file
    g_file_system->Remove(g_cfg->FileToRemove);
  }
  if (g_cfg->MakeDir) {   // Make Nachos directory
    g_file_system->Mkdir(g_cfg->DirToMake);
  }
  if (g_cfg->RemoveDir) {   // Remove Nachos file
    g_file_system->Rmdir(g_cfg->DirToRemove);
  }
  if (g_cfg->NbCopy != 0) {   // copy from UNIX to Nachos

    for (uint32_t i = 0; i < g_cfg->NbCopy; i++) {
      if ((strlen(g_cfg->ToCopyUnix[i]) != 0) &&
          (strlen(g_cfg->ToCopyNachos[i]) != 0))
        Copy(g_cfg->ToCopyUnix[i], g_cfg->ToCopyNachos[i]);
    }
  }
  if (g_cfg->Print) {   // print a Nachos file
    Print(g_cfg->FileToPrint);
  }
  if (g_cfg->ListDir) {   // list Nachos directory
    g_file_system->List();
  }
  if (g_cfg->PrintFileSyst) {   // print entire filesystem
    g_file_system->Print();
  }

  if (!strcmp(startfilename, "")) {
    printf("Warning: No program to start\n");
  } else {
    Process *p = new Process(startfilename, &err);
    if (err != NO_ERROR) {
      fprintf(stderr, g_syscall_error->GetFormat(err), startfilename);
      exit(ERROR);
    }
    g_machine->mmu->translationTable = p->addrspace->translationTable;
    Thread *t = new Thread(startfilename);
    g_object_addrs->AddObject(t);
    err = t->Start(p, p->addrspace->getCodeStartAddress64(), -1);
    if (err != NO_ERROR) {
      fprintf(stderr, "Unable to start initial process: %s\n", startfilename);
      exit(ERROR);
    }
  }

  g_current_thread->Finish();   // NOTE: if the procedure "main"
                                // returns, then the program "nachos"
                                // will exit (as any other normal program
                                // would).  But there may be other
                                // threads on the ready list.  We switch
                                // to those threads by saying that the
                                // "main" thread is finished, preventing
                                // it from returning.
  return (0);                   // Not reached...
}

/*
 * main.c
 */

#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "types.h"
#include "absyn.h"
#include "errormsg.h"
#include "temp.h" /* needed by translate.h */
#include "tree.h" /* needed by frame.h */
#include "assem.h"
#include "frame.h" /* needed by translate.h and printfrags prototype */
#include "translate.h"
#include "env.h"
#include "semant.h" /* function prototype for transProg */
#include "canon.h"
#include "prabsyn.h"
#include "printtree.h"
#include "escape.h"
#include "parse.h"
#include "codegen.h"
#include "regalloc.h"
#include <string.h>

extern bool anyErrors;

/*Lab6: complete the function doProc
 * 1. initialize the F_tempMap
 * 2. initialize the register lists (for register allocation)
 * 3. do register allocation
 * 4. output (print) the assembly code of each function
 
 * Uncommenting the following printf can help you debugging.*/

/* print the assembly language instructions to filename.s */
static void doProc(FILE *out, F_frame frame, T_stm body)
{
  assert(frame);
  assert(body);
  AS_proc proc;
  struct RA_result allocation;
  T_stmList stmList;
  AS_instrList iList;
  struct C_block blo;

  //  printf("doProc for function %s:\n", S_name(F_name(frame)));
  //  printStmList(stdout, T_StmList(body, NULL));
  //  printf("-------====IR tree=====-----\n");

  stmList = C_linearize(body);
  //  printStmList(stdout, stmList);
  //  printf("-------====Linearlized=====-----\n");

  blo = C_basicBlocks(stmList);
  C_stmListList stmLists = blo.stmLists;
  //  for (; stmLists; stmLists = stmLists->tail) {
  //  	printStmList(stdout, stmLists->head);
  // 	printf("------====Basic block=====-------\n");
  //  }

  stmList = C_traceSchedule(blo);
  //  printStmList(stdout, stmList);
  printf("-------====trace=====-----\n");
  iList = F_codegen(frame, stmList); /* 9 */

  //  AS_printInstrList(stdout, iList, Temp_layerMap(F_tempMap, Temp_name()));
  printf("----======before RA=======-----\n");

  //G_graph fg = FG_AssemFlowGraph(iList);  /* 10.1 */
  struct RA_result ra = RA_regAlloc(frame, iList); /* 11 */

  printf("----======After RA=======-----\n");
  //  AS_printInstrList (out, proc->body,
  //  Temp_layerMap(F_tempMap, ra.coloring));

  //Part of TA's implementation. Just for reference.

  //  AS_rewrite(ra.il, Temp_layerMap(F_tempMap, ra.coloring));
 	ra.il = AS_deleteRepeat(ra.il, ra.coloring);
  proc = F_procEntryExit3(frame, ra.il);

  string procName = S_name(F_name(frame));
  fprintf(out, ".text\n");
  fprintf(out, ".set %s_framesize, %d\n", Temp_labelstring(frame->label), frame->local * 8);
  fprintf(out, ".globl %s\n", procName);
  fprintf(out, ".type %s, @function\n", procName);
  fprintf(out, "%s:\n", procName);

  //prologue
  fprintf(out, "%s", proc->prolog);
  AS_printInstrList(out, proc->body,
                    Temp_layerMap(F_tempMap, ra.coloring));
  fprintf(out, "%s", proc->epilog);
}

void doStr(FILE *out, Temp_label label, string str)
{
  fprintf(out, ".section .rodata\n");
  fprintf(out, "%s:\n", S_name(label));
  fprintf(out, ".int %d\n", (int)strlen(str));
  fprintf(out, ".string \"");

  for (; *str != 0; str++)
  {
    if (*str == '\n')
    {
      fprintf(out, "\\n");
    }
    else if (*str == '\t')
    {
      fprintf(out, "\\t");
    }
    else
    {
      fprintf(out, "%c", *str);
    }
  }
  fprintf(out, "\"\n");
}

int main(int argc, string *argv)
{
  A_exp absyn_root;
  S_table base_env, base_tenv;
  F_fragList frags;
  char outfile[100];
  FILE *out = stdout;

  if (argc == 2)
  {
    absyn_root = parse(argv[1]);
    if (!absyn_root)
      return 1;

#if 0
   pr_exp(out, absyn_root, 0); /* print absyn data structure */
   fprintf(out, "\n");
#endif

    //Lab 6: escape analysis
    //If you have implemented escape analysis, uncomment this
    printf("-------====escape=====-----\n");
    Esc_findEscape(absyn_root); /* set varDec's escape field */
    F_tempMap = Temp_empty();
    F_initTempMap();

    printf("-------====SEM_transProg=====-----\n");
    frags = SEM_transProg(absyn_root);
    if (anyErrors)
      return 1; /* don't continue */
    /* convert the filename */
    printf("-------====do_Proc=====-----\n");
    sprintf(outfile, "%s.s", argv[1]);
    out = fopen(outfile, "w");
    /* Chapter 8, 9, 10, 11 & 12 */
    for (; frags; frags = frags->tail)
    {
      fprintf(stderr, "-------====frags=====-----\n");
      if (frags->head->kind == F_procFrag)
      {
        fprintf(stderr, "-------====do_frags=====-----\n");
        doProc(out, frags->head->u.proc.frame, frags->head->u.proc.body);
      }
      else if (frags->head->kind == F_stringFrag)
      {
        fprintf(stderr, "-------====do_str=====-----\n");
        doStr(out, frags->head->u.stringg.label, frags->head->u.stringg.str);
      }
      else
      {
        fprintf(stderr, "-------====nothing=====-----\n");
      }
    }

    fclose(out);
    return 0;
  }
  EM_error(0, "usage: tiger file.tig");
  return 1;
}

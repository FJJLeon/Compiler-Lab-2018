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

#define MAINdebug 0

#define DEBUG(format, ...) do { \
    if (MAINdebug)  \
        fprintf(stdout, "[%s@%s,%d]\t" format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

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
    if (!frame) {
        DEBUG("sb frame null");
    }
    if (!body) {
        DEBUG("sb body null");
    }
    AS_proc proc;
    struct RA_result allocation;
    T_stmList stmList;
    AS_instrList iList;
    struct C_block blo;

    Temp_map F_tempMap = Temp_empty();
DEBUG("---===print info===---");
    DEBUG("doProc for function %s:\n", S_name(F_name(frame)));
    printStmList(stdout, T_StmList(body, NULL));

DEBUG("-------====IR tree=====-----\n");

    stmList = C_linearize(body);
    printStmList(stdout, stmList);
DEBUG("-------====Linearlized=====-----\n");

    blo = C_basicBlocks(stmList);
    C_stmListList stmLists = blo.stmLists;
    for (; stmLists; stmLists = stmLists->tail) {
        printStmList(stdout, stmLists->head);
        DEBUG("------====Basic block=====-------\n");
    }

    stmList = C_traceSchedule(blo);
    printStmList(stdout, stmList);
DEBUG("-------====trace=====-----\n");
    iList  = F_codegen(frame, stmList); /* 9 */
/*
AS_instrList t = iList;
DEBUG("??Dsa");
  for (;t;t=t->tail) {
    if (t->head->kind == 0)
      printf("%s\n", t->head->u.OPER.assem);
    else if (t->head->kind == 1)
      printf("%s\n", t->head->u.MOVE.assem);
    else
      printf("%s\n", t->head->u.LABEL.assem);
  }
*/
DEBUG("---=== PLAIN OUT===---");
    AS_printInstrList(stdout, iList, Temp_layerMap(F_tempMap, Temp_name()));
    printf("----======before RA=======-----\n");

    ////G_graph fg = FG_AssemFlowGraph(iList);  /* 10.1 */
    struct RA_result ra = RA_regAlloc(frame, iList);  /* 11 */
DEBUG("---===after RA===---");
/*
assert(proc->body);
  AS_instrList t = proc->body;
DEBUG("??Dsa");
  for (;t;t=t->tail) {
      DEBUG("?sdasd");
    if (t->head->kind == 0)
      printf("%s\n", t->head->u.OPER.assem);
    else if (t->head->kind == 1)
      printf("%s\n", t->head->u.MOVE.assem);
    else
      printf("%s\n", t->head->u.LABEL.assem);
  }
DEBUG("xxx");
*/
    //Part of TA's implementation. Just for reference.
    
    AS_rewrite(ra.il, Temp_layerMap(F_tempMap, ra.coloring));
    proc =	F_procEntryExit3(frame, ra.il);

    string procName = S_name(F_name(frame));
    fprintf(out, ".text\n");
    fprintf(out, ".set %sframesize, %d\n", Temp_labelstring(frame->name), frame->local_cnt * F_wordSize);
    fprintf(out, ".globl %s\n", procName);
    fprintf(out, ".type %s, @function\n", procName);
    fprintf(out, "%s:\n", procName);
    
    //fprintf(stdout, "%s:\n", Temp_labelstring(F_name(frame)));
    //prologue
    //fprintf(out, "BEGIN function\n");
    fprintf(out, "%s", proc->prolog);
    AS_printInstrList(out, proc->body, Temp_layerMap(F_tempMap, ra.coloring));
    fprintf(out, "%s", proc->epilog);
    //fprintf(out, "END function\n");
    //fprintf(out, "END %s\n\n", Temp_labelstring(F_name(frame)));
    
}

void doStr(FILE *out, Temp_label label, string str) {
    DEBUG("string : %d", str[0]);
	fprintf(out, ".section .rodata\n");
	fprintf(out, "%s:\n", S_name(label));  
    //!!!!!!!!WHY HERE a . ????
    //it may contains zeros in the middle of string. To keep this work, we need to print all the charactors instead of using fprintf(str)

    fprintf(out, ".int %lu\n", strlen(str));
    fprintf(out, ".string \"");
    char c = *str;
    while(c) {
        if (c == '\n') 
            fprintf(out, "\\n");
        else if(c == '\t') 
            fprintf(out, "\\t");
        else 
            fprintf(out, "%c", c);
        str++;
        c = *str;
    }
    fprintf(out, "\"\n");
	//fprintf(out, ".string \"%s\"\n", str);
}

int main(int argc, string *argv)
{
    A_exp absyn_root;
    S_table base_env, base_tenv;
    F_fragList frags;
    char outfile[100];
    FILE *out = stdout;

    if (argc==2) {
        absyn_root = parse(argv[1]);
        if (!absyn_root)
            return 1;
     
#if 1
    pr_exp(out, absyn_root, 0); /* print absyn data structure */
    fprintf(out, "\n");
#endif

        //Lab 6: escape analysis
        //If you have implemented escape analysis, uncomment this
        Esc_findEscape(absyn_root); /* set varDec's escape field */
        DEBUG("---===before SEM_transProg===---");
        frags = SEM_transProg(absyn_root);
        if (anyErrors) return 1; /* don't continue */
        DEBUG("---===after SEM_transProg===---");
        /* convert the filename */
        sprintf(outfile, "%s.s", argv[1]);
        DEBUG("out file %s", outfile);
        out = fopen(outfile, "w");

        int count = 0;
        for (F_fragList tmp = frags; tmp; tmp=tmp->tail) {
            count++;
        }
        DEBUG("!!!! THERE are %d fragments", count);

        /* Chapter 8, 9, 10, 11 & 12 */
        int i=0;
        for (;frags;frags=frags->tail) {
            DEBUG("---===fragment %d ===---", ++i);
            if (frags->head->kind == F_procFrag) {
                DEBUG("doProc frag ProcLabel %s", Temp_labelstring(frags->head->u.proc.frame->name));
                doProc(out, frags->head->u.proc.frame, frags->head->u.proc.body);
            }
            else if (frags->head->kind == F_stringFrag) {
                DEBUG("doStr ProcLabel %s frag %s", 
                    Temp_labelstring(frags->head->u.stringg.label), frags->head->u.stringg.str);
                doStr(out, frags->head->u.stringg.label, frags->head->u.stringg.str);
            }
        }

        fclose(out);
        return 0;
    }
    EM_error(0,"usage: tiger file.tig");
    return 1;
}

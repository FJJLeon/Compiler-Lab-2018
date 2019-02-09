#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"


#define RAdebug 1

#define DEBUG(format, ...) do { \
    if (RAdebug)  \
        fprintf(stdout, "[%s@%s,%d] " format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

#define MAXLINE 100

static int rewriteNum;
static char* rewriteInst[1000];
static int rewriteOffset[1000];

static bool inTL(Temp_tempList list, Temp_temp temp) {
    for (; list; list = list->tail)
        if (list->head == temp)
            return 1;
    return 0;
}

static void replaceTemp(Temp_tempList list, Temp_temp old, Temp_temp new) {
    for (; list; list = list->tail)
        if (list->head == old)
            list->head = new;
}

AS_instrList rewriteProgram(F_frame f, AS_instrList il, Temp_tempList spills) {
    for (; spills; spills = spills->tail) {
        Temp_temp spill = spills->head;
		// framesize ++
        f->local_cnt++;
        AS_instrList instrs = il;
        for (; instrs; instrs = instrs->tail) {
            AS_instr instr = instrs->head;

            if (instr->kind == I_OPER || instr->kind == I_MOVE) {
                if (inTL(instr->u.OPER.dst, spill)) { 
					// assign new temp replace the spill 
                    Temp_temp temp = Temp_newtemp();
                    replaceTemp(instr->u.OPER.dst, spill, temp); 
					// store it after def
                    char *inst = checked_malloc(MAXLINE*(sizeof(char)));
					rewriteInst[rewriteNum] = inst;
					rewriteOffset[rewriteNum++] = f->local_cnt;
					sprintf(inst, "movq `s0, %s(%%rsp)", Temp_labelstring(f->name));
					AS_instr store = AS_Oper(inst, NULL, Temp_TempList(temp, NULL), NULL);
                    instrs->tail = AS_InstrList(store, instrs->tail);
                } else if (inTL(instr->u.OPER.src, spill)) {
					// assign new temp replace the spill 
                    Temp_temp temp = Temp_newtemp();
                    replaceTemp(instr->u.OPER.src, spill, temp);  
					// load it before use
                    char *inst = checked_malloc(MAXLINE*(sizeof(char)));
					rewriteInst[rewriteNum] = inst;
					rewriteOffset[rewriteNum++] = - f->local_cnt;
					sprintf(inst, "movq %s(%%rsp), `d0", Temp_labelstring(f->name));
					AS_instr load = AS_Oper(inst, Temp_TempList(temp, NULL), NULL, NULL);
                    instrs->head = load;
                    instrs->tail = AS_InstrList(instr, instrs->tail);
                }
            }

        }
    }

    return il;
}

struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
	//your code here
	DEBUG("--=== in RA ===---");
	Temp_map F_tempMap;
	G_graph flow_graph;
	struct Live_graph live_graph;
	struct COL_result color;
	rewriteNum = 0;
	while(1){
		flow_graph = FG_AssemFlowGraph(il, f);
		live_graph = Live_liveness(flow_graph);
		color = COL_color(live_graph.graph, F_tempMap, NULL, live_graph.moves);
		if (color.spills == NULL)
			break;

		DEBUG("rewrite Program");
		il = rewriteProgram(f, il, color.spills);
	}

	int framesize = f->local_cnt * F_wordSize;
	for (int i=0; i<rewriteNum; i++){
		if (rewriteOffset[i] > 0)
			sprintf(rewriteInst[i], "movq `s0, %d(%%rsp)", (- (rewriteOffset[i]) * F_wordSize) + framesize);
		else
			sprintf(rewriteInst[i], "movq %d(%%rsp), `d0", ((rewriteOffset[i]) * F_wordSize) + framesize);
	}

	struct RA_result ret;
	ret.coloring = color.coloring;
	ret.il = il;
	return ret;
}

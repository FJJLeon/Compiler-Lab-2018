#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

#define FGdebug 1

#define DEBUG(format, ...) do { \
    if (FGdebug)  \
        fprintf(stdout, "[%s@%s,%d]\t" format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)


Temp_tempList FG_def(G_node n) {
	//your code here.
	AS_instr inst = G_nodeInfo(n);
	switch (inst->kind){
		case I_OPER:
			return inst->u.OPER.dst;
		case I_MOVE:
			return inst->u.MOVE.dst;
		case I_LABEL:
			return NULL;
		default:
			assert(0);
	}
}

Temp_tempList FG_use(G_node n) {
	// NOTE: for two-address inst, both register is used
	//your code here.
	AS_instr inst = G_nodeInfo(n);
	switch (inst->kind){
		case I_OPER:
			return inst->u.OPER.src;
		case I_MOVE:
			return inst->u.MOVE.src;
		case I_LABEL:
			return NULL;
		default:
			assert(0);
	}
}

bool FG_isMove(G_node n) {
	//your code here.
	AS_instr inst = G_nodeInfo(n);
	if (inst->kind == I_MOVE)
		return TRUE;
	else 
		return FALSE;
}

/*
 * make a Directed Graph 
 * whose edge points to inst from its prev
 * or points to label from jmp.
 *     info of G_node = AS_inst
 * */
G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f) {
	//your code here.
	DEBUG("---===FlowGraph constructing===---");
	G_graph g = G_Graph();
	G_node prev = NULL;
	AS_instrList tmp = il;
	// table mapping label inst to node
	LB_table labelMap = LB_empty();

	for (; tmp; tmp=tmp->tail) {
		AS_instr inst = tmp->head;
		G_node n = G_Node(g, inst);
		if (prev) {
			G_addEdge(prev, n);
		}
		// jmp not direct succ to next
		if (inst->kind == I_OPER && strncmp("jmp", inst->u.OPER.assem, 3)==0 ) {
			prev = NULL;
		}
		else {
			prev = n;
		}
		// record label for jmp
		if (inst->kind == I_LABEL) {
			LB_enter(labelMap, inst->u.LABEL.label, n);
		}
	}
	// check each jmp, addEdge between jmp and target jumps
	G_nodeList nodes = G_nodes(g);
	for (; nodes; nodes = nodes->tail) {
		G_node n = nodes->head;
		AS_instr inst = G_nodeInfo(n);
		if (inst->kind == I_OPER && inst->u.OPER.jumps) {
			Temp_labelList targets = inst->u.OPER.jumps->labels;
			for (; targets; targets = targets->tail) {
				Temp_label tar = targets->head;
				G_node tarNode = (G_node) LB_look(labelMap, tar);
				if (tarNode){
					G_addEdge(n, tarNode);
				}
				else 
					DEBUG("target label exist but node not");
			}
		}
	}
	DEBUG("---===FlowGraph construct ok");
	return g;
}

/* Label table functions */
LB_table LB_empty(void) {
	return TAB_empty();
}

void LB_enter(LB_table t, Temp_label label, G_node node) {
	TAB_enter(t, label, node);
}

void *LB_look(LB_table t, Temp_label label) {
	return TAB_look(t, label);
}
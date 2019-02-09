#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

#define LVdebug 1

#define DEBUG(format, ...) do { \
    if (LVdebug)  \
        fprintf(stdout, "[%s@%s,%d]\t" format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

//helper functions
static Temp_tempList minusTL(Temp_tempList a, Temp_tempList b);
static Temp_tempList unionTL(Temp_tempList a, Temp_tempList b);
static bool isEqual(Temp_tempList a, Temp_tempList b);
static bool inTL(Temp_tempList tl, Temp_temp t);
static bool inMoveList(Live_moveList ml, G_node src, G_node dst);

static Temp_tempList L(Temp_temp h, Temp_tempList t) {
    return Temp_TempList(h, t);
}

/*the type of "tables" mapping Temp_temp to the node refer to temp*/
typedef struct TAB_table_ *TP_table;
TP_table TP_empty(void);
/* Enter the mapping "temp"->"node" to the table "t" */
void TP_enter(TP_table t, Temp_temp temp, G_node node);
/* Tell what "label" maps to in table "t" */
void *TP_look(TP_table t, Temp_temp temp);

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}


Temp_temp Live_gtemp(G_node n) {
	//your code here.
	// tell node n point to which temp in interfere graph
	return G_nodeInfo(n);
}

struct Live_graph Live_liveness(G_graph flow) {
	//your code here.
	struct Live_graph lg;

	// make input flowGraph to nodeList
	G_nodeList flowNodes = G_nodes(flow);
	
	G_table inTab = G_empty();
	G_table outTab = G_empty();

	for (G_nodeList nodes = G_nodes(flow); nodes; nodes = nodes->tail) {
		// init in out set for each node 
		G_enter(inTab, nodes->head, checked_malloc(sizeof(Temp_tempList)));
		G_enter(outTab, nodes->head, checked_malloc(sizeof(Temp_tempList)));
	}

	/* First: 
	 * set algorithm for liveness dataflow equation
	 * get liveness information of all temp in and out each node
	 */
	bool same = FALSE;
	while (!same) {
		same = TRUE;
		G_node node = NULL;
		for (G_nodeList nodes = G_nodes(flow); nodes; nodes = nodes->tail) {
			node = nodes->head;
			// save set state before this iteration
			Temp_tempList old_in  = *(Temp_tempList *)G_look(inTab, node);
			Temp_tempList old_out = *(Temp_tempList *)G_look(outTab, node);

			Temp_tempList in=NULL, out=NULL;
			// get out[n] = union ( in[s] ) where s in succ(n)
			G_nodeList succs = G_succ(node);
			for (; succs; succs = succs->tail) {
				Temp_tempList in_succ = *(Temp_tempList *)G_look(inTab, succs->head);
				out = unionTL(out, in_succ);
			}
			// get in[n] = use[n] U (out[n] - def[n])
			in = unionTL(FG_use(node), minusTL(out, FG_def(node)));
			// check set same, if not, set same flag to FALSE
			if (!(isEqual(old_in, in) && isEqual(old_out, out)))
				same = FALSE;
			// save new set state
			*(Temp_tempList*)G_look(inTab, node) = in;
			*(Temp_tempList*)G_look(outTab, node) = out;
		}
	}

	lg.graph = G_Graph();
	lg.moves = NULL;
	/* Second:
	 * build interfere graph with hard registers and virtual temps
	 * interfere means temps cannot store into same registers
	 * Third:
	 * at the same time, detect MOVE instr
	 * hoping allocate same reg for src and dst so that delete the MOVE
	 * */
	TP_table temp2NodeMap = TP_empty();
	// enter all temps
	for (G_nodeList nodes = G_nodes(flow); nodes; nodes = nodes->tail) {
		G_node node = nodes->head;
		Temp_tempList use  = FG_use(node);
		Temp_tempList def = FG_def(node);
		// enter all temps to temp2NodeMap table
		Temp_tempList temps = unionTL(use, def);
		for (; temps; temps=temps->tail) {
			if (!TP_look(temp2NodeMap, temps->head)) {
				G_node tempNode = G_Node(lg.graph, temps->head);
				TP_enter(temp2NodeMap, temps->head, tempNode);
			}
		}
	}

	Temp_tempList hardRegs = L(F_RAX(), L(F_RBX(), L(F_RCX(), L(F_RDX(),
							 L(F_RSI(), L(F_RDI(), L(F_RBP(), L(F_RSP(),
							 L(F_R8(),  L(F_R9(),  L(F_R10(), L(F_R11(),
							 L(F_R12(), L(F_R13(), L(F_R14(), L(F_R15(), NULL))))))))))))))));
	//enter all hard register
	for (Temp_tempList temps = hardRegs; temps; temps=temps->tail) {
		Temp_temp hardReg = temps->head;
		G_node hardnode = G_Node(lg.graph, hardReg);
		TP_enter(temp2NodeMap, hardReg, hardnode);
	}
	// make each hard register interfere with each other
	for (Temp_tempList temps = hardRegs; temps; temps=temps->tail) {
		G_node cur = TP_look(temp2NodeMap, temps->head);
		for (Temp_tempList nexts = temps->tail; nexts; nexts=nexts->tail) {
			G_node follows = TP_look(temp2NodeMap, nexts->head);
			G_addEdge(cur, follows);
			G_addEdge(follows, cur);
		}
	}
	// add interfere edge for temps
	for (G_nodeList nodes = G_nodes(flow); nodes; nodes=nodes->tail) {
		G_node node = nodes->head;
		Temp_tempList outNodes = *(Temp_tempList*)G_look(outTab, node);
		Temp_tempList def = FG_def(node);
		Temp_tempList useNodes = FG_use(node);

		for (; def; def = def->tail) {
			G_node defNode = TP_look(temp2NodeMap, def->head);

			for (Temp_tempList out = outNodes; out; out = out->tail) {
				G_node outNode = TP_look(temp2NodeMap, out->head);
				// special for MOVE, P162
				// for MOVE, add edge between out except use
				if (out->head != def->head && !G_goesTo(outNode, defNode) 
					&& (!FG_isMove(node) || !inTL(FG_use(node), out->head))) {
					G_addEdge(defNode, outNode);
					G_addEdge(outNode, defNode);
				}
			}
			// mark MOVE inst
			if (FG_isMove(node)) {
				for (Temp_tempList use = useNodes; use; use = use->tail) {
					G_node useNode = TP_look(temp2NodeMap, use->head);
					if (use->head != def->head && !inMoveList(lg.moves, useNode, defNode))
						lg.moves = Live_MoveList(useNode, defNode, lg.moves);
				}
			}
		}

	}

	return lg;
}


// check temp t in tempList tl
static bool inTL(Temp_tempList tl, Temp_temp t) {
	bool flag = FALSE;

	for (; tl; tl=tl->tail) {
		if (tl->head == t) {
			flag = TRUE;
			return flag;
		}
	}
	return flag;
}
// get the union of Temp_tempList a and b
static Temp_tempList unionTL(Temp_tempList a, Temp_tempList b) {
	Temp_tempList res = a;
	for (; b; b=b->tail) {
		if (inTL(a, b->head) == FALSE) {
			res = Temp_TempList(b->head, res);
		}
	}
	return res;
}
// get Temp_tempList a - b
static Temp_tempList minusTL(Temp_tempList a, Temp_tempList b) {
	Temp_tempList res = NULL;
	for (; a; a=a->tail) {
		if (inTL(b, a->head) == FALSE) {
			res = Temp_TempList(a->head, res);
		}
	}
	return res;
}
// check Temp_tempList a == b
static bool isEqual(Temp_tempList a, Temp_tempList b) {
	Temp_tempList tmp = a;
	for (; tmp; tmp=tmp->tail) {
		if (inTL(b, tmp->head) == FALSE)
			return FALSE;
	}

	tmp = b;
	for (; tmp; tmp=tmp->tail) {
		if (inTL(a, tmp->head) == FALSE)
			return FALSE;
	}

	return TRUE;
}
// check G_node pair (dst <--src) in Live_moveList
static bool inMoveList(Live_moveList ml, G_node src, G_node dst) {
	for (; ml; ml = ml->tail) 
		if (ml->src == src && ml->dst == dst) 
			return TRUE;
		
	return FALSE;
}

TP_table TP_empty(void) {
	return TAB_empty();
}
/* Enter the mapping "temp"->"node" to the table "t" */
void TP_enter(TP_table t, Temp_temp temp, G_node node) {
	TAB_enter(t, temp, node);
}
/* Tell what "label" maps to in table "t" */
void *TP_look(TP_table t, Temp_temp temp) {
	return TAB_look(t, temp);
}

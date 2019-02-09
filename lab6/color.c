#include <stdio.h>
#include <string.h>

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
#include "table.h"

#define COdebug 0

#define DEBUG(format, ...) do { \
    if (COdebug)  \
        fprintf(stdout, "[%s@%s,%d] " format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

#define K 15

// not dealed nodes
// initial = G_nodes(ig) - precolored
static G_nodeList simplifyWorklist;		// low degree & not move related
static G_nodeList freezeWorklist;		// low degree & move related
static G_nodeList spillWorklist;		// high degree nodes
// dealed nodes
static G_nodeList spilledNodes;			// actual spill 
static G_nodeList coalescedNodes;		// coalesed, u present v, v into here
static G_nodeList coloredNodes;			// colored
static G_nodeList selectStack;			// simplified nodes
// Move instrs
static Live_moveList coalescedMoves;	// coalesced move 
static Live_moveList constrainedMoves;  // constrained, src dst interfere
static Live_moveList frozenMoves;		// no coalesced yet
static Live_moveList worklistMoves;		// may coalesce
static Live_moveList activeMoves;		// not ready for coalesce, why

static void Build(G_graph ig);
static void AddEdge(G_node u, G_node v);
static void MakeWorklist(G_graph ig);
static G_nodeList Adjacent(G_node n);
static Live_moveList NodeMoves(G_node n);
static bool MoveRelated(G_node n);
static void Simplify();
static void DecrementDegree(G_node m);
static void EnableMoves(G_nodeList nodes);
static void Coalesce();
static void AddWorkList(G_node u);
static bool OK(G_node t, G_node r);
static bool Conservative(G_node u, G_node v);
static G_node GetAlias(G_node n);
static void Combine(G_node u, G_node v);
static void Freeze();
static void FreezeMoves(G_node u);
static void SelectSpill();
static void AssignColors(G_graph ig);

static void initialDS(Live_moveList moves);
static int getColor(Temp_temp temp);
static bool inML(Live_moveList ml, G_node src, G_node dst);
static Live_moveList minusML(Live_moveList a, Live_moveList b);
static Live_moveList unionML(Live_moveList a, Live_moveList b);
static bool precolored(G_node n);
static G_nodeList G_minusNL(G_nodeList u, G_nodeList v);
static G_nodeList G_unionNL(G_nodeList u, G_nodeList v);
static bool inTL(Temp_tempList a, Temp_temp t);

static G_table adjSet;
static G_table degreeTab; // G_node -> degree
static G_table aliasTab;  // G_node(coalesced) -> G_node
static G_table colorTab;  // G_node -> color

static char *hard_regs[17] = {"none", "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "%rsp"};

struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs, Live_moveList moves)
{
	// reset data structure 
	initialDS(moves);

	Build(ig);

	MakeWorklist(ig);
	// loop algorithm
	while (simplifyWorklist || worklistMoves || freezeWorklist || spillWorklist)
	{
		if (simplifyWorklist)
			Simplify();
		else if (worklistMoves)
			Coalesce();
		else if (freezeWorklist)
			Freeze();
		else if (spillWorklist)
			SelectSpill();
	}
	// try assign color
	AssignColors(ig);

	struct COL_result ret;
	ret.coloring = Temp_empty();
	G_nodeList nodes = G_nodes(ig);
	for (; nodes; nodes = nodes->tail)
	{// using string assign color
		int *color = G_look(colorTab, GetAlias(nodes->head));
		Temp_enter(ret.coloring, Live_gtemp(nodes->head), hard_regs[*color]);
	}

	Temp_tempList actualSpills = NULL;
    for (; spilledNodes; spilledNodes = spilledNodes->tail) {
        Temp_temp temp = G_nodeInfo(spilledNodes->head);
        actualSpills = Temp_TempList(temp, actualSpills);
    }
	ret.spills = actualSpills;

	return ret;
}

// interfere graph is built in liveness, node -> temp
// here we get degree & color & alias of each node(temp) 
static void Build(G_graph ig)
{
	int i = 0;
	// get the list of nodes from ig each of which points a temp
	G_nodeList nodes = G_nodes(ig);
	for (; nodes; nodes = nodes->tail) {
		DEBUG("node: %d\n", i++);
		// intial degree
		int *degree = checked_malloc(sizeof(int));
		*degree = G_degree(nodes->head);
		G_enter(degreeTab, nodes->head, (void *)degree);

		// intial color
		int *color = checked_malloc(sizeof(int));
		// tell the temp the node points, 0 for not precolored
		Temp_temp temp = Live_gtemp(nodes->head); 
		*color = getColor(temp);
		G_enter(colorTab, nodes->head, (void *)color);

		// initial alias
		G_node *alias = checked_malloc(sizeof(G_node));
		*alias = nodes->head;
		G_enter(aliasTab, nodes->head, (void *)alias);
	}
}

static int getColor(Temp_temp temp){
	if (temp == F_RAX())
		return 1;
	else if (temp == F_RBX())
		return 2;
	else if (temp == F_RCX())
		return 3;
	else if (temp == F_RDX())
		return 4;
	else if (temp == F_RSI())
		return 5;
	else if (temp == F_RDI())
		return 6;
	else if (temp == F_RBP())
		return 7;
	else if (temp == F_R8())
		return 8;
	else if (temp == F_R9())
		return 9;
	else if (temp == F_R10())
		return 10;
	else if (temp == F_R11())
		return 11;
	else if (temp == F_R12())
		return 12;
	else if (temp == F_R13())
		return 13;
	else if (temp == F_R14())
		return 14;
	else if (temp == F_R15())
		return 15;
	else if (temp == F_RSP())
		return 16;
	else // not precolored
		return 0;
}

static void AddEdge(G_node u, G_node v) 
{// adjSet is not necessary
 // (u,v) not in adjSet == !G_goesTo(u,v) 
	if (!G_goesTo(u, v) && u != v) {
		if (!precolored(u)) {
			G_addEdge(u, v);
			(*(int *)G_look(degreeTab, u))++;
		}
		if (!precolored(v)) {
			G_addEdge(v, u);
			(*(int *)G_look(degreeTab, v))++;
		}
	}
}

static void MakeWorklist(G_graph ig) 
{
	G_nodeList nodes = G_nodes(ig);
	for (; nodes; nodes = nodes->tail) {
		int *degree = G_look(degreeTab, nodes->head);
		int *color = G_look(colorTab, nodes->head);

		if (*color != 0) // precolored
			continue;
		// distribute into different node worklist
		if (*degree >= K) {
			spillWorklist = G_NodeList(nodes->head, spillWorklist);
		} 
		else if (MoveRelated(nodes->head)) {
			freezeWorklist = G_NodeList(nodes->head, freezeWorklist);
		}
		else {
			simplifyWorklist = G_NodeList(nodes->head, simplifyWorklist);
		}
	}
}

static G_nodeList Adjacent(G_node n)
{ // adjList(n) - selectStack - coalesced
	return G_minusNL(G_minusNL(G_adj(n), selectStack), coalescedNodes);
}

static Live_moveList NodeMoves(G_node n)
{// give related move inst with n
 // search m in union(activeMove, worklistMove)
	Live_moveList moves = NULL;
	G_node m = GetAlias(n); // if n is coalesced, get its alias
	for (Live_moveList ml = unionML(activeMoves, worklistMoves); ml; ml = ml->tail)
		if (GetAlias(ml->src) == m || GetAlias(ml->dst) == m)
			moves = Live_MoveList(ml->src, ml->dst, moves);
	return moves;
}

static bool MoveRelated(G_node n)
{
	return (NodeMoves(n) != NULL);
}

static void Simplify()
{// get one simplifiabel node and simplify it
	G_node n = simplifyWorklist->head;
	simplifyWorklist = simplifyWorklist->tail;
	selectStack = G_NodeList(n, selectStack);
	// dec degree of its adj
	for (G_nodeList nodes = Adjacent(n); nodes; nodes = nodes->tail)
		DecrementDegree(nodes->head);
}

static void DecrementDegree(G_node m)
{
	int *color = G_look(colorTab, m);
	// pass hard reg
	if (*color != 0)
		return;

	int *degree = G_look(degreeTab, m);
	(*degree)--;

	if (*degree == K - 1) {
		EnableMoves(G_NodeList(m, Adjacent(m)));
		// delete from high degree set
		spillWorklist = G_minusNL(spillWorklist, G_NodeList(m, NULL));
		// re-distribute
		if (MoveRelated(m))
			freezeWorklist = G_NodeList(m, freezeWorklist);
		else
			simplifyWorklist = G_NodeList(m, simplifyWorklist);
	}
}

static void EnableMoves(G_nodeList nodes)
{// make ready for moves of (k-1) degree neighbor
	for (; nodes; nodes = nodes->tail) {
		G_node n = nodes->head;
		for (Live_moveList m = NodeMoves(n); m; m = m->tail) {
			if (inML(activeMoves, m->src, m->dst)) {
				activeMoves = minusML(activeMoves, Live_MoveList(m->src, m->dst, NULL));
				worklistMoves = Live_MoveList(m->src, m->dst, worklistMoves);
			}
		}
	}
}

static void Coalesce()
{
	G_node u, v;
	G_node x = worklistMoves->src;
	G_node y = worklistMoves->dst;

	// make sure the first is hard reg
	if (precolored(GetAlias(y))) {
		u = GetAlias(y);
		v = GetAlias(x);
	}
	else {
		u = GetAlias(x);
		v = GetAlias(y);
	}
	worklistMoves = worklistMoves->tail;

	if (u == v) { // already coleased
		coalescedMoves = Live_MoveList(x, y, coalescedMoves);
		AddWorkList(u);
	}
	else if (precolored(v) || G_goesTo(u,v)) { // u,v both hard, no colesce
		constrainedMoves = Live_MoveList(x, y, constrainedMoves);
		AddWorkList(u);
		AddWorkList(v);
	}
	else if ( (precolored(u) && (OK(v, u)) )
		|| (!precolored(u) && Conservative(u, v)) ) {
		coalescedMoves = Live_MoveList(x, y, coalescedMoves);
		Combine(u, v);
		AddWorkList(u);
	}
	else {
		activeMoves = Live_MoveList(x, y, activeMoves);
	}
}

static void AddWorkList(G_node u)
{//  for not move-related in freeze, add to simplify
	if (!precolored(u) && !MoveRelated(u) && *(int *)G_look(degreeTab, u) < K) {
		freezeWorklist = G_minusNL(freezeWorklist, G_NodeList(u, NULL));
		simplifyWorklist = G_NodeList(u, simplifyWorklist);
	}
}


static bool OK(G_node v, G_node u)
{// for each t in adj(v), OK(t,u)
	for (G_nodeList t = Adjacent(v); t; t = t->tail) {
		if (*(int *)G_look(degreeTab, t->head) < K || precolored(t->head) || G_goesTo(t->head, u)) {
			continue;
		}
		else {
			return FALSE;
		}
	}
	return TRUE;
}

static bool Conservative(G_node u, G_node v)
{// Briggs: high degree neighbor number < K 
	G_nodeList nodes = G_unionNL(Adjacent(u), Adjacent(v));
	int k = 0;
	for (; nodes; nodes = nodes->tail) {
		G_node n = nodes->head;
		if (*(int *)G_look(degreeTab, n) >= K) {
			k++;
		}
	}
	return (k < K);
}

static G_node GetAlias(G_node n)
{// give n's present node (if coleased then not itself)
	assert(n);
	G_node *res = G_look(aliasTab, n);
	if (*res != n)
		return GetAlias(*res);
	else
		return *res;
}

// Combine v to u
static void Combine(G_node u, G_node v)
{
	if (G_inNodeList(v, freezeWorklist)) { // v in freezeWL
		freezeWorklist = G_minusNL(freezeWorklist, G_NodeList(v, NULL));
	}
	else {// v in spillWL
		spillWorklist = G_minusNL(spillWorklist, G_NodeList(v, NULL));
	}
	coalescedNodes = G_NodeList(v, coalescedNodes);
	// let u present v
	*(G_node *)G_look(aliasTab, v) = u;
	EnableMoves(G_NodeList(v, NULL));
	// give all neighbor of v to u
	for (G_nodeList t = Adjacent(v); t; t = t->tail) { 
		AddEdge(t->head, u);
		DecrementDegree(t->head);
	}
	// judge u again
	if (*(int *)G_look(degreeTab, u) >= K && G_inNodeList(u, freezeWorklist)) {
		freezeWorklist = G_minusNL(freezeWorklist, G_NodeList(u, NULL));
		spillWorklist = G_NodeList(u, spillWorklist);
	}
}

static void Freeze()
{// no simplify no coalease, choose freeze move from freezeWL, add to simplifyWL
	G_node u = freezeWorklist->head;
	freezeWorklist = freezeWorklist->tail;
	simplifyWorklist = G_NodeList(u, simplifyWorklist);
	FreezeMoves(u);
}

static void FreezeMoves(G_node u)
{
	for (Live_moveList m = NodeMoves(u); m; m = m->tail) {
		G_node x = m->src;
		G_node y = m->dst;
		G_node v;
		// get another node in move
		if (GetAlias(y) == GetAlias(u))
			v = GetAlias(x);
		else
			v = GetAlias(y);
		// remove the move in active, add to frozen
		activeMoves = minusML(activeMoves, Live_MoveList(x, y, NULL));
		frozenMoves = Live_MoveList(x, y, frozenMoves);
		// after freeze, another may not freeze
		if (NodeMoves(v) == NULL && *(int *)G_look(degreeTab, v) < K) {
			freezeWorklist = G_minusNL(freezeWorklist, G_NodeList(v, NULL));
			simplifyWorklist = G_NodeList(v, simplifyWorklist);
		}
	}
}

static void SelectSpill()
{// no above three, select spill, choose the max degree one
	G_node m = NULL;
	int max = 0;
	for (G_nodeList p = spillWorklist; p; p = p->tail) {
		int degree = *(int *)G_look(degreeTab, p->head);
		if (degree > max) {
			m = p->head;
			max = degree;
		}
	}
	spillWorklist = G_minusNL(spillWorklist, G_NodeList(m, NULL));
	simplifyWorklist = G_NodeList(m, simplifyWorklist);
	FreezeMoves(m);
}

static void AssignColors(G_graph ig)
{
	bool okColors[K + 2];
	spilledNodes = NULL;
	while (selectStack != NULL) {
		okColors[0] = FALSE;
		for (int i = 1; i < K + 1; i++) {
			okColors[i] = TRUE;
		}
		// pop stack
		G_node n = selectStack->head;
		selectStack = selectStack->tail;
		// make interfere color FALSE
		for (G_nodeList adjs = G_adj(n); adjs; adjs = adjs->tail) {
			int *color = G_look(colorTab, GetAlias(adjs->head));
			okColors[*color] = FALSE;
		}
		// search weather can be assigned
		int index;
		bool actualSpill = TRUE;
		for (index = 1; index < K + 1; index++) {
			if (okColors[index]) {
				actualSpill = FALSE;
				break;
			}
		}
		if (actualSpill) { // cannot assign
			spilledNodes = G_NodeList(n, spilledNodes);
		}
		else {
			*(int *)G_look(colorTab, n) = index;
		}
	}

	// assign coalesced nodes using alias color
	for (G_nodeList p = G_nodes(ig); p; p = p->tail) {
		*(int *)G_look(colorTab, p->head) = *(int *)G_look(colorTab, GetAlias(p->head));
	}
}

static Live_moveList unionML(Live_moveList a, Live_moveList b)
{
	Live_moveList res = a;
	for (Live_moveList p = b; p; p = p->tail)
		if (!inML(a, p->src, p->dst))
			res = Live_MoveList(p->src, p->dst, res);
	return res;
}

static Live_moveList minusML(Live_moveList a, Live_moveList b)
{
	Live_moveList res = NULL;
	for (Live_moveList p = a; p; p = p->tail)
		if (!inML(b, p->src, p->dst))
			res = Live_MoveList(p->src, p->dst, res);
	return res;
}

static bool inML(Live_moveList ml, G_node src, G_node dst)
{
	for (; ml; ml = ml->tail)
		if (ml->src == src && ml->dst == dst)
			return TRUE;

	return FALSE;
}

static bool precolored(G_node n)
{
	assert(n);
	return *(int *)G_look(colorTab, n);
}



static G_nodeList G_minusNL(G_nodeList u, G_nodeList v)
{
	G_nodeList res = NULL;
	for (G_nodeList nodes = u; nodes; nodes = nodes->tail)
		if (!G_inNodeList(nodes->head, v))
			res = G_NodeList(nodes->head, res);
	return res;
}
static G_nodeList G_unionNL(G_nodeList u, G_nodeList v)
{
	G_nodeList res = u;
	for (G_nodeList nodes = v; nodes; nodes = nodes->tail)
		if (!G_inNodeList(nodes->head, u))
			res = G_NodeList(nodes->head, res);
	return res;
}
static bool inTL(Temp_tempList a, Temp_temp t)
{
	for (; a; a = a->tail)
		if (a->head == t)
			return TRUE;
	return FALSE;
}

static void initialDS(Live_moveList moves) 
{
	simplifyWorklist = NULL;
	freezeWorklist = NULL;
	spillWorklist = NULL;

	spilledNodes = NULL;
	coalescedNodes = NULL;
	coloredNodes = NULL;
	selectStack = NULL;

	coalescedMoves = NULL;
	constrainedMoves = NULL;
	frozenMoves = NULL;
	// move instrs may can coalease from liveness
	worklistMoves = moves;
	activeMoves = NULL;

	degreeTab = G_empty();
	colorTab = G_empty();
	aliasTab = G_empty();
}

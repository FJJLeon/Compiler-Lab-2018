/*
 * flowgraph.h - Function prototypes to represent control flow graphs.
 */

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

Temp_tempList FG_def(G_node n);

Temp_tempList FG_use(G_node n);

bool FG_isMove(G_node n);

G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f);

/*the type of "tables" mapping Temp_label to the node coontaining the label*/
typedef struct TAB_table_ *LB_table;
LB_table LB_empty(void);

/* Enter the mapping "label"->"node" to the table "t" */
void LB_enter(LB_table t, Temp_label label, G_node node);

/* Tell what "label" maps to in table "t" */
void *LB_look(LB_table t, Temp_label label);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prog1.h"
 
/* used in maxargs */
/* functions */
int maxargsInStm(A_stm stm);
int maxargsInExp(A_exp exp);
int maxargsInExpList(A_expList exps);
int countPrintStm(A_expList exps);
int max(int left, int right);

/* used in interp */
/* ID table: record var*/
typedef struct table *Table_;
struct table { 
	string id;
	int value; 
	Table_ tail;
};

void scan(Table_ t); // scan ID table for debug
Table_ Table(string id, int value, Table_ tail) 
{
 	Table_ t = checked_malloc(sizeof(*t));
	t->id=id; t->value=value; t->tail=tail;
	//scan(t);
	return t;
}
/* IntAndTable: return struct for interExp */
typedef struct IntAndTable_ *IntAndTable;
struct IntAndTable_ {
	int i;
	Table_ t;
};
// malloc and init 
IntAndTable initIntAndTable(int i, Table_ t) {
	IntAndTable res = checked_malloc(sizeof(*res));
	res->i = i;
	res->t = t;
	return res;
}
/* functions */
Table_ interStm(A_stm stm, Table_ t);
IntAndTable interExp(A_exp exp, Table_ t);
int lookup(Table_ t, string key);
Table_ update(Table_ t, string id, int value);
IntAndTable interPrint(A_expList exps, Table_ t);
int calculate(int left, int right, A_binop op);

/////////////////////////////////////////////////////////////
int maxargs(A_stm stm)
{
	//TODO: put your code here.
	return maxargsInStm(stm);
}

int maxargsInStm(A_stm stm)
{
	switch(stm->kind)
	{
		case (A_compoundStm):
			return max(maxargsInStm(stm->u.compound.stm1), maxargsInStm(stm->u.compound.stm2));
		case (A_assignStm):
			return maxargsInExp(stm->u.assign.exp);
		case (A_printStm):
			return max(countPrintStm(stm->u.print.exps), maxargsInExpList(stm->u.print.exps));
		default:
			printf("%s", "error in stm");
			assert(0);
	}
}

int maxargsInExp(A_exp exp)
{
	switch (exp->kind)
	{
		case (A_idExp):
		case (A_numExp):
			return 0;
		case (A_opExp):
			return max(maxargsInExp(exp->u.op.left), maxargsInExp(exp->u.op.right));
		case (A_eseqExp):
			return max(maxargsInStm(exp->u.eseq.stm), maxargsInExp(exp->u.eseq.exp));
		default:
			printf("%s", "error in exp"); 
			assert(0);
	}
}

int maxargsInExpList(A_expList exps)
{
	switch (exps->kind)
	{
		case (A_pairExpList):
			return max(maxargsInExp(exps->u.pair.head), maxargsInExpList(exps->u.pair.tail));
		case (A_lastExpList):
			return maxargsInExp(exps->u.last);
		default:
			printf("%s", "error in explist");
			assert(0);
	}
}

int countPrintStm(A_expList exps)
{// count the number of args in a explist
	if (!exps)
		return 0;
	int count = 1;
	while (exps->kind == A_pairExpList) {
		count += 1;
		exps = exps->u.pair.tail;
	}
	return count;
}

int max(int left, int right)
{
	return left > right ? left : right;
}

////////////////////////////////////////////////////////
void interp(A_stm stm)
{
	//TODO: put your code here
	interStm(stm, NULL);
}

int lookup(Table_ t, string key)
{// scan table for key's value
	Table_ tmp = t;
	while (tmp){
		if (strcmp(tmp->id, key) == 0) {
			return tmp->value;
		}
		tmp = tmp->tail;
	}
	printf("%s not exist in lookup\n", key);
	assert(0);
	return 0;
}

Table_ update(Table_ t, string id, int value)
{
	Table_ tmp = Table(id, value, t);
	//scan(tmp);
	return tmp;
}

Table_ interStm(A_stm stm, Table_ t)
{
	switch (stm->kind)
	{
		case (A_compoundStm):
		{
			t = interStm(stm->u.compound.stm1, t);
			t = interStm(stm->u.compound.stm2, t);
			return t;
		}
		case (A_assignStm):
		{
			IntAndTable res = interExp(stm->u.assign.exp, t);
			return update(res->t, stm->u.assign.id, res->i);
		}
		case (A_printStm):
		{
			IntAndTable tmp = interPrint(stm->u.print.exps, t);
			return tmp->t;
		}
		default:
			printf("%s", "error in interStm");
			assert(0);
	}
}

IntAndTable interExp(A_exp exp, Table_ t)
{
	//printf("interExp, %d\n", exp->kind);
	switch (exp->kind)
	{
		case (A_idExp):
		{
			//printf("idExp,%s\n", exp->u.id);
			return initIntAndTable(lookup(t, exp->u.id), t);
		}
		case (A_numExp):
		{
			//printf("numExp,%d\n", exp->u.num); 
			return initIntAndTable(exp->u.num, t);
		}
		case (A_opExp):
		{
			IntAndTable left = interExp(exp->u.op.left, t);
			IntAndTable right = interExp(exp->u.op.right, left->t); // the tabel used for right may change
			return initIntAndTable(calculate(left->i, right->i, exp->u.op.oper), right->t);
		}
		case (A_eseqExp):
		{
			t = interStm(exp->u.eseq.stm, t);
			return interExp(exp->u.eseq.exp, t);
		}
		default:
			printf("%s", "error in interExp");
			assert(0);
	}
}

void scan(Table_ t) {
	printf("scan:");
	Table_ tmp = t;
	while (tmp != NULL){
		printf("%s = %d", tmp->id, tmp->value);
		tmp = tmp->tail;
	}
	printf(" \n");
}

IntAndTable interPrint(A_expList exps, Table_ t)
{
	//scan(t);
	IntAndTable res = initIntAndTable(0, t);
	while (exps->kind == A_pairExpList) {
		res = interExp(exps->u.pair.head, res->t);
		printf("%d ", res->i);
		exps = exps->u.pair.tail;
	}
	assert(exps->kind == A_lastExpList);
	res = interExp(exps->u.last, res->t);
	printf("%d\n", res->i);
	return res;
}

int calculate(int left, int right, A_binop op)
{
	switch (op)
	{
		case (A_plus):
			return left + right;
		case (A_minus):
			return left - right;
		case (A_times):
			return left * right;
		case (A_div):
			return left / right;
	}
}

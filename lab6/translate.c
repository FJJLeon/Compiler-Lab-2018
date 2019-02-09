#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

#include "printtree.h"
#include "prabsyn.h"
//LAB5: you can modify anything you want.
#define TRSdebug 0

#define debugout(format, ...) do { \
    if (TRSdebug)  \
        fprintf(stdout, "[%s@%s,%d]\t" format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

/*
	//should be put to .h, or 
	//error: dereferencing pointer to incomplete type
struct Tr_accessList_ {
	Tr_access head;
	Tr_accessList tail;	
};
struct Tr_level_ {
	F_frame frame;
	Tr_level parent;
};
*/

/*
 * Fragment: Data(String), Proc
 */

static Tr_level outer = NULL;
static F_fragList fraglist = NULL, fragtail = NULL;

static T_exp unEx(Tr_exp e);
static T_stm unNx(Tr_exp e);
static struct Cx unCx(Tr_exp e);

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
    Tr_expList p = checked_malloc(sizeof(*p));
    p->head = head;
    p->tail = tail;
    return p;
}

Tr_access Tr_Access(Tr_level level, F_access access)
{
    Tr_access p = checked_malloc(sizeof(*p));
    p->level = level;
    p->access = access;
    return p;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail)
{
    Tr_accessList p = checked_malloc(sizeof(*p));
    p->head = head;
    p->tail = tail;
    return p;
}

/*
 * main level 
 * called in SEM_transProg (semant.h)
 */  
Tr_level Tr_outermost(void)
{
	// static the same level
	if (!outer)
		outer = Tr_newLevel(NULL, Temp_newlabel(), NULL);
	return outer;
}

/*
 * create new level for nest function call
 * add static link also for saving in stack frame
 */
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals)
{
	Tr_level p = checked_malloc(sizeof(*p));
	p->parent = parent;
	U_boolList staticlink = U_BoolList(TRUE, formals);  // add static link
	p->frame = F_newFrame(name, staticlink);
	return p;
}

// helper for Tr_formals
static Tr_accessList makeFormalsAccessList(Tr_level level, F_accessList list) 
{
	if (list == NULL)
		return NULL;
	return Tr_AccessList(Tr_Access(level, list->head), makeFormalsAccessList(level, list->tail));
}
/*
 * semant.c get Tr_accessList, formals offset from frame pointer
 * skip the static link first 
 */
Tr_accessList Tr_formals(Tr_level level)
{
	F_accessList f_accesslist = F_formals(level->frame)->tail;
	return makeFormalsAccessList(level, f_accesslist);
}

/*
 * Tr_access = {Tr_level, F_access}
 * allocate F_access to local var 
 * via call F_allocLocal
 */
Tr_access Tr_allocLocal(Tr_level level, bool escape)
{
	F_access f_access = F_allocLocal(level->frame, escape);
	return Tr_Access(level, f_access);
}

/*
 * exit a function
 * remember the procFrag
 */ 
static void Tr_addFrag(F_frag frag) 
{
	if (fraglist) {
		fragtail->tail = F_FragList(frag, NULL);
		fragtail = fragtail->tail;
	}
	else {
		fraglist = F_FragList(frag, NULL);
		fragtail = fraglist;
	}
	
}

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals)
{
	// ??
	T_stm t_stm = T_Move(T_Temp(F_RAX()), unEx(body));  // move body result to return value register
    F_frag procfrag = F_ProcFrag(t_stm, level->frame);

	//t_stm = F_procEntryExit1(level->frame, t_stm);
																			
	// TODO: make real
	//F_frag procfrag = F_ProcFrag(NULL, NULL);
	// update FragList
	//fraglist = F_FragList(procfrag, fraglist);
	Tr_addFrag(procfrag);
}

// no use now
void Tr_initFrag(void)
{	
	fraglist = F_FragList(NULL, NULL);
	fragtail = fraglist;
}

F_fragList Tr_getResult(void)
{
	debugout("in get result");
	return fraglist;
}

/* ---============== Tr_ kind convetion ==========---*/
/*
 * patchList is used in Cx
 * remember NULL place to patch
 */

static patchList PatchList(Temp_label *head, patchList tail)
{
	patchList list;

	list = (patchList)checked_malloc(sizeof(struct patchList_));
	list->head = head;
	list->tail = tail;
	return list;
}

// use label patch tList
void doPatch(patchList tList, Temp_label label)
{
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second)
{
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}

static Tr_exp Tr_Ex(T_exp ex)
{
	Tr_exp res = checked_malloc(sizeof(*res));
	res->kind = Tr_ex;
	res->u.ex = ex;
	return res;
}

static Tr_exp Tr_Nx(T_stm nx)
{
	Tr_exp res = checked_malloc(sizeof(*res));
	res->kind = Tr_nx;
	res->u.nx = nx;
	return res;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm)
{
	Tr_exp res = checked_malloc(sizeof(*res));
	res->kind = Tr_cx;
	res->u.cx.trues = trues;
	res->u.cx.falses = falses;
	res->u.cx.stm = stm;
	return res;
}

/*
 * conversion among Ex, Nx, Cx
 * unXx: Tr_exp --> the input type of Tr_Xx 
 */
static T_exp unEx(Tr_exp e)
{ // Tr_exp --> T_exp
	switch (e->kind) {
		case Tr_ex:
			return e->u.ex;
		case Tr_cx:
		{
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			/*
			 * Key is that after doPatch 
			 * u.cx.stm will chaneg control flow, use Cjump to patched label 
			 * and T_Label set label for Cjump
			 */
			return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
						  T_Eseq(e->u.cx.stm,   // key
					 			T_Eseq(T_Label(f),
					  				T_Eseq(T_Move(T_Temp(r), T_Const(0)),
					   						T_Eseq(T_Label(t),
					    							T_Temp(r))))));
		}
		case Tr_nx:
			return T_Eseq(e->u.nx, T_Const(0));
		default:
			assert(0);
	}
	assert(0); /* can't get here */
}

static T_stm unNx(Tr_exp e)
{debugout("unNx stm kind %d", e->kind);
	switch (e->kind) {
		case Tr_ex:
			return T_Exp(e->u.ex); // ignore value, convert exp to stm
		case Tr_cx:
		{
			Temp_label ignore = Temp_newlabel();
			doPatch(e->u.cx.trues, ignore);
            doPatch(e->u.cx.falses, ignore);
			return T_Seq(e->u.cx.stm, T_Label(ignore));
		}
		case Tr_nx:
			return e->u.nx;
		default:
			assert(0);
	}
	assert(0); /* can't get here */
}

static struct Cx unCx(Tr_exp e)
{
	switch (e->kind) {
		case Tr_ex:
		{
			// TODO, how to deal with CONST 0 
			T_stm stm = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL);
			patchList t = PatchList(&(stm->u.CJUMP.true), NULL);
			patchList f = PatchList(&(stm->u.CJUMP.false), NULL);
			Tr_exp res = Tr_Cx(t, f, stm);
			return res->u.cx;
		}
		case Tr_cx:
			return e->u.cx;
		case Tr_nx:
			debugout("unCx failed to Tr_nx\n");
			assert(0);
		default:
			assert(0);
	}
	assert(0); /* can't get here */
}

/* ---============= IR ===========---*/
Tr_exp Tr_Nop()
{
	return Tr_Ex(T_Const(0));
}

Tr_exp Tr_Nil()
{
	return Tr_Ex(T_Const(0));
}

Tr_exp Tr_Int(int value)
{
	debugout("get value %d", value);
	return Tr_Ex(T_Const(value));
}

/*
 * P117
 * String as Fragment should be put into fraglist
 * return T_NAME(label)
 */
Tr_exp Tr_String(string s)
{
	debugout("Tr_string %s", s);
	Temp_label lab = Temp_newlabel();
	F_frag stringfrag = F_StringFrag(lab, s);
	//fraglist = F_FragList(stringfrag, fraglist);
	Tr_addFrag(stringfrag);
	return Tr_Ex(T_Name(lab));
}

Tr_exp Tr_Break(Temp_label done)
{
	return Tr_Nx(T_Jump(T_Name(done), Temp_LabelList(done, NULL)));
}

// helper for Tr_Call
T_expList makeTArgs(Tr_expList args)
{
	T_expList res = NULL, tail = NULL;
	Tr_expList tmp = args;
	debugout(" !!!!!!!!!");
	while (tmp) {
		printStmList(stdout, T_StmList(T_Exp(unEx(args->head)), NULL));
		if (res) {
			tail->tail = T_ExpList(unEx(tmp->head), NULL);
			tail = tail->tail;
		}
		else {
			res = T_ExpList(unEx(tmp->head), NULL);
			tail = res;
		}
		//res = T_ExpList(unEx(tmp->head), res);
		
		tmp = tmp->tail;
	}
	return res;
}
/*
 * CALL(Name f, [sl, e1,..., en])
 * pass a static link as argument at first 
 */
Tr_exp Tr_Call(Temp_label func, Tr_expList args, Tr_level caller, Tr_level callee)
{
	debugout("Tr_Call func: %s", Temp_labelstring(func));
	// Tr_expList --> T_expList
	T_expList t_args = makeTArgs(args);

	// callee func have nest parent level
	if (callee->parent) {
		// pass static link
		T_exp static_link = T_Temp(F_RBP()); //TODO : FP?
		while (caller != callee->parent) {
			static_link = F_Exp(F_formals(caller->frame)->head, static_link);
			caller = caller->parent;
		}
		return Tr_Ex(T_Call(T_Name(func), T_ExpList(static_link, t_args)));
	}
	else {
		return Tr_Ex(F_externalCall(Temp_labelstring(func), t_args));
	}
		
}

Tr_exp Tr_OpArithmetic(A_oper oper, Tr_exp left, Tr_exp right)
{
	T_exp l = unEx(left);
	T_exp r = unEx(right);
	switch (oper) {
		case A_minusOp:
			return Tr_Ex(T_Binop(T_minus, l, r));
		case A_plusOp:
			return Tr_Ex(T_Binop(T_plus, l, r));
		case A_timesOp:
			return Tr_Ex(T_Binop(T_mul, l, r));
		case A_divideOp:
			return Tr_Ex(T_Binop(T_div, l, r));
	}
	assert(0);
}

Tr_exp Tr_OpCmp(A_oper oper, Tr_exp left, Tr_exp right)
{
	T_exp l = unEx(left);
	T_exp r = unEx(right);
	T_stm t_stm;
	switch (oper) {
		case A_eqOp:
			t_stm = T_Cjump(T_eq, l, r, NULL, NULL);
			break;
		case A_neqOp:
			t_stm = T_Cjump(T_ne, l, r, NULL, NULL);
			break;
		case A_ltOp:
			t_stm = T_Cjump(T_lt, l, r, NULL, NULL);
			break;
		case A_gtOp:
			t_stm = T_Cjump(T_gt, l, r, NULL, NULL);
			break;
		case A_leOp:
			t_stm = T_Cjump(T_le, l, r, NULL, NULL);
			break;
		case A_geOp:
			t_stm = T_Cjump(T_ge, l, r, NULL, NULL);
			break;
		default:
			assert(0);
	}
	return Tr_Cx(PatchList(&t_stm->u.CJUMP.true, NULL), 
				PatchList(&t_stm->u.CJUMP.false, NULL),
				t_stm);
}

/*
 * string equal should invocate externalCall
 * */
Tr_exp Tr_opStrCmp(A_oper oper, Tr_exp left, Tr_exp right)
{	
	T_exp l = unEx(left);
	T_exp r = unEx(right);
	T_exp res = F_externalCall(String("stringEqual"), 
								T_ExpList(l, T_ExpList(r, NULL)));
	if (oper == A_eqOp)
		return Tr_Ex(res);
	else // negate result
		return Tr_Ex(T_Binop(T_minus, T_Const(1), res));	
}

Tr_exp Tr_Assign(Tr_exp lvalue, Tr_exp exp)
{
	debugout("what kind lvalue: %d, exp: %d", lvalue->kind, exp->kind);
	debugout("what tree kind lvalue: %d, exp: %d", 
					lvalue->u.ex->kind, exp->u.ex->kind);
	if (TRSdebug)
		printStmList(stdout, T_StmList(unNx(exp), NULL));

	return Tr_Nx(T_Move(unEx(lvalue), unEx(exp)));
}

Tr_exp Tr_Seq(Tr_exp seq, Tr_exp e)
{
	return Tr_Ex(T_Eseq(unNx(seq), unEx(e)));
}

/*
 * IfThen statement return no value
 */
Tr_exp Tr_IfThen(Tr_exp test, Tr_exp then)
{
	Temp_label body = Temp_newlabel();
	Temp_label end = Temp_newlabel();
	// patch 
	struct Cx cmp = unCx(test);
	doPatch(cmp.trues, body);
	doPatch(cmp.falses, end);

	T_stm then_stm = T_Seq(T_Label(body), unNx(then));
	return Tr_Nx(T_Seq(cmp.stm, T_Seq(then_stm, T_Label(end))));
}

Tr_exp Tr_IfThenElse(Tr_exp test, Tr_exp then, Tr_exp elsee)
{
	Temp_label t = Temp_newlabel();
	Temp_label f = Temp_newlabel();
	Temp_label done = Temp_newlabel();
	// patch 
	struct Cx cmp = unCx(test);
	doPatch(cmp.trues, t);
	doPatch(cmp.falses, f);

	Temp_temp result = Temp_newtemp();
	
	// then 
	T_stm then_stm = T_Seq(T_Label(t),
						T_Seq(T_Move(T_Temp(result), unEx(then)),
							T_Jump(T_Name(done), Temp_LabelList(done, NULL)))); 
	// else
	T_stm else_stm = T_Seq(T_Label(f),
						T_Seq(T_Move(T_Temp(result), unEx(elsee)),
							T_Jump(T_Name(done), Temp_LabelList(done, NULL))));
	// done
	T_exp done_exp = T_Eseq(T_Label(done), T_Temp(result));

	return Tr_Ex(T_Eseq(T_Seq(cmp.stm, T_Seq(then_stm, else_stm)), done_exp));
}

/*
 * test:
 * 		if not(condition) goto done
 * 		body
 * 		goto test
 * done:
 * 
 * T_Label(test)
 * cmp.stm
 * T_Label(body)
 * unNx(body)
 * T_Jump(T_Name(test), Temp_LabelList(test, NULL))
 * T_Label(done)
 */
Tr_exp Tr_While(Tr_exp test, Tr_exp body, Temp_label done)
{
	Temp_label test_label = Temp_newlabel();
	Temp_label body_label = Temp_newlabel();
	// patch 
	struct Cx cmp = unCx(test);
	//printStmList(stdout, T_StmList(cmp.stm, NULL));
	doPatch(cmp.trues, body_label);
	doPatch(cmp.falses, done);
	// test
	T_stm test_stm = T_Seq(T_Label(test_label), cmp.stm);
	// body
	T_stm body_stm = T_Seq(T_Label(body_label), 
						T_Seq(unNx(body), T_Jump(T_Name(test_label), Temp_LabelList(test_label, NULL))));
	
	return Tr_Nx(T_Seq(test_stm, T_Seq(body_stm, T_Label(done))));

	/*Tr_Nx(T_Seq(T_Label(test_label),
				  T_Seq(cmp.stm,
				   T_Seq(T_Label(body_label),
					T_Seq(unNx(body),
					 T_Seq(T_Jump(T_Name(test_label), Temp_LabelList(test_label, NULL)),
						   T_Label(done)))))));
						   */
}

/*
 * let var i:= lo
 * 	   var limit := hi
 * in while i<= limit
 * 		do (body; i:=i+1)
 * end
 * 
 * T_Move(unEx(i), unEx(lo))
    T_Move(T_Temp(limit), unEx(hi))
    T_Cjump(T_le, unEx(i), T_Temp(limit), b, done)
    T_Label(b)
    unNx(body)
    T_Cjump(T_eq, unEx(i), T_Temp(limit), done, inc)
    T_Label(inc)
    T_Move(unEx(i), T_Binop(T_plus, unEx(i), T_Const(1)))
    T_Jump(T_Name(b), Temp_LabelList(b, NULL))
    T_Label(done)
 */
Tr_exp Tr_For(Tr_access access, Tr_level level, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done)
{
	// let
	Tr_exp loopvar = Tr_simpleVar(access, level);
	T_stm istm = unNx(Tr_Assign(loopvar, lo));

	//Tr_access limac = Tr_allocLocal(level, FALSE);
	//Tr_exp limit = Tr_simpleVar(limac, level);
	//T_stm limstm = unNx(Tr_Assign(limit, hi));
	T_exp limit = T_Temp(Temp_newtemp());
	T_stm limstm = T_Move(limit, unEx(hi));

	// while i <= limit
	T_stm testStm = T_Cjump(T_le, unEx(loopvar), limit, NULL, NULL);
	patchList trues = PatchList(&testStm->u.CJUMP.true, NULL);
	patchList falses = PatchList(&testStm->u.CJUMP.false, NULL);

	struct Cx cmp = unCx(Tr_Cx(trues, falses, testStm));

	Temp_label test_label = Temp_newlabel();
	Temp_label body_label = Temp_newlabel();
	doPatch(cmp.trues, body_label);
	doPatch(cmp.falses, done);
	// do body
	T_stm bodyStm = T_Seq(unNx(body), 
						  T_Move(unEx(loopvar), 
								 T_Binop(T_plus, unEx(loopvar), T_Const(1))));
	// loop like while
	T_stm loop = T_Seq(T_Label(test_label),
					 T_Seq(cmp.stm,
					   T_Seq(T_Label(body_label),
						T_Seq(bodyStm,
					  	  T_Seq(T_Jump(T_Name(test_label), Temp_LabelList(test_label, NULL)),
						    T_Label(done))))));

	return Tr_Nx(T_Seq(T_Seq(istm, limstm), loop));
}
// type-id{id = exp{, id = exp}}
Tr_exp Tr_Record(int size, Tr_expList fields)
{
	debugout("record IR size %d", size);
	Temp_temp base = Temp_newtemp();
	T_exp call_malloc = F_externalCall("malloc",
                                T_ExpList(T_Const(size*F_wordSize), NULL));
	T_stm init_stm = T_Move(T_Temp(base), call_malloc);
	// note the order
	int i=size-1;
	T_stm itemseq = T_Move(T_Mem(T_Binop(T_plus, T_Temp(base), T_Const(F_wordSize*i))), unEx(fields->head));
	
	for (i--, fields=fields->tail; fields && i>=0; i--, fields = fields->tail) {
		debugout("in record field for, kind %d", fields->head->kind);
		debugout("exp kind %d", fields->head->u.ex->kind);
		debugout("const value %d", fields->head->u.ex->u.CONST);
		T_exp dest = T_Binop(T_plus, T_Temp(base), T_Const(F_wordSize*i));
		T_stm assign = T_Move(T_Mem(dest), unEx(fields->head));
		itemseq = T_Seq(assign, itemseq);
	}
	debugout("out record field for");
	return Tr_Ex(T_Eseq(T_Seq(init_stm, itemseq), T_Temp(base)));
}

//????
Tr_exp Tr_Array(Tr_exp size, Tr_exp init)
{
	//return Tr_Ex(F_externalCall("initArray", T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
    T_exp base = T_Temp(Temp_newtemp());
    T_stm inits = T_Move(base, F_externalCall("initArray", T_ExpList(unEx(size),
                                                               T_ExpList(unEx(init),NULL))));
    //T_stm init = T_Move(base, F_externalCall("malloc", T_ExpList(T_Binop(T_mul, T_Const(F_wordsize),unEx(size)),NULL)));
    T_exp finall = T_Eseq(inits, base); 
    return Tr_Ex(finall);   
}

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level)
{
	/*
	T_exp access_frame = T_Temp(F_FP());
    while (level != access->level) {
        access_frame = T_Mem(T_Binop(T_plus, access_frame, T_Const(F_wordSize * 2)));  // track up
        level = level->parent;
    }
	*/
	T_exp static_link = T_Temp(F_RBP()); //TODO : FP
	while (level != access->level) {
		static_link = F_Exp(F_formals(level->frame)->head, static_link);
		level = level->parent;
	}
	debugout(" simpleVar static link");
	printStmList(stdout, T_StmList(T_Exp(static_link), NULL));

	return Tr_Ex(F_Exp(access->access, static_link));
}

Tr_exp Tr_fieldVar(Tr_exp addr, int off)
{
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(addr),
						T_Const(F_wordSize*off))));
}

Tr_exp Tr_subscriptVar(Tr_exp addr, Tr_exp off)
{
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(addr),
						T_Binop(T_mul, T_Const(F_wordSize), unEx(off)))));
}
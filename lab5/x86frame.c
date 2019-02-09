#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

/*Lab5: Your implementation here.*/
const int F_wordSize = 8;

Temp_temp F_FP(void)
{
    return Temp_newtemp();
}

struct F_frame_ {
    Temp_label name;
    F_accessList formals;
    int local_cnt;
};

struct F_access_ {
    enum { inFrame, inReg } kind;
    union {
        int offs;		/* InFrame */
        Temp_temp reg;	/* InReg */
    } u;
};

static F_access InFrame(int offs)
{
    F_access res = checked_malloc(sizeof(*res));
    res->kind = inFrame;
    res->u.offs = offs;
    return res;
}

static F_access InReg(Temp_temp t)
{
    F_access res = checked_malloc(sizeof(*res));
    res->kind = inReg;
    res->u.reg = t;
    return res;
}

F_accessList F_AccessList(F_access head, F_accessList tail)
{
    F_accessList l = checked_malloc(sizeof(*l));
    l->head = head;
    l->tail = tail;
    return l;
}

static F_accessList makeFrameAccessList(U_boolList formals) {
	U_boolList formal = formals;
	F_accessList res = malloc(sizeof(*res));
	int regcount = 0;
	int stackcount = 0;
	while (formal) {
		if (formal || regcount == 4) { // escape  in stack frame
			res = F_AccessList(InFrame(stackcount * F_wordSize), res);
			stackcount++;
		}
		else {	// in reg
			res = F_AccessList(InReg(Temp_newtemp()), res);
			regcount++;
		}
		formal = formal->tail;
	}
	return res;
}

F_frame F_newFrame(Temp_label name, U_boolList formals) {
    F_frame f = checked_malloc(sizeof(*f));
    f->name = name;
    f->formals = makeFrameAccessList(formals);
    f->local_cnt = 0;
	return f;
}

F_accessList F_formals(F_frame f)
{
    return f->formals;
}

Temp_label F_name(F_frame f)
{
    return f->name;
}

F_access F_allocLocal(F_frame f, bool escape)
{
    if (escape) {
        f->local_cnt++; 
        return InFrame(-F_wordSize * f->local_cnt);
    } else {
        return InReg(Temp_newtemp());
    }
}

F_frag F_StringFrag(Temp_label label, string str) {
    F_frag res = checked_malloc(sizeof(*res));
    res->kind = F_stringFrag;
    res->u.stringg.label = label;
    res->u.stringg.str = str;
    return res;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
    F_frag res = checked_malloc(sizeof(*res));
    res->kind = F_procFrag;
    res->u.proc.body = body;
    res->u.proc.frame = frame;
    return res;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
    F_fragList res = checked_malloc(sizeof(*res));
    res->head = head;
    res->tail = tail;
    return res;
}


T_exp F_Exp(F_access access, T_exp fp)
{
    if (access->kind == inFrame) {
        return T_Mem(T_Binop(T_plus, fp, T_Const(access->u.offs)));
    } else {
        return T_Temp(access->u.reg);
    }
}

T_exp F_externalCall(string s, T_expList args) {
    return T_Call(T_Name(Temp_namedlabel(s)), args);
}
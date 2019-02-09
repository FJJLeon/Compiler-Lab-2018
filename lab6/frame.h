/*Lab5: This header file is not complete. Please finish it with more definition.*/
#ifndef FRAME_H
#define FRAME_H

#include "util.h"
#include "temp.h"
#include "tree.h"
#include "assem.h"


extern const int F_wordSize;

typedef struct F_frame_ *F_frame;
typedef struct F_access_ *F_access;
typedef struct F_accessList_ *F_accessList;

struct F_accessList_ { F_access head; F_accessList tail; };
F_accessList F_AccessList(F_access head, F_accessList tail);

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

// fragement for string or procedure
/* ---=== declaration for fragments ===--- */
typedef struct F_frag_ *F_frag;
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};
 
F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ {
	F_frag head;
	F_fragList tail;
};
F_fragList F_FragList(F_frag head, F_fragList tail);

/* ---=== helper interface ===---*/
Temp_map F_tempMap;
Temp_tempList F_registers(void);  // TODO
Temp_label F_name(F_frame f);
string F_getLabel(F_frame f);
F_accessList F_formals(F_frame f);

// create a new frame called by Tr_newLevel
F_frame F_newFrame(Temp_label name, U_boolList formals);
F_access F_allocLocal(F_frame f, bool escape);

T_exp F_Exp(F_access access, T_exp fp);
T_exp F_externalCall(string s, T_expList args);

F_frag F_string(Temp_temp name, string str);
F_frag F_newProcFrag(T_stm body, F_frame frame);

T_stm F_procEntryExit1(F_frame f, T_stm stm);
AS_instrList F_procEntryExit2(AS_instrList body); // p153
AS_proc F_procEntryExit3(F_frame f, AS_instrList body);

/*---=== hard register ===---*/
Temp_temp F_RAX(void);
Temp_temp F_RBX(void);
Temp_temp F_RCX(void);
Temp_temp F_RDX(void);
Temp_temp F_RSI(void);
Temp_temp F_RDI(void);
Temp_temp F_RBP(void);
Temp_temp F_RSP(void);
Temp_temp F_R8(void);
Temp_temp F_R9(void);
Temp_temp F_R10(void);
Temp_temp F_R11(void);
Temp_temp F_R12(void);
Temp_temp F_R13(void);
Temp_temp F_R14(void);
Temp_temp F_R15(void);

Temp_temp F_FP(void);
Temp_temp F_SP(void);
Temp_temp F_ZERO(void);
Temp_temp F_RA(void);
Temp_temp F_RV(void);

Temp_temp F_ARGUREGS(int);

#endif
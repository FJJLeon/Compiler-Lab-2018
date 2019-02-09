#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"
#include "assem.h"

#define FMdebug 0

#define DEBUG(format, ...) do { \
    if (FMdebug)  \
        fprintf(stdout, "[%s@%s,%d] " format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

/*Lab5: Your implementation here.*/
#define argregs L(F_RDI(), L(F_RSI(), L(F_RDX(), L(F_RCX(), L(F_R8(),L(F_R9(), NULL))))))
#define callersaves L(F_R10(), L(F_R11(), argregs))
#define calleesaves L(F_RBX(), L(F_RBP(), L(F_R12(), L(F_R13(), L(F_R14(), L(F_R15(), NULL))))))
static Temp_tempList L(Temp_temp h, Temp_tempList t) {
    return Temp_TempList(h, t);
}

const int F_wordSize = 8;
static int F_argsNum = 6; // number of argument passed by register

F_accessList F_AccessList(F_access head, F_accessList tail)
{
    F_accessList l = checked_malloc(sizeof(*l));
    l->head = head;
    l->tail = tail;
    return l;
}

Temp_label F_name(F_frame f)
{
    return f->name;
}

string F_getLabel(F_frame f)
{
    return Temp_labelstring(f->name);
}

F_accessList F_formals(F_frame f)
{
    return f->formals;
}

static F_accessList makeFrameAccessList(F_frame f, U_boolList formals) {
	F_accessList res = malloc(sizeof(*res));
	int regcount = 0;
	int stackcount = 0;
    U_boolList formal = formals;
	while (formal) {
		if (formal->head || regcount == 4) { // escape  in stack frame
			res = F_AccessList(InFrame(stackcount * F_wordSize), res);
			stackcount++;
            f->local_cnt++;
		}
		else {	// in reg
			res = F_AccessList(InReg(Temp_newtemp()), res);
			regcount++;
		}
		formal = formal->tail;
	}
	return res;
}

Temp_tempList F_registers(void) {
    return L(F_RAX(), L(F_RBX(), L(F_RCX(), L(F_RDX(),
            L(F_RSI(), L(F_RDI(), L(F_RBP(), L(F_RSP(),
            L(F_R8(),  L(F_R9(),  L(F_R10(), L(F_R11(),
            L(F_R12(), L(F_R13(), L(F_R14(), L(F_R15(), NULL))))))))))))))));
}

/* create a new frame called by Tr_newLevel
 * note: here formals is added static link at head by translate
 * */
F_frame F_newFrame(Temp_label name, U_boolList formals) {
    
    F_frame f = checked_malloc(sizeof(*f));
    f->name = name;
    f->local_cnt = 0;
    //f->formals = makeFrameAccessList(f, formals);
    
    int regcount = 0, stackcount = 0;
    F_accessList res = NULL, tail = NULL;
    U_boolList formal = formals;
    while (formal) {
        F_access ac = NULL;
        if (regcount < F_argsNum) {
            if (formal->head) {
                DEBUG("escape exist %d", regcount);
                // inc locak_cnt, which for getting frame size
                f->local_cnt++;
                ac = InFrame(-(f->local_cnt) * F_wordSize);
            }
            else {
                ac = InReg(Temp_newtemp());
            }
            regcount++;
        }
        else {
            ac = InFrame((stackcount)*F_wordSize);
            stackcount++;
        }
        // add F_access to tail
        //res = F_AccessList(ac, res); // this is add to head, seems wrong
        if (res) {
            tail->tail = F_AccessList(ac, NULL);
            tail = tail->tail;
        }
        else {
            res = F_AccessList(ac, NULL);
            tail = res;
        }
        
        formal = formal->tail;
    }
    f->formals = res;
	return f;
}

// alloc local variable according to escape flag
F_access F_allocLocal(F_frame f, bool escape)
{
    DEBUG("alloc local %d in frame %s, escape %d", 
                    f->local_cnt+1, Temp_labelstring(F_name(f)), escape);
    if (escape) {
        f->local_cnt++; 
        return InFrame(-F_wordSize * f->local_cnt);
    } else {
        return InReg(Temp_newtemp());
    }
}

T_exp F_Exp(F_access access, T_exp fp)
{
    if (access->kind == inFrame) {
        return T_Mem(T_Binop(T_plus, T_Const(access->u.offs), fp));
    } else {
        return T_Temp(access->u.reg);
    }
}

// call lib function such as initArray stringEqual
T_exp F_externalCall(string s, T_expList args) {
    return T_Call(T_Name(Temp_namedlabel(s)), args);
}

/* ---=== constructor for fragments ===--- */
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

/* ---=== hard register ===--- */
static Temp_temp rax = NULL; // return value
static Temp_temp rbx = NULL; // callee saved
static Temp_temp rcx = NULL; // 4th args
static Temp_temp rdx = NULL; // 3th args
static Temp_temp rsi = NULL; // 2sd args
static Temp_temp rdi = NULL; // 1st args
static Temp_temp rbp = NULL; // callee saved ?? frame pointer
static Temp_temp r8  = NULL; // 5th args
static Temp_temp r9  = NULL; // 6th args
static Temp_temp r10 = NULL; // caller saved
static Temp_temp r11 = NULL; // caller saved
static Temp_temp r12 = NULL; // callee saved
static Temp_temp r13 = NULL; // callee saved
static Temp_temp r14 = NULL; // callee saved
static Temp_temp r15 = NULL; // callee saved
static Temp_temp rsp = NULL; // stack point

Temp_temp F_RAX(void){ if (!rax) rax = Temp_newtemp(); return rax;}
Temp_temp F_RBX(void){ if (!rbx) rbx = Temp_newtemp(); return rbx;}
Temp_temp F_RCX(void){ if (!rcx) rcx = Temp_newtemp(); return rcx;}
Temp_temp F_RDX(void){ if (!rdx) rdx = Temp_newtemp(); return rdx;}
Temp_temp F_RSI(void){ if (!rsi) rsi = Temp_newtemp(); return rsi;}
Temp_temp F_RDI(void){ if (!rdi) rdi = Temp_newtemp(); return rdi;}
Temp_temp F_RBP(void){ if (!rbp) rbp = Temp_newtemp(); return rbp;}
Temp_temp F_R8(void){  if (!r8)  r8  = Temp_newtemp(); return r8;}
Temp_temp F_R9(void){  if (!r9)  r9  = Temp_newtemp(); return r9;}
Temp_temp F_R10(void){ if (!r10) r10 = Temp_newtemp(); return r10;}
Temp_temp F_R11(void){ if (!r11) r11 = Temp_newtemp(); return r11;}
Temp_temp F_R12(void){ if (!r12) r12 = Temp_newtemp(); return r12;}
Temp_temp F_R13(void){ if (!r13) r13 = Temp_newtemp(); return r13;}
Temp_temp F_R14(void){ if (!r14) r14 = Temp_newtemp(); return r14;}
Temp_temp F_R15(void){ if (!r15) r15 = Temp_newtemp(); return r15;}
Temp_temp F_RSP(void){ if (!rsp) rsp = Temp_newtemp(); return rsp;}

/* registers for passing arguments 
   RDI  RSI  RDX  RCX  R8  R9 */
Temp_temp F_ARGUREGS(int i) {
    /*
    if (!rdi) {rdi = Temp_newtemp();}
    if (!rsi) {rsi = Temp_newtemp();}
    if (!rdx) {rdx = Temp_newtemp();}
    if (!rcx) {rcx = Temp_newtemp();}
    if (!r8)  {r8  = Temp_newtemp();}
    if (!r9)  {r9  = Temp_newtemp();}
    */
    switch (i){
        case 0: return F_RDI();
        case 1: return F_RSI();
        case 2: return F_RDX();
        case 3: return F_RCX();
        case 4: return F_R8();
        case 5: return F_R9();
        default:
            assert(0);
    }
}

// TODO: delete FP
Temp_temp F_FP(void)
{
    return F_RBP();
}

// TODO:
AS_proc F_procEntryExit3(F_frame f, AS_instrList il){
  int fsize = f->local_cnt * F_wordSize;
	AS_proc proc = checked_malloc(sizeof(*proc));
  char *prolog = checked_malloc(100 * sizeof(char));
	sprintf(prolog, "subq $%d, %%rsp\n", fsize);
  proc->prolog = prolog;

  char *epilog = checked_malloc(100 * sizeof(char));
	sprintf(epilog, "addq $%d, %%rsp\nret\n", fsize);
  proc->epilog = epilog;

  proc->body = il;
}

T_stm F_procEntryExit1(F_frame f, T_stm stm) {
/*
    // callee-saved reg
    // note, not-necessary move will be deleted after RA
    Temp_tempList calleeTmp = calleesaves;
    Temp_tempList saveHead, saveTail; 
    saveHead = saveTail = L(Temp_newtemp(), NULL);
    for(; calleeTmp; calleeTmp=calleeTmp->tail) {
        emit(AS_Move("movq `s0, `d0", L(saveTail->head, NULL), L(calleeTmp->head, NULL)));
        saveTail->tail = L(Temp_newtemp(), NULL);
        saveTail = saveTail->tail;
    }
    // escape switch
    F_accessList formals = f->formals;
    int cn = 0;
    for (int regcount=0; regcount<F_argsNum && formals; regcount++){
        Temp_temp st;
        switch(regcount){
            case 0: st = F_RDI(); break;
            case 1: st = F_RSI(); break;
            case 2: st = F_RDX(); break;
            case 3: st = F_RCX(); break;
            case 4: st = F_R8();  break;
            case 5: st = F_R9();  break;
        }
        if (formals->head->kind == inFrame){
            //fix fp
            Temp_temp d = Temp_newtemp();
            emit(AS_Oper("movq `s0, `d0", L(d, NULL), L(F_RSP(), NULL), AS_Targets(NULL)));
            char * inst = checked_malloc(MAXLEN * sizeof(char));
            sprintf(inst, "addq $%sframesize, `d0", Temp_labelstring(F_name(frame)));
            emit(AS_Oper(inst, L(d, NULL), NULL, AS_Targets(NULL)));

            //use fp
            inst = checked_malloc(MAXLEN * sizeof(char));
            cn++;
            sprintf(inst, "movq `s0, %d(`d0)", - (cn) * F_wordSize);
            emit(AS_Oper(inst, L(d, NULL), L(st, NULL), AS_Targets(NULL)));
        }else{
            emit(AS_Move("movq `s0, `d0", L(formals->head->u.reg, NULL), L(st, NULL)));
        }
        formals = formals->tail;
    }

    calleeTmp = calleesaves;
    saveTail = saveHead;
    for(; calleeTmp; calleeTmp=calleeTmp->tail) {
        emit(AS_Move("movq `s0, `d0", L(calleeTmp->head, NULL), L(saveTail->head, NULL)));
        saveTail = saveTail->tail;
    }
*/

}
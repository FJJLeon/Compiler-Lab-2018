#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

//Lab 6: put your code here
#define CGdebug 0

#define DEBUG(format, ...) do { \
    if (CGdebug)  \
        fprintf(stdout, "[%s@%s,%d] " format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

#define MAXLEN 40
#define argregs L(F_RDI(), L(F_RSI(), L(F_RDX(), L(F_RCX(), L(F_R8(),L(F_R9(), NULL))))))
#define callersaves L(F_R10(), L(F_R11(), argregs))
#define calleesaves L(F_RBX(), L(F_RBP(), L(F_R12(), L(F_R13(), L(F_R14(), L(F_R15(), NULL))))))

// function for munch Tree
static void munchStm(T_stm s);
static Temp_temp munchExp(T_exp e);
static Temp_tempList munchArgs(int i, T_expList args);

static int F_argsNum = 6;
static F_frame frame;

// helper func
static Temp_tempList L(Temp_temp h, Temp_tempList t) {
    return Temp_TempList(h, t);
}

// emit instr without hard register
static AS_instrList iList = NULL, last = NULL;
static void emit(AS_instr inst) {
    if (last != NULL)
        last = last->tail = AS_InstrList(inst, NULL);
    else
        last = iList = AS_InstrList(inst, NULL);
}


AS_instrList F_codegen(F_frame f, T_stmList stmList) {
    DEBUG("this is codegen");
    AS_instrList list;
    T_stmList sl;

    frame = f;

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
    // view change
    F_accessList formals = f->formals;
    int stackcount = 0;
    for (int regcount=0; regcount<F_argsNum && formals; regcount++){
        Temp_temp hr;
        switch(regcount){
            case 0: hr = F_RDI(); break;
            case 1: hr = F_RSI(); break;
            case 2: hr = F_RDX(); break;
            case 3: hr = F_RCX(); break;
            case 4: hr = F_R8();  break;
            case 5: hr = F_R9();  break;
        }
        if (formals->head->kind == inFrame){
            //fix FP
            Temp_temp d = Temp_newtemp();
            emit(AS_Oper("movq `s0, `d0", L(d, NULL), L(F_RSP(), NULL), AS_Targets(NULL)));
            char * inst = checked_malloc(MAXLEN * sizeof(char));
            sprintf(inst, "addq $%sframesize, `d0", Temp_labelstring(F_name(frame)));
            emit(AS_Oper(inst, L(d, NULL), NULL, AS_Targets(NULL)));

            //use FP
            inst = checked_malloc(MAXLEN * sizeof(char));
            stackcount++;
            sprintf(inst, "movq `s0, %d(`d0)", - (stackcount) * F_wordSize);
            emit(AS_Oper(inst, L(d, NULL), L(hr, NULL), AS_Targets(NULL)));
        }else{
            emit(AS_Move("movq `s0, `d0", L(formals->head->u.reg, NULL), L(hr, NULL)));
        }
        formals = formals->tail;
    }
    /* miscellaneons initialization as necessary */
    for (sl=stmList; sl; sl=sl->tail)
        munchStm(sl->head);
    // restore
    calleeTmp = calleesaves;
    saveTail = saveHead;
    for(; calleeTmp; calleeTmp=calleeTmp->tail) {
        emit(AS_Move("movq `s0, `d0", L(calleeTmp->head, NULL), L(saveTail->head, NULL)));
        saveTail = saveTail->tail;
    }

    // make static return
    list = iList;
    iList = last = NULL;
    return list;
}

static void munchStm(T_stm s) {
    switch (s->kind) {
        case T_SEQ:
        {/*struct {T_stm left, right;} SEQ*/
            DEBUG("ERROR, T_SEQ impossible after canon\n");
            assert(0);
            //munchStm(s->u.SEQ.left);
            //munchStm(s->u.SEQ.right);
            return;
        }
        case T_LABEL:
        {/*Temp_label LABEL*/
            Temp_label label = s->u.LABEL;
            // note , label will follow a ':' auto, why?
            //char* inst = checked_malloc(MAXLEN * sizeof(char));
            //sprintf(inst, "%s", Temp_labelstring(label));
            emit(AS_Label(Temp_labelstring(label), label));
            //emit(AS_Label(inst, label));
            return;
        }
        case T_JUMP:
        {/*struct {T_exp exp; Temp_labelList jumps;} JUMP
            jmp L   exp point the target address, jumps for possible label */
            T_exp e = s->u.JUMP.exp;
            assert(e->kind == T_NAME);
            char* inst = checked_malloc(MAXLEN * sizeof(char));
            sprintf(inst, "jmp %s", Temp_labelstring(e->u.NAME));
            DEBUG("what is inst: %s\n", inst);
            emit(AS_Oper("jmp `j0", NULL, NULL, AS_Targets(s->u.JUMP.jumps)));
            // CHANGED
            return;
        }
        case T_CJUMP:
        {/*struct {T_relOp op; T_exp left, right; Temp_label true, false;} CJUMP*/
            char* inst = checked_malloc(MAXLEN * sizeof(char));
            char* jop;
            switch(s->u.CJUMP.op){
                case T_eq: jop = String("je");  break;
                case T_ne: jop = String("jne"); break;
                case T_lt: jop = String("jl");  break;
                case T_gt: jop = String("jg");  break;
                case T_le: jop = String("jle"); break;
                case T_ge: jop = String("jge"); break;
                case T_ult:jop = String("jb");  break;
                case T_ule:jop = String("jbe"); break;
                case T_ugt:jop = String("ja");  break;
                case T_uge:jop = String("jae"); break;
                default: assert(0);
            }
            Temp_temp left = munchExp(s->u.CJUMP.left);
            Temp_temp right= munchExp(s->u.CJUMP.right);
            Temp_label t =   s->u.CJUMP.true;
            // jmp to true label, fasle label should follow after canon
            sprintf(inst, "%s %s", jop, Temp_labelstring(t));
// TODO: src dst reg right?
// s0 means the first in src templist, need match
            emit(AS_Oper("cmp `s0, `s1", NULL, L(right, L(left, NULL)), AS_Targets(NULL)));
            emit(AS_Oper(inst, NULL, NULL, AS_Targets(Temp_LabelList(t, NULL))));
            return;
        }
        case T_MOVE:
        {/*struct {T_exp dst, src;} MOVE*/
            T_exp dst = s->u.MOVE.dst, src = s->u.MOVE.src;
            DEBUG("\tT_Move dst kind %d", dst->kind);
            if (dst->kind == T_TEMP) {
                DEBUG("\tT_Move dst Temp");
                emit(AS_Move("movq `s0, `d0", L(munchExp(dst), NULL), L(munchExp(src), NULL)));
                return;
            }
            if (dst->kind == T_MEM) {
                DEBUG("\tT_Move dst T_MEM");
                char* inst = checked_malloc(MAXLEN * sizeof(char));
                if (dst->u.MEM->kind == T_BINOP){
                    DEBUG(" into MOVE - MEM - BINOP");
                    assert(dst->u.MEM->u.BINOP.op == T_plus);
                    if (dst->u.MEM->u.BINOP.left->kind == T_CONST) {
                        T_exp e1 = dst->u.MEM->u.BINOP.right, e2 = src;
                        /*MOVE(MEM(+(CONST(i), e1)), e2)*/
                        sprintf(inst, "movq `s0, %d(`s1)", dst->u.MEM->u.BINOP.left->u.CONST);
// WHY: book here is AS_Oper
// this is not MOVE inst used in RA
                        emit(AS_Oper(inst, NULL, L(munchExp(e2), L(munchExp(e1), NULL)), AS_Targets(NULL)));
                        return;
                    }
                    else if (dst->u.MEM->u.BINOP.right->kind == T_CONST) {
                        T_exp e1 = dst->u.MEM->u.BINOP.left, e2 = src;
                        /*MOVE(MEM(+(e1, CONST(i))), e2)*/
                        sprintf(inst, "movq `s0, %d(`s1)", dst->u.MEM->u.BINOP.right->u.CONST);
                        emit(AS_Oper(inst, NULL, L(munchExp(e2), L(munchExp(e1), NULL)), AS_Targets(NULL)));
                        return;
                    }
                    else {
                        emit(AS_Oper("movq `s0, (`s1)", NULL, L(munchExp(src), L(munchExp(dst->u.MEM), NULL)), AS_Targets(NULL)));
                        return;
                    }
                }
                else {
                    emit(AS_Oper("movq `s0, (`s1)", NULL, L(munchExp(src), L(munchExp(dst->u.MEM), NULL)), AS_Targets(NULL)));
                    return;
                }
            }
            DEBUG("something wrong\n");
            assert(0);
            return;
        }
        case T_EXP:
        {/*T_exp EXP*/
            munchExp(s->u.EXP);
            return;
        }
    }
}

static Temp_temp munchExp(T_exp e) {
    DEBUG("this is munchExp %d", e->kind);
    Temp_temp r = Temp_newtemp();
    switch(e->kind) {
        case T_BINOP:
        {/*struct {T_binOp op; T_exp left, right;} BINOP*/
            DEBUG("0 FOR BINOP munch left and right");
            Temp_temp left = munchExp(e->u.BINOP.left);
            Temp_temp right= munchExp(e->u.BINOP.right);
            T_binOp op = e->u.BINOP.op;
            string opInst;
            switch(op) {
                case T_plus: 
                    DEBUG("BINOP PLUS");
                    opInst = "addq `s0, `d0"; break;
                case T_minus:opInst = "subq `s0, `d0"; break;
                case T_mul:  opInst = "imul `s0, `d0"; break;
                case T_div:
                {// X86-64 div instr 
                 // cqto: convert quad to octal word
                    emit(AS_Move("movq `s0, `d0", L(F_RAX(), NULL), L(left, NULL)));
                    emit(AS_Oper("cqto", L(F_RDX(), L(F_RAX(), NULL)), L(F_RAX(), NULL), NULL));
                    emit(AS_Oper("idiv `s0", L(F_RAX(), L(F_RAX(), NULL)), L(right, NULL), NULL));
                    emit(AS_Move("movq `s0, `d0", L(r, NULL), L(F_RAX(), NULL)));
                    return r;
                }
                case T_and: case T_or: case T_lshift:
                case T_rshift: case T_arshift: case T_xor:
                    DEBUG("ERROR, unsuported BINOP op for tiger\n");
                    assert(0);
            }
            emit(AS_Move("movq `s0, `d0", L(r, NULL), L(left, NULL)));
            emit(AS_Oper(opInst, L(r, NULL), L(right, L(r, NULL)), NULL));
            return r;
        }
        case T_MEM:
        {/* T_exp MEM; */
            DEBUG("1 FOR MEM");
            Temp_temp mem = munchExp(e->u.MEM);
            //emit(AS_Oper("movq (`s0), `d0", L(r, NULL), L(mem, NULL), NULL));
            // use SP + K + FS replace FP + K
            if (e->u.MEM->kind == T_BINOP && e->u.MEM->u.BINOP.right->u.TEMP == F_FP()){
                char * inst = checked_malloc(MAXLEN * sizeof(char));
                sprintf(inst, "movq %sframesize + %d(`s0), `d0", Temp_labelstring(F_name(frame)), e->u.MEM->u.BINOP.left->u.CONST);
                emit(AS_Oper(inst, L(r, NULL), L(F_RSP(), NULL), AS_Targets(NULL)));
            }else if (e->u.MEM->kind == T_BINOP && e->u.MEM->u.BINOP.left->kind == T_CONST){
                char * inst = checked_malloc(MAXLEN * sizeof(char));
                sprintf(inst, "movq %d(`s0), `d0", e->u.MEM->u.BINOP.left->u.CONST);
                emit(AS_Oper(inst, L(r, NULL), L(munchExp(e->u.MEM->u.BINOP.right), NULL), AS_Targets(NULL)));
            }
            else
                emit(AS_Oper("movq (`s0), `d0", L(r, NULL), L(munchExp(e->u.MEM), NULL), AS_Targets(NULL)));
            
            return r;
        }
        case T_TEMP:
        {/* Temp_temp TEMP */
// TODO: FP
            DEBUG("2 FOR TEMP");
            //DEBUG("2 FOR TEMP %s", Temp_look(F_tempmap, e->u.TEMP));
            
            if (e->u.TEMP == F_FP()){
                emit(AS_Oper("movq `s0, `d0", L(r, NULL), L(F_RSP(), NULL), AS_Targets(NULL)));
                char * inst = checked_malloc(MAXLEN * sizeof(char));
                sprintf(inst, "addq $%sframesize, `d0", Temp_labelstring(F_name(frame)));
                //sprintf(inst, "addq $%d, `d0", frame->local_cnt * F_wordSize);
                emit(AS_Oper(inst, L(r, NULL), NULL, AS_Targets(NULL)));
                return r;
            }
            else 
                return e->u.TEMP;
        }
        case T_ESEQ:
        {/* struct {T_stm stm; T_exp exp;} ESEQ*/
            DEBUG("ERROR: T_ESEQ impossible after canon\n");
            assert(0);
        }
        case T_NAME:
        {/*Temp_label NAME*/
            DEBUG("4 FOR NAME");
            char* inst = checked_malloc(MAXLEN * sizeof(char));
            sprintf(inst, "movq $%s, `d0", Temp_labelstring(e->u.NAME));
            emit(AS_Oper(inst, L(r, NULL), NULL, AS_Targets(NULL)));
            return r;
        }
        case T_CONST:
        {/* int CONST */
            DEBUG("5 FOR CONST %d", e->u.CONST);
            char *inst = checked_malloc(MAXLEN * sizeof(char));
            sprintf(inst, "movq $%d, `d0", e->u.CONST);
            emit(AS_Oper(inst, L(r, NULL), NULL, AS_Targets(NULL)));
            return r;
        }
        case T_CALL:
        {/*struct {T_exp fun; T_expList->r args;} CALL
            args: rdi->rsi->rdx->rcx->r8->r9->push
         */
            DEBUG("6 FOR CALL");
            assert(e->u.CALL.fun->kind == T_NAME);

            Temp_label func = e->u.CALL.fun->u.NAME;
            T_expList args = e->u.CALL.args;

            DEBUG("func %s codegen", Temp_labelstring(func));
            Temp_tempList l = munchArgs(0, args);
            DEBUG("munch args finish");

            // calldefs defed for liveness (CH P151)
            Temp_tempList calldefs = L(F_RAX(), callersaves);
            
            char *inst = checked_malloc(MAXLEN * sizeof(char));
            sprintf(inst, "CALL %s", Temp_labelstring(func));
            emit(AS_Oper(inst, calldefs, l, NULL));

            // TODO pop?

            emit(AS_Oper("movq `s0, `d0", L(r, NULL), L(F_RAX(), NULL), NULL));
            return r;
        }
        default:
            assert(0);
    }
}

static Temp_tempList munchArgs(int i, T_expList args) {
    DEBUG("munchArgs count %d", i);
    // return call use registers
    if (!args){
        DEBUG("count %d return null", i);
        return NULL;
    }

    if (i<F_argsNum) {
        Temp_temp dst = F_ARGUREGS(i);
        Temp_tempList ret = L(dst, munchArgs(i+1, args->tail));
        DEBUG("--==munch exp for args %d==--", i);
        Temp_temp src = munchExp(args->head);
        DEBUG("--==munch exp for args %d OK==--", i);
        emit(AS_Move("movq `s0, `d0", L(dst, NULL), L(src,NULL)));
        DEBUG("count %d return reg", i);
        return ret;
    }
    else {
        munchArgs(i+1, args->tail);
        emit(AS_Oper("pushq `s0", NULL, L(munchExp(args->head), NULL), NULL));
        DEBUG("count %d return push", i);
        return NULL;
    }

}
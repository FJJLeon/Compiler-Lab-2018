#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

#include "printtree.h"
#include "prabsyn.h"
/*Lab4: Your implementation of lab4*/
#define SEMdebug 0

#define debugout(format, ...) do { \
    if (SEMdebug)  \
        fprintf(stdout, "[%s@%s,%d]\t" format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)

// typedef void* Tr_exp; // no use in lab4
struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;
	e.exp = exp;
	e.ty = ty;
	return e;
}

Ty_ty actual_ty(Ty_ty ty)
{
	while (ty && ty->kind == Ty_name) // skip Ty_Name
		ty = ty->u.name.ty;
	return ty;
}

F_fragList SEM_transProg(A_exp exp)
{
	debugout("---=== IN SEM tree===---");
	//pr_exp(stdout, exp, 0);
	/*
	//debugout("SEM_transProg\n");
	// create main level
	Temp_label main_label = Temp_newlabel();
	Tr_level main_level = Tr_newLevel(Tr_outermost(), main_label, NULL);
	E_enventry fun_entry = E_FunEntry(main_level, main_label, NULL, Ty_Void()); 
	// translate body
	struct expty body_expty = transExp(E_base_venv(), E_base_tenv(), exp, main_level, NULL);
	// end body 
	//Tr_procEntryExit(fun_entry->u.fun.level, body_expty.exp, NULL);
	Tr_Func(body_expty.exp,fun_entry->u.fun.level );
	return Tr_getResult();
	*/
	//Tr_level main = Tr_outermost();
	//Tr_initFrag();
	Temp_label main_label = Temp_namedlabel("tigermain");
	Tr_level main_level = Tr_newLevel(Tr_outermost(), main_label, NULL);
    struct expty body_expty = transExp(E_base_venv(), E_base_tenv(), exp, main_level, NULL);
    
	//Tr_Func(body_expty.exp, main_level);
    Tr_procEntryExit(main_level, body_expty.exp, NULL);
	F_fragList tmp = Tr_getResult();
	debugout(" frag kind %d", tmp->head->kind);
	debugout("----====SEM STM BODY===---");
	//printStmList(stdout, T_StmList(tmp->head->u.proc.body, NULL));
	debugout("----====SEM STM BODY===---");
	return Tr_getResult();
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level clevel, Temp_label breakflag)
{
	switch(a->kind) {
		case (A_varExp):
			return transVar(venv, tenv, a->u.var, clevel, breakflag);
		case (A_nilExp):
			return expTy(Tr_Nil(), Ty_Nil());
		case (A_intExp):
			debugout("!!!!!!!what in intExp %d", a->u.intt);
			return expTy(Tr_Int(a->u.intt), Ty_Int(notloop));
		case (A_stringExp):
			return expTy(Tr_String(a->u.stringg), Ty_String());
		case (A_callExp):
		{
			// check function symbol
			E_enventry x = S_look(venv, a->u.call.func);
			debugout("call func name: %s\n", S_name(a->u.call.func));
			debugout("call func name: %s in entry", Temp_labelstring(x->u.fun.label));
			if (!x) {
				debugout("x is null\n");
			}
			else {
				debugout("x kind %d\n", x->kind);
			}
				
			if (!x || x->kind != E_funEntry) {
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
				debugout("undefined func %s", S_name(a->u.call.func));
                return expTy(Tr_Nop(), Ty_Int(0));
			}
			// check input paras with expect
			A_expList input = a->u.call.args;
			Ty_tyList expect = x->u.fun.formals;
			struct expty res;
			// build para list for Tr_Call 
			Tr_expList checked = NULL, tail = NULL;
			for (input && expect; input && expect; input = input->tail, expect = expect->tail) {
				res = transExp(venv, tenv, input->head, clevel, NULL);
				if (actual_ty(expect->head)->kind != actual_ty(res.ty)->kind)
					EM_error(input->head->pos, "para type mismatch");
				debugout(" checked para for Tr_call");
				if (res.exp->u.ex->u.CONST)
					debugout("const %d", res.exp->u.ex->u.CONST);

				if (checked) {
					tail->tail = Tr_ExpList(res.exp, NULL);
					tail = tail->tail;
				}
				else {
					checked = Tr_ExpList(res.exp, NULL);
					tail = checked;
				}
				// checked = Tr_ExpList(res.exp, checked);
				
			}
			if (input)
				EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));
			if (expect)
				EM_error(a->pos, "too few params in function %s", S_name(a->u.call.func));
			// diff from
			Tr_exp res_exp = Tr_Call(x->u.fun.label, checked, clevel, x->u.fun.level);
			//Tr_exp res_exp = Tr_Call(x->u.fun.level, x->u.fun.label, checked, clevel);
			return expTy(res_exp, x->u.fun.result);
		}
		case (A_opExp):
		{
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, a->u.op.left, clevel, breakflag);
			struct expty right = transExp(venv, tenv, a->u.op.right, clevel, breakflag);
			// check type
			if (oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp) {
				// arithmetic operation, integer required
				debugout("what right ty %d\n", right.ty->kind);
				if (left.ty->kind != Ty_int) {
					EM_error(a->u.op.left->pos, "integer required");
					return expTy(NULL, Ty_Int(notloop));
				}	
				if (right.ty->kind != Ty_int) {
					EM_error(a->u.op.right->pos, "integer required");
					return expTy(NULL, Ty_Int(notloop));
				}
				// diff from
				return expTy(Tr_OpArithmetic(oper, left.exp, right.exp), Ty_Int(notloop));
			}
			else if (oper == A_eqOp || oper == A_neqOp) { // string equal should external call
				Tr_exp res;
				if (left.ty->kind == Ty_string)
					res = Tr_opStrCmp(oper, left.exp, right.exp);
				else 
					res = Tr_OpCmp(oper, left.exp, right.exp);

				return expTy(res, Ty_Int(0));
			}
			else { // other operation, just same
				debugout("opExp kind %d, %d\n", actual_ty(left.ty)->kind, actual_ty(right.ty)->kind);
				if (actual_ty(left.ty)->kind != actual_ty(right.ty)->kind
					&& actual_ty(left.ty)->kind != Ty_nil
					&& actual_ty(right.ty)->kind != Ty_nil)
					EM_error(a->pos, "same type required");
				Tr_exp tr_exp = Tr_OpCmp(oper, left.exp, right.exp);
				return expTy(tr_exp, Ty_Int(notloop));
			}
			assert(0);
		}
		case (A_recordExp):
		{
			debugout("recordExp type %s", S_name(a->u.record.typ));
			// check record symbol
			Ty_ty ty = actual_ty(S_look(tenv, a->u.record.typ));
			if (!ty) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
				return expTy(NULL, Ty_Int(notloop));
			}
			if (ty->kind != Ty_record) {
				EM_error(a->pos, "not record type %s", S_name(a->u.record.typ));
				return expTy(NULL, ty);
			}
			// check field
			A_efieldList input = a->u.record.fields;
			Ty_fieldList expect = ty->u.record;
			struct expty res;
			// build field list
			Tr_expList fields = NULL;
			int num = 0;
			for (input && expect; input && expect; input=input->tail, expect=expect->tail) {
				if (input->head->name != expect->head->name)
					EM_error(a->pos, "record field mismatch");
				res = transExp(venv, tenv, input->head->exp, clevel, breakflag);
				debugout("record kind %d, %d\n", actual_ty(res.ty)->kind, actual_ty(expect->head->ty)->kind);
				if (actual_ty(res.ty)->kind != actual_ty(expect->head->ty)->kind
					&& actual_ty(res.ty)->kind != Ty_nil) // record field can be Ty_nil
					EM_error(a->pos, "record type mismatch");
				debugout("record get const value %d", res.exp->u.ex->u.CONST);
				num++;
				fields = Tr_ExpList(res.exp, fields);
			}
			if (input || expect)
				EM_error(a->pos, "record number mismatch");
			Tr_exp res_exp = Tr_Record(num, fields);
			//debugout("recordExp");
			return expTy(res_exp, ty);
		}
		case (A_seqExp):
		{/* exp; exp; exp*/
			A_expList cur_exp = a->u.seq;
			// if ()
			Ty_ty res_ty = Ty_Void();
			Tr_exp res_tr = NULL;
			struct expty tmp;
			while(cur_exp) {
				tmp = transExp(venv, tenv, cur_exp->head, clevel, breakflag);
				// set to the last exp
				res_ty = tmp.ty;
				
				if (res_tr)
					res_tr = Tr_Seq(res_tr, tmp.exp);
				else 
					res_tr = tmp.exp;

				cur_exp = cur_exp->tail;
			}
			return expTy(res_tr, res_ty);
		}
		case (A_assignExp):
		{
			debugout("A_assignExp");
			struct expty lvalue = transVar(venv, tenv, a->u.assign.var, clevel, breakflag);
			struct expty right = transExp(venv, tenv, a->u.assign.exp, clevel, breakflag);
			// check loop variable assigned
			if (lvalue.ty->kind == Ty_int && lvalue.ty->u.loop == isloop) {
                    EM_error(a->pos, "loop variable can't be assigned");
            }
			// check type
			if (lvalue.ty->kind != right.ty->kind)
				EM_error(a->pos, "unmatched assign exp");
			
			return expTy(Tr_Assign(lvalue.exp, right.exp), Ty_Void());
		}
		case (A_ifExp):
		{
			// check test(int)
			struct expty test = transExp(venv, tenv, a->u.iff.test, clevel, breakflag);
			if (test.ty->kind != Ty_int) {
				debugout("\t\t\ttype of test exp not int");
				EM_error(a->pos, "type of test exp not int");
			}
			// check then and else
			struct expty then = transExp(venv, tenv, a->u.iff.then, clevel, breakflag);
			if (a->u.iff.elsee) {
				debugout("ifExp else kind %d, then ty kind %d\n", a->u.iff.elsee->kind, then.ty->kind);
				struct expty elsee = transExp(venv, tenv, a->u.iff.elsee, clevel, breakflag);
				if (then.ty->kind != elsee.ty->kind && elsee.ty->kind != Ty_nil){
					debugout("\t\t\t then exp and else exp type mismatch");
					EM_error(a->pos, "then exp and else exp type mismatch");
				}
					
				return expTy(Tr_IfThenElse(test.exp, then.exp, elsee.exp), then.ty);
			}
			else {
				debugout("ifExp then kind %d\n", then.ty->kind);
				struct expty elsee = expTy(NULL, Ty_Void());
				if (actual_ty(then.ty)->kind != Ty_void || actual_ty(elsee.ty)->kind != Ty_void){
					debugout("\t\t\tif-then exp's body must produce no value");
					EM_error(a->pos, "if-then exp's body must produce no value");
				}

				return expTy(Tr_IfThen(test.exp, then.exp), Ty_Void());
			}
			assert(0);
		}
		case (A_whileExp):
		{
			// new break
			Temp_label done = Temp_newlabel();
			// check test(int) and body(no value)
			struct expty test = transExp(venv, tenv, a->u.whilee.test, clevel, breakflag);
			struct expty body = transExp(venv, tenv, a->u.whilee.body, clevel, done);
			if (test.ty->kind != Ty_int)
				EM_error(a->pos, "type of test exp not int");
			if (body.ty->kind != Ty_void)
				EM_error(a->pos, "while body must produce no value");

			return expTy(Tr_While(test.exp, body.exp, done), Ty_Void());
		}
		case (A_forExp):
		{
			// new break
			Temp_label done = Temp_newlabel();
			// check lo and hi (int)
			struct expty lo = transExp(venv, tenv, a->u.forr.lo, clevel, breakflag);
			struct expty hi = transExp(venv, tenv, a->u.forr.hi, clevel, breakflag);
			if (lo.ty->kind != Ty_int)
				EM_error(a->u.forr.lo->pos, "for exp's range type is not integer");
			if (hi.ty->kind != Ty_int)
				EM_error(a->u.forr.hi->pos, "for exp's range type is not integer");
			// check body (no value)
			S_beginScope(venv);
			Tr_access access = Tr_allocLocal(clevel, a->u.forr.escape);
			S_enter(venv, a->u.forr.var, E_VarEntry(access, Ty_Int(isloop)));
			struct expty body = transExp(venv, tenv, a->u.forr.body, clevel, done);
			if (body.ty->kind != Ty_void)
				EM_error(a->u.forr.body->pos, "for body must produce no value");
			S_endScope(venv);
			return expTy(Tr_For(access, clevel, lo.exp, hi.exp, body.exp, done), Ty_Void());
		}
		case (A_breakExp):
		{
			if (breakflag == NULL) {
				EM_error(a->pos, "break is not inside a loop");
				return expTy(Tr_Nop(), Ty_Void());
			}
			// diff from 
			return expTy(Tr_Break(breakflag), Ty_Void());
			//return expTy(Tr_Jump(breakflag), Ty_Void());
		}
		case (A_letExp):
		{
			struct expty exp;
			A_decList d;
			S_beginScope(venv);
			S_beginScope(tenv);
			Tr_exp dec;
			Tr_expList decL = NULL;
			for (d = a->u.let.decs; d; d = d->tail) {
				// record dec IR
				dec = transDec(venv, tenv, d->head, clevel, breakflag);
				decL = Tr_ExpList(dec, decL);
			}
			exp = transExp(venv, tenv, a->u.let.body, clevel, breakflag);

			Tr_exp res = exp.exp;
			for (; decL; decL=decL->tail) {
				res = Tr_Seq(decL->head, res);
			}

			S_endScope(tenv);
			S_endScope(venv);
			return expTy(res, exp.ty);
		}
		case (A_arrayExp):
		{
			// check array symbol
			Ty_ty ty = actual_ty(S_look(tenv, a->u.array.typ));
			if (!ty) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.array.typ));
				return expTy(Tr_Nop(), Ty_Int(notloop));
			}
			if (ty->kind != Ty_array) {
				EM_error(a->pos, "not array type %s", S_name(a->u.array.typ));
				return expTy(Tr_Nop(), ty);
			}
			// check array init
			struct expty size = transExp(venv, tenv, a->u.array.size, clevel, breakflag);
			if (size.ty->kind != Ty_int)
				EM_error(a->u.array.size->pos, "type of array size not int");
			struct expty init = transExp(venv, tenv, a->u.array.init, clevel, breakflag);
			debugout("kind %d, %d\n", actual_ty(init.ty)->kind, actual_ty(ty->u.array)->kind);
			if (actual_ty(init.ty)->kind != actual_ty(ty->u.array)->kind)
				EM_error(a->u.array.init->pos, "type mismatch");
			
			return expTy(Tr_Array(size.exp, init.exp), ty);
		}
		default:
        	assert(0);
	}
} 

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level clevel, Temp_label breakflag)
{
	switch (v->kind) {
		case (A_simpleVar):
		{
			debugout("simpleVar %s", S_name(v->u.simple));
			E_enventry x = S_look(venv, v->u.simple);
			if (x && x->kind == E_varEntry)
				return expTy(Tr_simpleVar(x->u.var.access, clevel), actual_ty(x->u.var.ty));
			else {
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(Tr_Nop(), Ty_Int(notloop));
			}
		}
		case (A_fieldVar):
		{
			// check record field: lvalue . id
			struct expty lvalue = transVar(venv, tenv, v->u.field.var, clevel, breakflag);
			if (actual_ty(lvalue.ty)->kind != Ty_record) {
				EM_error(v->u.field.var->pos, "not a record type");
				return expTy(Tr_Nop(), Ty_Int(notloop));
			}
			// check sym exist
			S_symbol sym = v->u.field.sym;
			debugout("sym: %s\n", S_name(sym));
			int order = 0;
			debugout("fieldVar stop05\n");
			if (!actual_ty(lvalue.ty)->u.record)
				assert(0);
			debugout("fieldVar stop08\n");
			for (Ty_fieldList field = lvalue.ty->u.record; field; field = field->tail) {
				if (!field->head->name)
					assert(0);
				debugout("in fieldVar, field name: %s\n", S_name(field->head->name));
				if (field->head->name == sym)
					return expTy(Tr_fieldVar(lvalue.exp, order), field->head->ty);
				order++;
			}
			debugout("fieldVar stop11\n");
			EM_error(v->u.field.var->pos, "field %s doesn't exist", S_name(sym));
            return expTy(Tr_Nop(), Ty_Int(notloop));
		}
		case (A_subscriptVar):
		{
			// check array subscript: lvalue [ exp ]
			struct expty lvalue = transVar(venv, tenv, v->u.subscript.var, clevel, breakflag);
			if (actual_ty(lvalue.ty)->kind != Ty_array) {
				EM_error(v->u.subscript.var->pos, "array type required");
				return expTy(Tr_Nop(), Ty_Int(notloop));
			}
			// check index
			struct expty index = transExp(venv, tenv, v->u.subscript.exp, clevel, breakflag);
			if (index.ty->kind != Ty_int) {
				EM_error(v->pos, "type of index not int");
				return expTy(Tr_Nop(), Ty_Int(notloop));
			}

			return expTy(Tr_subscriptVar(lvalue.exp, index.exp), lvalue.ty->u.array);
		}
		default:
        	assert(0);
	}
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList params) {
	if (params == NULL) {
		return NULL;
	}
	else {
		Ty_ty ty = S_look(tenv, params->head->typ);
		if (!ty) {
			EM_error(params->head->pos, "undefined type %s", S_name(params->head->typ));
			ty = Ty_Int(notloop);
		}
		return Ty_TyList(ty, makeFormalTyList(tenv, params->tail));
	}
}

U_boolList makeFormalEscList(A_fieldList params) {
    if (params == NULL) {
        return NULL;
    }
	else {
		return U_BoolList(params->head->escape, makeFormalEscList(params->tail));
	}
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level clevel, Temp_label breakflag)
{
	if (!clevel) {
		debugout("saonima\n");
		assert(0);
	}
	switch (d->kind) {
		case (A_functionDec):
		{
			debugout("A_functionDec %s", S_name(d->u.function->head->name));
			A_fundecList funcL;
			// put function head into venv first
			for (funcL=d->u.function; funcL; funcL=funcL->tail) {
				// check function name 
				A_fundec func = funcL->head;
				//if (S_look(venv, func->name) != NULL) 
				//	EM_error(func->pos, "two functions have the same name");
				// check params
				Ty_tyList formal_tys = makeFormalTyList(tenv, func->params);
				// check result
				Ty_ty resultTy;
				if (func->result) {
					resultTy = S_look(tenv, func->result);
					debugout("what resyltTy %d\n", resultTy->kind);
					if (!resultTy) {
						EM_error(func->pos, "undefined result type %s", S_name(func->result));
						resultTy = Ty_Void();
					}
				}
				else {
					resultTy = Ty_Void();
				}
				// create new level
				Temp_label func_label = Temp_newlabel();
				U_boolList formal_escapes = makeFormalEscList(func->params);
				Tr_level new_level = Tr_newLevel(clevel, func_label, formal_escapes);
				// add dec
				debugout("func name %s enter venv\n", S_name(func->name));
				E_enventry funcEntry = E_FunEntry(new_level, func_label, formal_tys, resultTy);
				S_enter(venv, func->name, funcEntry);
			}
			//debugout("funcDecstop5\n");
			// deal function body 
			for (funcL=d->u.function; funcL; funcL=funcL->tail) {
				A_fundec func = funcL->head;
				S_beginScope(venv);
				E_enventry funcEntry = S_look(venv, func->name);
				if (!funcEntry)
					assert(0);
				debugout("funcDecstop51, name:%s\n", S_name(func->name));
				// get formal accesses
				Tr_accessList formal_accesses = Tr_formals(funcEntry->u.fun.level);
				//if (!formal_accesses)
				//	assert(0);
				Ty_tyList formalTys = funcEntry->u.fun.formals;
				A_fieldList fields = func->params;
				//debugout("funcDecstop53\n");
				// put function args into env
				while (fields) {
					//Ty_ty type = S_look(tenv, fields->head->typ);
					debugout("funcDecstop531\n");
					E_enventry varEntry = E_VarEntry(formal_accesses->head, formalTys->head);
					//debugout("funcDecstop532\n");
					S_enter(venv, fields->head->name, varEntry);
					//debugout("funcDecstop533\n");
					fields = fields->tail;
                    formal_accesses = formal_accesses->tail;
				}
				
				//debugout("funcDecstop55\n");
				struct expty res = transExp(venv, tenv, func->body, funcEntry->u.fun.level, breakflag);
				Ty_ty resultTy = funcEntry->u.fun.result;
				debugout("funcDec kind %d, %d\n", res.ty->kind, resultTy->kind);
				if (res.ty->kind != actual_ty(resultTy)->kind) { // mismatch
					if (resultTy->kind != Ty_void)  // and not procedure
						EM_error(func->pos, "function result type mismatch");
					else // but is procedure
						EM_error(func->pos, "procedure returns value");
				}
				Tr_procEntryExit(funcEntry->u.fun.level, res.exp, formal_accesses);
				S_endScope(venv);
				debugout("funcDecstop8\n");
			}
			
			return Tr_Nop();
		}
		case (A_varDec):
		{
			debugout("varDec %s", S_name(d->u.var.var));
			struct expty init = transExp(venv, tenv, d->u.var.init, clevel, breakflag);
			debugout("varDec %s init out", S_name(d->u.var.var));
			Tr_access access = Tr_allocLocal(clevel, d->u.var.escape);
			if (d->u.var.typ) { // type limit
				Ty_ty ty = S_look(tenv, d->u.var.typ);
				if (!ty)
					EM_error(d->u.var.init->pos, "type not exist");
				debugout("varDec %d, %d\n",actual_ty(init.ty)->kind, actual_ty(ty)->kind);
				if (actual_ty(init.ty)->kind != actual_ty(ty)->kind &&
					actual_ty(init.ty)->kind != Ty_nil &&
					actual_ty(ty)->kind != Ty_nil)
					EM_error(d->u.var.init->pos, "type mismatch");
			}
			else if (actual_ty(init.ty)->kind == Ty_nil) {
				EM_error(d->pos, "init should not be nil without type specified");
			}
			S_enter(venv, d->u.var.var, E_VarEntry(access, init.ty));
			// ?
			return Tr_Assign(Tr_simpleVar(access, clevel), init.exp);
		}
		case (A_typeDec):
		{
			A_nametyList nametyL;
			// put type head into tenv first
			for (nametyL = d->u.type; nametyL; nametyL=nametyL->tail) {
				A_namety namety = nametyL->head;
				debugout("typeDec name %s\n", S_name(namety->name));
				//if (S_look(tenv, namety->name) != NULL) // type name exist, no error?
				//	EM_error(d->pos, "two types have the same name");
				//else
					S_enter(tenv, namety->name, Ty_Name(namety->name, NULL));
			}
			// deal type body
			for (nametyL = d->u.type; nametyL; nametyL=nametyL->tail) {
				A_namety namety = nametyL->head;

			//	Ty_ty ty = S_look(tenv, namety->name);
				// seems wrong
				Ty_ty tmp = transTy(tenv, namety->ty);
				switch(namety->ty->kind) {
                        case A_nameTy:
                            {
                                Ty_ty ty = S_look(tenv, namety->name);
								if (tmp->kind != Ty_name)
									assert(0);
								ty->kind = tmp->kind;
                                ty->u.name.sym = tmp->u.name.sym;
								ty->u.name.ty = tmp->u.name.ty;
                                break;
                            }
                        case A_recordTy:
                            {
                                Ty_ty ty = S_look(tenv, namety->name);
								if (tmp->kind != Ty_record)
									assert(0);
                                ty->kind = Ty_record;
                                ty->u.record = tmp->u.record;
                                break;
                            }
                        case A_arrayTy:
                            {
                                Ty_ty ty = S_look(tenv, namety->name);
								if (tmp->kind != Ty_array)
									assert(0);
                                ty->kind = Ty_array;
                                ty->u.array = tmp->u.array;
                                break;
                            }
                    }
			}
			// illegal cycle 
			for (nametyL = d->u.type; nametyL; nametyL=nametyL->tail) {
				A_namety namety = nametyL->head;
				Ty_ty begin = S_look(tenv, namety->name);
				Ty_ty tmp = begin;
				while (tmp->kind == Ty_name) {
					// now the Recursive definition can be used
					tmp = tmp->u.name.ty;
					if (tmp == begin) {
						EM_error(d->pos, "illegal type cycle");
						begin->u.name.ty = Ty_Int(notloop);
						break;
					}
				}
			}
			return Tr_Nop();
			break;
		}
		default:
        	assert(0);
	}
}

Ty_fieldList makeRecordTyList(S_table tenv, A_fieldList fields)
{
	if (fields == NULL)
		return NULL;
	Ty_ty ty = S_look(tenv, fields->head->typ);
	if (!ty) {
		EM_error(fields->head->pos, "undefined type %s", S_name(fields->head->typ));
		ty = Ty_Int(notloop);
	}
	Ty_field field = Ty_Field(fields->head->name, ty); 
	return Ty_FieldList(field, makeRecordTyList(tenv, fields->tail));
}	

Ty_ty transTy (S_table tenv, A_ty a)
{
	switch (a->kind) {
		case (A_nameTy):
		{
			Ty_ty ty = S_look(tenv, a->u.name);
			if (!ty) {
				EM_error(a->pos, "undefined type");
				return Ty_Int(notloop);
			}
			return Ty_Name(a->u.name, ty);
		}
		case (A_recordTy):
		{
			return Ty_Record(makeRecordTyList(tenv, a->u.record));
		}
		case (A_arrayTy):
		{
			Ty_ty ty = S_look(tenv, a->u.array);
			if (!ty) {
				EM_error(a->pos, "undefined type");
				return Ty_Int(notloop);
			}
			return Ty_Array(ty);
		}
	}
}

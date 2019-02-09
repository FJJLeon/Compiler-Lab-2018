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

/*Lab4: Your implementation of lab4*/
#define debug 0

typedef void* Tr_exp; // no use in lab4
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

void SEM_transProg(A_exp exp)
{
	transExp(E_base_venv(), E_base_tenv(), exp);
}

struct expty transExp(S_table venv, S_table tenv, A_exp a)
{
	switch(a->kind) {
		case (A_varExp):
			return transVar(venv, tenv, a->u.var);
		case (A_nilExp):
			return expTy(NULL, Ty_Nil());
		case (A_intExp):
			return expTy(NULL, Ty_Int(notloop));
		case (A_stringExp):
			return expTy(NULL, Ty_String());
		case (A_callExp):
		{
			// check function symbol
			E_enventry x = S_look(venv, a->u.call.func);
			if (x && x->kind == E_funEntry) {
				A_expList input = a->u.call.args;
				Ty_tyList expect = x->u.fun.formals;
				// chech para
				for (input && expect; input && expect; input = input->tail, expect = expect->tail) {
					struct expty res = transExp(venv, tenv, input->head);
					if (actual_ty(expect->head)->kind != actual_ty(res.ty)->kind)
						EM_error(input->head->pos, "para type mismatch");
				}
				if (input)
					EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));
				if (expect)
					EM_error(a->pos, "too few params");

				return expTy(NULL, x->u.fun.result);
			}
			else {
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
				return expTy(NULL, Ty_Int(notloop));
			}
		}
		case (A_opExp):
		{
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, a->u.op.left);
			struct expty right = transExp(venv, tenv, a->u.op.right);
			// check type
			if (oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp) {
				if (debug)
					printf("what right ty %d\n", right.ty->kind);
				if (left.ty->kind != Ty_int)
					EM_error(a->u.op.left->pos, "integer required");
				if (right.ty->kind != Ty_int)
					EM_error(a->u.op.right->pos, "integer required");
			}
			else {
				if (debug)
					printf("opExp kind %d, %d\n", actual_ty(left.ty)->kind, actual_ty(right.ty)->kind);
				if (actual_ty(left.ty)->kind != actual_ty(right.ty)->kind
					&& actual_ty(left.ty)->kind != Ty_nil
					&& actual_ty(right.ty)->kind != Ty_nil)
					EM_error(a->pos, "same type required");
			}
			return expTy(NULL, Ty_Int(notloop));
		}
		case (A_recordExp):
		{
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
			for (input && expect; input && expect; input=input->tail, expect=expect->tail) {
				if (input->head->name != expect->head->name)
					EM_error(a->pos, "record field mismatch");
				struct expty res = transExp(venv, tenv, input->head->exp);
				if (debug)
					printf("record kind %d, %d\n", actual_ty(res.ty)->kind, actual_ty(expect->head->ty)->kind);
				if (actual_ty(res.ty)->kind != actual_ty(expect->head->ty)->kind
					&& actual_ty(res.ty)->kind != Ty_nil) // record field can be Ty_nil
					EM_error(a->pos, "record type mismatch");
			}
			if (input || expect)
				EM_error(a->pos, "record number mismatch");

			return expTy(NULL, ty);
		}
		case (A_seqExp):
		{
			A_expList cur_exp = a->u.seq;
			struct expty res = expTy(NULL, Ty_Void());
			while(cur_exp) {
				res = transExp(venv, tenv, cur_exp->head);
				cur_exp = cur_exp->tail;
			}
			return res;
		}
		case (A_assignExp):
		{
			struct expty lvalue = transVar(venv, tenv, a->u.assign.var);
			struct expty right = transExp(venv, tenv, a->u.assign.exp);
			// check loop variable assigned
			if (lvalue.ty->kind == Ty_int && lvalue.ty->u.loop == isloop) {
                    EM_error(a->pos, "loop variable can't be assigned");
                }
			// check type
			if (lvalue.ty->kind != right.ty->kind)
				EM_error(a->pos, "unmatched assign exp");
			
			return expTy(NULL, Ty_Void());
		}
		case (A_ifExp):
		{
			// check test(int)
			struct expty test = transExp(venv, tenv, a->u.iff.test);
			if (test.ty->kind != Ty_int)
				EM_error(a->pos, "type of test exp not int");
			// check then and else
			struct expty then = transExp(venv, tenv, a->u.iff.then);
			if (debug)
				printf("ifExp else kind %d, then ty kind %d\n", a->u.iff.elsee->kind, then.ty->kind);
			if (a->u.iff.elsee) {
				struct expty elsee = transExp(venv, tenv, a->u.iff.elsee);
				if (then.ty->kind != elsee.ty->kind && elsee.ty->kind != Ty_nil)
					EM_error(a->pos, "then exp and else exp type mismatch");
				return expTy(NULL, then.ty);
			}
			else {
				if (debug)
					printf("ifExp then kind %d\n", then.ty->kind);
				struct expty elsee = expTy(NULL, Ty_Void());
				if (actual_ty(then.ty)->kind != Ty_void || actual_ty(elsee.ty)->kind != Ty_void)
					EM_error(a->pos, "if-then exp's body must produce no value");
				return expTy(NULL, Ty_Void());
			}
		}
		case (A_whileExp):
		{
			// check test(int) and body(no value)
			struct expty test = transExp(venv, tenv, a->u.whilee.test);
			struct expty body = transExp(venv, tenv, a->u.whilee.body);
			if (test.ty->kind != Ty_int)
				EM_error(a->pos, "type of test exp not int");
			if (body.ty->kind != Ty_void)
				EM_error(a->pos, "while body must produce no value");
			return expTy(NULL, Ty_Void());
		}
		case (A_forExp):
		{
			// check lo and hi (int)
			struct expty lo = transExp(venv, tenv, a->u.forr.lo);
			struct expty hi = transExp(venv, tenv, a->u.forr.hi);
			if (lo.ty->kind != Ty_int)
				EM_error(a->u.forr.lo->pos, "for exp's range type is not integer");
			if (hi.ty->kind != Ty_int)
				EM_error(a->u.forr.hi->pos, "for exp's range type is not integer");
			// check body (no value)
			S_beginScope(venv);
			S_enter(venv, a->u.forr.var, E_VarEntry(Ty_Int(isloop)));
			struct expty body = transExp(venv, tenv, a->u.forr.body);
			if (body.ty->kind != Ty_void)
				EM_error(a->u.forr.body->pos, "for body must produce no value");
			S_endScope(venv);
			return expTy(NULL, Ty_Void());
		}
		case (A_breakExp):
		{
			return expTy(NULL, Ty_Void());
		}
		case (A_letExp):
		{
			struct expty exp;
			A_decList d;
			S_beginScope(venv);
			S_beginScope(tenv);
			for (d = a->u.let.decs; d; d = d->tail)
				transDec(venv, tenv, d->head);
			exp = transExp(venv, tenv, a->u.let.body);
			S_endScope(tenv);
			S_endScope(venv);
			return exp;
		}
		case (A_arrayExp):
		{
			// check array symbol
			Ty_ty ty = actual_ty(S_look(tenv, a->u.array.typ));
			if (!ty) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.array.typ));
				return expTy(NULL, Ty_Int(notloop));
			}
			if (ty->kind != Ty_array) {
				EM_error(a->pos, "not array type %s", S_name(a->u.array.typ));
				return expTy(NULL, ty);
			}
			// check array init
			struct expty size = transExp(venv, tenv, a->u.array.size);
			if (size.ty->kind != Ty_int)
				EM_error(a->u.array.size->pos, "type of array size not int");
			struct expty init = transExp(venv, tenv, a->u.array.init);
			if (debug)
				printf("kind %d, %d", actual_ty(init.ty)->kind, actual_ty(ty->u.array)->kind);
			if (actual_ty(init.ty)->kind != actual_ty(ty->u.array)->kind)
				EM_error(a->u.array.init->pos, "type mismatch");
			
			return expTy(NULL, ty);
		}
		default:
        	assert(0);
	}
} 

struct expty transVar(S_table venv, S_table tenv, A_var v)
{
	switch (v->kind) {
		case (A_simpleVar):
		{
			E_enventry x = S_look(venv, v->u.simple);
			if (x && x->kind == E_varEntry)
				return expTy(NULL, actual_ty(x->u.var.ty));
			else {
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(NULL, Ty_Int(notloop));
			}
		}
		case (A_fieldVar):
		{
			// check record field: lvalue . id
			struct expty lvalue = transVar(venv, tenv, v->u.field.var);
			if (lvalue.ty->kind != Ty_record) {
				EM_error(v->u.field.var->pos, "not a record type");
				return expTy(NULL, Ty_Int(notloop));
			}
			// check sym exist
			S_symbol sym = v->u.field.sym;
			for (Ty_fieldList field = lvalue.ty->u.record; field; field = field->tail) {
				if (field->head->name == sym)
					return expTy(NULL, field->head->ty);
			}
			EM_error(v->u.field.var->pos, "field %s doesn't exist", S_name(sym));
            return expTy(NULL, Ty_Int(notloop));
		}
		case (A_subscriptVar):
		{
			// check array subscript: lvalue [ exp ]
			struct expty lvalue = transVar(venv, tenv, v->u.subscript.var);
			if (lvalue.ty->kind != Ty_array) {
				EM_error(v->u.subscript.var->pos, "array type required");
				return expTy(NULL, Ty_Int(notloop));
			}
			// check index
			struct expty index = transExp(venv, tenv, v->u.subscript.exp);
			if (index.ty->kind != Ty_int) {
				EM_error(v->pos, "type of index not int");
				return expTy(NULL, Ty_Int(notloop));
			}

			return expTy(NULL, lvalue.ty->u.array);
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

void transDec(S_table venv, S_table tenv, A_dec d)
{
	switch (d->kind) {
		case (A_functionDec):
		{
			A_fundecList funcL;
			// put function head into venv first
			for (funcL=d->u.function; funcL; funcL=funcL->tail) {
				// check function name 
				A_fundec func = funcL->head;
				if (S_look(venv, func->name) != NULL) 
					EM_error(func->pos, "two functions have the same name");
				// check params
				Ty_tyList formalTys = makeFormalTyList(tenv, func->params);
				// check result
				Ty_ty resultTy;
				if (func->result) {
					resultTy = S_look(tenv, func->result);
					if (debug)
						printf("what resyltTy %d\n	", resultTy->kind);
					if (!resultTy) {
						EM_error(func->pos, "undefined result type %s", S_name(func->result));
						resultTy = Ty_Void();
					}
				}
				else {
					resultTy = Ty_Void();
				}

				E_enventry funcEntry = E_FunEntry(formalTys, resultTy);
				S_enter(venv, func->name, funcEntry);
			}
			// deal function body 
			for (funcL=d->u.function; funcL; funcL=funcL->tail) {
				A_fundec func = funcL->head;
				S_beginScope(venv);
				E_enventry funcEntry = S_look(venv, func->name);
				Ty_tyList formalTys = funcEntry->u.fun.formals;
				// put function args into env
				for (A_fieldList params = func->params; params && formalTys; params=params->tail, formalTys=formalTys->tail)
					S_enter(venv, params->head->name, E_VarEntry(formalTys->head));

				struct expty res = transExp(venv, tenv, func->body);
				Ty_ty resultTy = funcEntry->u.fun.result;
				if (debug)
					printf("funcDec kind %d, %d\n", res.ty->kind, resultTy->kind);
				if (res.ty->kind != actual_ty(resultTy)->kind) { // mismatch
					if (resultTy->kind != Ty_void)  // and not procedure
						EM_error(func->pos, "function result type mismatch");
					else // but is procedure
						EM_error(func->pos, "procedure returns value");
				}
				S_endScope(venv);
			}
			break;
		}
		case (A_varDec):
		{
			struct expty init = transExp(venv, tenv, d->u.var.init);
			if (d->u.var.typ) { // type limit
				Ty_ty ty = S_look(tenv, d->u.var.typ);
				if (!ty)
					EM_error(d->u.var.init->pos, "type not exist");
				if (debug)
					printf("varDec %d, %d",actual_ty(init.ty)->kind, actual_ty(ty)->kind);
				if (actual_ty(init.ty) != actual_ty(ty))
					EM_error(d->pos, "type mismatch");
			}
			else if (actual_ty(init.ty)->kind == Ty_nil) {
				EM_error(d->pos, "init should not be nil without type specified");
			}
			S_enter(venv, d->u.var.var, E_VarEntry(init.ty));
			break;
		}
		case (A_typeDec):
		{
			A_nametyList nametyL;
			// put type head into tenv first
			for (nametyL = d->u.type; nametyL; nametyL=nametyL->tail) {
				A_namety namety = nametyL->head;
				if (S_look(tenv, namety->name) != NULL) // type name exist, no error?
					EM_error(d->pos, "two types have the same name");
				//else
					S_enter(tenv, namety->name, Ty_Name(namety->name, NULL));
			}
			// deal type body
			for (nametyL = d->u.type; nametyL; nametyL=nametyL->tail) {
				A_namety namety = nametyL->head;
				Ty_ty ty = S_look(tenv, namety->name);
				ty->u.name.ty = transTy(tenv, namety->ty);
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

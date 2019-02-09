#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "env.h" 
#include "translate.h"

/*Lab4: Your implementation of lab4*/

E_enventry E_VarEntry(Tr_access access, Ty_ty ty)
{
	E_enventry entry = checked_malloc(sizeof(*entry));
	entry->kind = E_varEntry;
	entry->u.var.access = access;
	entry->u.var.ty = ty;
	return entry;
}

E_enventry E_ROVarEntry(Tr_access access, Ty_ty ty)
{
	E_enventry entry = checked_malloc(sizeof(*entry));

	entry->u.var.access = access;
	entry->u.var.ty = ty;
	entry->readonly = 1;
	return entry;
}

E_enventry E_FunEntry(Tr_level level, Temp_label label, Ty_tyList formals, Ty_ty result)
{
	E_enventry entry = checked_malloc(sizeof(*entry));

	entry->kind = E_funEntry;
	entry->u.fun.level = level;
	entry->u.fun.label = label;
	entry->u.fun.formals = formals;
	entry->u.fun.result = result;
	return entry;
}

//sym->value
//type_id(name, S_symbol) -> type (Ty_ty)
S_table E_base_tenv(void)
{
	S_table table;
	S_symbol ty_int;
	S_symbol ty_string;

	table = S_empty();

	//basic type: string
	ty_int = S_Symbol("int");
	S_enter(table, ty_int, Ty_Int(0));	

	//basic type: string
	ty_string = S_Symbol("string");
	S_enter(table, ty_string, Ty_String());	

	return table;
}

S_table E_base_venv(void)
{
	/*
	S_table venv;

	Ty_ty result;
	Ty_tyList formals;
	
	Temp_label label = NULL;
	Tr_level level;
	
	level = Tr_outermost();
	venv = S_empty();

	S_enter(venv,S_Symbol("flush"),E_FunEntry(level,label,NULL,NULL));
	
	result = Ty_Int(0);

	formals = checked_malloc(sizeof(*formals));
	formals->head = Ty_Int(0);
	formals->tail = NULL;
	S_enter(venv,S_Symbol("exit"),E_FunEntry(level,label,formals,NULL));

	S_enter(venv,S_Symbol("not"),E_FunEntry(level,label,formals,result));

	result = Ty_String();
	
	S_enter(venv,S_Symbol("chr"),E_FunEntry(level,label,formals,result));

	S_enter(venv,S_Symbol("getchar"),E_FunEntry(level,label,NULL,result));

	formals = checked_malloc(sizeof(*formals));
	formals->head = Ty_String();
	formals->tail = NULL;

	S_enter(venv,S_Symbol("print"),E_FunEntry(level,label,formals,NULL));

	result = Ty_Int(0);
	S_enter(venv,S_Symbol("ord"),E_FunEntry(level,label,formals,result));

	S_enter(venv,S_Symbol("size"),E_FunEntry(level,label,formals,result));

	result = Ty_String();
	formals = checked_malloc(sizeof(*formals));
	formals->head = Ty_String();
	formals->tail = checked_malloc(sizeof(*formals));
	formals->tail->head = Ty_String();
	S_enter(venv,S_Symbol("concat"),E_FunEntry(level,label,formals,result));

	formals = checked_malloc(sizeof(*formals));
	formals->head = Ty_String();
	formals->tail = checked_malloc(sizeof(*formals));
	formals->tail->head = Ty_Int(0);
	formals->tail->tail = checked_malloc(sizeof(*formals));
	formals->tail->tail->head = Ty_Int(0);
	S_enter(venv,S_Symbol("substring"),E_FunEntry(level,label,formals,result));

	return venv;
	*/
	
	Temp_label out = Temp_newlabel();
    Tr_level out_level = Tr_newLevel(Tr_outermost(), out, U_BoolList(1, NULL));
    S_table res = S_empty();
    S_enter(res, S_Symbol("print"), E_FunEntry(out_level, out, Ty_TyList(Ty_String(), NULL), Ty_Void()));
    S_enter(res, S_Symbol("flush"), E_FunEntry(out_level, out, NULL, Ty_Void()));
    S_enter(res, S_Symbol("getchar"), E_FunEntry(out_level, out, NULL, Ty_String()));
    S_enter(res, S_Symbol("ord"), E_FunEntry(out_level, out, Ty_TyList(Ty_String(), NULL), Ty_Int(0)));
    S_enter(res, S_Symbol("chr"), E_FunEntry(out_level, out, Ty_TyList(Ty_Int(0), NULL), Ty_String()));
    S_enter(res, S_Symbol("size"), E_FunEntry(out_level, out, Ty_TyList(Ty_String(), NULL), Ty_Int(0)));
    S_enter(res, S_Symbol("substring"), E_FunEntry(out_level, out, Ty_TyList(Ty_String(), Ty_TyList(Ty_Int(0), Ty_TyList(Ty_Int(0), NULL))), Ty_String()));
    S_enter(res, S_Symbol("concat"), E_FunEntry(out_level, out, Ty_TyList(Ty_String(), Ty_TyList(Ty_String(), NULL)), Ty_String()));
    S_enter(res, S_Symbol("not"), E_FunEntry(out_level, out, Ty_TyList(Ty_Int(0), NULL), Ty_Int(0)));
    S_enter(res, S_Symbol("exit"), E_FunEntry(out_level, out, Ty_TyList(Ty_Int(0), NULL), Ty_Void()));
    return res;

}

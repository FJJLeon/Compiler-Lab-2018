#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "env.h"
#include "types.h"

/*Lab4: Your implementation of lab4*/

E_enventry E_VarEntry(Ty_ty ty)
{
	E_enventry entry = checked_malloc(sizeof(*entry));
	entry->kind = E_varEntry;
	entry->u.var.ty = ty;
	return entry;
}

E_enventry E_FunEntry(Ty_tyList formals, Ty_ty result)
{
	E_enventry entry = checked_malloc(sizeof(*entry));
	entry->kind = E_funEntry;
	entry->u.fun.formals = formals;
	entry->u.fun.result = result;
	return entry;
}

S_table E_base_tenv(void) // basic Ty_ty environment
{
	S_table table = S_empty();
	// basis Ty_ty: int
	S_enter(table, S_Symbol("int"), Ty_Int(0));  // why notloop not found
	// basic Ty_ty: string
	S_enter(table, S_Symbol("string"), Ty_String());
	return table;
}

S_table E_base_venv(void) // basic E_enventry environment
{
	S_table table;
	table = S_empty();
	S_enter(table, S_Symbol("print"), E_FunEntry(Ty_TyList(Ty_String(), NULL), Ty_Void()));
	S_enter(table, S_Symbol("getchar"), E_FunEntry(NULL, Ty_String()));
	S_enter(table, S_Symbol("ord"), E_FunEntry(Ty_TyList(Ty_String(), NULL), Ty_Int(0)));
    S_enter(table, S_Symbol("chr"), E_FunEntry(Ty_TyList(Ty_Int(0), NULL), Ty_String()));
	return table;
}

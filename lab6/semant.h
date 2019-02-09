#ifndef __SEMANT_H_
#define __SEMANT_H_

#include "absyn.h"
#include "symbol.h"
#include "temp.h"
#include "frame.h"
#include "translate.h"

struct expty;

/* in lab4 is
struct expty transExp(S_table venv, S_table tenv, A_exp a);
struct expty transVar(S_table venv, S_table tenv, A_var v);
void transDec(S_table venv, S_table tenv, A_dec d);
*/
struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level clevel, Temp_label breakflag);
struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level clevel, Temp_label breakflag);
Tr_exp       transDec(S_table venv, S_table tenv, A_dec d, Tr_level clevel, Temp_label breakflag);
Ty_ty		 transTy (              S_table tenv, A_ty a);

//void SEM_transProg(A_exp exp);
F_fragList SEM_transProg(A_exp exp);

#endif

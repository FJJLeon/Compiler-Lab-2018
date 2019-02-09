#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"

#define ESCdebug 1

#define DEBUG(format, ...) do { \
    if (ESCdebug)  \
        fprintf(stdout, "[%s@%s,%d]\t" format "\n", \
           __func__, __FILE__, __LINE__, ##__VA_ARGS__ );  \
    } while (0)


static void traverseExp(S_table env, int depth, A_exp e);
static void traverseDec(S_table env, int depth, A_dec d);
static void traverseVar(S_table env, int depth, A_var v);


void Esc_findEscape(A_exp exp) {
	//your code here
	// find Escaped, mark escape as TRUE
	traverseExp(S_empty(), 0, exp);
}

escapeEntry EscapeEntry(int d, bool* e){
	escapeEntry p = checked_malloc(sizeof(*p));
	p->depth = d;
	p->escape = e;
	return p;
}

static void traverseExp(S_table env, int depth, A_exp e) {
	switch (e->kind) {
		case A_varExp:
		{/* A_var var */
			traverseVar(env, depth, e->u.var);
			return;
		}
		case A_nilExp:
		case A_intExp:
		case A_stringExp:
			return;
		case A_callExp:
		{/* struct {S_symbol func; A_expList args;} call */
			A_expList args = e->u.call.args;
			for (; args; args=args->tail){
				traverseExp(env, depth, args->head);
			}
			return;
		}
		case A_opExp:
		{/* struct {A_oper oper; A_exp left; A_exp right;} op */
			traverseExp(env, depth, e->u.op.left);
			traverseExp(env, depth, e->u.op.right);
			return;
		}
		case A_recordExp:
		{/* struct {S_symbol typ; A_efieldList fields;} record */
			DEBUG("recordExp");
			A_efieldList efl = e->u.record.fields;
			for (; efl; efl=efl->tail) {
				/*struct A_efield_ {S_symbol name; A_exp exp;}*/
				A_efield ef = efl->head;
				traverseExp(env, depth, ef->exp);
			}
			return;
		}
		case A_seqExp:
		{/*A_expList seq*/
			A_expList seq = e->u.seq;
			for (; seq; seq = seq->tail) {
				traverseExp(env, depth, seq->head);
			}
			return;
		}
		case A_assignExp:
		{/*struct {A_var var; A_exp exp;} assign*/
			traverseVar(env, depth, e->u.assign.var);
			traverseExp(env, depth, e->u.assign.exp);
			return;
		}
		case A_ifExp:
		{/*struct {A_exp test, then, elsee;} iff*/
			traverseExp(env, depth, e->u.iff.test);
			traverseExp(env, depth, e->u.iff.then);
			if (e->u.iff.elsee)
				traverseExp(env, depth, e->u.iff.elsee);
			return;
		}
		case A_whileExp:
		{/*struct {A_exp test, body;} whilee*/
			traverseExp(env, depth, e->u.whilee.test);
			traverseExp(env, depth, e->u.whilee.body);
			return;
		}
		case A_forExp:
		{/*struct {S_symbol var; A_exp lo,hi,body; bool escape;} forr*/
			traverseExp(env, depth, e->u.forr.lo);
			traverseExp(env, depth, e->u.forr.hi);
			S_beginScope(env);
			e->u.forr.escape = FALSE;
			S_enter(env, e->u.forr.var, EscapeEntry(depth, &(e->u.forr.escape)));
			traverseExp(env, depth, e->u.forr.body);
			S_endScope(env);
			return;
		}
		case A_breakExp:
			return;
		case A_letExp:
		{/*struct {A_decList decs; A_exp body;} let*/
			A_decList decs = e->u.let.decs;
			S_beginScope(env);
			for (; decs; decs = decs->tail) {
				traverseDec(env, depth, decs->head);
			}
			traverseExp(env, depth, e->u.let.body);
			S_endScope(env);
			return;
		}
		case A_arrayExp:
		{/* struct {S_symbol typ; A_exp size, init;} */
			traverseExp(env, depth, e->u.array.size);
			traverseExp(env, depth, e->u.array.init);
			return;
		}
		default:
			DEBUG("find escape error");
			assert(0);
	}
	assert(0);
}

static void traverseDec(S_table env, int depth, A_dec d) {
	switch (d->kind) {
		case A_functionDec:
		{/*A_fundecList function*/
			A_fundecList funcL = d->u.function;
			for (; funcL; funcL = funcL->tail) {
				/*struct A_fundec_ {A_pos pos;
                 		S_symbol name; A_fieldList params; 
		 				S_symbol result; A_exp body;}*/
				A_fundec func = funcL->head;
				A_fieldList params = func->params;
				/*struct A_field_ {S_symbol name, typ; A_pos pos; bool escape;}*/
				S_beginScope(env);
				for (; params; params = params->tail) {
					A_field p = params->head;
					p->escape = FALSE;
					S_enter(env, p->name, EscapeEntry(depth, &(p->escape)));
				}
				traverseExp(env, depth+1, func->body);
				S_endScope(env);
			}
			return;
		}
		case A_varDec:
		{/* struct {S_symbol var; S_symbol typ; A_exp init; bool escape;} var */
			S_symbol var = d->u.var.var;
			DEBUG("vardec %s in depth %d ", S_name(var), depth);
			traverseExp(env, depth, d->u.var.init);
			DEBUG("vardec out1");
			d->u.var.escape = FALSE;
			S_enter(env, var, EscapeEntry(depth, &(d->u.var.escape)));
			DEBUG("vardec out");
			return;
		}
		case A_typeDec:
		{/* A_nametyList type */
			return;
		}
		default:
			DEBUG("error");
			assert(0);
	}
	assert(0);
}

static void traverseVar(S_table env, int depth, A_var v) {
	switch (v->kind) {
		case A_simpleVar:
		{/* S_symbol simple */
			S_symbol simple = v->u.simple;
			escapeEntry entry = S_look(env, simple);
			DEBUG("simple var %s in depth %d", S_name(simple), depth);
			assert(entry);
			if (entry && depth > (entry->depth)){
				DEBUG(" var %s escape ", S_name(simple));
				*(entry->escape) = TRUE;
			}
			return;
		}
		case A_fieldVar:
		{/*struct {A_var var; S_symbol sym;} field*/
			traverseVar(env, depth, v->u.field.var);
			return;
		}
		case A_subscriptVar:
		{/*struct {A_var var; A_exp exp;} subscript */
			traverseVar(env, depth, v->u.subscript.var);
			traverseExp(env, depth, v->u.subscript.exp);
			return;
		}
		default:
			DEBUG("error");
			assert(0);
	}
	assert(0);
}
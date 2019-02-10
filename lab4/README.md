## Lab 4: Type Checking 类型检查

### Description
Write a type-checking phase for your compiler, a module **semant.c** matching the following header file:
```
/* semant.h */
void SEM_transProg(A_exp exp);
```
that type-checks an abstract syntax tree and produces any appropriate error messages about mismatching types or undeclared identifiers.

Also provide the implementation of the Env module described in this chapter. You must use precisely the Absyn interface described in Figure 4.7, but you are free to follow or ignore any advice given in this chapter about the internal organization of the Semant module.
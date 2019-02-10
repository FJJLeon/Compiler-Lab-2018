## Lab 5: Frames & Translation to Trees 生成中间表达式树
* 这个lab仅检查了fragment的数量，仍存在较多BUG
* 使用 GDB 查看 SIGSEGV 时的 back trace 有助于 DEBUG

### Description
Augment semant.c to allocate locations for local variables, and to keep track of the nesting level.

Try to keep all the machine-specific details in your machine-dependent Frame module (x86frame.c), not in Semant or Translate.

Design translate.h, implement translate.c, and rewrite the Semant structure to call upon Translate appropriately. The result of calling SEM_transProg should be a F_fragList.

Files related in Lab 5 include: 
* tiger.*
* semant.c
* frame.h
* x86frame.c
* translate.*

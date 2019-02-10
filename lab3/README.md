## Lab 3: Parsing 语法分析

### Description
Use Yacc to implement a parser for the Tiger language. Appendix A describes, among other things, the syntax of Tiger.
You should turn in the file **tiger.y** and read the file **absyn.h**.

Supporting files available in Lab 3 include: 
* `Makefile`: The “Makefile”
* `errormsg.[ch]`: The ErrorMessage structure, useful for producing errormessages with file names and line numbers
* `tiger.lex`: You should use *your own* tiger.lex in lab 2 for subsequent labs
* `tiger.y`: The skeleton of a file you must fill in

You won’t need tokens.h anymore; instead, the header file for tokens is y.tab.h, which is produced automatically by Yacc from the token specification of your grammar.

Your grammar should have as few shift-reduce conflicts as possible, and no reduce-reduce conflicts.

**Update**: There exist some redundant commas and empty lines in the files under **refs-2** and **parse.c**. you only need to copy your **tiger.lex** and **tiger.y** to the new lab environment.

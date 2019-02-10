## Lab2: Lexical Analysis 词法分析

### Description
Use Lex to implement a lexical analyzer for the Tiger language. Appendix A describes, among other things, the lexical tokens of Tiger.

This chapter has left out some of the specifics of how the lexical analyzer should be initialized and how it should communicate with the rest of the compiler. You can learn this from the Lex manual, but the “skeleton” files in the lab directory will also help get you started.

Along with the tiger.lex file you should turn in documentation for the following point:
* how you handle comments;
* how you handle strings;
* error handling;
* end-of-file handling;
* other interesting features of your lexer.

**Notice**: Before you start this lab, you should carefully read the chapter 2 of the textbook, and you may need to get some help from the [lex manual](http://dinosaur.compilertools.net/lex/index.html) and the *Tiger Language Reference Manual* (Appendix A).

**Update**: We have added an extra testcase named as **testcases/test52.tig** and the corresponding output named as **refs/test52.out** to test escape sequences.

### Environment
if your system lacks Lex and Yacc, you can install them with the following command (on Ubuntu).
```
shell% sudo apt-get install flex bison
```

### Grade Test
The lab environment contains a grading script named as **gradeMe.sh**, you can use it to evaluate your code, and that's how we grade your code, too. If you pass all the tests, the script will print a successful hint, otherwise, it will output some error messages. You can execute the script with the following commands.
```
shell% ./gradeMe.sh
shell% ...
shell% [^_^]: Pass #If you pass all the tests, you will see these messages.
shell% TOTAL SCORE: 100
```
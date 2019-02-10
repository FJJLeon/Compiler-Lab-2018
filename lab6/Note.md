### 之前需要的文件
* Tiger.{lex, y}   词法分析
* Env.{h,c}       环境
* Translate.{h,c}  翻译成IR
* Semant.{h,c}   语义分析
* Frame.h     栈帧布局头文件
* X86frame.c    针对于X86的栈帧
* Type.{h,c}      Ty_ 类型和函数声明

### 需要写
* Escape,类型translate递归函数traverseExp、Dec、Var，设置变量的escape与否, 逃逸变量需要存在栈中
* Codegen, 	用 **maximal Munch** 算法覆盖IR，选择相应瓦片的指令
* Flowgraph, 构造数据流图，从上一条有向连接到下一条指令，jmp不连接到下一条，而连接到可能的label，这应该就是 T_JUMP需要在jump中间指明所有可能的跳转目标label的原因，不知道跳哪个，都是活跃的，活跃分析要用
* Liveness, 活跃分析，利用数据流图（指令的流图）先用**集合方程**算法得到 变量（temp）的 **活跃信息**，通过变量的活跃信息，得到变量的**冲突图**，同时活跃的即为冲突，连边。注意MOVE指令的特殊处理，P162，对于在USE集合中的出口活跃变量，暂时不设为冲突（连边），另外把传送指令标记到MoveList备用
* X86frame, 栈帧布局
* regalloc，寄存器分配，图着色，好麻烦

### Log
* gcc -Wl,--wrap,getchar -m64 ./testcases/trec.tig.s runtime.c -o test.out 生成 可执行文件
* 注意连续多个dec的顺序
* 关于 static link ，caller 是调用者函数的level，callee是被调用函数所定义的层级，如果callee所在层级是 outmost，即callee level的parent不存在，则是 external 函数
* 判断相等较好的写法，讲常量写在左边，防止写成一个等号变成赋值
* CALL的指令传输到活跃分析的 def 包括 callersaves、RV、argureg
* codegen的 s1 s0 是有用的，指定寄存器，有点僵硬
* 按书上的说法，应该是在semant Tr_procExitEntry的时候加上出入保护callersaved的 IR ，然后codegen的时候和 body 一同处理，学长不是这么作， 而是在 codegen的主函数 处理 body 前后完成 这些语句的直接输出
* X64 除法的汇编需要 cqto
* 字符串相等的判断需要调用外部函数，在semant中判断 A_opExp里面判断 eq neq时检查是否 为string 类型，调用外部 stringEqual 函数
* 记得修改 Runtime.c 的long，不然不可能满分。。。

### 栈帧布局 AR
* 栈帧stack frame 也叫 activation record
* 栈帧像冰柱，向下增长
* 相对于 current frame的函数，previous frame 是 caller（调用者）
* FP和SP相差一个常数，该常数可以在 函数定义时根据 escape的局部变量和调用其他函数的情况来确定？（存疑）
* 书上的栈帧布局不是X86的
* 变量传递的问题，X86-64将前6个参数放在寄存器中传递，后续的放在frame中，若对前6个参数有取地址操作，则也必须在函数入口处将其保存到frame中(P93倒数第3段)
* 现代计算机可以将返回地址放入指定寄存器，但X86-64仍旧放到存储器栈帧中，与所有参数相邻
* 允许声明嵌套函数（Tiger可以）的语言中，内层函数可以使用外层函数声明的变量（即访问外部函数的栈帧），称为块结构，在lab中使用静态链实现（P95），调用f时，传递给f一个指针，这个指针指向静态包含f的那个函数，这个指针为静态链
* F_frame 表示有关形参和分配在栈帧中的局部变量的信息，调用
F_newframe(f,l)创建新栈帧，f为有k个形参的函数，l是k个布尔值组成的表，表示formals是否逃逸，用U_BoolList类型表示
    * F_frame 包含 所有形参的位置、实现 视角位移的指令、已分配的栈帧大小、函数开始处的机器代码标号
* F_Access 描述可以存放在栈中或寄存器中的形参和局部变量，是抽象数据类型，内容只在Frame模块可见（似乎已经破坏了）
    * Inframe(X) 指出相对 FP 偏移为 X的存储位置
    * InReg(t) 指出使用寄存器 t
* 视角位移 view shift？ P97
* 局部变量 可以保存在 栈中，也可以保存在寄存器，semant阶段调用 F_allocLocal(f, boolean) 分配局部变量
* Frame不应该知道静态链信息，静态链由Translate负责
    * Tr_newLevel(parent, f,  formals) 创建一个新层级，在U_BoolList 开头指明一个额外参数 作为逃逸的静态链来调用 F_newFrame(label, U_BoolList(True, formals))
    * Semant 调用 Tr_newLevel时，需要传递 包围层的 level值，Tiger主函数中创建层次需要传递 Tr_outermost() ，该层次中声明了库函数，如 printi、initArray、stringEqual，这些函数的调用要用 F_externalCall
    * 追随静态链需要生成一条由MEM和+结点组成的链，读取使用层和定义层之间栈帧的静态链
* 函数定义 被翻译成入口处理（prologue）、函数体（body）、出口处理（epilogue）组成的汇编代码 P121
* 无帧指针FP的情形
    * Translate阶段生成应用FP这个虚拟帧指针的树，在codegen中需要用SP+K+FS 来代替所有对 FP+K的引用，fs=framesize
    * 但是此时并不知道fs的大小，可以在函数体的代码中 生成 sp+Lxx_framesize，同时期望入口代码（应有F_procEntryExit3生成）生成常数 Lxx_framesize （.set）
* 寄存器表
    * 特殊寄存器 specialregs  RV FP SP RA ZERO
    * 传递实参（包括静态链）的寄存器，argregs
    * 被调用者保护的寄存器 calleesaves
    * 调用者保护的寄存器 callersaves
    * 上面4表无交集 且需要包括所有可能的硬寄存器
* F_procEntryExit2 生成下沉指令，告诉寄存器分配器 RA+SP+calleeSaves是出口活跃的
* 似乎frame需要一个新成员来记录实参信息，需要使用传递实参需要空间的大小
* 字符串处理 P191 zh-CN












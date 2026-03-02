1、修改Makefile.Linux文件为Clang16+O3编译模式【加了O3在小包测试才会测出优势，O3会把入栈出栈操作消除】

2、Clang16编译器禁止隐式函数声明，需要添加-Wno-implicit-function-declaration编译选项
![image.png](https://raw.gitcode.com/user-images/assets/9300108/4b637ef6-f754-4d4a-b0f6-f7f988926ce5/image.png 'image.png')


3、修改Makefile.benchmarks文件，在第72行新增memcmp编译文件
4、修改memmove.c文件115到116行，memmove主要测试内存重叠且dst大于src的场景，其他场景与memcpy实现代码一致，不用重复测。修改memmove.c如下：
ts->ts_src = opta + (char *)valloc(ts->ts_srcsize+64);
//ts->ts_dest = valloc(ts->ts_destsize);
ts->ts_dest = ts->ts_src + 64; 

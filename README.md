# SimpleNotepad
最开始是Vibe Coding出来的垃圾，后来翻出来做了个tcc特别版和Watcom1.9特别版

## 怎么贡献
你真的想贡献它吗

现在需要的是语法高亮，但是作者不懂<RichEdit.h>

## 规范
无论如何，tinycc版本和watcom的版本一律是C89，并且请注意：不要删掉末尾的换行，C89认为这是UB，详见：

```txt
Simple_Notepad_Watcom.c(628): Warning! W138: No newline at end of file
Simple_Notepad_Watcom.c: 628 lines, included 43517, 1 warnings, 0 errors
```


test:
.asciz "Hello world!~\n"
.space 256
test2:
len = . - test

.quad 0xaaaaaaaaaa
.word len
.quad 0xaaaaaaaaaa
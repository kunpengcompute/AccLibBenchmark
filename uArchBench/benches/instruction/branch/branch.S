.global bench_branch_taken
.global bench_branch_not_taken

.text

.align 6
bench_branch_taken:
    mov x1, x0
1:
    subs x1, x1, #1
    beq 2f
    b 1b
2:
    ret

.align 6
bench_branch_not_taken:
    mov x1, x0
1:
    subs x1, x1, #1
    bne 1b
    ret

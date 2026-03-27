.global bench_sub_latency
.global bench_sub_throughput

.text

.align 6
bench_sub_latency:
    mov x1, x0
1:
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1

    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1

    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1

    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    sub x0, x0, #1
    subs x1, x1, #1
    bne 1b
    ret

.align 6
bench_sub_throughput:
    mov x1, x0
1:
    sub x0, x0, #1
    sub x2, x2, #1
    sub x3, x3, #1
    sub x4, x4, #1
    sub x5, x5, #1
    sub x6, x6, #1
    sub x7, x7, #1
    sub x8, x8, #1

    sub x9, x9, #1
    sub x10, x10, #1
    sub x11, x11, #1
    sub x12, x12, #1
    sub x13, x13, #1
    sub x14, x14, #1
    sub x15, x15, #1
    sub x16, x16, #1

    sub x17, x17, #1
    sub x0, x0, #1
    sub x2, x2, #1
    sub x3, x3, #1
    sub x4, x4, #1
    sub x5, x5, #1
    sub x6, x6, #1
    sub x7, x7, #1
    
    sub x8, x8, #1
    sub x9, x9, #1
    sub x10, x10, #1
    sub x11, x11, #1
    sub x12, x12, #1
    sub x13, x13, #1
    sub x14, x14, #1
    sub x15, x15, #1
    subs x1, x1, #1
    bne 1b
    ret

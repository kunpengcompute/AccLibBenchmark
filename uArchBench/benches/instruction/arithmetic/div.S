.global bench_div_latency
.global bench_div_throughput

.text

.align 6
bench_div_latency:
    mov x1, x0
1:
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2

    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2

    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2

    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    udiv x0, x0, x2
    subs x1, x1, #1
    bne 1b
    ret

.align 6
bench_div_throughput:
    mov x1, x0
    mov x17, #3
1:
    udiv x0, x0, x17
    udiv x2, x2, x17
    udiv x3, x3, x17
    udiv x4, x4, x17
    udiv x5, x5, x17
    udiv x6, x6, x17
    udiv x7, x7, x17
    udiv x8, x8, x17

    udiv x9, x9, x17
    udiv x10, x10, x17
    udiv x11, x11, x17
    udiv x12, x12, x17
    udiv x13, x13, x17
    udiv x14, x14, x17
    udiv x15, x15, x17
    udiv x16, x16, x17

    udiv x0, x0, x17
    udiv x2, x2, x17
    udiv x3, x3, x17
    udiv x4, x4, x17
    udiv x5, x5, x17
    udiv x6, x6, x17
    udiv x7, x7, x17
    udiv x8, x8, x17

    udiv x9, x9, x17
    udiv x10, x10, x17
    udiv x11, x11, x17
    udiv x12, x12, x17
    udiv x13, x13, x17
    udiv x14, x14, x17
    udiv x15, x15, x17
    udiv x16, x16, x17
    
    subs x1, x1, #1
    bne 1b
    ret

.global bench_add_latency
.global bench_add_throughput

.text

.align 6
bench_add_latency:
    mov x1, x0
    mov x0, #0
1:
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1

    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1

    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1

    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    add x0, x0, #1
    subs x1, x1, #1
    bne 1b
    ret

.align 6
bench_add_throughput:
    mov x1, x0
1:
    add x0, x0, #1
    add x2, x2, #1
    add x3, x3, #1
    add x4, x4, #1
    add x5, x5, #1
    add x6, x6, #1
    add x7, x7, #1
    add x8, x8, #1

    add x9, x9, #1
    add x10, x10, #1
    add x11, x11, #1
    add x12, x12, #1
    add x13, x13, #1
    add x14, x14, #1
    add x15, x15, #1
    add x16, x16, #1

    add x17, x17, #1
    add x0, x0, #1
    add x2, x2, #1
    add x3, x3, #1
    add x4, x4, #1
    add x5, x5, #1
    add x6, x6, #1
    add x7, x7, #1
    
    add x8, x8, #1
    add x9, x9, #1
    add x10, x10, #1
    add x11, x11, #1
    add x12, x12, #1
    add x13, x13, #1
    add x14, x14, #1
    add x15, x15, #1
    subs x1, x1, #1
    bne 1b
    ret

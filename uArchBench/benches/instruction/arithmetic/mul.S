.global bench_mul_latency
.global bench_mul_throughput

.text

.align 6
bench_mul_latency:
    mov x1, x0
    mov x2, #3
    mov x0, #2
1:
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2

    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2

    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2

    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    mul x0, x0, x2
    subs x1, x1, #1
    bne 1b
    ret

.align 6
bench_mul_throughput:
    mov x1, x0
    mov x17, #3
1:
    mul x0, x0, x17
    mul x2, x2, x17
    mul x3, x3, x17
    mul x4, x4, x17
    mul x5, x5, x17
    mul x6, x6, x17
    mul x7, x7, x17
    mul x8, x8, x17

    mul x9, x9, x17
    mul x10, x10, x17
    mul x11, x11, x17
    mul x12, x12, x17
    mul x13, x13, x17
    mul x14, x14, x17
    mul x15, x15, x17
    mul x16, x16, x17

    mul x0, x0, x17
    mul x2, x2, x17
    mul x3, x3, x17
    mul x4, x4, x17
    mul x5, x5, x17
    mul x6, x6, x17
    mul x7, x7, x17
    mul x8, x8, x17
    
    mul x9, x9, x17
    mul x10, x10, x17
    mul x11, x11, x17
    mul x12, x12, x17
    mul x13, x13, x17
    mul x14, x14, x17
    mul x15, x15, x17
    mul x16, x16, x17
    subs x1, x1, #1
    bne 1b
    ret

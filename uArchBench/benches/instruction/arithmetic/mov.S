.global bench_mov_latency
.global bench_mov_throughput

.text

.align 6
bench_mov_latency:
    mov x1, x0
    mov x0, #0
1:
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0

    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0

    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0

    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    mov x0, x0
    subs x1, x1, #1
    bne 1b
    ret

.align 6
bench_mov_throughput:
    mov x1, x0
1:
    mov x0, x2
    mov x3, x2
    mov x4, x2
    mov x5, x2
    mov x6, x2
    mov x7, x2
    mov x8, x2
    mov x9, x2

    mov x10, x2
    mov x11, x2
    mov x12, x2
    mov x13, x2
    mov x14, x2
    mov x15, x2
    mov x16, x2
    mov x17, x2

    mov x0, x2
    mov x3, x2
    mov x4, x2
    mov x5, x2
    mov x6, x2
    mov x7, x2
    mov x8, x2
    mov x9, x2
    
    mov x10, x2
    mov x11, x2
    mov x12, x2
    mov x13, x2
    mov x14, x2
    mov x15, x2
    mov x16, x2
    mov x17, x2
    subs x1, x1, #1
    bne 1b
    ret

.global bench_load_latency
.global bench_load_throughput

.text

.align 6
bench_load_latency:
    mov x2, x0
    mov x0, x1
1:
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]

    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]

    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]

    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]
    ldr x0, [x0]

    subs x2, x2, #1
    bne 1b
    ret

.align 6
bench_load_throughput:
    mov x2, x0
1:
    ldr x0, [x1]
    ldr x3, [x1]
    ldr x4, [x1]
    ldr x5, [x1]
    ldr x6, [x1]
    ldr x7, [x1]
    ldr x8, [x1]
    ldr x9, [x1]

    ldr x10, [x1]
    ldr x11, [x1]
    ldr x12, [x1]
    ldr x13, [x1]
    ldr x14, [x1]
    ldr x15, [x1]
    ldr x16, [x1]
    ldr x17, [x1]

    ldr x0, [x1]
    ldr x3, [x1]
    ldr x4, [x1]
    ldr x5, [x1]
    ldr x6, [x1]
    ldr x7, [x1]
    ldr x8, [x1]
    ldr x9, [x1]

    ldr x10, [x1]
    ldr x11, [x1]
    ldr x12, [x1]
    ldr x13, [x1]
    ldr x14, [x1]
    ldr x15, [x1]
    ldr x16, [x1]
    ldr x17, [x1]

    subs x2, x2, #1
    bne 1b
    ret

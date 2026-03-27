.global bench_store_to_load_latency

.text

.align 6
bench_store_to_load_latency:
    mov x2, x0
    mov x0, x1
1:
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]

    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    str x0, [x1]
    ldr x0, [x1]
    subs x2, x2, #1
    bne 1b
    ret

.global bench_neon_add_latency
.global bench_neon_add_throughput

.text

.align 6
bench_neon_add_latency:
    mov x2, x0
    ldr q1, [x1]
1:
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    fadd v0.4s, v0.4s, v1.4s
    subs x2, x2, #1
    bne 1b
    ret

.align 6
bench_neon_add_throughput:
    mov x2, x0
    ldr q7, [x1]
1:
    fadd v0.4s, v0.4s, v7.4s
    fadd v1.4s, v1.4s, v7.4s
    fadd v2.4s, v2.4s, v7.4s
    fadd v3.4s, v3.4s, v7.4s
    fadd v4.4s, v4.4s, v7.4s
    fadd v5.4s, v5.4s, v7.4s
    fadd v6.4s, v6.4s, v7.4s
    fadd v16.4s, v16.4s, v7.4s
    fadd v17.4s, v17.4s, v7.4s
    fadd v18.4s, v18.4s, v7.4s
    fadd v19.4s, v19.4s, v7.4s
    fadd v20.4s, v20.4s, v7.4s
    fadd v21.4s, v21.4s, v7.4s
    fadd v22.4s, v22.4s, v7.4s
    fadd v23.4s, v23.4s, v7.4s
    fadd v24.4s, v24.4s, v7.4s
    fadd v25.4s, v25.4s, v7.4s
    fadd v26.4s, v26.4s, v7.4s
    fadd v27.4s, v27.4s, v7.4s
    fadd v28.4s, v28.4s, v7.4s
    fadd v29.4s, v29.4s, v7.4s
    fadd v30.4s, v30.4s, v7.4s
    fadd v31.4s, v31.4s, v7.4s
    fadd v0.4s, v0.4s, v7.4s
    fadd v1.4s, v1.4s, v7.4s
    fadd v2.4s, v2.4s, v7.4s
    fadd v3.4s, v3.4s, v7.4s
    fadd v4.4s, v4.4s, v7.4s
    fadd v5.4s, v5.4s, v7.4s
    fadd v6.4s, v6.4s, v7.4s
    fadd v16.4s, v16.4s, v7.4s
    fadd v17.4s, v17.4s, v7.4s
    subs x2, x2, #1
    bne 1b
    ret

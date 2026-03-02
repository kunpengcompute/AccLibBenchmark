/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2023 [Your Name or Company].  All rights reserved.
 * Use is subject to license terms.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libmicro.h"

#define DEFS 8192

// 默认的差异位置
#define DEFD -1  // -1 表示相等（无差异）

static long long opts = DEFS;
static int optf;
static int optd = DEFD;  // 差异位置：-1 表示相等，0 到 opts-1 表示差异出现的索引
static int opta = 0;

typedef struct {
    char *ts_src1_base;  // valloc 返回的原始基地址
    char *ts_src2_base;  // valloc 返回的原始基地址
    char *ts_src1;       // 实际使用的指针 (含 opta 偏移)
    char *ts_src2;       // 实际使用的指针 (含 opta 偏移)
    int ts_srcsize;
} tsd_t;

int benchmark_init()
{
    lm_tsdsize = sizeof(tsd_t);

    (void)sprintf(lm_optstr, "s:fd:a:");

    (void)sprintf(lm_usage,
                  "       [-s buffer-size (default %d)]\n"
                  "       [-a relative alignment (default page aligned)]\n"
                  "       [-f (rotate buffers to keep them out of cache)]\n"
                  "       [-d difference-offset (0 to size-1 for difference, -1 for equal, default %d)]\n"
                  "notes: measures memcmp()\n",
                  DEFS, DEFD);

    (void)sprintf(lm_header, "%8s%12s", "size", "diff_idx");

    return (0);
}

int benchmark_optswitch(int opt, char *optarg)
{
    switch (opt) {
        case 'f':
            optf++;
            break;
        case 's':
            opts = sizetoll(optarg);
            break;
        case 'd':
            optd = sizetoint(optarg);
            break;
        case 'a':
            opta = sizetoint(optarg);
            break;
        default:
            return (-1);
    }
    return (0);
}

int benchmark_initworker(void *tsd)
{
    tsd_t *ts = (tsd_t *)tsd;

    if (optf)
        ts->ts_srcsize = 64 * 1024 * 1024;
    else
        ts->ts_srcsize = (int)opts + opta;  // opts 是实际比较大小，opta 是对齐偏移

    // 分配原始基地址
    ts->ts_src1_base = (char *)valloc(ts->ts_srcsize);
    ts->ts_src2_base = (char *)valloc(ts->ts_srcsize);

    // 计算带偏移量的起始地址
    ts->ts_src1 = opta + ts->ts_src1_base;
    ts->ts_src2 = opta + ts->ts_src2_base;

    if (ts->ts_src1 == NULL || ts->ts_src2 == NULL) {
        fprintf(stderr, "Error: valloc failed\n");
        return (-1);
    }

    // 1. 初始化两个缓冲区为相同的值
    memset(ts->ts_src1, 0xAA, ts->ts_srcsize);
    memset(ts->ts_src2, 0xAA, ts->ts_srcsize);

    // 2. 如果 optd != -1，设置差异
    if (optd != -1) {
        if (optd < opts) {
            // 在指定位置设置差异，确保 memcmp 会比较到此处
            ts->ts_src2[optd] = 0xBB;
        } else {
            fprintf(stderr, "Warning: Difference offset %d is >= size %lld. Buffers will be equal.\n", optd, opts);
            optd = -1;
        }
    }

    return (0);
}

int benchmark(void *tsd, result_t *res)
{
    tsd_t *ts = (tsd_t *)tsd;
    int i;
    char *src1 = ts->ts_src1;
    char *src2 = ts->ts_src2;

    // 旋转的步长：至少 1024 字节，或等于 opts
    int bump = (int)opts;
    if (bump < 1024)
        bump = 1024;

    for (i = 0; i < lm_optB; i++) {
        volatile int result = memcmp(src1, src2, opts);
        (void)result;

        if (optf) {
            src1 += bump;
            src2 += bump;

            // 注意：这里需要检查是否超过了 *总分配的末尾*
            if (src1 + opts > ts->ts_src1_base + ts->ts_srcsize)
                src1 = ts->ts_src1_base + opta;

            if (src2 + opts > ts->ts_src2_base + ts->ts_srcsize)
                src2 = ts->ts_src2_base + opta;
        }
    }

    res->re_count = i;

    return (0);
}

char *benchmark_result()
{
    static char result[256];
    char *diff_str;

    if (optd == -1) {
        diff_str = "EQUAL";
    } else {
        static char diff_idx_str[12];
        (void)sprintf(diff_idx_str, "%d", optd);
        diff_str = diff_idx_str;
    }

    (void)sprintf(result, "%8lld%12s", opts, diff_str);

    return (result);
}
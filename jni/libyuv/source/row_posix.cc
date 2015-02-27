/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// This module is for GCC x86 and x64
#if !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) || defined(__i386__))

// TODO(nfullagar): For Native Client: When new toolchain becomes available,
// take advantage of bundle lock / unlock feature. This will reduce the amount
// of manual bundle alignment done below, and bundle alignment could even be
// moved into each macro that doesn't use %%nacl: such as MEMOPREG. Consider
// unmunging functions to reduce complex addressing modes.

#if defined(__native_client__) && defined(__x86_64__)
#define MEMACCESS(base) "%%nacl:(%%r15,%q" #base ")"
#define MEMACCESS2(offset, base) "%%nacl:" #offset "(%%r15,%q" #base ")"
#define MEMLEA(offset, base) #offset "(%q" #base ")"
#define MEMLEA4(offset, base, index, scale) \
    #offset "(%q" #base ",%q" #index "," #scale ")"
#define MEMMOVESTRING(s, d) "%%nacl:(%q" #s "),%%nacl:(%q" #d "), %%r15"
#define MEMSTORESTRING(reg, d) "%%" #reg ",%%nacl:(%q" #d "), %%r15"
#define MEMOPREG(opcode, offset, base, index, scale, reg) \
    "lea " #offset "(%q" #base ",%q" #index "," #scale "),%%r14d\n" \
    #opcode " (%%r15,%%r14),%%" #reg "\n"
#define MEMOPMEM(opcode, reg, offset, base, index, scale) \
    "lea " #offset "(%q" #base ",%q" #index "," #scale "),%%r14d\n" \
    #opcode " %%" #reg ",(%%r15,%%r14)\n"
#define MEMOP(opcode, offset, base, index, scale) \
    "lea " #offset "(%q" #base ",%q" #index "," #scale "),%%r14d\n" \
    #opcode " (%%r15,%%r14)"
#define BUNDLEALIGN ".p2align 5 \n"
#else
#define MEMACCESS(base) "(%" #base ")"
#define MEMACCESS2(offset, base) #offset "(%" #base ")"
#define MEMLEA(offset, base) #offset "(%" #base ")"
#define MEMLEA4(offset, base, index, scale) \
    #offset "(%" #base ",%" #index "," #scale ")"
#define MEMMOVESTRING(s, d)
#define MEMSTORESTRING(reg, d)
#define MEMOPREG(opcode, offset, base, index, scale, reg) \
    #opcode " " #offset "(%" #base ",%" #index "," #scale "),%%" #reg "\n"
#define MEMOPMEM(opcode, reg, offset, base, index, scale) \
    #opcode " %%" #reg ","#offset "(%" #base ",%" #index "," #scale ")\n"
#define MEMOP(opcode, offset, base, index, scale) \
    #opcode " " #offset "(%" #base ",%" #index "," #scale ")"
#define BUNDLEALIGN
#endif

#if defined(HAS_ARGBTOYROW_SSSE3) || defined(HAS_ARGBGRAYROW_SSSE3)

// Constants for ARGB
static vec8 kARGBToY = {
  13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0
};

// JPeg full range.
static vec8 kARGBToYJ = {
  15, 75, 38, 0, 15, 75, 38, 0, 15, 75, 38, 0, 15, 75, 38, 0
};
#endif  // defined(HAS_ARGBTOYROW_SSSE3) || defined(HAS_ARGBGRAYROW_SSSE3)

#if defined(HAS_ARGBTOYROW_SSSE3) || defined(HAS_I422TOARGBROW_SSSE3)

static vec8 kARGBToU = {
  112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0
};

static vec8 kARGBToUJ = {
  127, -84, -43, 0, 127, -84, -43, 0, 127, -84, -43, 0, 127, -84, -43, 0
};

static vec8 kARGBToV = {
  -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0,
};

static vec8 kARGBToVJ = {
  -20, -107, 127, 0, -20, -107, 127, 0, -20, -107, 127, 0, -20, -107, 127, 0
};

// Constants for BGRA
static vec8 kBGRAToY = {
  0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13
};

static vec8 kBGRAToU = {
  0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112
};

static vec8 kBGRAToV = {
  0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18
};

// Constants for ABGR
static vec8 kABGRToY = {
  33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0
};

static vec8 kABGRToU = {
  -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0
};

static vec8 kABGRToV = {
  112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0
};

// Constants for RGBA.
static vec8 kRGBAToY = {
  0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33
};

static vec8 kRGBAToU = {
  0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38
};

static vec8 kRGBAToV = {
  0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112
};

static uvec8 kAddY16 = {
  16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u
};

static vec16 kAddYJ64 = {
  64, 64, 64, 64, 64, 64, 64, 64
};

static uvec8 kAddUV128 = {
  128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
  128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u
};

static uvec16 kAddUVJ128 = {
  0x8080u, 0x8080u, 0x8080u, 0x8080u, 0x8080u, 0x8080u, 0x8080u, 0x8080u
};
#endif  // defined(HAS_ARGBTOYROW_SSSE3) || defined(HAS_I422TOARGBROW_SSSE3)

#ifdef HAS_RGB24TOARGBROW_SSSE3

// Shuffle table for converting RGB24 to ARGB.
static uvec8 kShuffleMaskRGB24ToARGB = {
  0u, 1u, 2u, 12u, 3u, 4u, 5u, 13u, 6u, 7u, 8u, 14u, 9u, 10u, 11u, 15u
};

// Shuffle table for converting RAW to ARGB.
static uvec8 kShuffleMaskRAWToARGB = {
  2u, 1u, 0u, 12u, 5u, 4u, 3u, 13u, 8u, 7u, 6u, 14u, 11u, 10u, 9u, 15u
};

// Shuffle table for converting ARGB to RGB24.
static uvec8 kShuffleMaskARGBToRGB24 = {
  0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u, 128u, 128u, 128u, 128u
};

// Shuffle table for converting ARGB to RAW.
static uvec8 kShuffleMaskARGBToRAW = {
  2u, 1u, 0u, 6u, 5u, 4u, 10u, 9u, 8u, 14u, 13u, 12u, 128u, 128u, 128u, 128u
};

// Shuffle table for converting ARGBToRGB24 for I422ToRGB24.  First 8 + next 4
static uvec8 kShuffleMaskARGBToRGB24_0 = {
  0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 128u, 128u, 128u, 128u, 10u, 12u, 13u, 14u
};

// Shuffle table for converting ARGB to RAW.
static uvec8 kShuffleMaskARGBToRAW_0 = {
  2u, 1u, 0u, 6u, 5u, 4u, 10u, 9u, 128u, 128u, 128u, 128u, 8u, 14u, 13u, 12u
};
#endif  // HAS_RGB24TOARGBROW_SSSE3

#if defined(TESTING) && defined(__x86_64__)
void TestRow_SSE2(const uint8* src_y, uint8* dst_argb, int pix) {
  asm volatile (
    ".p2align  5                               \n"
    "mov       %%eax,%%eax                     \n"
    "mov       %%ebx,%%ebx                     \n"
    "mov       %%ecx,%%ecx                     \n"
    "mov       %%edx,%%edx                     \n"
    "mov       %%esi,%%esi                     \n"
    "mov       %%edi,%%edi                     \n"
    "mov       %%ebp,%%ebp                     \n"
    "mov       %%esp,%%esp                     \n"
    ".p2align  5                               \n"
    "mov       %%r8d,%%r8d                     \n"
    "mov       %%r9d,%%r9d                     \n"
    "mov       %%r10d,%%r10d                   \n"
    "mov       %%r11d,%%r11d                   \n"
    "mov       %%r12d,%%r12d                   \n"
    "mov       %%r13d,%%r13d                   \n"
    "mov       %%r14d,%%r14d                   \n"
    "mov       %%r15d,%%r15d                   \n"
    ".p2align  5                               \n"
    "lea       (%%rax),%%eax                   \n"
    "lea       (%%rbx),%%ebx                   \n"
    "lea       (%%rcx),%%ecx                   \n"
    "lea       (%%rdx),%%edx                   \n"
    "lea       (%%rsi),%%esi                   \n"
    "lea       (%%rdi),%%edi                   \n"
    "lea       (%%rbp),%%ebp                   \n"
    "lea       (%%rsp),%%esp                   \n"
    ".p2align  5                               \n"
    "lea       (%%r8),%%r8d                    \n"
    "lea       (%%r9),%%r9d                    \n"
    "lea       (%%r10),%%r10d                  \n"
    "lea       (%%r11),%%r11d                  \n"
    "lea       (%%r12),%%r12d                  \n"
    "lea       (%%r13),%%r13d                  \n"
    "lea       (%%r14),%%r14d                  \n"
    "lea       (%%r15),%%r15d                  \n"

    ".p2align  5                               \n"
    "lea       0x10(%%rax),%%eax               \n"
    "lea       0x10(%%rbx),%%ebx               \n"
    "lea       0x10(%%rcx),%%ecx               \n"
    "lea       0x10(%%rdx),%%edx               \n"
    "lea       0x10(%%rsi),%%esi               \n"
    "lea       0x10(%%rdi),%%edi               \n"
    "lea       0x10(%%rbp),%%ebp               \n"
    "lea       0x10(%%rsp),%%esp               \n"
    ".p2align  5                               \n"
    "lea       0x10(%%r8),%%r8d                \n"
    "lea       0x10(%%r9),%%r9d                \n"
    "lea       0x10(%%r10),%%r10d              \n"
    "lea       0x10(%%r11),%%r11d              \n"
    "lea       0x10(%%r12),%%r12d              \n"
    "lea       0x10(%%r13),%%r13d              \n"
    "lea       0x10(%%r14),%%r14d              \n"
    "lea       0x10(%%r15),%%r15d              \n"

    ".p2align  5                               \n"
    "add       0x10,%%eax                      \n"
    "add       0x10,%%ebx                      \n"
    "add       0x10,%%ecx                      \n"
    "add       0x10,%%edx                      \n"
    "add       0x10,%%esi                      \n"
    "add       0x10,%%edi                      \n"
    "add       0x10,%%ebp                      \n"
    "add       0x10,%%esp                      \n"
    ".p2align  5                               \n"
    "add       0x10,%%r8d                      \n"
    "add       0x10,%%r9d                      \n"
    "add       0x10,%%r10d                     \n"
    "add       0x10,%%r11d                     \n"
    "add       0x10,%%r12d                     \n"
    "add       0x10,%%r13d                     \n"
    "add       0x10,%%r14d                     \n"
    "add       0x10,%%r15d                     \n"

    ".p2align  4                               \n"
  "1:                                          \n"
    "movq      "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x8,0)",%0              \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src_y),     // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // TESTING

#ifdef HAS_I400TOARGBROW_SSE2
void I400ToARGBRow_SSE2(const uint8* src_y, uint8* dst_argb, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pslld     $0x18,%%xmm5                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movq      "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x8,0)",%0              \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm0,%%xmm0                   \n"
    "punpckhwd %%xmm1,%%xmm1                   \n"
    "por       %%xmm5,%%xmm0                   \n"
    "por       %%xmm5,%%xmm1                   \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqa    %%xmm1,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src_y),     // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void I400ToARGBRow_Unaligned_SSE2(const uint8* src_y, uint8* dst_argb,
                                  int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pslld     $0x18,%%xmm5                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movq      "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x8,0)",%0              \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm0,%%xmm0                   \n"
    "punpckhwd %%xmm1,%%xmm1                   \n"
    "por       %%xmm5,%%xmm0                   \n"
    "por       %%xmm5,%%xmm1                   \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqu    %%xmm1,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src_y),     // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // HAS_I400TOARGBROW_SSE2

#ifdef HAS_RGB24TOARGBROW_SSSE3
void RGB24ToARGBRow_SSSE3(const uint8* src_rgb24, uint8* dst_argb, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"  // generate mask 0xff000000
    "pslld     $0x18,%%xmm5                    \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqu    0x10(%0),%%xmm1                 \n"
    "movdqu    0x20(%0),%%xmm3                 \n"
    "lea       0x30(%0),%0                     \n"
    "movdqa    %%xmm3,%%xmm2                   \n"
    "palignr   $0x8,%%xmm1,%%xmm2              \n"
    "pshufb    %%xmm4,%%xmm2                   \n"
    "por       %%xmm5,%%xmm2                   \n"
    "palignr   $0xc,%%xmm0,%%xmm1              \n"
    "pshufb    %%xmm4,%%xmm0                   \n"
    "movdqa    %%xmm2,0x20(%1)                 \n"
    "por       %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm4,%%xmm1                   \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "por       %%xmm5,%%xmm1                   \n"
    "palignr   $0x4,%%xmm3,%%xmm3              \n"
    "pshufb    %%xmm4,%%xmm3                   \n"
    "movdqa    %%xmm1,0x10(%1)                 \n"
    "por       %%xmm5,%%xmm3                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm3,0x30(%1)                 \n"
    "lea       0x40(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src_rgb24),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "m"(kShuffleMaskRGB24ToARGB)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void RAWToARGBRow_SSSE3(const uint8* src_raw, uint8* dst_argb, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"  // generate mask 0xff000000
    "pslld     $0x18,%%xmm5                    \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqu    0x10(%0),%%xmm1                 \n"
    "movdqu    0x20(%0),%%xmm3                 \n"
    "lea       0x30(%0),%0                     \n"
    "movdqa    %%xmm3,%%xmm2                   \n"
    "palignr   $0x8,%%xmm1,%%xmm2              \n"
    "pshufb    %%xmm4,%%xmm2                   \n"
    "por       %%xmm5,%%xmm2                   \n"
    "palignr   $0xc,%%xmm0,%%xmm1              \n"
    "pshufb    %%xmm4,%%xmm0                   \n"
    "movdqa    %%xmm2,0x20(%1)                 \n"
    "por       %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm4,%%xmm1                   \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "por       %%xmm5,%%xmm1                   \n"
    "palignr   $0x4,%%xmm3,%%xmm3              \n"
    "pshufb    %%xmm4,%%xmm3                   \n"
    "movdqa    %%xmm1,0x10(%1)                 \n"
    "por       %%xmm5,%%xmm3                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm3,0x30(%1)                 \n"
    "lea       0x40(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src_raw),   // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "m"(kShuffleMaskRAWToARGB)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void RGB565ToARGBRow_SSE2(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "mov       $0x1080108,%%eax                \n"
    "movd      %%eax,%%xmm5                    \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    "mov       $0x20802080,%%eax               \n"
    "movd      %%eax,%%xmm6                    \n"
    "pshufd    $0x0,%%xmm6,%%xmm6              \n"
    "pcmpeqb   %%xmm3,%%xmm3                   \n"
    "psllw     $0xb,%%xmm3                     \n"
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "psllw     $0xa,%%xmm4                     \n"
    "psrlw     $0x5,%%xmm4                     \n"
    "pcmpeqb   %%xmm7,%%xmm7                   \n"
    "psllw     $0x8,%%xmm7                     \n"
    "sub       %0,%1                           \n"
    "sub       %0,%1                           \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "pand      %%xmm3,%%xmm1                   \n"
    "psllw     $0xb,%%xmm2                     \n"
    "pmulhuw   %%xmm5,%%xmm1                   \n"
    "pmulhuw   %%xmm5,%%xmm2                   \n"
    "psllw     $0x8,%%xmm1                     \n"
    "por       %%xmm2,%%xmm1                   \n"
    "pand      %%xmm4,%%xmm0                   \n"
    "pmulhuw   %%xmm6,%%xmm0                   \n"
    "por       %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm1,%%xmm2                   \n"
    "punpcklbw %%xmm0,%%xmm1                   \n"
    "punpckhbw %%xmm0,%%xmm2                   \n"
    "movdqa    %%xmm1,(%1,%0,2)                \n"
    "movdqa    %%xmm2,0x10(%1,%0,2)            \n"
    "lea       0x10(%0),%0                     \n"
    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  :
  : "memory", "cc", "eax"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}

void ARGB1555ToARGBRow_SSE2(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "mov       $0x1080108,%%eax                \n"
    "movd      %%eax,%%xmm5                    \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    "mov       $0x42004200,%%eax               \n"
    "movd      %%eax,%%xmm6                    \n"
    "pshufd    $0x0,%%xmm6,%%xmm6              \n"
    "pcmpeqb   %%xmm3,%%xmm3                   \n"
    "psllw     $0xb,%%xmm3                     \n"
    "movdqa    %%xmm3,%%xmm4                   \n"
    "psrlw     $0x6,%%xmm4                     \n"
    "pcmpeqb   %%xmm7,%%xmm7                   \n"
    "psllw     $0x8,%%xmm7                     \n"
    "sub       %0,%1                           \n"
    "sub       %0,%1                           \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "psllw     $0x1,%%xmm1                     \n"
    "psllw     $0xb,%%xmm2                     \n"
    "pand      %%xmm3,%%xmm1                   \n"
    "pmulhuw   %%xmm5,%%xmm2                   \n"
    "pmulhuw   %%xmm5,%%xmm1                   \n"
    "psllw     $0x8,%%xmm1                     \n"
    "por       %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "pand      %%xmm4,%%xmm0                   \n"
    "psraw     $0x8,%%xmm2                     \n"
    "pmulhuw   %%xmm6,%%xmm0                   \n"
    "pand      %%xmm7,%%xmm2                   \n"
    "por       %%xmm2,%%xmm0                   \n"
    "movdqa    %%xmm1,%%xmm2                   \n"
    "punpcklbw %%xmm0,%%xmm1                   \n"
    "punpckhbw %%xmm0,%%xmm2                   \n"
    "movdqa    %%xmm1,(%1,%0,2)                \n"
    "movdqa    %%xmm2,0x10(%1,%0,2)            \n"
    "lea       0x10(%0),%0                     \n"
    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  :
  : "memory", "cc", "eax"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}

void ARGB4444ToARGBRow_SSE2(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "mov       $0xf0f0f0f,%%eax                \n"
    "movd      %%eax,%%xmm4                    \n"
    "pshufd    $0x0,%%xmm4,%%xmm4              \n"
    "movdqa    %%xmm4,%%xmm5                   \n"
    "pslld     $0x4,%%xmm5                     \n"
    "sub       %0,%1                           \n"
    "sub       %0,%1                           \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "pand      %%xmm4,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm3                   \n"
    "psllw     $0x4,%%xmm1                     \n"
    "psrlw     $0x4,%%xmm3                     \n"
    "por       %%xmm1,%%xmm0                   \n"
    "por       %%xmm3,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm2,%%xmm0                   \n"
    "punpckhbw %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,(%1,%0,2)                \n"
    "movdqa    %%xmm1,0x10(%1,%0,2)            \n"
    "lea       0x10(%0),%0                     \n"
    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  :
  : "memory", "cc", "eax"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void ARGBToRGB24Row_SSSE3(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "movdqa    %3,%%xmm6                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqu    0x10(%0),%%xmm1                 \n"
    "movdqu    0x20(%0),%%xmm2                 \n"
    "movdqu    0x30(%0),%%xmm3                 \n"
    "lea       0x40(%0),%0                     \n"
    "pshufb    %%xmm6,%%xmm0                   \n"
    "pshufb    %%xmm6,%%xmm1                   \n"
    "pshufb    %%xmm6,%%xmm2                   \n"
    "pshufb    %%xmm6,%%xmm3                   \n"
    "movdqa    %%xmm1,%%xmm4                   \n"
    "psrldq    $0x4,%%xmm1                     \n"
    "pslldq    $0xc,%%xmm4                     \n"
    "movdqa    %%xmm2,%%xmm5                   \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pslldq    $0x8,%%xmm5                     \n"
    "movdqu    %%xmm0,(%1)                     \n"
    "por       %%xmm5,%%xmm1                   \n"
    "psrldq    $0x8,%%xmm2                     \n"
    "pslldq    $0x4,%%xmm3                     \n"
    "por       %%xmm3,%%xmm2                   \n"
    "movdqu    %%xmm1,0x10(%1)                 \n"
    "movdqu    %%xmm2,0x20(%1)                 \n"
    "lea       0x30(%1),%1                     \n"
    "sub       $0x10,%2                        \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  : "m"(kShuffleMaskARGBToRGB24)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}

void ARGBToRAWRow_SSSE3(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "movdqa    %3,%%xmm6                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm0                     \n"
    "movdqu    0x10(%0),%%xmm1                 \n"
    "movdqu    0x20(%0),%%xmm2                 \n"
    "movdqu    0x30(%0),%%xmm3                 \n"
    "lea       0x40(%0),%0                     \n"
    "pshufb    %%xmm6,%%xmm0                   \n"
    "pshufb    %%xmm6,%%xmm1                   \n"
    "pshufb    %%xmm6,%%xmm2                   \n"
    "pshufb    %%xmm6,%%xmm3                   \n"
    "movdqa    %%xmm1,%%xmm4                   \n"
    "psrldq    $0x4,%%xmm1                     \n"
    "pslldq    $0xc,%%xmm4                     \n"
    "movdqa    %%xmm2,%%xmm5                   \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pslldq    $0x8,%%xmm5                     \n"
    "movdqu    %%xmm0,(%1)                     \n"
    "por       %%xmm5,%%xmm1                   \n"
    "psrldq    $0x8,%%xmm2                     \n"
    "pslldq    $0x4,%%xmm3                     \n"
    "por       %%xmm3,%%xmm2                   \n"
    "movdqu    %%xmm1,0x10(%1)                 \n"
    "movdqu    %%xmm2,0x20(%1)                 \n"
    "lea       0x30(%1),%1                     \n"
    "sub       $0x10,%2                        \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  : "m"(kShuffleMaskARGBToRAW)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}

void ARGBToRGB565Row_SSE2(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm3,%%xmm3                   \n"
    "psrld     $0x1b,%%xmm3                    \n"
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "psrld     $0x1a,%%xmm4                    \n"
    "pslld     $0x5,%%xmm4                     \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pslld     $0xb,%%xmm5                     \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "pslld     $0x8,%%xmm0                     \n"
    "psrld     $0x3,%%xmm1                     \n"
    "psrld     $0x5,%%xmm2                     \n"
    "psrad     $0x10,%%xmm0                    \n"
    "pand      %%xmm3,%%xmm1                   \n"
    "pand      %%xmm4,%%xmm2                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "por       %%xmm2,%%xmm1                   \n"
    "por       %%xmm1,%%xmm0                   \n"
    "packssdw  %%xmm0,%%xmm0                   \n"
    "lea       0x10(%0),%0                     \n"
    "movq      %%xmm0,(%1)                     \n"
    "lea       0x8(%1),%1                      \n"
    "sub       $0x4,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void ARGBToARGB1555Row_SSE2(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "psrld     $0x1b,%%xmm4                    \n"
    "movdqa    %%xmm4,%%xmm5                   \n"
    "pslld     $0x5,%%xmm5                     \n"
    "movdqa    %%xmm4,%%xmm6                   \n"
    "pslld     $0xa,%%xmm6                     \n"
    "pcmpeqb   %%xmm7,%%xmm7                   \n"
    "pslld     $0xf,%%xmm7                     \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm3                   \n"
    "psrad     $0x10,%%xmm0                    \n"
    "psrld     $0x3,%%xmm1                     \n"
    "psrld     $0x6,%%xmm2                     \n"
    "psrld     $0x9,%%xmm3                     \n"
    "pand      %%xmm7,%%xmm0                   \n"
    "pand      %%xmm4,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm2                   \n"
    "pand      %%xmm6,%%xmm3                   \n"
    "por       %%xmm1,%%xmm0                   \n"
    "por       %%xmm3,%%xmm2                   \n"
    "por       %%xmm2,%%xmm0                   \n"
    "packssdw  %%xmm0,%%xmm0                   \n"
    "lea       0x10(%0),%0                     \n"
    "movq      %%xmm0,(%1)                     \n"
    "lea       0x8(%1),%1                      \n"
    "sub       $0x4,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}

void ARGBToARGB4444Row_SSE2(const uint8* src, uint8* dst, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "psllw     $0xc,%%xmm4                     \n"
    "movdqa    %%xmm4,%%xmm3                   \n"
    "psrlw     $0x8,%%xmm3                     \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm3,%%xmm0                   \n"
    "pand      %%xmm4,%%xmm1                   \n"
    "psrlq     $0x4,%%xmm0                     \n"
    "psrlq     $0x8,%%xmm1                     \n"
    "por       %%xmm1,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "lea       0x10(%0),%0                     \n"
    "movq      %%xmm0,(%1)                     \n"
    "lea       0x8(%1),%1                      \n"
    "sub       $0x4,%2                         \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(pix)   // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#endif
  );
}
#endif  // HAS_RGB24TOARGBROW_SSSE3

#ifdef HAS_ARGBTOYROW_SSSE3
void ARGBToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kARGBToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void ARGBToYRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kARGBToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBTOYROW_SSSE3

#ifdef HAS_ARGBTOYJROW_SSSE3
void ARGBToYJRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %3,%%xmm4                       \n"
    "movdqa    %4,%%xmm5                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "paddw     %%xmm5,%%xmm0                   \n"
    "paddw     %%xmm5,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kARGBToYJ),  // %3
    "m"(kAddYJ64)    // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void ARGBToYJRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %3,%%xmm4                       \n"
    "movdqa    %4,%%xmm5                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "paddw     %%xmm5,%%xmm0                   \n"
    "paddw     %%xmm5,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kARGBToYJ),  // %3
    "m"(kAddYJ64)    // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBTOYJROW_SSSE3

#ifdef HAS_ARGBTOUVROW_SSSE3
// TODO(fbarchard): pass xmm constants to single block of assembly.
// fpic on GCC 4.2 for OSX runs out of GPR registers. "m" effectively takes
// 3 registers - ebx, ebp and eax. "m" can be passed with 3 normal registers,
// or 4 if stack frame is disabled. Doing 2 assembly blocks is a work around
// and considered unsafe.
void ARGBToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToU),  // %0
    "m"(kARGBToV),  // %1
    "m"(kAddUV128)  // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(pavgb,0x00,0,4,1,xmm0)            //  pavgb   (%0,%4,1),%%xmm0
    MEMOPREG(pavgb,0x10,0,4,1,xmm1)            //  pavgb   0x10(%0,%4,1),%%xmm1
    MEMOPREG(pavgb,0x20,0,4,1,xmm2)            //  pavgb   0x20(%0,%4,1),%%xmm2
    MEMOPREG(pavgb,0x30,0,4,1,xmm6)            //  pavgb   0x30(%0,%4,1),%%xmm6
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps    %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_argb)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

// TODO(fbarchard): Share code with ARGBToUVRow_SSSE3.
void ARGBToUVJRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                        uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToUJ),  // %0
    "m"(kARGBToVJ),  // %1
    "m"(kAddUVJ128)  // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(pavgb,0x00,0,4,1,xmm0)            //  pavgb   (%0,%4,1),%%xmm0
    MEMOPREG(pavgb,0x10,0,4,1,xmm1)            //  pavgb   0x10(%0,%4,1),%%xmm1
    MEMOPREG(pavgb,0x20,0,4,1,xmm2)            //  pavgb   0x20(%0,%4,1),%%xmm2
    MEMOPREG(pavgb,0x30,0,4,1,xmm6)            //  pavgb   0x30(%0,%4,1),%%xmm6
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "paddw     %%xmm5,%%xmm0                   \n"
    "paddw     %%xmm5,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_argb)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void ARGBToUVRow_Unaligned_SSSE3(const uint8* src_argb0, int src_stride_argb,
                                 uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToU),         // %0
    "m"(kARGBToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm7)           //  movdqu  (%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm0                   \n"
    MEMOPREG(movdqu,0x10,0,4,1,xmm7)           //  movdqu  0x10(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm1                   \n"
    MEMOPREG(movdqu,0x20,0,4,1,xmm7)           //  movdqu  0x20(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm2                   \n"
    MEMOPREG(movdqu,0x30,0,4,1,xmm7)           //  movdqu  0x30(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm6                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_argb)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void ARGBToUVJRow_Unaligned_SSSE3(const uint8* src_argb0, int src_stride_argb,
                                  uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToUJ),         // %0
    "m"(kARGBToVJ),         // %1
    "m"(kAddUVJ128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm7)           //  movdqu  (%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm0                   \n"
    MEMOPREG(movdqu,0x10,0,4,1,xmm7)           //  movdqu  0x10(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm1                   \n"
    MEMOPREG(movdqu,0x20,0,4,1,xmm7)           //  movdqu  0x20(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm2                   \n"
    MEMOPREG(movdqu,0x30,0,4,1,xmm7)           //  movdqu  0x30(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm6                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "paddw     %%xmm5,%%xmm0                   \n"
    "paddw     %%xmm5,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_argb))
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void ARGBToUV444Row_SSSE3(const uint8* src_argb, uint8* dst_u, uint8* dst_v,
                          int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToU),  // %0
    "m"(kARGBToV),  // %1
    "m"(kAddUV128)  // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm6                   \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm2                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm2                     \n"
    "packsswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    "pmaddubsw %%xmm3,%%xmm0                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm2                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm2                     \n"
    "packsswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,2,1)           //  movdqa  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),        // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6"
#endif
  );
}

void ARGBToUV444Row_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_u,
                                    uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToU),  // %0
    "m"(kARGBToV),  // %1
    "m"(kAddUV128)  // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm6                   \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm2                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm2                     \n"
    "packsswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    "pmaddubsw %%xmm3,%%xmm0                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm2                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm2                     \n"
    "packsswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,2,1)           //  movdqu  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),        // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6"
#endif
  );
}

void ARGBToUV422Row_SSSE3(const uint8* src_argb0,
                          uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToU),  // %0
    "m"(kARGBToV),  // %1
    "m"(kAddUV128)  // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void ARGBToUV422Row_Unaligned_SSSE3(const uint8* src_argb0,
                                    uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kARGBToU),  // %0
    "m"(kARGBToV),  // %1
    "m"(kAddUV128)  // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void BGRAToYRow_SSSE3(const uint8* src_bgra, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_bgra),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kBGRAToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void BGRAToYRow_Unaligned_SSSE3(const uint8* src_bgra, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_bgra),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kBGRAToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void BGRAToUVRow_SSSE3(const uint8* src_bgra0, int src_stride_bgra,
                       uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kBGRAToU),         // %0
    "m"(kBGRAToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(pavgb,0x00,0,4,1,xmm0)            //  pavgb   (%0,%4,1),%%xmm0
    MEMOPREG(pavgb,0x10,0,4,1,xmm1)            //  pavgb   0x10(%0,%4,1),%%xmm1
    MEMOPREG(pavgb,0x20,0,4,1,xmm2)            //  pavgb   0x20(%0,%4,1),%%xmm2
    MEMOPREG(pavgb,0x30,0,4,1,xmm6)            //  pavgb   0x30(%0,%4,1),%%xmm6
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_bgra0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_bgra)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void BGRAToUVRow_Unaligned_SSSE3(const uint8* src_bgra0, int src_stride_bgra,
                                 uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kBGRAToU),         // %0
    "m"(kBGRAToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm7)           //  movdqu  (%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm0                   \n"
    MEMOPREG(movdqu,0x10,0,4,1,xmm7)           //  movdqu  0x10(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm1                   \n"
    MEMOPREG(movdqu,0x20,0,4,1,xmm7)           //  movdqu  0x20(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm2                   \n"
    MEMOPREG(movdqu,0x30,0,4,1,xmm7)           //  movdqu  0x30(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm6                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_bgra0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_bgra)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void ABGRToYRow_SSSE3(const uint8* src_abgr, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_abgr),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kABGRToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void ABGRToYRow_Unaligned_SSSE3(const uint8* src_abgr, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_abgr),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kABGRToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void RGBAToYRow_SSSE3(const uint8* src_rgba, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_rgba),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kRGBAToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void RGBAToYRow_Unaligned_SSSE3(const uint8* src_rgba, uint8* dst_y, int pix) {
  asm volatile (
    "movdqa    %4,%%xmm5                       \n"
    "movdqa    %3,%%xmm4                       \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm4,%%xmm3                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "phaddw    %%xmm3,%%xmm2                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm2                     \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_rgba),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kRGBAToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void ABGRToUVRow_SSSE3(const uint8* src_abgr0, int src_stride_abgr,
                       uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kABGRToU),         // %0
    "m"(kABGRToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(pavgb,0x00,0,4,1,xmm0)            //  pavgb   (%0,%4,1),%%xmm0
    MEMOPREG(pavgb,0x10,0,4,1,xmm1)            //  pavgb   0x10(%0,%4,1),%%xmm1
    MEMOPREG(pavgb,0x20,0,4,1,xmm2)            //  pavgb   0x20(%0,%4,1),%%xmm2
    MEMOPREG(pavgb,0x30,0,4,1,xmm6)            //  pavgb   0x30(%0,%4,1),%%xmm6
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_abgr0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_abgr)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void ABGRToUVRow_Unaligned_SSSE3(const uint8* src_abgr0, int src_stride_abgr,
                                 uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kABGRToU),         // %0
    "m"(kABGRToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm7)           //  movdqu  (%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm0                   \n"
    MEMOPREG(movdqu,0x10,0,4,1,xmm7)           //  movdqu  0x10(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm1                   \n"
    MEMOPREG(movdqu,0x20,0,4,1,xmm7)           //  movdqu  0x20(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm2                   \n"
    MEMOPREG(movdqu,0x30,0,4,1,xmm7)           //  movdqu  0x30(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm6                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_abgr0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_abgr)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void RGBAToUVRow_SSSE3(const uint8* src_rgba0, int src_stride_rgba,
                       uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kRGBAToU),         // %0
    "m"(kRGBAToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(pavgb,0x00,0,4,1,xmm0)            //  pavgb   (%0,%4,1),%%xmm0
    MEMOPREG(pavgb,0x10,0,4,1,xmm1)            //  pavgb   0x10(%0,%4,1),%%xmm1
    MEMOPREG(pavgb,0x20,0,4,1,xmm2)            //  pavgb   0x20(%0,%4,1),%%xmm2
    MEMOPREG(pavgb,0x30,0,4,1,xmm6)            //  pavgb   0x30(%0,%4,1),%%xmm6
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_rgba0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_rgba))
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}

void RGBAToUVRow_Unaligned_SSSE3(const uint8* src_rgba0, int src_stride_rgba,
                                 uint8* dst_u, uint8* dst_v, int width) {
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm3                       \n"
    "movdqa    %2,%%xmm5                       \n"
  :
  : "m"(kRGBAToU),         // %0
    "m"(kRGBAToV),         // %1
    "m"(kAddUV128)         // %2
  );
  asm volatile (
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqu    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqu    "MEMACCESS2(0x30,0)",%%xmm6     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm7)           //  movdqu  (%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm0                   \n"
    MEMOPREG(movdqu,0x10,0,4,1,xmm7)           //  movdqu  0x10(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm1                   \n"
    MEMOPREG(movdqu,0x20,0,4,1,xmm7)           //  movdqu  0x20(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm2                   \n"
    MEMOPREG(movdqu,0x30,0,4,1,xmm7)           //  movdqu  0x30(%0,%4,1),%%xmm7
    "pavgb     %%xmm7,%%xmm6                   \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm7                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm7                   \n"
    "shufps    $0x88,%%xmm6,%%xmm2             \n"
    "shufps    $0xdd,%%xmm6,%%xmm7             \n"
    "pavgb     %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm6                   \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm2                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "phaddw    %%xmm2,%%xmm0                   \n"
    "phaddw    %%xmm6,%%xmm1                   \n"
    "psraw     $0x8,%%xmm0                     \n"
    "psraw     $0x8,%%xmm1                     \n"
    "packsswb  %%xmm1,%%xmm0                   \n"
    "paddb     %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movlps    %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movhps,xmm0,0x00,1,2,1)           //  movhps  %%xmm0,(%1,%2,1)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_rgba0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_rgba)) // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBTOUVROW_SSSE3

#ifdef HAS_I422TOARGBROW_SSSE3
#define UB 127 /* min(63,static_cast<int8>(2.018 * 64)) */
#define UG -25 /* static_cast<int8>(-0.391 * 64 - 0.5) */
#define UR 0

#define VB 0
#define VG -52 /* static_cast<int8>(-0.813 * 64 - 0.5) */
#define VR 102 /* static_cast<int8>(1.596 * 64 + 0.5) */

// Bias
#define BB UB * 128 + VB * 128
#define BG UG * 128 + VG * 128
#define BR UR * 128 + VR * 128

#define YG 74 /* static_cast<int8>(1.164 * 64 + 0.5) */

struct {
  vec8 kUVToB;  // 0
  vec8 kUVToG;  // 16
  vec8 kUVToR;  // 32
  vec16 kUVBiasB;  // 48
  vec16 kUVBiasG;  // 64
  vec16 kUVBiasR;  // 80
  vec16 kYSub16;  // 96
  vec16 kYToRgb;  // 112
  vec8 kVUToB;  // 128
  vec8 kVUToG;  // 144
  vec8 kVUToR;  // 160
} static SIMD_ALIGNED(kYuvConstants) = {
  { UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB },
  { UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG },
  { UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR },
  { BB, BB, BB, BB, BB, BB, BB, BB },
  { BG, BG, BG, BG, BG, BG, BG, BG },
  { BR, BR, BR, BR, BR, BR, BR, BR },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { YG, YG, YG, YG, YG, YG, YG, YG },
  { VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB },
  { VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG },
  { VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR }
};


// Read 8 UV from 411
#define READYUV444                                                             \
    "movq       (%[u_buf]),%%xmm0              \n"                             \
    "movq       (%[u_buf],%[v_buf],1),%%xmm1   \n"                             \
    "lea        0x8(%[u_buf]),%[u_buf]         \n"                             \
    "punpcklbw  %%xmm1,%%xmm0                  \n"                             \

// Read 4 UV from 422, upsample to 8 UV
#define READYUV422                                                             \
    "movd       (%[u_buf]),%%xmm0              \n"                             \
    "movd       (%[u_buf],%[v_buf],1),%%xmm1   \n"                             \
    "lea        0x4(%[u_buf]),%[u_buf]         \n"                             \
    "punpcklbw  %%xmm1,%%xmm0                  \n"                             \
    "punpcklwd  %%xmm0,%%xmm0                  \n"                             \

// Read 2 UV from 411, upsample to 8 UV
#define READYUV411                                                             \
    "movd       (%[u_buf]),%%xmm0              \n"                             \
    "movd       (%[u_buf],%[v_buf],1),%%xmm1   \n"                             \
    "lea        0x2(%[u_buf]),%[u_buf]         \n"                             \
    "punpcklbw  %%xmm1,%%xmm0                  \n"                             \
    "punpcklwd  %%xmm0,%%xmm0                  \n"                             \
    "punpckldq  %%xmm0,%%xmm0                  \n"                             \

// Read 4 UV from NV12, upsample to 8 UV
#define READNV12                                                               \
    "movq       (%[uv_buf]),%%xmm0             \n"                             \
    "lea        0x8(%[uv_buf]),%[uv_buf]       \n"                             \
    "punpcklwd  %%xmm0,%%xmm0                  \n"                             \

// Convert 8 pixels: 8 UV and 8 Y
#define YUVTORGB                                                               \
    "movdqa     %%xmm0,%%xmm1                  \n"                             \
    "movdqa     %%xmm0,%%xmm2                  \n"                             \
    "pmaddubsw  (%[kYuvConstants]),%%xmm0      \n"                             \
    "pmaddubsw  16(%[kYuvConstants]),%%xmm1    \n"                             \
    "pmaddubsw  32(%[kYuvConstants]),%%xmm2    \n"                             \
    "psubw      48(%[kYuvConstants]),%%xmm0    \n"                             \
    "psubw      64(%[kYuvConstants]),%%xmm1    \n"                             \
    "psubw      80(%[kYuvConstants]),%%xmm2    \n"                             \
    "movq       (%[y_buf]),%%xmm3              \n"                             \
    "lea        0x8(%[y_buf]),%[y_buf]         \n"                             \
    "punpcklbw  %%xmm4,%%xmm3                  \n"                             \
    "psubsw     96(%[kYuvConstants]),%%xmm3    \n"                             \
    "pmullw     112(%[kYuvConstants]),%%xmm3   \n"                             \
    "paddsw     %%xmm3,%%xmm0                  \n"                             \
    "paddsw     %%xmm3,%%xmm1                  \n"                             \
    "paddsw     %%xmm3,%%xmm2                  \n"                             \
    "psraw      $0x6,%%xmm0                    \n"                             \
    "psraw      $0x6,%%xmm1                    \n"                             \
    "psraw      $0x6,%%xmm2                    \n"                             \
    "packuswb   %%xmm0,%%xmm0                  \n"                             \
    "packuswb   %%xmm1,%%xmm1                  \n"                             \
    "packuswb   %%xmm2,%%xmm2                  \n"                             \

// Convert 8 pixels: 8 VU and 8 Y
#define YVUTORGB                                                               \
    "movdqa     %%xmm0,%%xmm1                  \n"                             \
    "movdqa     %%xmm0,%%xmm2                  \n"                             \
    "pmaddubsw  128(%[kYuvConstants]),%%xmm0   \n"                             \
    "pmaddubsw  144(%[kYuvConstants]),%%xmm1   \n"                             \
    "pmaddubsw  160(%[kYuvConstants]),%%xmm2   \n"                             \
    "psubw      48(%[kYuvConstants]),%%xmm0    \n"                             \
    "psubw      64(%[kYuvConstants]),%%xmm1    \n"                             \
    "psubw      80(%[kYuvConstants]),%%xmm2    \n"                             \
    "movq       (%[y_buf]),%%xmm3              \n"                             \
    "lea        0x8(%[y_buf]),%[y_buf]         \n"                             \
    "punpcklbw  %%xmm4,%%xmm3                  \n"                             \
    "psubsw     96(%[kYuvConstants]),%%xmm3    \n"                             \
    "pmullw     112(%[kYuvConstants]),%%xmm3   \n"                             \
    "paddsw     %%xmm3,%%xmm0                  \n"                             \
    "paddsw     %%xmm3,%%xmm1                  \n"                             \
    "paddsw     %%xmm3,%%xmm2                  \n"                             \
    "psraw      $0x6,%%xmm0                    \n"                             \
    "psraw      $0x6,%%xmm1                    \n"                             \
    "psraw      $0x6,%%xmm2                    \n"                             \
    "packuswb   %%xmm0,%%xmm0                  \n"                             \
    "packuswb   %%xmm1,%%xmm1                  \n"                             \
    "packuswb   %%xmm2,%%xmm2                  \n"                             \

void OMITFP I444ToARGBRow_SSSE3(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* dst_argb,
                                int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV444
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,(%[dst_argb])            \n"
    "movdqa    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToRGB24Row_SSSE3(const uint8* y_buf,
                                 const uint8* u_buf,
                                 const uint8* v_buf,
                                 uint8* dst_rgb24,
                                 int width) {
// fpic 32 bit gcc 4.2 on OSX runs out of GPR regs.
#if defined(__i386__)
  asm volatile (
    "movdqa    %[kShuffleMaskARGBToRGB24_0],%%xmm5 \n"
    "movdqa    %[kShuffleMaskARGBToRGB24],%%xmm6   \n"
  :: [kShuffleMaskARGBToRGB24_0]"m"(kShuffleMaskARGBToRGB24_0),
    [kShuffleMaskARGBToRGB24]"m"(kShuffleMaskARGBToRGB24));
#endif

  asm volatile (
#if !defined(__i386__)
    "movdqa    %[kShuffleMaskARGBToRGB24_0],%%xmm5 \n"
    "movdqa    %[kShuffleMaskARGBToRGB24],%%xmm6   \n"
#endif
    "sub       %[u_buf],%[v_buf]               \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm2,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm6,%%xmm1                   \n"
    "palignr   $0xc,%%xmm0,%%xmm1              \n"
    "movq      %%xmm0,(%[dst_rgb24])           \n"
    "movdqu    %%xmm1,0x8(%[dst_rgb24])        \n"
    "lea       0x18(%[dst_rgb24]),%[dst_rgb24] \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgb24]"+r"(dst_rgb24),  // %[dst_rgb24]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB)
#if !defined(__i386__)
    , [kShuffleMaskARGBToRGB24_0]"m"(kShuffleMaskARGBToRGB24_0),
    [kShuffleMaskARGBToRGB24]"m"(kShuffleMaskARGBToRGB24)
#endif
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}

void OMITFP I422ToRAWRow_SSSE3(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* dst_raw,
                               int width) {
// fpic 32 bit gcc 4.2 on OSX runs out of GPR regs.
#if defined(__i386__)
  asm volatile (
    "movdqa    %[kShuffleMaskARGBToRAW_0],%%xmm5 \n"
    "movdqa    %[kShuffleMaskARGBToRAW],%%xmm6   \n"
  :: [kShuffleMaskARGBToRAW_0]"m"(kShuffleMaskARGBToRAW_0),
    [kShuffleMaskARGBToRAW]"m"(kShuffleMaskARGBToRAW));
#endif

  asm volatile (
#if !defined(__i386__)
    "movdqa    %[kShuffleMaskARGBToRAW_0],%%xmm5 \n"
    "movdqa    %[kShuffleMaskARGBToRAW],%%xmm6   \n"
#endif
    "sub       %[u_buf],%[v_buf]               \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm2,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm6,%%xmm1                   \n"
    "palignr   $0xc,%%xmm0,%%xmm1              \n"
    "movq      %%xmm0,(%[dst_raw])             \n"
    "movdqu    %%xmm1,0x8(%[dst_raw])          \n"
    "lea       0x18(%[dst_raw]),%[dst_raw]     \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_raw]"+r"(dst_raw),  // %[dst_raw]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB)
#if !defined(__i386__)
    , [kShuffleMaskARGBToRAW_0]"m"(kShuffleMaskARGBToRAW_0),
    [kShuffleMaskARGBToRAW]"m"(kShuffleMaskARGBToRAW)
#endif
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}

void OMITFP I422ToARGBRow_SSSE3(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* dst_argb,
                                int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,(%[dst_argb])            \n"
    "movdqa    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I411ToARGBRow_SSSE3(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* dst_argb,
                                int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV411
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,(%[dst_argb])            \n"
    "movdqa    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP NV12ToARGBRow_SSSE3(const uint8* y_buf,
                                const uint8* uv_buf,
                                uint8* dst_argb,
                                int width) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READNV12
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,(%[dst_argb])            \n"
    "movdqa    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [uv_buf]"+r"(uv_buf),    // %[uv_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP NV21ToARGBRow_SSSE3(const uint8* y_buf,
                                const uint8* uv_buf,
                                uint8* dst_argb,
                                int width) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READNV12
    YVUTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm0,(%[dst_argb])            \n"
    "movdqa    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [uv_buf]"+r"(uv_buf),    // %[uv_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I444ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* dst_argb,
                                          int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV444
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqu    %%xmm0,(%[dst_argb])            \n"
    "movdqu    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* dst_argb,
                                          int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqu    %%xmm0,(%[dst_argb])            \n"
    "movdqu    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I411ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* dst_argb,
                                          int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV411
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqu    %%xmm0,(%[dst_argb])            \n"
    "movdqu    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP NV12ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* uv_buf,
                                          uint8* dst_argb,
                                          int width) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READNV12
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqu    %%xmm0,(%[dst_argb])            \n"
    "movdqu    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [uv_buf]"+r"(uv_buf),    // %[uv_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP NV21ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* uv_buf,
                                          uint8* dst_argb,
                                          int width) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READNV12
    YVUTORGB
    "punpcklbw %%xmm1,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm0                   \n"
    "punpckhwd %%xmm2,%%xmm1                   \n"
    "movdqu    %%xmm0,(%[dst_argb])            \n"
    "movdqu    %%xmm1,0x10(%[dst_argb])        \n"
    "lea       0x20(%[dst_argb]),%[dst_argb]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [uv_buf]"+r"(uv_buf),    // %[uv_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToBGRARow_SSSE3(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* dst_bgra,
                                int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "punpcklbw %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm2,%%xmm5                   \n"
    "movdqa    %%xmm5,%%xmm0                   \n"
    "punpcklwd %%xmm1,%%xmm5                   \n"
    "punpckhwd %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm5,(%[dst_bgra])            \n"
    "movdqa    %%xmm0,0x10(%[dst_bgra])        \n"
    "lea       0x20(%[dst_bgra]),%[dst_bgra]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_bgra]"+r"(dst_bgra),  // %[dst_bgra]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToABGRRow_SSSE3(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* dst_abgr,
                                int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm2                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm1                   \n"
    "punpcklwd %%xmm0,%%xmm2                   \n"
    "punpckhwd %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,(%[dst_abgr])            \n"
    "movdqa    %%xmm1,0x10(%[dst_abgr])        \n"
    "lea       0x20(%[dst_abgr]),%[dst_abgr]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_abgr]"+r"(dst_abgr),  // %[dst_abgr]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToRGBARow_SSSE3(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* dst_rgba,
                                int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "punpcklbw %%xmm2,%%xmm1                   \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "movdqa    %%xmm5,%%xmm0                   \n"
    "punpcklwd %%xmm1,%%xmm5                   \n"
    "punpckhwd %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm5,(%[dst_rgba])            \n"
    "movdqa    %%xmm0,0x10(%[dst_rgba])        \n"
    "lea       0x20(%[dst_rgba]),%[dst_rgba]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgba]"+r"(dst_rgba),  // %[dst_rgba]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToBGRARow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* dst_bgra,
                                          int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "punpcklbw %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm2,%%xmm5                   \n"
    "movdqa    %%xmm5,%%xmm0                   \n"
    "punpcklwd %%xmm1,%%xmm5                   \n"
    "punpckhwd %%xmm1,%%xmm0                   \n"
    "movdqu    %%xmm5,(%[dst_bgra])            \n"
    "movdqu    %%xmm0,0x10(%[dst_bgra])        \n"
    "lea       0x20(%[dst_bgra]),%[dst_bgra]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_bgra]"+r"(dst_bgra),  // %[dst_bgra]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToABGRRow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* dst_abgr,
                                          int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "punpcklbw %%xmm1,%%xmm2                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm1                   \n"
    "punpcklwd %%xmm0,%%xmm2                   \n"
    "punpckhwd %%xmm0,%%xmm1                   \n"
    "movdqu    %%xmm2,(%[dst_abgr])            \n"
    "movdqu    %%xmm1,0x10(%[dst_abgr])        \n"
    "lea       0x20(%[dst_abgr]),%[dst_abgr]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_abgr]"+r"(dst_abgr),  // %[dst_abgr]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP I422ToRGBARow_Unaligned_SSSE3(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* dst_rgba,
                                          int width) {
  asm volatile (
    "sub       %[u_buf],%[v_buf]               \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    READYUV422
    YUVTORGB
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "punpcklbw %%xmm2,%%xmm1                   \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "movdqa    %%xmm5,%%xmm0                   \n"
    "punpcklwd %%xmm1,%%xmm5                   \n"
    "punpckhwd %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm5,(%[dst_rgba])            \n"
    "movdqa    %%xmm0,0x10(%[dst_rgba])        \n"
    "lea       0x20(%[dst_rgba]),%[dst_rgba]   \n"
    "sub       $0x8,%[width]                   \n"
    "jg        1b                              \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgba]"+r"(dst_rgba),  // %[dst_rgba]
    [width]"+rm"(width)    // %[width]
  : [kYuvConstants]"r"(&kYuvConstants.kUVToB) // %[kYuvConstants]
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

#endif  // HAS_I422TOARGBROW_SSSE3

#ifdef HAS_YTOARGBROW_SSE2
void YToARGBRow_SSE2(const uint8* y_buf,
                     uint8* dst_argb,
                     int width) {
  asm volatile (
    "pxor      %%xmm5,%%xmm5                   \n"
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "pslld     $0x18,%%xmm4                    \n"
    "mov       $0x00100010,%%eax               \n"
    "movd      %%eax,%%xmm3                    \n"
    "pshufd    $0x0,%%xmm3,%%xmm3              \n"
    "mov       $0x004a004a,%%eax               \n"
    "movd      %%eax,%%xmm2                    \n"
    "pshufd    $0x0,%%xmm2,%%xmm2              \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    // Step 1: Scale Y contribution to 8 G values. G = (y - 16) * 1.164
    "movq      (%0),%%xmm0                     \n"
    "lea       0x8(%0),%0                      \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "psubusw   %%xmm3,%%xmm0                   \n"
    "pmullw    %%xmm2,%%xmm0                   \n"
    "psrlw     $6, %%xmm0                      \n"
    "packuswb  %%xmm0,%%xmm0                   \n"

    // Step 2: Weave into ARGB
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm0,%%xmm0                   \n"
    "punpckhwd %%xmm1,%%xmm1                   \n"
    "por       %%xmm4,%%xmm0                   \n"
    "por       %%xmm4,%%xmm1                   \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "movdqa    %%xmm1,16(%1)                   \n"
    "lea       32(%1),%1                       \n"

    "sub       $0x8,%2                         \n"
    "jg        1b                              \n"
  : "+r"(y_buf),     // %0
    "+r"(dst_argb),  // %1
    "+rm"(width)     // %2
  :
  : "memory", "cc", "eax"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#endif
  );
}
#endif  // HAS_YTOARGBROW_SSE2

#ifdef HAS_MIRRORROW_SSSE3
// Shuffle table for reversing the bytes.
static uvec8 kShuffleMirror = {
  15u, 14u, 13u, 12u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u
};

void MirrorRow_SSSE3(const uint8* src, uint8* dst, int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
    "movdqa    %3,%%xmm5                       \n"
    "lea       -0x10(%0),%0                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0,%2),%%xmm0                  \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(temp_width)  // %2
  : "m"(kShuffleMirror) // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif
  );
}
#endif  // HAS_MIRRORROW_SSSE3

#ifdef HAS_MIRRORROW_SSE2
void MirrorRow_SSE2(const uint8* src, uint8* dst, int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
    "lea       -0x10(%0),%0                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0,%2),%%xmm0                  \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "psllw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm1,%%xmm0                   \n"
    "pshuflw   $0x1b,%%xmm0,%%xmm0             \n"
    "pshufhw   $0x1b,%%xmm0,%%xmm0             \n"
    "pshufd    $0x4e,%%xmm0,%%xmm0             \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,(%1)                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(temp_width)  // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}
#endif  // HAS_MIRRORROW_SSE2

#ifdef HAS_MIRRORROW_UV_SSSE3
// Shuffle table for reversing the bytes of UV channels.
static uvec8 kShuffleMirrorUV = {
  14u, 12u, 10u, 8u, 6u, 4u, 2u, 0u, 15u, 13u, 11u, 9u, 7u, 5u, 3u, 1u
};
void MirrorUVRow_SSSE3(const uint8* src, uint8* dst_u, uint8* dst_v,
                       int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
    "movdqa    %4,%%xmm1                       \n"
    "lea       -16(%0,%3,2),%0                 \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "lea       -16(%0),%0                      \n"
    "pshufb    %%xmm1,%%xmm0                   \n"
    "sub       $8,%3                           \n"
    "movlpd    %%xmm0,(%1)                     \n"
    "movhpd    %%xmm0,(%1,%2)                  \n"
    "lea       8(%1),%1                        \n"
    "jg        1b                              \n"
  : "+r"(src),      // %0
    "+r"(dst_u),    // %1
    "+r"(dst_v),    // %2
    "+r"(temp_width)  // %3
  : "m"(kShuffleMirrorUV)  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}
#endif  // HAS_MIRRORROW_UV_SSSE3

#ifdef HAS_ARGBMIRRORROW_SSSE3
// Shuffle table for reversing the bytes.
static uvec8 kARGBShuffleMirror = {
  12u, 13u, 14u, 15u, 8u, 9u, 10u, 11u, 4u, 5u, 6u, 7u, 0u, 1u, 2u, 3u
};

void ARGBMirrorRow_SSSE3(const uint8* src, uint8* dst, int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
    "lea       "MEMLEA4(-0x10,0,2,4)",%0       \n"
    "movdqa    %3,%%xmm5                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "lea       "MEMLEA(-0x10,0)",%0            \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(temp_width)  // %2
  : "m"(kARGBShuffleMirror)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBMIRRORROW_SSSE3

#ifdef HAS_SPLITUVROW_SSE2
void SplitUVRow_SSE2(const uint8* src_uv, uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb    %%xmm5,%%xmm5                    \n"
    "psrlw      $0x8,%%xmm5                      \n"
    "sub        %1,%2                            \n"
    ".p2align   4                                \n"
  "1:                                            \n"
    "movdqa     (%0),%%xmm0                      \n"
    "movdqa     0x10(%0),%%xmm1                  \n"
    "lea        0x20(%0),%0                      \n"
    "movdqa     %%xmm0,%%xmm2                    \n"
    "movdqa     %%xmm1,%%xmm3                    \n"
    "pand       %%xmm5,%%xmm0                    \n"
    "pand       %%xmm5,%%xmm1                    \n"
    "packuswb   %%xmm1,%%xmm0                    \n"
    "psrlw      $0x8,%%xmm2                      \n"
    "psrlw      $0x8,%%xmm3                      \n"
    "packuswb   %%xmm3,%%xmm2                    \n"
    "movdqa     %%xmm0,(%1)                      \n"
    "movdqa     %%xmm2,(%1,%2)                   \n"
    "lea        0x10(%1),%1                      \n"
    "sub        $0x10,%3                         \n"
    "jg         1b                               \n"
  : "+r"(src_uv),     // %0
    "+r"(dst_u),      // %1
    "+r"(dst_v),      // %2
    "+r"(pix)         // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}

void SplitUVRow_Unaligned_SSE2(const uint8* src_uv, uint8* dst_u, uint8* dst_v,
                               int pix) {
  asm volatile (
    "pcmpeqb    %%xmm5,%%xmm5                    \n"
    "psrlw      $0x8,%%xmm5                      \n"
    "sub        %1,%2                            \n"
    ".p2align   4                                \n"
  "1:                                            \n"
    "movdqu     (%0),%%xmm0                      \n"
    "movdqu     0x10(%0),%%xmm1                  \n"
    "lea        0x20(%0),%0                      \n"
    "movdqa     %%xmm0,%%xmm2                    \n"
    "movdqa     %%xmm1,%%xmm3                    \n"
    "pand       %%xmm5,%%xmm0                    \n"
    "pand       %%xmm5,%%xmm1                    \n"
    "packuswb   %%xmm1,%%xmm0                    \n"
    "psrlw      $0x8,%%xmm2                      \n"
    "psrlw      $0x8,%%xmm3                      \n"
    "packuswb   %%xmm3,%%xmm2                    \n"
    "movdqu     %%xmm0,(%1)                      \n"
    "movdqu     %%xmm2,(%1,%2)                   \n"
    "lea        0x10(%1),%1                      \n"
    "sub        $0x10,%3                         \n"
    "jg         1b                               \n"
  : "+r"(src_uv),     // %0
    "+r"(dst_u),      // %1
    "+r"(dst_v),      // %2
    "+r"(pix)         // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}
#endif  // HAS_SPLITUVROW_SSE2

#ifdef HAS_MERGEUVROW_SSE2
void MergeUVRow_SSE2(const uint8* src_u, const uint8* src_v, uint8* dst_uv,
                     int width) {
  asm volatile (
    "sub       %0,%1                             \n"
    ".p2align   4                                \n"
  "1:                                            \n"
    "movdqa    (%0),%%xmm0                       \n"
    "movdqa    (%0,%1,1),%%xmm1                  \n"
    "lea       0x10(%0),%0                       \n"
    "movdqa    %%xmm0,%%xmm2                     \n"
    "punpcklbw %%xmm1,%%xmm0                     \n"
    "punpckhbw %%xmm1,%%xmm2                     \n"
    "movdqa    %%xmm0,(%2)                       \n"
    "movdqa    %%xmm2,0x10(%2)                   \n"
    "lea       0x20(%2),%2                       \n"
    "sub       $0x10,%3                          \n"
    "jg        1b                                \n"
  : "+r"(src_u),     // %0
    "+r"(src_v),     // %1
    "+r"(dst_uv),    // %2
    "+r"(width)      // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2"
#endif
  );
}

void MergeUVRow_Unaligned_SSE2(const uint8* src_u, const uint8* src_v,
                               uint8* dst_uv, int width) {
  asm volatile (
    "sub       %0,%1                             \n"
    ".p2align   4                                \n"
  "1:                                            \n"
    "movdqu    (%0),%%xmm0                       \n"
    "movdqu    (%0,%1,1),%%xmm1                  \n"
    "lea       0x10(%0),%0                       \n"
    "movdqa    %%xmm0,%%xmm2                     \n"
    "punpcklbw %%xmm1,%%xmm0                     \n"
    "punpckhbw %%xmm1,%%xmm2                     \n"
    "movdqu    %%xmm0,(%2)                       \n"
    "movdqu    %%xmm2,0x10(%2)                   \n"
    "lea       0x20(%2),%2                       \n"
    "sub       $0x10,%3                          \n"
    "jg        1b                                \n"
  : "+r"(src_u),     // %0
    "+r"(src_v),     // %1
    "+r"(dst_uv),    // %2
    "+r"(width)      // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2"
#endif
  );
}
#endif  // HAS_MERGEUVROW_SSE2

#ifdef HAS_COPYROW_SSE2
void CopyRow_SSE2(const uint8* src, uint8* dst, int count) {
  asm volatile (
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqa    %%xmm1,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "sub       $0x20,%2                        \n"
    "jg        1b                              \n"
  : "+r"(src),   // %0
    "+r"(dst),   // %1
    "+r"(count)  // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}
#endif  // HAS_COPYROW_SSE2

#ifdef HAS_COPYROW_X86
void CopyRow_X86(const uint8* src, uint8* dst, int width) {
  size_t width_tmp = static_cast<size_t>(width);
  asm volatile (
    "shr       $0x2,%2                         \n"
    "rep movsl "MEMMOVESTRING(0,1)"            \n"
  : "+S"(src),  // %0
    "+D"(dst),  // %1
    "+c"(width_tmp) // %2
  :
  : "memory", "cc"
  );
}
#endif  // HAS_COPYROW_X86

#ifdef HAS_COPYROW_ERMS
// Unaligned Multiple of 1.
void CopyRow_ERMS(const uint8* src, uint8* dst, int width) {
  size_t width_tmp = static_cast<size_t>(width);
  asm volatile (
    "rep movsb "MEMMOVESTRING(0,1)"           \n"
  : "+S"(src),  // %0
    "+D"(dst),  // %1
    "+c"(width_tmp) // %2
  :
  : "memory", "cc"
  );
}
#endif  // HAS_COPYROW_ERMS

#ifdef HAS_SETROW_X86
void SetRow_X86(uint8* dst, uint32 v32, int width) {
  size_t width_tmp = static_cast<size_t>(width);
  asm volatile (
    "shr       $0x2,%1                         \n"
    "rep stosl "MEMSTORESTRING(eax,0)"         \n"
    : "+D"(dst),       // %0
      "+c"(width_tmp)  // %1
    : "a"(v32)         // %2
    : "memory", "cc");
}

void ARGBSetRows_X86(uint8* dst, uint32 v32, int width,
                   int dst_stride, int height) {
  for (int y = 0; y < height; ++y) {
    size_t width_tmp = static_cast<size_t>(width);
    uint32* d = reinterpret_cast<uint32*>(dst);
    asm volatile (
      "rep stosl "MEMSTORESTRING(eax,0)"       \n"
      : "+D"(d),         // %0
        "+c"(width_tmp)  // %1
      : "a"(v32)         // %2
      : "memory", "cc");
    dst += dst_stride;
  }
}
#endif  // HAS_SETROW_X86

#ifdef HAS_YUY2TOYROW_SSE2
void YUY2ToYRow_SSE2(const uint8* src_yuy2, uint8* dst_y, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "sub       $0x10,%2                        \n"
    "jg        1b                              \n"
  : "+r"(src_yuy2),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void YUY2ToUVRow_SSE2(const uint8* src_yuy2, int stride_yuy2,
                      uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    BUNDLEALIGN
    MEMOPREG(movdqa,0x00,0,4,1,xmm2)           //  movdqa  (%0,%4,1),%%xmm2
    MEMOPREG(movdqa,0x10,0,4,1,xmm3)           //  movdqa  0x10(%0,%4,1),%%xmm3
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_yuy2))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}

void YUY2ToUV422Row_SSE2(const uint8* src_yuy2,
                         uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void YUY2ToYRow_Unaligned_SSE2(const uint8* src_yuy2,
                               uint8* dst_y, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_yuy2),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void YUY2ToUVRow_Unaligned_SSE2(const uint8* src_yuy2,
                                int stride_yuy2,
                                uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm2)           //  movdqu  (%0,%4,1),%%xmm2
    MEMOPREG(movdqu,0x10,0,4,1,xmm3)           //  movdqu  0x10(%0,%4,1),%%xmm3
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_yuy2))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}

void YUY2ToUV422Row_Unaligned_SSE2(const uint8* src_yuy2,
                                   uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void UYVYToYRow_SSE2(const uint8* src_uyvy, uint8* dst_y, int pix) {
  asm volatile (
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_uyvy),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}

void UYVYToUVRow_SSE2(const uint8* src_uyvy, int stride_uyvy,
                      uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    BUNDLEALIGN
    MEMOPREG(movdqa,0x00,0,4,1,xmm2)           //  movdqa  (%0,%4,1),%%xmm2
    MEMOPREG(movdqa,0x10,0,4,1,xmm3)           //  movdqa  0x10(%0,%4,1),%%xmm3
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_uyvy),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_uyvy))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}

void UYVYToUV422Row_SSE2(const uint8* src_uyvy,
                         uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_uyvy),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void UYVYToYRow_Unaligned_SSE2(const uint8* src_uyvy,
                               uint8* dst_y, int pix) {
  asm volatile (
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_uyvy),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}

void UYVYToUVRow_Unaligned_SSE2(const uint8* src_uyvy, int stride_uyvy,
                                uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    BUNDLEALIGN
    MEMOPREG(movdqu,0x00,0,4,1,xmm2)           //  movdqu  (%0,%4,1),%%xmm2
    MEMOPREG(movdqu,0x10,0,4,1,xmm3)           //  movdqu  0x10(%0,%4,1),%%xmm3
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_uyvy),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_uyvy))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}

void UYVYToUV422Row_Unaligned_SSE2(const uint8* src_uyvy,
                                   uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrlw     $0x8,%%xmm5                     \n"
    "sub       %1,%2                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm1,0x00,1,2,1)             //  movq    %%xmm1,(%1,%2)
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "sub       $0x10,%3                        \n"
    "jg        1b                              \n"
  : "+r"(src_uyvy),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_v),       // %2
    "+r"(pix)          // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // HAS_YUY2TOYROW_SSE2

#ifdef HAS_ARGBBLENDROW_SSE2
// Blend 8 pixels at a time.
void ARGBBlendRow_SSE2(const uint8* src_argb0, const uint8* src_argb1,
                       uint8* dst_argb, int width) {
  asm volatile (
    "pcmpeqb   %%xmm7,%%xmm7                   \n"
    "psrlw     $0xf,%%xmm7                     \n"
    "pcmpeqb   %%xmm6,%%xmm6                   \n"
    "psrlw     $0x8,%%xmm6                     \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psllw     $0x8,%%xmm5                     \n"
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "pslld     $0x18,%%xmm4                    \n"
    "sub       $0x1,%3                         \n"
    "je        91f                             \n"
    "jl        99f                             \n"

    // 1 pixel loop until destination pointer is aligned.
  "10:                                         \n"
    "test      $0xf,%2                         \n"
    "je        19f                             \n"
    "movd      "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x4,0)",%0              \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movd      "MEMACCESS(1)",%%xmm2           \n"
    "psrlw     $0x8,%%xmm3                     \n"
    "pshufhw   $0xf5,%%xmm3,%%xmm3             \n"
    "pshuflw   $0xf5,%%xmm3,%%xmm3             \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movd      "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x4,1)",%1              \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x1,%3                         \n"
    "movd      %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x4,2)",%2              \n"
    "jge       10b                             \n"

  "19:                                         \n"
    "add       $1-4,%3                         \n"
    "jl        49f                             \n"

    // 4 pixel loop.
    ".p2align  2                               \n"
  "41:                                         \n"
    "movdqu    "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movdqu    "MEMACCESS(1)",%%xmm2           \n"
    "psrlw     $0x8,%%xmm3                     \n"
    "pshufhw   $0xf5,%%xmm3,%%xmm3             \n"
    "pshuflw   $0xf5,%%xmm3,%%xmm3             \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movdqu    "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jge       41b                             \n"

  "49:                                         \n"
    "add       $0x3,%3                         \n"
    "jl        99f                             \n"

    // 1 pixel loop.
  "91:                                         \n"
    "movd      "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x4,0)",%0              \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movd      "MEMACCESS(1)",%%xmm2           \n"
    "psrlw     $0x8,%%xmm3                     \n"
    "pshufhw   $0xf5,%%xmm3,%%xmm3             \n"
    "pshuflw   $0xf5,%%xmm3,%%xmm3             \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movd      "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x4,1)",%1              \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x1,%3                         \n"
    "movd      %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x4,2)",%2              \n"
    "jge       91b                             \n"
  "99:                                         \n"
  : "+r"(src_argb0),    // %0
    "+r"(src_argb1),    // %1
    "+r"(dst_argb),     // %2
    "+r"(width)         // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBBLENDROW_SSE2

#ifdef HAS_ARGBBLENDROW_SSSE3
// Shuffle table for isolating alpha.
static uvec8 kShuffleAlpha = {
  3u, 0x80, 3u, 0x80, 7u, 0x80, 7u, 0x80,
  11u, 0x80, 11u, 0x80, 15u, 0x80, 15u, 0x80
};

// Blend 8 pixels at a time
// Shuffle table for reversing the bytes.

// Same as SSE2, but replaces
//    psrlw      xmm3, 8          // alpha
//    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
//    pshuflw    xmm3, xmm3,0F5h
// with..
//    pshufb     xmm3, kShuffleAlpha // alpha

void ARGBBlendRow_SSSE3(const uint8* src_argb0, const uint8* src_argb1,
                        uint8* dst_argb, int width) {
  asm volatile (
    "pcmpeqb   %%xmm7,%%xmm7                   \n"
    "psrlw     $0xf,%%xmm7                     \n"
    "pcmpeqb   %%xmm6,%%xmm6                   \n"
    "psrlw     $0x8,%%xmm6                     \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psllw     $0x8,%%xmm5                     \n"
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "pslld     $0x18,%%xmm4                    \n"
    "sub       $0x1,%3                         \n"
    "je        91f                             \n"
    "jl        99f                             \n"

    // 1 pixel loop until destination pointer is aligned.
  "10:                                         \n"
    "test      $0xf,%2                         \n"
    "je        19f                             \n"
    "movd      "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x4,0)",%0              \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movd      "MEMACCESS(1)",%%xmm2           \n"
    "pshufb    %4,%%xmm3                       \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movd      "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x4,1)",%1              \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x1,%3                         \n"
    "movd      %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x4,2)",%2              \n"
    "jge       10b                             \n"

  "19:                                         \n"
    "add       $1-4,%3                         \n"
    "jl        49f                             \n"
    "test      $0xf,%0                         \n"
    "jne       41f                             \n"
    "test      $0xf,%1                         \n"
    "jne       41f                             \n"

    // 4 pixel loop.
    ".p2align  2                               \n"
  "40:                                         \n"
    "movdqa    "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movdqa    "MEMACCESS(1)",%%xmm2           \n"
    "pshufb    %4,%%xmm3                       \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movdqa    "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jge       40b                             \n"
    "jmp       49f                             \n"

    // 4 pixel unaligned loop.
    ".p2align  2                               \n"
  "41:                                         \n"
    "movdqu    "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movdqu    "MEMACCESS(1)",%%xmm2           \n"
    "pshufb    %4,%%xmm3                       \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movdqu    "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jge       41b                             \n"

  "49:                                         \n"
    "add       $0x3,%3                         \n"
    "jl        99f                             \n"

    // 1 pixel loop.
  "91:                                         \n"
    "movd      "MEMACCESS(0)",%%xmm3           \n"
    "lea       "MEMLEA(0x4,0)",%0              \n"
    "movdqa    %%xmm3,%%xmm0                   \n"
    "pxor      %%xmm4,%%xmm3                   \n"
    "movd      "MEMACCESS(1)",%%xmm2           \n"
    "pshufb    %4,%%xmm3                       \n"
    "pand      %%xmm6,%%xmm2                   \n"
    "paddw     %%xmm7,%%xmm3                   \n"
    "pmullw    %%xmm3,%%xmm2                   \n"
    "movd      "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x4,1)",%1              \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "por       %%xmm4,%%xmm0                   \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm2                     \n"
    "paddusb   %%xmm2,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm1                   \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x1,%3                         \n"
    "movd      %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x4,2)",%2              \n"
    "jge       91b                             \n"
  "99:                                         \n"
  : "+r"(src_argb0),    // %0
    "+r"(src_argb1),    // %1
    "+r"(dst_argb),     // %2
    "+r"(width)         // %3
  : "m"(kShuffleAlpha)  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBBLENDROW_SSSE3

#ifdef HAS_ARGBATTENUATEROW_SSE2
// Attenuate 4 pixels at a time.
// aligned to 16 bytes
void ARGBAttenuateRow_SSE2(const uint8* src_argb, uint8* dst_argb, int width) {
  asm volatile (
    "pcmpeqb   %%xmm4,%%xmm4                   \n"
    "pslld     $0x18,%%xmm4                    \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "psrld     $0x8,%%xmm5                     \n"

    // 4 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "pshufhw   $0xff,%%xmm0,%%xmm2             \n"
    "pshuflw   $0xff,%%xmm2,%%xmm2             \n"
    "pmulhuw   %%xmm2,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm1           \n"
    "punpckhbw %%xmm1,%%xmm1                   \n"
    "pshufhw   $0xff,%%xmm1,%%xmm2             \n"
    "pshuflw   $0xff,%%xmm2,%%xmm2             \n"
    "pmulhuw   %%xmm2,%%xmm1                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm2           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "pand      %%xmm4,%%xmm2                   \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "pand      %%xmm5,%%xmm0                   \n"
    "por       %%xmm2,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),    // %0
    "+r"(dst_argb),    // %1
    "+r"(width)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBATTENUATEROW_SSE2

#ifdef HAS_ARGBATTENUATEROW_SSSE3
// Shuffle table duplicating alpha
static uvec8 kShuffleAlpha0 = {
  3u, 3u, 3u, 3u, 3u, 3u, 128u, 128u, 7u, 7u, 7u, 7u, 7u, 7u, 128u, 128u,
};
static uvec8 kShuffleAlpha1 = {
  11u, 11u, 11u, 11u, 11u, 11u, 128u, 128u,
  15u, 15u, 15u, 15u, 15u, 15u, 128u, 128u,
};
// Attenuate 4 pixels at a time.
// aligned to 16 bytes
void ARGBAttenuateRow_SSSE3(const uint8* src_argb, uint8* dst_argb, int width) {
  asm volatile (
    "pcmpeqb   %%xmm3,%%xmm3                   \n"
    "pslld     $0x18,%%xmm3                    \n"
    "movdqa    %3,%%xmm4                       \n"
    "movdqa    %4,%%xmm5                       \n"

    // 4 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "pshufb    %%xmm4,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm1           \n"
    "punpcklbw %%xmm1,%%xmm1                   \n"
    "pmulhuw   %%xmm1,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm1           \n"
    "pshufb    %%xmm5,%%xmm1                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm2           \n"
    "punpckhbw %%xmm2,%%xmm2                   \n"
    "pmulhuw   %%xmm2,%%xmm1                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm2           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "pand      %%xmm3,%%xmm2                   \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "por       %%xmm2,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),    // %0
    "+r"(dst_argb),    // %1
    "+r"(width)        // %2
  : "m"(kShuffleAlpha0),  // %3
    "m"(kShuffleAlpha1)  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBATTENUATEROW_SSSE3

#ifdef HAS_ARGBUNATTENUATEROW_SSE2
// Unattenuate 4 pixels at a time.
// aligned to 16 bytes
void ARGBUnattenuateRow_SSE2(const uint8* src_argb, uint8* dst_argb,
                             int width) {
  uintptr_t alpha = 0;
  asm volatile (
    // 4 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movzb     "MEMACCESS2(0x03,0)",%3         \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    MEMOPREG(movd,0x00,4,3,4,xmm2)             //  movd      0x0(%4,%3,4),%%xmm2
    "movzb     "MEMACCESS2(0x07,0)",%3         \n"
    MEMOPREG(movd,0x00,4,3,4,xmm3)             //  movd      0x0(%4,%3,4),%%xmm3
    "pshuflw   $0x40,%%xmm2,%%xmm2             \n"
    "pshuflw   $0x40,%%xmm3,%%xmm3             \n"
    "movlhps   %%xmm3,%%xmm2                   \n"
    "pmulhuw   %%xmm2,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm1           \n"
    "movzb     "MEMACCESS2(0x0b,0)",%3         \n"
    "punpckhbw %%xmm1,%%xmm1                   \n"
    MEMOPREG(movd,0x00,4,3,4,xmm2)             //  movd      0x0(%4,%3,4),%%xmm2
    "movzb     "MEMACCESS2(0x0f,0)",%3         \n"
    MEMOPREG(movd,0x00,4,3,4,xmm3)             //  movd      0x0(%4,%3,4),%%xmm3
    "pshuflw   $0x40,%%xmm2,%%xmm2             \n"
    "pshuflw   $0x40,%%xmm3,%%xmm3             \n"
    "movlhps   %%xmm3,%%xmm2                   \n"
    "pmulhuw   %%xmm2,%%xmm1                   \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),    // %0
    "+r"(dst_argb),    // %1
    "+r"(width),       // %2
    "+r"(alpha)        // %3
  : "r"(fixed_invtbl8)  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBUNATTENUATEROW_SSE2

#ifdef HAS_ARGBGRAYROW_SSSE3
// Convert 8 ARGB pixels (64 bytes) to 8 Gray ARGB pixels
void ARGBGrayRow_SSSE3(const uint8* src_argb, uint8* dst_argb, int width) {
  asm volatile (
    "movdqa    %3,%%xmm4                       \n"
    "movdqa    %4,%%xmm5                       \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "pmaddubsw %%xmm4,%%xmm0                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "phaddw    %%xmm1,%%xmm0                   \n"
    "paddw     %%xmm5,%%xmm0                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm2           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm3     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "psrld     $0x18,%%xmm2                    \n"
    "psrld     $0x18,%%xmm3                    \n"
    "packuswb  %%xmm3,%%xmm2                   \n"
    "packuswb  %%xmm2,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm3                   \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "punpcklbw %%xmm2,%%xmm3                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm3,%%xmm0                   \n"
    "punpckhwd %%xmm3,%%xmm1                   \n"
    "sub       $0x8,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqa    %%xmm1,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),   // %0
    "+r"(dst_argb),   // %1
    "+r"(width)       // %2
  : "m"(kARGBToYJ),   // %3
    "m"(kAddYJ64)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBGRAYROW_SSSE3

#ifdef HAS_ARGBSEPIAROW_SSSE3
//    b = (r * 35 + g * 68 + b * 17) >> 7
//    g = (r * 45 + g * 88 + b * 22) >> 7
//    r = (r * 50 + g * 98 + b * 24) >> 7
// Constant for ARGB color to sepia tone
static vec8 kARGBToSepiaB = {
  17, 68, 35, 0, 17, 68, 35, 0, 17, 68, 35, 0, 17, 68, 35, 0
};

static vec8 kARGBToSepiaG = {
  22, 88, 45, 0, 22, 88, 45, 0, 22, 88, 45, 0, 22, 88, 45, 0
};

static vec8 kARGBToSepiaR = {
  24, 98, 50, 0, 24, 98, 50, 0, 24, 98, 50, 0, 24, 98, 50, 0
};

// Convert 8 ARGB pixels (32 bytes) to 8 Sepia ARGB pixels.
void ARGBSepiaRow_SSSE3(uint8* dst_argb, int width) {
  asm volatile (
    "movdqa    %2,%%xmm2                       \n"
    "movdqa    %3,%%xmm3                       \n"
    "movdqa    %4,%%xmm4                       \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm6     \n"
    "pmaddubsw %%xmm2,%%xmm0                   \n"
    "pmaddubsw %%xmm2,%%xmm6                   \n"
    "phaddw    %%xmm6,%%xmm0                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm5           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "pmaddubsw %%xmm3,%%xmm5                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "phaddw    %%xmm1,%%xmm5                   \n"
    "psrlw     $0x7,%%xmm5                     \n"
    "packuswb  %%xmm5,%%xmm5                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm5           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "pmaddubsw %%xmm4,%%xmm5                   \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "phaddw    %%xmm1,%%xmm5                   \n"
    "psrlw     $0x7,%%xmm5                     \n"
    "packuswb  %%xmm5,%%xmm5                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm6           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "psrld     $0x18,%%xmm6                    \n"
    "psrld     $0x18,%%xmm1                    \n"
    "packuswb  %%xmm1,%%xmm6                   \n"
    "packuswb  %%xmm6,%%xmm6                   \n"
    "punpcklbw %%xmm6,%%xmm5                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklwd %%xmm5,%%xmm0                   \n"
    "punpckhwd %%xmm5,%%xmm1                   \n"
    "sub       $0x8,%1                         \n"
    "movdqa    %%xmm0,"MEMACCESS(0)"           \n"
    "movdqa    %%xmm1,"MEMACCESS2(0x10,0)"     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "jg        1b                              \n"
  : "+r"(dst_argb),      // %0
    "+r"(width)          // %1
  : "m"(kARGBToSepiaB),  // %2
    "m"(kARGBToSepiaG),  // %3
    "m"(kARGBToSepiaR)   // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}
#endif  // HAS_ARGBSEPIAROW_SSSE3

#ifdef HAS_ARGBCOLORMATRIXROW_SSSE3
// Tranform 8 ARGB pixels (32 bytes) with color matrix.
// Same as Sepia except matrix is provided.
void ARGBColorMatrixRow_SSSE3(const uint8* src_argb, uint8* dst_argb,
                              const int8* matrix_argb, int width) {
  asm volatile (
    "movd      "MEMACCESS(3)",%%xmm2           \n"
    "movd      "MEMACCESS2(0x4,3)",%%xmm3      \n"
    "movd      "MEMACCESS2(0x8,3)",%%xmm4      \n"
    "movd      "MEMACCESS2(0xc,3)",%%xmm5      \n"
    "pshufd    $0x0,%%xmm2,%%xmm2              \n"
    "pshufd    $0x0,%%xmm3,%%xmm3              \n"
    "pshufd    $0x0,%%xmm4,%%xmm4              \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm7     \n"
    "pmaddubsw %%xmm2,%%xmm0                   \n"
    "pmaddubsw %%xmm2,%%xmm7                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm6           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "pmaddubsw %%xmm3,%%xmm6                   \n"
    "pmaddubsw %%xmm3,%%xmm1                   \n"
    "phaddsw   %%xmm7,%%xmm0                   \n"
    "phaddsw   %%xmm1,%%xmm6                   \n"
    "psraw     $0x6,%%xmm0                     \n"
    "psraw     $0x6,%%xmm6                     \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "packuswb  %%xmm6,%%xmm6                   \n"
    "punpcklbw %%xmm6,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm1           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm7     \n"
    "pmaddubsw %%xmm4,%%xmm1                   \n"
    "pmaddubsw %%xmm4,%%xmm7                   \n"
    "phaddsw   %%xmm7,%%xmm1                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm6           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm7     \n"
    "pmaddubsw %%xmm5,%%xmm6                   \n"
    "pmaddubsw %%xmm5,%%xmm7                   \n"
    "phaddsw   %%xmm7,%%xmm6                   \n"
    "psraw     $0x6,%%xmm1                     \n"
    "psraw     $0x6,%%xmm6                     \n"
    "packuswb  %%xmm1,%%xmm1                   \n"
    "packuswb  %%xmm6,%%xmm6                   \n"
    "punpcklbw %%xmm6,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm6                   \n"
    "punpcklwd %%xmm1,%%xmm0                   \n"
    "punpckhwd %%xmm1,%%xmm6                   \n"
    "sub       $0x8,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqa    %%xmm6,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),      // %0
    "+r"(dst_argb),      // %1
    "+r"(width)          // %2
  : "r"(matrix_argb)     // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBCOLORMATRIXROW_SSSE3

#ifdef HAS_ARGBQUANTIZEROW_SSE2
// Quantize 4 ARGB pixels (16 bytes).
// aligned to 16 bytes
void ARGBQuantizeRow_SSE2(uint8* dst_argb, int scale, int interval_size,
                          int interval_offset, int width) {
  asm volatile (
    "movd      %2,%%xmm2                       \n"
    "movd      %3,%%xmm3                       \n"
    "movd      %4,%%xmm4                       \n"
    "pshuflw   $0x40,%%xmm2,%%xmm2             \n"
    "pshufd    $0x44,%%xmm2,%%xmm2             \n"
    "pshuflw   $0x40,%%xmm3,%%xmm3             \n"
    "pshufd    $0x44,%%xmm3,%%xmm3             \n"
    "pshuflw   $0x40,%%xmm4,%%xmm4             \n"
    "pshufd    $0x44,%%xmm4,%%xmm4             \n"
    "pxor      %%xmm5,%%xmm5                   \n"
    "pcmpeqb   %%xmm6,%%xmm6                   \n"
    "pslld     $0x18,%%xmm6                    \n"

    // 4 pixel loop.
    ".p2align  2                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "pmulhuw   %%xmm2,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm1           \n"
    "punpckhbw %%xmm5,%%xmm1                   \n"
    "pmulhuw   %%xmm2,%%xmm1                   \n"
    "pmullw    %%xmm3,%%xmm0                   \n"
    "movdqa    "MEMACCESS(0)",%%xmm7           \n"
    "pmullw    %%xmm3,%%xmm1                   \n"
    "pand      %%xmm6,%%xmm7                   \n"
    "paddw     %%xmm4,%%xmm0                   \n"
    "paddw     %%xmm4,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "por       %%xmm7,%%xmm0                   \n"
    "sub       $0x4,%1                         \n"
    "movdqa    %%xmm0,"MEMACCESS(0)"           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "jg        1b                              \n"
  : "+r"(dst_argb),       // %0
    "+r"(width)           // %1
  : "r"(scale),           // %2
    "r"(interval_size),   // %3
    "r"(interval_offset)  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBQUANTIZEROW_SSE2

#ifdef HAS_ARGBSHADEROW_SSE2
// Shade 4 pixels at a time by specified value.
// Aligned to 16 bytes.
void ARGBShadeRow_SSE2(const uint8* src_argb, uint8* dst_argb, int width,
                       uint32 value) {
  asm volatile (
    "movd      %3,%%xmm2                       \n"
    "punpcklbw %%xmm2,%%xmm2                   \n"
    "punpcklqdq %%xmm2,%%xmm2                  \n"

    // 4 pixel loop.
    ".p2align  2                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "punpckhbw %%xmm1,%%xmm1                   \n"
    "pmulhuw   %%xmm2,%%xmm0                   \n"
    "pmulhuw   %%xmm2,%%xmm1                   \n"
    "psrlw     $0x8,%%xmm0                     \n"
    "psrlw     $0x8,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_argb),  // %1
    "+r"(width)      // %2
  : "r"(value)       // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2"
#endif
  );
}
#endif  // HAS_ARGBSHADEROW_SSE2

#ifdef HAS_ARGBMULTIPLYROW_SSE2
// Multiply 2 rows of ARGB pixels together, 4 pixels at a time.
void ARGBMultiplyRow_SSE2(const uint8* src_argb0, const uint8* src_argb1,
                          uint8* dst_argb, int width) {
  asm volatile (
    "pxor      %%xmm5,%%xmm5                   \n"

    // 4 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqu    "MEMACCESS(1)",%%xmm2           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "movdqu    %%xmm0,%%xmm1                   \n"
    "movdqu    %%xmm2,%%xmm3                   \n"
    "punpcklbw %%xmm0,%%xmm0                   \n"
    "punpckhbw %%xmm1,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "punpckhbw %%xmm5,%%xmm3                   \n"
    "pmulhuw   %%xmm2,%%xmm0                   \n"
    "pmulhuw   %%xmm3,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),  // %0
    "+r"(src_argb1),  // %1
    "+r"(dst_argb),   // %2
    "+r"(width)       // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBMULTIPLYROW_SSE2

#ifdef HAS_ARGBADDROW_SSE2
// Add 2 rows of ARGB pixels together, 4 pixels at a time.
void ARGBAddRow_SSE2(const uint8* src_argb0, const uint8* src_argb1,
                     uint8* dst_argb, int width) {
  asm volatile (
    // 4 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqu    "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),  // %0
    "+r"(src_argb1),  // %1
    "+r"(dst_argb),   // %2
    "+r"(width)       // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}
#endif  // HAS_ARGBADDROW_SSE2

#ifdef HAS_ARGBSUBTRACTROW_SSE2
// Subtract 2 rows of ARGB pixels, 4 pixels at a time.
void ARGBSubtractRow_SSE2(const uint8* src_argb0, const uint8* src_argb1,
                          uint8* dst_argb, int width) {
  asm volatile (
    // 4 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqu    "MEMACCESS(1)",%%xmm1           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "psubusb   %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jg        1b                              \n"
  : "+r"(src_argb0),  // %0
    "+r"(src_argb1),  // %1
    "+r"(dst_argb),   // %2
    "+r"(width)       // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}
#endif  // HAS_ARGBSUBTRACTROW_SSE2

#ifdef HAS_SOBELXROW_SSSE3
// SobelX as a matrix is
// -1  0  1
// -2  0  2
// -1  0  1
void SobelXRow_SSSE3(const uint8* src_y0, const uint8* src_y1,
                     const uint8* src_y2, uint8* dst_sobelx, int width) {
  asm volatile (
    "sub       %0,%1                           \n"
    "sub       %0,%2                           \n"
    "sub       %0,%3                           \n"
    "pxor      %%xmm5,%%xmm5                   \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movq      "MEMACCESS(0)",%%xmm0           \n"
    "movq      "MEMACCESS2(0x2,0)",%%xmm1      \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm1                   \n"
    "psubw     %%xmm1,%%xmm0                   \n"
    BUNDLEALIGN
    MEMOPREG(movq,0x00,0,1,1,xmm1)             //  movq      (%0,%1,1),%%xmm1
    MEMOPREG(movq,0x02,0,1,1,xmm2)             //  movq      0x2(%0,%1,1),%%xmm2
    "punpcklbw %%xmm5,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "psubw     %%xmm2,%%xmm1                   \n"
    BUNDLEALIGN
    MEMOPREG(movq,0x00,0,2,1,xmm2)             //  movq      (%0,%2,1),%%xmm2
    MEMOPREG(movq,0x02,0,2,1,xmm3)             //  movq      0x2(%0,%2,1),%%xmm3
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "punpcklbw %%xmm5,%%xmm3                   \n"
    "psubw     %%xmm3,%%xmm2                   \n"
    "paddw     %%xmm2,%%xmm0                   \n"
    "paddw     %%xmm1,%%xmm0                   \n"
    "paddw     %%xmm1,%%xmm0                   \n"
    "pabsw     %%xmm0,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "sub       $0x8,%4                         \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm0,0x00,0,3,1)             //  movq      %%xmm0,(%0,%3,1)
    "lea       "MEMLEA(0x8,0)",%0              \n"
    "jg        1b                              \n"
  : "+r"(src_y0),      // %0
    "+r"(src_y1),      // %1
    "+r"(src_y2),      // %2
    "+r"(dst_sobelx),  // %3
    "+r"(width)        // %4
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}
#endif  // HAS_SOBELXROW_SSSE3

#ifdef HAS_SOBELYROW_SSSE3
// SobelY as a matrix is
// -1 -2 -1
//  0  0  0
//  1  2  1
void SobelYRow_SSSE3(const uint8* src_y0, const uint8* src_y1,
                     uint8* dst_sobely, int width) {
  asm volatile (
    "sub       %0,%1                           \n"
    "sub       %0,%2                           \n"
    "pxor      %%xmm5,%%xmm5                   \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movq      "MEMACCESS(0)",%%xmm0           \n"
    MEMOPREG(movq,0x00,0,1,1,xmm1)             //  movq      (%0,%1,1),%%xmm1
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "punpcklbw %%xmm5,%%xmm1                   \n"
    "psubw     %%xmm1,%%xmm0                   \n"
    BUNDLEALIGN
    "movq      "MEMACCESS2(0x1,0)",%%xmm1      \n"
    MEMOPREG(movq,0x01,0,1,1,xmm2)             //  movq      0x1(%0,%1,1),%%xmm2
    "punpcklbw %%xmm5,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "psubw     %%xmm2,%%xmm1                   \n"
    BUNDLEALIGN
    "movq      "MEMACCESS2(0x2,0)",%%xmm2      \n"
    MEMOPREG(movq,0x02,0,1,1,xmm3)             //  movq      0x2(%0,%1,1),%%xmm3
    "punpcklbw %%xmm5,%%xmm2                   \n"
    "punpcklbw %%xmm5,%%xmm3                   \n"
    "psubw     %%xmm3,%%xmm2                   \n"
    "paddw     %%xmm2,%%xmm0                   \n"
    "paddw     %%xmm1,%%xmm0                   \n"
    "paddw     %%xmm1,%%xmm0                   \n"
    "pabsw     %%xmm0,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "sub       $0x8,%3                         \n"
    BUNDLEALIGN
    MEMOPMEM(movq,xmm0,0x00,0,2,1)             //  movq      %%xmm0,(%0,%2,1)
    "lea       "MEMLEA(0x8,0)",%0              \n"
    "jg        1b                              \n"
  : "+r"(src_y0),      // %0
    "+r"(src_y1),      // %1
    "+r"(dst_sobely),  // %2
    "+r"(width)        // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}
#endif  // HAS_SOBELYROW_SSSE3

#ifdef HAS_SOBELROW_SSE2
// Adds Sobel X and Sobel Y and stores Sobel into ARGB.
// A = 255
// R = Sobel
// G = Sobel
// B = Sobel
void SobelRow_SSE2(const uint8* src_sobelx, const uint8* src_sobely,
                   uint8* dst_argb, int width) {
  asm volatile (
    "sub       %0,%1                           \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"
    "pslld     $0x18,%%xmm5                    \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,0,1,1,xmm1)           //  movdqa    (%0,%1,1),%%xmm1
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "paddusb   %%xmm1,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "punpcklbw %%xmm0,%%xmm2                   \n"
    "punpckhbw %%xmm0,%%xmm0                   \n"
    "movdqa    %%xmm2,%%xmm1                   \n"
    "punpcklwd %%xmm2,%%xmm1                   \n"
    "punpckhwd %%xmm2,%%xmm2                   \n"
    "por       %%xmm5,%%xmm1                   \n"
    "por       %%xmm5,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm3                   \n"
    "punpcklwd %%xmm0,%%xmm3                   \n"
    "punpckhwd %%xmm0,%%xmm0                   \n"
    "por       %%xmm5,%%xmm3                   \n"
    "por       %%xmm5,%%xmm0                   \n"
    "sub       $0x10,%3                        \n"
    "movdqa    %%xmm1,"MEMACCESS(2)"           \n"
    "movdqa    %%xmm2,"MEMACCESS2(0x10,2)"     \n"
    "movdqa    %%xmm3,"MEMACCESS2(0x20,2)"     \n"
    "movdqa    %%xmm0,"MEMACCESS2(0x30,2)"     \n"
    "lea       "MEMLEA(0x40,2)",%2             \n"
    "jg        1b                              \n"
  : "+r"(src_sobelx),  // %0
    "+r"(src_sobely),  // %1
    "+r"(dst_argb),    // %2
    "+r"(width)        // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
  );
}
#endif  // HAS_SOBELROW_SSE2

#ifdef HAS_SOBELXYROW_SSE2
// Mixes Sobel X, Sobel Y and Sobel into ARGB.
// A = 255
// R = Sobel X
// G = Sobel
// B = Sobel Y
void SobelXYRow_SSE2(const uint8* src_sobelx, const uint8* src_sobely,
                     uint8* dst_argb, int width) {
  asm volatile (
    "sub       %0,%1                           \n"
    "pcmpeqb   %%xmm5,%%xmm5                   \n"

    // 8 pixel loop.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,0,1,1,xmm1)           //  movdqa    (%0,%1,1),%%xmm1
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "paddusb   %%xmm1,%%xmm2                   \n"
    "movdqa    %%xmm0,%%xmm3                   \n"
    "punpcklbw %%xmm5,%%xmm3                   \n"
    "punpckhbw %%xmm5,%%xmm0                   \n"
    "movdqa    %%xmm1,%%xmm4                   \n"
    "punpcklbw %%xmm2,%%xmm4                   \n"
    "punpckhbw %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm4,%%xmm6                   \n"
    "punpcklwd %%xmm3,%%xmm6                   \n"
    "punpckhwd %%xmm3,%%xmm4                   \n"
    "movdqa    %%xmm1,%%xmm7                   \n"
    "punpcklwd %%xmm0,%%xmm7                   \n"
    "punpckhwd %%xmm0,%%xmm1                   \n"
    "sub       $0x10,%3                        \n"
    "movdqa    %%xmm6,"MEMACCESS(2)"           \n"
    "movdqa    %%xmm4,"MEMACCESS2(0x10,2)"     \n"
    "movdqa    %%xmm7,"MEMACCESS2(0x20,2)"     \n"
    "movdqa    %%xmm1,"MEMACCESS2(0x30,2)"     \n"
    "lea       "MEMLEA(0x40,2)",%2             \n"
    "jg        1b                              \n"
  : "+r"(src_sobelx),  // %0
    "+r"(src_sobely),  // %1
    "+r"(dst_argb),    // %2
    "+r"(width)        // %3
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_SOBELXYROW_SSE2

#ifdef HAS_COMPUTECUMULATIVESUMROW_SSE2
// Creates a table of cumulative sums where each value is a sum of all values
// above and to the left of the value, inclusive of the value.
void ComputeCumulativeSumRow_SSE2(const uint8* row, int32* cumsum,
                                  const int32* previous_cumsum, int width) {
  asm volatile (
    "pxor      %%xmm0,%%xmm0                   \n"
    "pxor      %%xmm1,%%xmm1                   \n"
    "sub       $0x4,%3                         \n"
    "jl        49f                             \n"
    "test      $0xf,%1                         \n"
    "jne       49f                             \n"

  // 4 pixel loop                              \n"
    ".p2align  2                               \n"
  "40:                                         \n"
    "movdqu    "MEMACCESS(0)",%%xmm2           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm2,%%xmm4                   \n"
    "punpcklbw %%xmm1,%%xmm2                   \n"
    "movdqa    %%xmm2,%%xmm3                   \n"
    "punpcklwd %%xmm1,%%xmm2                   \n"
    "punpckhwd %%xmm1,%%xmm3                   \n"
    "punpckhbw %%xmm1,%%xmm4                   \n"
    "movdqa    %%xmm4,%%xmm5                   \n"
    "punpcklwd %%xmm1,%%xmm4                   \n"
    "punpckhwd %%xmm1,%%xmm5                   \n"
    "paddd     %%xmm2,%%xmm0                   \n"
    "movdqa    "MEMACCESS(2)",%%xmm2           \n"
    "paddd     %%xmm0,%%xmm2                   \n"
    "paddd     %%xmm3,%%xmm0                   \n"
    "movdqa    "MEMACCESS2(0x10,2)",%%xmm3     \n"
    "paddd     %%xmm0,%%xmm3                   \n"
    "paddd     %%xmm4,%%xmm0                   \n"
    "movdqa    "MEMACCESS2(0x20,2)",%%xmm4     \n"
    "paddd     %%xmm0,%%xmm4                   \n"
    "paddd     %%xmm5,%%xmm0                   \n"
    "movdqa    "MEMACCESS2(0x30,2)",%%xmm5     \n"
    "lea       "MEMLEA(0x40,2)",%2             \n"
    "paddd     %%xmm0,%%xmm5                   \n"
    "movdqa    %%xmm2,"MEMACCESS(1)"           \n"
    "movdqa    %%xmm3,"MEMACCESS2(0x10,1)"     \n"
    "movdqa    %%xmm4,"MEMACCESS2(0x20,1)"     \n"
    "movdqa    %%xmm5,"MEMACCESS2(0x30,1)"     \n"
    "lea       "MEMLEA(0x40,1)",%1             \n"
    "sub       $0x4,%3                         \n"
    "jge       40b                             \n"

  "49:                                         \n"
    "add       $0x3,%3                         \n"
    "jl        19f                             \n"

  // 1 pixel loop                              \n"
    ".p2align  2                               \n"
  "10:                                         \n"
    "movd      "MEMACCESS(0)",%%xmm2           \n"
    "lea       "MEMLEA(0x4,0)",%0              \n"
    "punpcklbw %%xmm1,%%xmm2                   \n"
    "punpcklwd %%xmm1,%%xmm2                   \n"
    "paddd     %%xmm2,%%xmm0                   \n"
    "movdqu    "MEMACCESS(2)",%%xmm2           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "paddd     %%xmm0,%%xmm2                   \n"
    "movdqu    %%xmm2,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "sub       $0x1,%3                         \n"
    "jge       10b                             \n"

  "19:                                         \n"
  : "+r"(row),  // %0
    "+r"(cumsum),  // %1
    "+r"(previous_cumsum),  // %2
    "+r"(width)  // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_COMPUTECUMULATIVESUMROW_SSE2

#ifdef HAS_CUMULATIVESUMTOAVERAGEROW_SSE2
void CumulativeSumToAverageRow_SSE2(const int32* topleft, const int32* botleft,
                                    int width, int area, uint8* dst,
                                    int count) {
  asm volatile (
    "movd      %5,%%xmm4                       \n"
    "cvtdq2ps  %%xmm4,%%xmm4                   \n"
    "rcpss     %%xmm4,%%xmm4                   \n"
    "pshufd    $0x0,%%xmm4,%%xmm4              \n"
    "sub       $0x4,%3                         \n"
    "jl        49f                             \n"

  // 4 pixel loop                              \n"
    ".p2align  2                               \n"
    BUNDLEALIGN
  "40:                                         \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "movdqa    "MEMACCESS2(0x20,0)",%%xmm2     \n"
    "movdqa    "MEMACCESS2(0x30,0)",%%xmm3     \n"
    BUNDLEALIGN
    MEMOPREG(psubd,0x00,0,4,4,xmm0)            // psubd    0x00(%0,%4,4),%%xmm0
    MEMOPREG(psubd,0x10,0,4,4,xmm1)            // psubd    0x10(%0,%4,4),%%xmm1
    MEMOPREG(psubd,0x20,0,4,4,xmm2)            // psubd    0x20(%0,%4,4),%%xmm2
    MEMOPREG(psubd,0x30,0,4,4,xmm3)            // psubd    0x30(%0,%4,4),%%xmm3
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "psubd     "MEMACCESS(1)",%%xmm0           \n"
    "psubd     "MEMACCESS2(0x10,1)",%%xmm1     \n"
    "psubd     "MEMACCESS2(0x20,1)",%%xmm2     \n"
    "psubd     "MEMACCESS2(0x30,1)",%%xmm3     \n"
    BUNDLEALIGN
    MEMOPREG(paddd,0x00,1,4,4,xmm0)            // paddd    0x00(%1,%4,4),%%xmm0
    MEMOPREG(paddd,0x10,1,4,4,xmm1)            // paddd    0x10(%1,%4,4),%%xmm1
    MEMOPREG(paddd,0x20,1,4,4,xmm2)            // paddd    0x20(%1,%4,4),%%xmm2
    MEMOPREG(paddd,0x30,1,4,4,xmm3)            // paddd    0x30(%1,%4,4),%%xmm3
    "lea       "MEMLEA(0x40,1)",%1             \n"
    "cvtdq2ps  %%xmm0,%%xmm0                   \n"
    "cvtdq2ps  %%xmm1,%%xmm1                   \n"
    "mulps     %%xmm4,%%xmm0                   \n"
    "mulps     %%xmm4,%%xmm1                   \n"
    "cvtdq2ps  %%xmm2,%%xmm2                   \n"
    "cvtdq2ps  %%xmm3,%%xmm3                   \n"
    "mulps     %%xmm4,%%xmm2                   \n"
    "mulps     %%xmm4,%%xmm3                   \n"
    "cvtps2dq  %%xmm0,%%xmm0                   \n"
    "cvtps2dq  %%xmm1,%%xmm1                   \n"
    "cvtps2dq  %%xmm2,%%xmm2                   \n"
    "cvtps2dq  %%xmm3,%%xmm3                   \n"
    "packssdw  %%xmm1,%%xmm0                   \n"
    "packssdw  %%xmm3,%%xmm2                   \n"
    "packuswb  %%xmm2,%%xmm0                   \n"
    "movdqu    %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "sub       $0x4,%3                         \n"
    "jge       40b                             \n"

  "49:                                         \n"
    "add       $0x3,%3                         \n"
    "jl        19f                             \n"

  // 1 pixel loop                              \n"
    ".p2align  2                               \n"
    BUNDLEALIGN
  "10:                                         \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    MEMOPREG(psubd,0x00,0,4,4,xmm0)            // psubd    0x00(%0,%4,4),%%xmm0
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "psubd     "MEMACCESS(1)",%%xmm0           \n"
    BUNDLEALIGN
    MEMOPREG(paddd,0x00,1,4,4,xmm0)            // paddd    0x00(%1,%4,4),%%xmm0
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "cvtdq2ps  %%xmm0,%%xmm0                   \n"
    "mulps     %%xmm4,%%xmm0                   \n"
    "cvtps2dq  %%xmm0,%%xmm0                   \n"
    "packssdw  %%xmm0,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "movd      %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x4,2)",%2              \n"
    "sub       $0x1,%3                         \n"
    "jge       10b                             \n"
  "19:                                         \n"
  : "+r"(topleft),  // %0
    "+r"(botleft),  // %1
    "+r"(dst),      // %2
    "+rm"(count)    // %3
  : "r"(static_cast<intptr_t>(width)),  // %4
    "rm"(area)     // %5
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#endif
  );
}
#endif  // HAS_CUMULATIVESUMTOAVERAGEROW_SSE2

#ifdef HAS_ARGBAFFINEROW_SSE2
// TODO(fbarchard): Find 64 bit way to avoid masking.
// Copy ARGB pixels from source image with slope to a row of destination.
// Caveat - in 64 bit, movd is used with 64 bit gpr due to Mac gcc producing
// an error if movq is used. movd  %%xmm0,%1

LIBYUV_API
void ARGBAffineRow_SSE2(const uint8* src_argb, int src_argb_stride,
                        uint8* dst_argb, const float* src_dudv, int width) {
  intptr_t src_argb_stride_temp = src_argb_stride;
  intptr_t temp = 0;
  asm volatile (
    "movq      "MEMACCESS(3)",%%xmm2           \n"
    "movq      "MEMACCESS2(0x08,3)",%%xmm7     \n"
    "shl       $0x10,%1                        \n"
    "add       $0x4,%1                         \n"
    "movd      %1,%%xmm5                       \n"
    "sub       $0x4,%4                         \n"
    "jl        49f                             \n"

    "pshufd    $0x44,%%xmm7,%%xmm7             \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    "movdqa    %%xmm2,%%xmm0                   \n"
    "addps     %%xmm7,%%xmm0                   \n"
    "movlhps   %%xmm0,%%xmm2                   \n"
    "movdqa    %%xmm7,%%xmm4                   \n"
    "addps     %%xmm4,%%xmm4                   \n"
    "movdqa    %%xmm2,%%xmm3                   \n"
    "addps     %%xmm4,%%xmm3                   \n"
    "addps     %%xmm4,%%xmm4                   \n"

  // 4 pixel loop                              \n"
    ".p2align  4                               \n"
  "40:                                         \n"
    "cvttps2dq %%xmm2,%%xmm0                   \n"
    "cvttps2dq %%xmm3,%%xmm1                   \n"
    "packssdw  %%xmm1,%%xmm0                   \n"
    "pmaddwd   %%xmm5,%%xmm0                   \n"
#if defined(__x86_64__)
// TODO(fbarchard): use a real movd to zero upper with %w1 for x64 and nacl.
    "movd      %%xmm0,%1                       \n"
    "mov       %1,%5                           \n"
    "and       $0x0fffffff,%1                  \n"
    "shr       $32,%5                          \n"
    "pshufd    $0xEE,%%xmm0,%%xmm0             \n"
#else
    "movd      %%xmm0,%1                       \n"
    "pshufd    $0x39,%%xmm0,%%xmm0             \n"
    "movd      %%xmm0,%5                       \n"
    "pshufd    $0x39,%%xmm0,%%xmm0             \n"
#endif
#if defined(__x86_64__) && defined(__native_client__)
    BUNDLEALIGN
    "lea       (%q0,%q1,1),%%r14d              \n"
    "movd      (%%r15,%%r14,1),%%xmm1          \n"
    "lea       (%q0,%q5,1),%%r14d              \n"
    "movd      (%%r15,%%r14,1),%%xmm6          \n"
#else
    "movd      (%0,%1,1),%%xmm1                \n"
    "movd      (%0,%5,1),%%xmm6                \n"
#endif
    "punpckldq %%xmm6,%%xmm1                   \n"
    "addps     %%xmm4,%%xmm2                   \n"
    "movq      %%xmm1,"MEMACCESS(2)"           \n"
#if defined(__x86_64__)
    "movd      %%xmm0,%1                       \n"
    "mov       %1,%5                           \n"
    "and       $0x0fffffff,%1                  \n"
    "shr       $32,%5                          \n"
#else
    "movd      %%xmm0,%1                       \n"
    "pshufd    $0x39,%%xmm0,%%xmm0             \n"
    "movd      %%xmm0,%5                       \n"
#endif
#if defined(__x86_64__) && defined(__native_client__)
    BUNDLEALIGN
    "lea       (%q0,%q1,1),%%r14d              \n"
    "movd      (%%r15,%%r14,1),%%xmm0          \n"
    "lea       (%q0,%q5,1),%%r14d              \n"
    "movd      (%%r15,%%r14,1),%%xmm6          \n"
#else
    "movd      (%0,%1,1),%%xmm0                \n"
    "movd      (%0,%5,1),%%xmm6                \n"
#endif
    "punpckldq %%xmm6,%%xmm0                   \n"
    "addps     %%xmm4,%%xmm3                   \n"
    "sub       $0x4,%4                         \n"
    "movq      %%xmm0,"MEMACCESS2(0x08,2)"     \n"
    "lea       "MEMLEA(0x10,2)",%2             \n"
    "jge       40b                             \n"

  "49:                                         \n"
    "add       $0x3,%4                         \n"
    "jl        19f                             \n"

  // 1 pixel loop                              \n"
    ".p2align  4                               \n"
  "10:                                         \n"
    "cvttps2dq %%xmm2,%%xmm0                   \n"
    "packssdw  %%xmm0,%%xmm0                   \n"
    "pmaddwd   %%xmm5,%%xmm0                   \n"
    "addps     %%xmm7,%%xmm2                   \n"
    "movd      %%xmm0,%1                       \n"
#if defined(__x86_64__)
    "and       $0x0fffffff,%1                  \n"
#endif
#if defined(__x86_64__) && defined(__native_client__)
    BUNDLEALIGN
    "lea       (%q0,%q1,1),%%r14d              \n"
    "movd      (%%r15,%%r14,1),%%xmm0          \n"
#else
    "movd      (%0,%1,1),%%xmm0                \n"
#endif
    "sub       $0x1,%4                         \n"
    "movd      %%xmm0,"MEMACCESS(2)"           \n"
    "lea       "MEMLEA(0x04,2)",%2             \n"
    "jge       10b                             \n"
  "19:                                         \n"
  : "+r"(src_argb),  // %0
    "+r"(src_argb_stride_temp),  // %1
    "+r"(dst_argb),  // %2
    "+r"(src_dudv),  // %3
    "+rm"(width),    // %4
    "+r"(temp)   // %5
  :
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBAFFINEROW_SSE2

#ifdef HAS_INTERPOLATEROW_SSSE3
// Bilinear filter 16x2 -> 16x1
void InterpolateRow_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                          ptrdiff_t src_stride, int dst_width,
                          int source_y_fraction) {
  asm volatile (
    "sub       %1,%0                           \n"
    "shr       %3                              \n"
    "cmp       $0x0,%3                         \n"
    "je        100f                            \n"
    "cmp       $0x20,%3                        \n"
    "je        75f                             \n"
    "cmp       $0x40,%3                        \n"
    "je        50f                             \n"
    "cmp       $0x60,%3                        \n"
    "je        25f                             \n"

    "movd      %3,%%xmm0                       \n"
    "neg       %3                              \n"
    "add       $0x80,%3                        \n"
    "movd      %3,%%xmm5                       \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "punpcklwd %%xmm5,%%xmm5                   \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"

    // General purpose row blend.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm2)
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm2,%%xmm0                   \n"
    "punpckhbw %%xmm2,%%xmm1                   \n"
    "pmaddubsw %%xmm5,%%xmm0                   \n"
    "pmaddubsw %%xmm5,%%xmm1                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
    "jmp       99f                             \n"

    // Blend 25 / 75.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "25:                                         \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm1)
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        25b                             \n"
    "jmp       99f                             \n"

    // Blend 50 / 50.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "50:                                         \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm1)
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        50b                             \n"
    "jmp       99f                             \n"

    // Blend 75 / 25.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "75:                                         \n"
    "movdqa    "MEMACCESS(1)",%%xmm1           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm0)
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        75b                             \n"
    "jmp       99f                             \n"

    // Blend 100 / 0 - Copy row unchanged.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "100:                                        \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    "sub       $0x10,%2                        \n"
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        100b                            \n"

  "99:                                         \n"
  : "+r"(dst_ptr),    // %0
    "+r"(src_ptr),    // %1
    "+r"(dst_width),  // %2
    "+r"(source_y_fraction)  // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm5"
#endif
  );
}
#endif  // HAS_INTERPOLATEROW_SSSE3

#ifdef HAS_INTERPOLATEROW_SSE2
// Bilinear filter 16x2 -> 16x1
void InterpolateRow_SSE2(uint8* dst_ptr, const uint8* src_ptr,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) {
  asm volatile (
    "sub       %1,%0                           \n"
    "shr       %3                              \n"
    "cmp       $0x0,%3                         \n"
    "je        100f                            \n"
    "cmp       $0x20,%3                        \n"
    "je        75f                             \n"
    "cmp       $0x40,%3                        \n"
    "je        50f                             \n"
    "cmp       $0x60,%3                        \n"
    "je        25f                             \n"

    "movd      %3,%%xmm0                       \n"
    "neg       %3                              \n"
    "add       $0x80,%3                        \n"
    "movd      %3,%%xmm5                       \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "punpcklwd %%xmm5,%%xmm5                   \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    "pxor      %%xmm4,%%xmm4                   \n"

    // General purpose row blend.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm2)           //  movdqa    (%1,%4,1),%%xmm2
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm3                   \n"
    "punpcklbw %%xmm4,%%xmm2                   \n"
    "punpckhbw %%xmm4,%%xmm3                   \n"
    "punpcklbw %%xmm4,%%xmm0                   \n"
    "punpckhbw %%xmm4,%%xmm1                   \n"
    "psubw     %%xmm0,%%xmm2                   \n"
    "psubw     %%xmm1,%%xmm3                   \n"
    "paddw     %%xmm2,%%xmm2                   \n"
    "paddw     %%xmm3,%%xmm3                   \n"
    "pmulhw    %%xmm5,%%xmm2                   \n"
    "pmulhw    %%xmm5,%%xmm3                   \n"
    "paddw     %%xmm2,%%xmm0                   \n"
    "paddw     %%xmm3,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)           //  movdqa    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
    "jmp       99f                             \n"

    // Blend 25 / 75.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "25:                                         \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm1)           //  movdqa    (%1,%4,1),%%xmm1
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)           //  movdqa    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        25b                             \n"
    "jmp       99f                             \n"

    // Blend 50 / 50.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "50:                                         \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm1)           //  movdqa    (%1,%4,1),%%xmm1
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)           //  movdqa    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        50b                             \n"
    "jmp       99f                             \n"

    // Blend 75 / 25.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "75:                                         \n"
    "movdqa    "MEMACCESS(1)",%%xmm1           \n"
    MEMOPREG(movdqa,0x00,1,4,1,xmm0)           //  movdqa    (%1,%4,1),%%xmm0
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)           //  movdqa    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        75b                             \n"
    "jmp       99f                             \n"

    // Blend 100 / 0 - Copy row unchanged.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "100:                                        \n"
    "movdqa    "MEMACCESS(1)",%%xmm0           \n"
    "sub       $0x10,%2                        \n"
    MEMOPMEM(movdqa,xmm0,0x00,1,0,1)           //  movdqa    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        100b                            \n"

  "99:                                         \n"
  : "+r"(dst_ptr),    // %0
    "+r"(src_ptr),    // %1
    "+r"(dst_width),  // %2
    "+r"(source_y_fraction)  // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_INTERPOLATEROW_SSE2

#ifdef HAS_INTERPOLATEROW_SSSE3
// Bilinear filter 16x2 -> 16x1
void InterpolateRow_Unaligned_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                                    ptrdiff_t src_stride, int dst_width,
                                    int source_y_fraction) {
  asm volatile (
    "sub       %1,%0                           \n"
    "shr       %3                              \n"
    "cmp       $0x0,%3                         \n"
    "je        100f                            \n"
    "cmp       $0x20,%3                        \n"
    "je        75f                             \n"
    "cmp       $0x40,%3                        \n"
    "je        50f                             \n"
    "cmp       $0x60,%3                        \n"
    "je        25f                             \n"

    "movd      %3,%%xmm0                       \n"
    "neg       %3                              \n"
    "add       $0x80,%3                        \n"
    "movd      %3,%%xmm5                       \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "punpcklwd %%xmm5,%%xmm5                   \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"

    // General purpose row blend.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm2)
    "movdqu    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm2,%%xmm0                   \n"
    "punpckhbw %%xmm2,%%xmm1                   \n"
    "pmaddubsw %%xmm5,%%xmm0                   \n"
    "pmaddubsw %%xmm5,%%xmm1                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
    "jmp       99f                             \n"

    // Blend 25 / 75.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "25:                                         \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm1)
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        25b                             \n"
    "jmp       99f                             \n"

    // Blend 50 / 50.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "50:                                         \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm1)
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        50b                             \n"
    "jmp       99f                             \n"

    // Blend 75 / 25.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "75:                                         \n"
    "movdqu    "MEMACCESS(1)",%%xmm1           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm0)
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        75b                             \n"
    "jmp       99f                             \n"

    // Blend 100 / 0 - Copy row unchanged.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "100:                                        \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    "sub       $0x10,%2                        \n"
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        100b                            \n"

  "99:                                         \n"
  : "+r"(dst_ptr),    // %0
    "+r"(src_ptr),    // %1
    "+r"(dst_width),  // %2
    "+r"(source_y_fraction)  // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm5"
#endif
  );
}
#endif   // HAS_INTERPOLATEROW_SSSE3

#ifdef HAS_INTERPOLATEROW_SSE2
// Bilinear filter 16x2 -> 16x1
void InterpolateRow_Unaligned_SSE2(uint8* dst_ptr, const uint8* src_ptr,
                                   ptrdiff_t src_stride, int dst_width,
                                   int source_y_fraction) {
  asm volatile (
    "sub       %1,%0                           \n"
    "shr       %3                              \n"
    "cmp       $0x0,%3                         \n"
    "je        100f                            \n"
    "cmp       $0x20,%3                        \n"
    "je        75f                             \n"
    "cmp       $0x40,%3                        \n"
    "je        50f                             \n"
    "cmp       $0x60,%3                        \n"
    "je        25f                             \n"

    "movd      %3,%%xmm0                       \n"
    "neg       %3                              \n"
    "add       $0x80,%3                        \n"
    "movd      %3,%%xmm5                       \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "punpcklwd %%xmm5,%%xmm5                   \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    "pxor      %%xmm4,%%xmm4                   \n"

    // General purpose row blend.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm2)           //  movdqu    (%1,%4,1),%%xmm2
    "movdqu    %%xmm0,%%xmm1                   \n"
    "movdqu    %%xmm2,%%xmm3                   \n"
    "punpcklbw %%xmm4,%%xmm2                   \n"
    "punpckhbw %%xmm4,%%xmm3                   \n"
    "punpcklbw %%xmm4,%%xmm0                   \n"
    "punpckhbw %%xmm4,%%xmm1                   \n"
    "psubw     %%xmm0,%%xmm2                   \n"
    "psubw     %%xmm1,%%xmm3                   \n"
    "paddw     %%xmm2,%%xmm2                   \n"
    "paddw     %%xmm3,%%xmm3                   \n"
    "pmulhw    %%xmm5,%%xmm2                   \n"
    "pmulhw    %%xmm5,%%xmm3                   \n"
    "paddw     %%xmm2,%%xmm0                   \n"
    "paddw     %%xmm3,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)           //  movdqu    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        1b                              \n"
    "jmp       99f                             \n"

    // Blend 25 / 75.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "25:                                         \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm1)           //  movdqu    (%1,%4,1),%%xmm1
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)           //  movdqu    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        25b                             \n"
    "jmp       99f                             \n"

    // Blend 50 / 50.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "50:                                         \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm1)           //  movdqu    (%1,%4,1),%%xmm1
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)           //  movdqu    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        50b                             \n"
    "jmp       99f                             \n"

    // Blend 75 / 25.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "75:                                         \n"
    "movdqu    "MEMACCESS(1)",%%xmm1           \n"
    MEMOPREG(movdqu,0x00,1,4,1,xmm0)           //  movdqu    (%1,%4,1),%%xmm0
    "pavgb     %%xmm1,%%xmm0                   \n"
    "pavgb     %%xmm1,%%xmm0                   \n"
    "sub       $0x10,%2                        \n"
    BUNDLEALIGN
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)           //  movdqu    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        75b                             \n"
    "jmp       99f                             \n"

    // Blend 100 / 0 - Copy row unchanged.
    ".p2align  4                               \n"
    BUNDLEALIGN
  "100:                                        \n"
    "movdqu    "MEMACCESS(1)",%%xmm0           \n"
    "sub       $0x10,%2                        \n"
    MEMOPMEM(movdqu,xmm0,0x00,1,0,1)           //  movdqu    %%xmm0,(%1,%0,1)
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        100b                            \n"

  "99:                                         \n"
  : "+r"(dst_ptr),    // %0
    "+r"(src_ptr),    // %1
    "+r"(dst_width),  // %2
    "+r"(source_y_fraction)  // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // HAS_INTERPOLATEROW_SSE2

#ifdef HAS_HALFROW_SSE2
void HalfRow_SSE2(const uint8* src_uv, int src_uv_stride,
                  uint8* dst_uv, int pix) {
  asm volatile (
    "sub       %0,%1                           \n"
    ".p2align  4                               \n"
    BUNDLEALIGN
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    MEMOPREG(pavgb,0x00,0,3,1,xmm0)            //  pavgb     (%0,%3),%%xmm0
    "sub       $0x10,%2                        \n"
    MEMOPMEM(movdqa,xmm0,0x00,0,1,1)           //  movdqa    %%xmm0,(%0,%1)
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "jg        1b                              \n"
  : "+r"(src_uv),  // %0
    "+r"(dst_uv),  // %1
    "+r"(pix)      // %2
  : "r"(static_cast<intptr_t>(src_uv_stride))  // %3
  : "memory", "cc"
#if defined(__SSE2__)
      , "xmm0"
#endif
  );
}
#endif  // HAS_HALFROW_SSE2

#ifdef HAS_ARGBTOBAYERROW_SSSE3
void ARGBToBayerRow_SSSE3(const uint8* src_argb, uint8* dst_bayer,
                          uint32 selector, int pix) {
  asm volatile (
    // NaCL caveat - assumes movd is from GPR
    "movd      %3,%%xmm5                       \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm5,%%xmm1                   \n"
    "punpckldq %%xmm1,%%xmm0                   \n"
    "sub       $0x8,%2                         \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_bayer), // %1
    "+r"(pix)        // %2
  : "g"(selector)    // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBTOBAYERROW_SSSE3

#ifdef HAS_ARGBSHUFFLEROW_SSSE3
// For BGRAToARGB, ABGRToARGB, RGBAToARGB, and ARGBToRGBA.
void ARGBShuffleRow_SSSE3(const uint8* src_argb, uint8* dst_argb,
                          const uint8* shuffler, int pix) {
  asm volatile (
    "movdqa    "MEMACCESS(3)",%%xmm5           \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    "MEMACCESS(0)",%%xmm0           \n"
    "movdqa    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm5,%%xmm1                   \n"
    "sub       $0x8,%2                         \n"
    "movdqa    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqa    %%xmm1,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "r"(shuffler)    // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}

void ARGBShuffleRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_argb,
                                    const uint8* shuffler, int pix) {
  asm volatile (
    "movdqa    "MEMACCESS(3)",%%xmm5           \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "movdqu    "MEMACCESS2(0x10,0)",%%xmm1     \n"
    "lea       "MEMLEA(0x20,0)",%0             \n"
    "pshufb    %%xmm5,%%xmm0                   \n"
    "pshufb    %%xmm5,%%xmm1                   \n"
    "sub       $0x8,%2                         \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "movdqu    %%xmm1,"MEMACCESS2(0x10,1)"     \n"
    "lea       "MEMLEA(0x20,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "r"(shuffler)    // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBSHUFFLEROW_SSSE3

#ifdef HAS_ARGBSHUFFLEROW_AVX2
// For BGRAToARGB, ABGRToARGB, RGBAToARGB, and ARGBToRGBA.
void ARGBShuffleRow_AVX2(const uint8* src_argb, uint8* dst_argb,
                         const uint8* shuffler, int pix) {
  asm volatile (
    "vbroadcastf128 "MEMACCESS(3)",%%ymm5      \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "vmovdqu   "MEMACCESS(0)",%%ymm0           \n"
    "vmovdqu   "MEMACCESS2(0x20,0)",%%ymm1     \n"
    "lea       "MEMLEA(0x40,0)",%0             \n"
    "vpshufb   %%ymm5,%%ymm0,%%ymm0            \n"
    "vpshufb   %%ymm5,%%ymm1,%%ymm1            \n"
    "sub       $0x10,%2                        \n"
    "vmovdqu   %%ymm0,"MEMACCESS(1)"           \n"
    "vmovdqu   %%ymm1,"MEMACCESS2(0x20,1)"     \n"
    "lea       "MEMLEA(0x40,1)",%1             \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "r"(shuffler)    // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBSHUFFLEROW_AVX2

#ifdef HAS_ARGBSHUFFLEROW_SSE2
// For BGRAToARGB, ABGRToARGB, RGBAToARGB, and ARGBToRGBA.
void ARGBShuffleRow_SSE2(const uint8* src_argb, uint8* dst_argb,
                         const uint8* shuffler, int pix) {
  uintptr_t pixel_temp = 0u;
  asm volatile (
    "pxor      %%xmm5,%%xmm5                   \n"
    "mov       "MEMACCESS(4)",%k2              \n"
    "cmp       $0x3000102,%k2                  \n"
    "je        3012f                           \n"
    "cmp       $0x10203,%k2                    \n"
    "je        123f                            \n"
    "cmp       $0x30201,%k2                    \n"
    "je        321f                            \n"
    "cmp       $0x2010003,%k2                  \n"
    "je        2103f                           \n"

  "1:                                          \n"
    BUNDLEALIGN
    "movzb     "MEMACCESS(4)",%2               \n"
    MEMOP(movzb,0x00,0,2,1)",%2                \n"  //  movzb     (%0,%2,1),%2
    "mov       %b2,"MEMACCESS(1)"              \n"
    "movzb     "MEMACCESS2(0x1,4)",%2          \n"
    MEMOP(movzb,0x00,0,2,1)",%2                \n"  //  movzb     (%0,%2,1),%2
    "mov       %b2,"MEMACCESS2(0x1,1)"         \n"
    BUNDLEALIGN
    "movzb     "MEMACCESS2(0x2,4)",%2          \n"
    MEMOP(movzb,0x00,0,2,1)",%2                \n"  //  movzb     (%0,%2,1),%2
    "mov       %b2,"MEMACCESS2(0x2,1)"         \n"
    "movzb     "MEMACCESS2(0x3,4)",%2          \n"
    MEMOP(movzb,0x00,0,2,1)",%2                \n"  //  movzb     (%0,%2,1),%2
    "mov       %b2,"MEMACCESS2(0x3,1)"         \n"
    "lea       "MEMLEA(0x4,0)",%0              \n"
    "lea       "MEMLEA(0x4,1)",%1              \n"
    "sub       $0x1,%3                         \n"
    "jg        1b                              \n"
    "jmp       99f                             \n"

    ".p2align  4                               \n"
  "123:                                        \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "punpckhbw %%xmm5,%%xmm1                   \n"
    "pshufhw   $0x1b,%%xmm0,%%xmm0             \n"
    "pshuflw   $0x1b,%%xmm0,%%xmm0             \n"
    "pshufhw   $0x1b,%%xmm1,%%xmm1             \n"
    "pshuflw   $0x1b,%%xmm1,%%xmm1             \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        123b                            \n"
    "jmp       99f                             \n"

    ".p2align  4                               \n"
  "321:                                        \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "punpckhbw %%xmm5,%%xmm1                   \n"
    "pshufhw   $0x39,%%xmm0,%%xmm0             \n"
    "pshuflw   $0x39,%%xmm0,%%xmm0             \n"
    "pshufhw   $0x39,%%xmm1,%%xmm1             \n"
    "pshuflw   $0x39,%%xmm1,%%xmm1             \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        321b                            \n"
    "jmp       99f                             \n"

    ".p2align  4                               \n"
  "2103:                                       \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "punpckhbw %%xmm5,%%xmm1                   \n"
    "pshufhw   $0x93,%%xmm0,%%xmm0             \n"
    "pshuflw   $0x93,%%xmm0,%%xmm0             \n"
    "pshufhw   $0x93,%%xmm1,%%xmm1             \n"
    "pshuflw   $0x93,%%xmm1,%%xmm1             \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        2103b                           \n"
    "jmp       99f                             \n"

    ".p2align  4                               \n"
  "3012:                                       \n"
    "movdqu    "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x10,0)",%0             \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm5,%%xmm0                   \n"
    "punpckhbw %%xmm5,%%xmm1                   \n"
    "pshufhw   $0xc6,%%xmm0,%%xmm0             \n"
    "pshuflw   $0xc6,%%xmm0,%%xmm0             \n"
    "pshufhw   $0xc6,%%xmm1,%%xmm1             \n"
    "pshuflw   $0xc6,%%xmm1,%%xmm1             \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqu    %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x10,1)",%1             \n"
    "jg        3012b                           \n"

  "99:                                         \n"
  : "+r"(src_argb),    // %0
    "+r"(dst_argb),    // %1
    "+d"(pixel_temp),  // %2
    "+r"(pix)         // %3
  : "r"(shuffler)      // %4
  : "memory", "cc"
#if defined(__native_client__) && defined(__x86_64__)
    , "r14"
#endif
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
  );
}
#endif  // HAS_ARGBSHUFFLEROW_SSE2

#ifdef HAS_I422TOYUY2ROW_SSE2
void I422ToYUY2Row_SSE2(const uint8* src_y,
                        const uint8* src_u,
                        const uint8* src_v,
                        uint8* dst_frame, int width) {
 asm volatile (
    "sub       %1,%2                             \n"
    ".p2align  4                                 \n"
  "1:                                            \n"
    "movq      (%1),%%xmm2                       \n"
    "movq      (%1,%2,1),%%xmm3                  \n"
    "lea       0x8(%1),%1                        \n"
    "punpcklbw %%xmm3,%%xmm2                     \n"
    "movdqu    (%0),%%xmm0                       \n"
    "lea       0x10(%0),%0                       \n"
    "movdqa    %%xmm0,%%xmm1                     \n"
    "punpcklbw %%xmm2,%%xmm0                     \n"
    "punpckhbw %%xmm2,%%xmm1                     \n"
    "movdqu    %%xmm0,(%3)                       \n"
    "movdqu    %%xmm1,0x10(%3)                   \n"
    "lea       0x20(%3),%3                       \n"
    "sub       $0x10,%4                          \n"
    "jg         1b                               \n"
    : "+r"(src_y),  // %0
      "+r"(src_u),  // %1
      "+r"(src_v),  // %2
      "+r"(dst_frame),  // %3
      "+rm"(width)  // %4
    :
    : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}
#endif  // HAS_I422TOYUY2ROW_SSE2

#ifdef HAS_I422TOUYVYROW_SSE2
void I422ToUYVYRow_SSE2(const uint8* src_y,
                        const uint8* src_u,
                        const uint8* src_v,
                        uint8* dst_frame, int width) {
 asm volatile (
    "sub        %1,%2                            \n"
    ".p2align  4                                 \n"
  "1:                                            \n"
    "movq      (%1),%%xmm2                       \n"
    "movq      (%1,%2,1),%%xmm3                  \n"
    "lea       0x8(%1),%1                        \n"
    "punpcklbw %%xmm3,%%xmm2                     \n"
    "movdqu    (%0),%%xmm0                       \n"
    "movdqa    %%xmm2,%%xmm1                     \n"
    "lea       0x10(%0),%0                       \n"
    "punpcklbw %%xmm0,%%xmm1                     \n"
    "punpckhbw %%xmm0,%%xmm2                     \n"
    "movdqu    %%xmm1,(%3)                       \n"
    "movdqu    %%xmm2,0x10(%3)                   \n"
    "lea       0x20(%3),%3                       \n"
    "sub       $0x10,%4                          \n"
    "jg         1b                               \n"
    : "+r"(src_y),  // %0
      "+r"(src_u),  // %1
      "+r"(src_v),  // %2
      "+r"(dst_frame),  // %3
      "+rm"(width)  // %4
    :
    : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}
#endif  // HAS_I422TOUYVYROW_SSE2

#ifdef HAS_FIXEDDIV_X86
// Divide num by div and return as 16.16 fixed point result.
int FixedDiv_X86(int num, int div) {
  asm volatile (
    "cdq                                       \n"
    "shld      $0x10,%%eax,%%edx               \n"
    "shl       $0x10,%%eax                     \n"
    "idiv      %1                              \n"
    "mov       %0, %%eax                       \n"
    : "+a"(num)  // %0
    : "c"(div)   // %1
    : "memory", "cc", "edx"
  );
  return num;
}
#endif  // HAS_FIXEDDIV_X86

#ifdef HAS_ARGBPOLYNOMIALROW_SSE2
void ARGBPolynomialRow_SSE2(const uint8* src_argb,
                            uint8* dst_argb, const float* poly,
                            int width) {
  asm volatile (
    "pxor      %%xmm3,%%xmm3                   \n"

    // 2 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movq      "MEMACCESS(0)",%%xmm0           \n"
    "lea       "MEMLEA(0x8,0)",%0              \n"
    "punpcklbw %%xmm3,%%xmm0                   \n"
    "movdqa    %%xmm0,%%xmm4                   \n"
    "punpcklwd %%xmm3,%%xmm0                   \n"
    "punpckhwd %%xmm3,%%xmm4                   \n"
    "cvtdq2ps  %%xmm0,%%xmm0                   \n"
    "cvtdq2ps  %%xmm4,%%xmm4                   \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm4,%%xmm5                   \n"
    "mulps     "MEMACCESS2(0x10,3)",%%xmm0     \n"
    "mulps     "MEMACCESS2(0x10,3)",%%xmm4     \n"
    "addps     "MEMACCESS(3)",%%xmm0           \n"
    "addps     "MEMACCESS(3)",%%xmm4           \n"
    "movdqa    %%xmm1,%%xmm2                   \n"
    "movdqa    %%xmm5,%%xmm6                   \n"
    "mulps     %%xmm1,%%xmm2                   \n"
    "mulps     %%xmm5,%%xmm6                   \n"
    "mulps     %%xmm2,%%xmm1                   \n"
    "mulps     %%xmm6,%%xmm5                   \n"
    "mulps     "MEMACCESS2(0x20,3)",%%xmm2     \n"
    "mulps     "MEMACCESS2(0x20,3)",%%xmm6     \n"
    "mulps     "MEMACCESS2(0x30,3)",%%xmm1     \n"
    "mulps     "MEMACCESS2(0x30,3)",%%xmm5     \n"
    "addps     %%xmm2,%%xmm0                   \n"
    "addps     %%xmm6,%%xmm4                   \n"
    "addps     %%xmm1,%%xmm0                   \n"
    "addps     %%xmm5,%%xmm4                   \n"
    "cvttps2dq %%xmm0,%%xmm0                   \n"
    "cvttps2dq %%xmm4,%%xmm4                   \n"
    "packuswb  %%xmm4,%%xmm0                   \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "sub       $0x2,%2                         \n"
    "movq      %%xmm0,"MEMACCESS(1)"           \n"
    "lea       "MEMLEA(0x8,1)",%1              \n"
    "jg        1b                              \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_argb),  // %1
    "+r"(width)      // %2
  : "r"(poly)        // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}
#endif  // HAS_ARGBPOLYNOMIALROW_SSE2

#ifdef HAS_ARGBPOLYNOMIALROW_AVX2
void ARGBPolynomialRow_AVX2(const uint8* src_argb,
                            uint8* dst_argb, const float* poly,
                            int width) {
  asm volatile (
    "vbroadcastf128 "MEMACCESS(3)",%%ymm4       \n"
    "vbroadcastf128 "MEMACCESS2(0x10,3)",%%ymm5 \n"
    "vbroadcastf128 "MEMACCESS2(0x20,3)",%%ymm6 \n"
    "vbroadcastf128 "MEMACCESS2(0x30,3)",%%ymm7 \n"

    // 2 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "vpmovzxbd   "MEMACCESS(0)",%%ymm0         \n"  // 2 ARGB pixels
    "lea         "MEMLEA(0x8,0)",%0            \n"
    "vcvtdq2ps   %%ymm0,%%ymm0                 \n"  // X 8 floats
    "vmulps      %%ymm0,%%ymm0,%%ymm2          \n"  // X * X
    "vmulps      %%ymm7,%%ymm0,%%ymm3          \n"  // C3 * X
    "vfmadd132ps %%ymm5,%%ymm4,%%ymm0          \n"  // result = C0 + C1 * X
    "vfmadd231ps %%ymm6,%%ymm2,%%ymm0          \n"  // result += C2 * X * X
    "vfmadd231ps %%ymm3,%%ymm2,%%ymm0          \n"  // result += C3 * X * X * X
    "vcvttps2dq  %%ymm0,%%ymm0                 \n"
    "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
    "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
    "vpackuswb   %%xmm0,%%xmm0,%%xmm0          \n"
    "sub         $0x2,%2                       \n"
    "movq        %%xmm0,"MEMACCESS(1)"         \n"
    "lea         "MEMLEA(0x8,1)",%1            \n"
    "jg          1b                            \n"
    "vzeroupper                                \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_argb),  // %1
    "+r"(width)      // %2
  : "r"(poly)        // %3
  : "memory", "cc"
#if defined(__SSE2__)
// TODO(fbarchard): declare ymm usage when applicable.
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
}
#endif  // HAS_ARGBPOLYNOMIALROW_AVX2

#ifdef HAS_ARGBCOLORTABLEROW_X86
// Tranform ARGB pixels with color table.
void ARGBColorTableRow_X86(uint8* dst_argb, const uint8* table_argb,
                           int width) {
  uintptr_t pixel_temp = 0u;
  asm volatile (
    // 1 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movzb     (%0),%1                         \n"
    "lea       0x4(%0),%0                      \n"
    "movzb     (%3,%1,4),%1                    \n"
    "mov       %b1,-0x4(%0)                    \n"
    "movzb     -0x3(%0),%1                     \n"
    "movzb     0x1(%3,%1,4),%1                 \n"
    "mov       %b1,-0x3(%0)                    \n"
    "movzb     -0x2(%0),%1                     \n"
    "movzb     0x2(%3,%1,4),%1                 \n"
    "mov       %b1,-0x2(%0)                    \n"
    "movzb     -0x1(%0),%1                     \n"
    "movzb     0x3(%3,%1,4),%1                 \n"
    "mov       %b1,-0x1(%0)                    \n"
    "dec       %2                              \n"
    "jg        1b                              \n"
  : "+r"(dst_argb),   // %0
    "+d"(pixel_temp), // %1
    "+r"(width)       // %2
  : "r"(table_argb)   // %3
  : "memory", "cc");
}
#endif  // HAS_ARGBCOLORTABLEROW_X86

#ifdef HAS_RGBCOLORTABLEROW_X86
// Tranform RGB pixels with color table.
void RGBColorTableRow_X86(uint8* dst_argb, const uint8* table_argb, int width) {
    uintptr_t pixel_temp = 0u;
  asm volatile (
    // 1 pixel loop.
    ".p2align  4                               \n"
  "1:                                          \n"
    "movzb     (%0),%1                         \n"
    "lea       0x4(%0),%0                      \n"
    "movzb     (%3,%1,4),%1                    \n"
    "mov       %b1,-0x4(%0)                    \n"
    "movzb     -0x3(%0),%1                     \n"
    "movzb     0x1(%3,%1,4),%1                 \n"
    "mov       %b1,-0x3(%0)                    \n"
    "movzb     -0x2(%0),%1                     \n"
    "movzb     0x2(%3,%1,4),%1                 \n"
    "mov       %b1,-0x2(%0)                    \n"
    "dec       %2                              \n"
    "jg        1b                              \n"
  : "+r"(dst_argb),   // %0
    "+d"(pixel_temp), // %1
    "+r"(width)       // %2
  : "r"(table_argb)   // %3
  : "memory", "cc");
}
#endif  // HAS_RGBCOLORTABLEROW_X86

#endif  // defined(__x86_64__) || defined(__i386__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
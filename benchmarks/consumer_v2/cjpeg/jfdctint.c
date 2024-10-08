/*
(C) 2014 EEMBC(R).  All rights reserved.

All EEMBC Benchmark Software are products of EEMBC
and are provided under the terms of the EEMBC Benchmark License Agreements.
The EEMBC Benchmark Software are proprietary intellectual properties of EEMBC and its Members
and is protected under all applicable laws, including all applicable copyright laws.
If you received this EEMBC Benchmark Software without having
a currently effective EEMBC Benchmark License Agreement, you must discontinue use.
Please refer to LICENSE.md for the specific license agreement that pertains to this Benchmark Software.
*/

/*==============================================================================
 *$RCSfile: jfdctint.c,v $
 *
 *   DESC : Jpeg Compression
 *
 *  EEMBC : Consumer Subcommittee
 *
 * AUTHOR :
 *          Modified for Multi Instance Test Harness by Ron Olson,
 *          IBM Corporation
 *
 *    CVS : $Revision: 1.1 $
 *          $Date: 2004/05/10 22:36:04 $
 *          $Author: rick $
 *          $Source: D:/cvs/eembc2/consumer/cjpegv2/jfdctint.c,v $
 *
 * NOTE   :
 *
 *------------------------------------------------------------------------------
 *
 * HISTORY :
 *
 * $Log: jfdctint.c,v $
 * Revision 1.1  2004/05/10 22:36:04  rick
 * Rename Consumer V1.1 benchmarks used in V2
 *
 * Revision 1.3  2004/01/22 20:16:54  rick
 * Copyright update and cleanup
 *
 * Revision 1.2  2002/04/22 22:54:52  rick
 * Standard Comment blocks
 *
 *
 *------------------------------------------------------------------------------
 * Other Copyright Notice (if any):
 *
 * For conditions of distribution and use, see the accompanying README file.
 * ===========================================================================*/
/*
 * jfdctint.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a slow-but-accurate integer implementation of the
 * forward DCT (Discrete Cosine Transform).
 *
 * A 2-D DCT can be done by 1-D DCT on each row followed by 1-D DCT
 * on each column.  Direct algorithms are also available, but they are
 * much more complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on an algorithm described in
 *   C. Loeffler, A. Ligtenberg and G. Moschytz, "Practical Fast 1-D DCT
 *   Algorithms with 11 Multiplications", Proc. Int'l. Conf. on Acoustics,
 *   Speech, and Signal Processing 1989 (ICASSP '89), pp. 988-991.
 * The primary algorithm described there uses 11 multiplies and 29 adds.
 * We use their alternate method with 12 multiplies and 32 adds.
 * The advantage of this method is that no data path contains more than one
 * multiplication; this allows a very simple and accurate implementation in
 * scaled fixed-point arithmetic, with a minimal number of shifts.
 */

#define JPEG_INTERNALS
#include "jinclude.h"

#include <th_file.h>

#include "jpeglib.h"
#include "jdct.h"        /* Private declarations for DCT subsystem */

#ifdef DCT_ISLOW_SUPPORTED


/*
 * This module is specialized to the case DCTSIZE = 8.
 */

#if DCTSIZE != 8
  Sorry, this code only copes with 8x8 DCTs. /* deliberate syntax err */
#endif


/*
 * The poop on this scaling stuff is as follows:
 *
 * Each 1-D DCT step produces outputs which are a factor of sqrt(N)
 * larger than the true DCT outputs.  The final outputs are therefore
 * a factor of N larger than desired; since N=8 this can be cured by
 * a simple right shift at the end of the algorithm.  The advantage of
 * this arrangement is that we save two multiplications per 1-D DCT,
 * because the y0 and y4 outputs need not be divided by sqrt(N).
 * In the IJG code, this factor of 8 is removed by the quantization step
 * (in jcdctmgr.c), NOT in this module.
 *
 * We have to do addition and subtraction of the integer inputs, which
 * is no problem, and multiplication by fractional constants, which is
 * a problem to do in integer arithmetic.  We multiply all the constants
 * by CONST_SCALE and convert them to integer constants (thus retaining
 * CONST_BITS bits of precision in the constants).  After doing a
 * multiplication we have to divide the product by CONST_SCALE, with proper
 * rounding, to produce the correct output.  This division can be done
 * cheaply as a right shift of CONST_BITS bits.  We postpone shifting
 * as long as possible so that partial sums can be added together with
 * full fractional precision.
 *
 * The outputs of the first pass are scaled up by PASS1_BITS bits so that
 * they are represented to better-than-integral precision.  These outputs
 * require BITS_IN_JSAMPLE + PASS1_BITS + 3 bits; this fits in a 16-bit word
 * with the recommended scaling.  (For 12-bit sample data, the intermediate
 * array is e_s32 anyway.)
 *
 * To avoid overflow of the 32-bit intermediate results in pass 2, we must
 * have BITS_IN_JSAMPLE + CONST_BITS + PASS1_BITS <= 26.  Error analysis
 * shows that the values given below are the most effective.
 */

#if BITS_IN_JSAMPLE == 8
#define CONST_BITS  13
#define PASS1_BITS  2
#else
#define CONST_BITS  13
#define PASS1_BITS  1        /* lose a little precision to avoid overflow */
#endif

/* Some C compilers fail to reduce "FIX(constant)" at compile time, thus
 * causing a lot of useless floating-point operations at run time.
 * To get around this we use the following pre-calculated constants.
 * If you change CONST_BITS you may want to add appropriate values.
 * (With a reasonable C compiler, you can just rely on the FIX() macro...)
 */

#if CONST_BITS == 13
#define FIX_0_298631336  ((e_s32)  2446)    /* FIX(0.298631336) */
#define FIX_0_390180644  ((e_s32)  3196)    /* FIX(0.390180644) */
#define FIX_0_541196100  ((e_s32)  4433)    /* FIX(0.541196100) */
#define FIX_0_765366865  ((e_s32)  6270)    /* FIX(0.765366865) */
#define FIX_0_899976223  ((e_s32)  7373)    /* FIX(0.899976223) */
#define FIX_1_175875602  ((e_s32)  9633)    /* FIX(1.175875602) */
#define FIX_1_501321110  ((e_s32)  12299)    /* FIX(1.501321110) */
#define FIX_1_847759065  ((e_s32)  15137)    /* FIX(1.847759065) */
#define FIX_1_961570560  ((e_s32)  16069)    /* FIX(1.961570560) */
#define FIX_2_053119869  ((e_s32)  16819)    /* FIX(2.053119869) */
#define FIX_2_562915447  ((e_s32)  20995)    /* FIX(2.562915447) */
#define FIX_3_072711026  ((e_s32)  25172)    /* FIX(3.072711026) */
#else
#define FIX_0_298631336  FIX(0.298631336)
#define FIX_0_390180644  FIX(0.390180644)
#define FIX_0_541196100  FIX(0.541196100)
#define FIX_0_765366865  FIX(0.765366865)
#define FIX_0_899976223  FIX(0.899976223)
#define FIX_1_175875602  FIX(1.175875602)
#define FIX_1_501321110  FIX(1.501321110)
#define FIX_1_847759065  FIX(1.847759065)
#define FIX_1_961570560  FIX(1.961570560)
#define FIX_2_053119869  FIX(2.053119869)
#define FIX_2_562915447  FIX(2.562915447)
#define FIX_3_072711026  FIX(3.072711026)
#endif

#if USE_RVV
#define IN0 "v0"
#define IN1 "v1"
#define IN2 "v2"
#define IN3 "v3"
#define IN4 "v4"
#define IN5 "v5"
#define IN6 "v6"
#define IN7 "v7"
#define TMP0 "v8"
#define TMP1 "v9"
#define TMP2 "v10"
#define TMP3 "v11"
#define TMP4 "v12"
#define TMP5 "v13"
#define TMP6 "v14"
#define TMP7 "v15"
#define TMP10 "v16"
#define TMP11 "v17"
#define TMP12 "v18"
#define TMP13 "v19"
#define Z1 "v20"
#define Z2 "v21"
#define Z3 "v22"
#define Z4 "v23"
#define Z5 "v0"
#define OUT0 "v24"
#define OUT1 "v25"
#define OUT2 "v26"
#define OUT3 "v27"
#define OUT4 "v28"
#define OUT5 "v29"
#define OUT6 "v30"
#define OUT7 "v31"

#define VOP_VV(op, vd, vs2, vs1) __asm__ volatile("v" op ".vv " vd ", " vs2 ", " vs1);
#define VOP_VI(op, vd, vs2, rs1) __asm__ volatile("v" op ".vi " vd ", " vs2 ", " #rs1);
#define VOP_VX(op, vd, vs2, rs1) __asm__ volatile("v" op ".vx " vd ", " vs2 ", %0" : : "r"(rs1));

#define VMUL_VV(vd, vs2, vs1) VOP_VV("mul", vd, vs2, vs1)
#define VMUL_VX(vd, vs2, rs1) VOP_VX("mul", vd, vs2, rs1)
#define VADD_VV(vd, vs2, vs1) VOP_VV("add", vd, vs2, vs1)
#define VADD_VX(vd, vs2, rs1) VOP_VX("add", vd, vs2, rs1)
#define VSUB_VV(vd, vs2, vs1) VOP_VV("sub", vd, vs2, vs1)
#define VSLL_VI(vd, vs2, rs1) VOP_VI("sll", vd, vs2, rs1)
#define VSRA_VI(vd, vs2, rs1) VOP_VI("sra", vd, vs2, rs1)

#define V_DESCALE(vr, n)                          \
    VADD_VX(vr, vr, ONE << (n-1));                \
    VSRA_VI(vr, vr, n);                           \

#endif



/* Multiply an e_s32 variable by an e_s32 constant to yield an e_s32 result.
 * For 8-bit samples with the recommended scaling, all the variable
 * and constant values involved are no more than 16 bits wide, so a
 * 16x16->32 bit multiply can be used instead of a full 32x32 multiply.
 * For 12-bit samples, a full 32-bit multiplication will be needed.
 */

#if BITS_IN_JSAMPLE == 8
#define MULTIPLY(var,constant)  MULTIPLY16C16(var,constant)
#else
#define MULTIPLY(var,constant)  ((var) * (constant))
#endif


/*
 * Perform the forward DCT on one block of samples.
 */

GLOBAL(void)
jpeg_fdct_islow (DCTELEM * data)
{
  e_s32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  e_s32 tmp10, tmp11, tmp12, tmp13;
  e_s32 z1, z2, z3, z4, z5;
  DCTELEM *dataptr;
  int ctr;
  SHIFT_TEMPS

#if USE_RVV
  /* Pass 1: process rows. */
  /* Note results are scaled up by sqrt(8) compared to a true DCT; */
  /* furthermore, we scale the results by 2**PASS1_BITS. */
  /* DCTELEM * rvv_out = malloc(sizeof(DCTELEM) * DCTSIZE2); */
  /* memcpy(rvv_out, data, sizeof(DCTELEM) * DCTSIZE2); */
  /* dataptr = rvv_out; */
  dataptr = data;

  for (ctr = 0; ctr < DCTSIZE;) {
    size_t vl;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(DCTSIZE));
    __asm__ volatile("vlseg8e32.v v0, (%0)" : : "r"(&dataptr[ctr * DCTSIZE]));
    VADD_VV(TMP0, IN0, IN7);
    VSUB_VV(TMP7, IN0, IN7);
    VADD_VV(TMP1, IN1, IN6);
    VSUB_VV(TMP6, IN1, IN6);
    VADD_VV(TMP2, IN2, IN5);
    VSUB_VV(TMP5, IN2, IN5);
    VADD_VV(TMP3, IN3, IN4);
    VSUB_VV(TMP4, IN3, IN4);

    VADD_VV(TMP10, TMP0, TMP3);
    VSUB_VV(TMP13, TMP0, TMP3);
    VADD_VV(TMP11, TMP1, TMP2);
    VSUB_VV(TMP12, TMP1, TMP2);

    VADD_VV(OUT0, TMP10, TMP11);
    VSLL_VI(OUT0, OUT0, PASS1_BITS);
    VSUB_VV(OUT4, TMP10, TMP11);
    VSLL_VI(OUT4, OUT4, PASS1_BITS);

    VADD_VV(Z1, TMP12, TMP13);
    VMUL_VX(Z1, Z1, FIX_0_541196100);

    VMUL_VX(OUT2, TMP13, FIX_0_765366865);
    VADD_VV(OUT2, OUT2, Z1);
    V_DESCALE(OUT2, CONST_BITS-PASS1_BITS);

    VMUL_VX(OUT6, TMP12, - FIX_1_847759065);
    VADD_VV(OUT6, OUT6, Z1);
    V_DESCALE(OUT6, CONST_BITS-PASS1_BITS);

    VADD_VV(Z1, TMP4, TMP7);
    VADD_VV(Z2, TMP5, TMP6);
    VADD_VV(Z3, TMP4, TMP6);
    VADD_VV(Z4, TMP5, TMP7);

    VADD_VV(Z5, Z3, Z4);
    VMUL_VX(Z5, Z5, FIX_1_175875602);

    VMUL_VX(TMP4, TMP4, FIX_0_298631336);
    VMUL_VX(TMP5, TMP5, FIX_2_053119869);
    VMUL_VX(TMP6, TMP6, FIX_3_072711026);
    VMUL_VX(TMP7, TMP7, FIX_1_501321110);

    VMUL_VX(Z1, Z1, - FIX_0_899976223);
    VMUL_VX(Z2, Z2, - FIX_2_562915447);
    VMUL_VX(Z3, Z3, - FIX_1_961570560);
    VMUL_VX(Z4, Z4, - FIX_0_390180644);

    VADD_VV(Z3, Z3, Z5);
    VADD_VV(Z4, Z4, Z5);

    VADD_VV(OUT7, TMP4, Z1);
    VADD_VV(OUT7, OUT7, Z3);
    V_DESCALE(OUT7, CONST_BITS-PASS1_BITS);

    VADD_VV(OUT5, TMP5, Z2);
    VADD_VV(OUT5, OUT5, Z4);
    V_DESCALE(OUT5, CONST_BITS-PASS1_BITS);

    VADD_VV(OUT3, TMP6, Z2);
    VADD_VV(OUT3, OUT3, Z3);
    V_DESCALE(OUT3, CONST_BITS-PASS1_BITS);

    VADD_VV(OUT1, TMP7, Z1);
    VADD_VV(OUT1, OUT1, Z4);
    V_DESCALE(OUT1, CONST_BITS-PASS1_BITS);

    __asm__ volatile("vsseg8e32.v v24, (%0)" : : "r"(&dataptr[ctr * DCTSIZE]));

    ctr += vl;
  }
#else

  /* Pass 1: process rows. */
  /* Note results are scaled up by sqrt(8) compared to a true DCT; */
  /* furthermore, we scale the results by 2**PASS1_BITS. */

  dataptr = data;
  for (ctr = DCTSIZE-1; ctr >= 0; ctr--) {
    tmp0 = dataptr[0] + dataptr[7];
    tmp7 = dataptr[0] - dataptr[7];
    tmp1 = dataptr[1] + dataptr[6];
    tmp6 = dataptr[1] - dataptr[6];
    tmp2 = dataptr[2] + dataptr[5];
    tmp5 = dataptr[2] - dataptr[5];
    tmp3 = dataptr[3] + dataptr[4];
    tmp4 = dataptr[3] - dataptr[4];

    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
     */

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = (DCTELEM) ((tmp10 + tmp11) << PASS1_BITS);
    dataptr[4] = (DCTELEM) ((tmp10 - tmp11) << PASS1_BITS);

    z1 = MULTIPLY(tmp12 + tmp13, FIX_0_541196100);
    dataptr[2] = (DCTELEM) DESCALE(z1 + MULTIPLY(tmp13, FIX_0_765366865),
                   CONST_BITS-PASS1_BITS);
    dataptr[6] = (DCTELEM) DESCALE(z1 + MULTIPLY(tmp12, - FIX_1_847759065),
                   CONST_BITS-PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * cK represents cos(K*pi/16).
     * i0..i3 in the paper are tmp4..tmp7 here.
     */

    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;
    z5 = MULTIPLY(z3 + z4, FIX_1_175875602); /* sqrt(2) * c3 */

    tmp4 = MULTIPLY(tmp4, FIX_0_298631336); /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 = MULTIPLY(tmp5, FIX_2_053119869); /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 = MULTIPLY(tmp6, FIX_3_072711026); /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 = MULTIPLY(tmp7, FIX_1_501321110); /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 = MULTIPLY(z1, - FIX_0_899976223); /* sqrt(2) * (c7-c3) */
    z2 = MULTIPLY(z2, - FIX_2_562915447); /* sqrt(2) * (-c1-c3) */
    z3 = MULTIPLY(z3, - FIX_1_961570560); /* sqrt(2) * (-c3-c5) */
    z4 = MULTIPLY(z4, - FIX_0_390180644); /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;

    dataptr[7] = (DCTELEM) DESCALE(tmp4 + z1 + z3, CONST_BITS-PASS1_BITS);
    dataptr[5] = (DCTELEM) DESCALE(tmp5 + z2 + z4, CONST_BITS-PASS1_BITS);
    dataptr[3] = (DCTELEM) DESCALE(tmp6 + z2 + z3, CONST_BITS-PASS1_BITS);
    dataptr[1] = (DCTELEM) DESCALE(tmp7 + z1 + z4, CONST_BITS-PASS1_BITS);

    dataptr += DCTSIZE;        /* advance pointer to next row */
  }
#endif

#if USE_RVV
  dataptr = data;
  for (ctr = 0; ctr < DCTSIZE; ) {
    size_t vl;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(DCTSIZE));
    __asm__ volatile("vle32.v v0, (%0)" : : "r"(&dataptr[DCTSIZE * 0 + ctr]));
    __asm__ volatile("vle32.v v1, (%0)" : : "r"(&dataptr[DCTSIZE * 1 + ctr]));
    __asm__ volatile("vle32.v v2, (%0)" : : "r"(&dataptr[DCTSIZE * 2 + ctr]));
    __asm__ volatile("vle32.v v3, (%0)" : : "r"(&dataptr[DCTSIZE * 3 + ctr]));
    __asm__ volatile("vle32.v v4, (%0)" : : "r"(&dataptr[DCTSIZE * 4 + ctr]));
    __asm__ volatile("vle32.v v5, (%0)" : : "r"(&dataptr[DCTSIZE * 5 + ctr]));
    __asm__ volatile("vle32.v v6, (%0)" : : "r"(&dataptr[DCTSIZE * 6 + ctr]));
    __asm__ volatile("vle32.v v7, (%0)" : : "r"(&dataptr[DCTSIZE * 7 + ctr]));

    VADD_VV(TMP0, IN0, IN7);
    VSUB_VV(TMP7, IN0, IN7);
    VADD_VV(TMP1, IN1, IN6);
    VSUB_VV(TMP6, IN1, IN6);
    VADD_VV(TMP2, IN2, IN5);
    VSUB_VV(TMP5, IN2, IN5);
    VADD_VV(TMP3, IN3, IN4);
    VSUB_VV(TMP4, IN3, IN4);

    VADD_VV(TMP10, TMP0, TMP3);
    VSUB_VV(TMP13, TMP0, TMP3);
    VADD_VV(TMP11, TMP1, TMP2);
    VSUB_VV(TMP12, TMP1, TMP2);

    VADD_VV(OUT0, TMP10, TMP11);
    V_DESCALE(OUT0, PASS1_BITS);
    VSUB_VV(OUT4, TMP10, TMP11);
    V_DESCALE(OUT4, PASS1_BITS);

    VADD_VV(Z1, TMP12, TMP13);
    VMUL_VX(Z1, Z1, FIX_0_541196100);

    VMUL_VX(OUT2, TMP13, FIX_0_765366865);
    VADD_VV(OUT2, OUT2, Z1);
    V_DESCALE(OUT2, CONST_BITS+PASS1_BITS);

    VMUL_VX(OUT6, TMP12, - FIX_1_847759065);
    VADD_VV(OUT6, OUT6, Z1);
    V_DESCALE(OUT6, CONST_BITS+PASS1_BITS);

    VADD_VV(Z1, TMP4, TMP7);
    VADD_VV(Z2, TMP5, TMP6);
    VADD_VV(Z3, TMP4, TMP6);
    VADD_VV(Z4, TMP5, TMP7);

    VADD_VV(Z5, Z3, Z4);
    VMUL_VX(Z5, Z5, FIX_1_175875602);

    VMUL_VX(TMP4, TMP4, FIX_0_298631336);
    VMUL_VX(TMP5, TMP5, FIX_2_053119869);
    VMUL_VX(TMP6, TMP6, FIX_3_072711026);
    VMUL_VX(TMP7, TMP7, FIX_1_501321110);

    VMUL_VX(Z1, Z1, - FIX_0_899976223);
    VMUL_VX(Z2, Z2, - FIX_2_562915447);
    VMUL_VX(Z3, Z3, - FIX_1_961570560);
    VMUL_VX(Z4, Z4, - FIX_0_390180644);

    VADD_VV(Z3, Z3, Z5);
    VADD_VV(Z4, Z4, Z5);

    VADD_VV(OUT7, TMP4, Z1);
    VADD_VV(OUT7, OUT7, Z3);
    V_DESCALE(OUT7, CONST_BITS+PASS1_BITS);

    VADD_VV(OUT5, TMP5, Z2);
    VADD_VV(OUT5, OUT5, Z4);
    V_DESCALE(OUT5, CONST_BITS+PASS1_BITS);

    VADD_VV(OUT3, TMP6, Z2);
    VADD_VV(OUT3, OUT3, Z3);
    V_DESCALE(OUT3, CONST_BITS+PASS1_BITS);

    VADD_VV(OUT1, TMP7, Z1);
    VADD_VV(OUT1, OUT1, Z4);
    V_DESCALE(OUT1, CONST_BITS+PASS1_BITS);

    __asm__ volatile("vse32.v v24, (%0)" : :"r"(&dataptr[DCTSIZE * 0 + ctr]));
    __asm__ volatile("vse32.v v25, (%0)" : :"r"(&dataptr[DCTSIZE * 1 + ctr]));
    __asm__ volatile("vse32.v v26, (%0)" : :"r"(&dataptr[DCTSIZE * 2 + ctr]));
    __asm__ volatile("vse32.v v27, (%0)" : :"r"(&dataptr[DCTSIZE * 3 + ctr]));
    __asm__ volatile("vse32.v v28, (%0)" : :"r"(&dataptr[DCTSIZE * 4 + ctr]));
    __asm__ volatile("vse32.v v29, (%0)" : :"r"(&dataptr[DCTSIZE * 5 + ctr]));
    __asm__ volatile("vse32.v v30, (%0)" : :"r"(&dataptr[DCTSIZE * 6 + ctr]));
    __asm__ volatile("vse32.v v31, (%0)" : :"r"(&dataptr[DCTSIZE * 7 + ctr]));
    ctr += vl;
  }
#else
  /* Pass 2: process columns.
   * We remove the PASS1_BITS scaling, but leave the results scaled up
   * by an overall factor of 8.
   */

  dataptr = data;
  for (ctr = DCTSIZE-1; ctr >= 0; ctr--) {
    tmp0 = dataptr[DCTSIZE*0] + dataptr[DCTSIZE*7];
    tmp7 = dataptr[DCTSIZE*0] - dataptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] + dataptr[DCTSIZE*6];
    tmp6 = dataptr[DCTSIZE*1] - dataptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] + dataptr[DCTSIZE*5];
    tmp5 = dataptr[DCTSIZE*2] - dataptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] + dataptr[DCTSIZE*4];
    tmp4 = dataptr[DCTSIZE*3] - dataptr[DCTSIZE*4];

    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
     */

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[DCTSIZE*0] = (DCTELEM) DESCALE(tmp10 + tmp11, PASS1_BITS);
    dataptr[DCTSIZE*4] = (DCTELEM) DESCALE(tmp10 - tmp11, PASS1_BITS);

    z1 = MULTIPLY(tmp12 + tmp13, FIX_0_541196100);
    dataptr[DCTSIZE*2] = (DCTELEM) DESCALE(z1 + MULTIPLY(tmp13, FIX_0_765366865),
                       CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*6] = (DCTELEM) DESCALE(z1 + MULTIPLY(tmp12, - FIX_1_847759065),
                       CONST_BITS+PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * cK represents cos(K*pi/16).
     * i0..i3 in the paper are tmp4..tmp7 here.
     */

    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;
    z5 = MULTIPLY(z3 + z4, FIX_1_175875602); /* sqrt(2) * c3 */

    tmp4 = MULTIPLY(tmp4, FIX_0_298631336); /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 = MULTIPLY(tmp5, FIX_2_053119869); /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 = MULTIPLY(tmp6, FIX_3_072711026); /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 = MULTIPLY(tmp7, FIX_1_501321110); /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 = MULTIPLY(z1, - FIX_0_899976223); /* sqrt(2) * (c7-c3) */
    z2 = MULTIPLY(z2, - FIX_2_562915447); /* sqrt(2) * (-c1-c3) */
    z3 = MULTIPLY(z3, - FIX_1_961570560); /* sqrt(2) * (-c3-c5) */
    z4 = MULTIPLY(z4, - FIX_0_390180644); /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;

    dataptr[DCTSIZE*7] = (DCTELEM) DESCALE(tmp4 + z1 + z3,
                       CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*5] = (DCTELEM) DESCALE(tmp5 + z2 + z4,
                       CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*3] = (DCTELEM) DESCALE(tmp6 + z2 + z3,
                       CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*1] = (DCTELEM) DESCALE(tmp7 + z1 + z4,
                       CONST_BITS+PASS1_BITS);

    dataptr++;            /* advance pointer to next column */
  }
#endif

}

#endif /* DCT_ISLOW_SUPPORTED */

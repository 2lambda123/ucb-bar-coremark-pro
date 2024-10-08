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
 *$RCSfile: jcsample.c,v $
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
 *          $Source: D:/cvs/eembc2/consumer/cjpegv2/jcsample.c,v $
 *          
 * NOTE   :
 *
 *------------------------------------------------------------------------------
 *
 * HISTORY :
 *
 * $Log: jcsample.c,v $
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
 * jcsample.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains downsampling routines.
 *
 * Downsampling input data is counted in "row groups".  A row group
 * is defined to be max_v_samp_factor pixel rows of each component,
 * from which the downsampler produces v_samp_factor sample rows.
 * A single row group is processed in each call to the downsampler module.
 *
 * The downsampler is responsible for edge-expansion of its output data
 * to fill an integral number of DCT blocks horizontally.  The source buffer
 * may be modified if it is helpful for this purpose (the source buffer is
 * allocated wide enough to correspond to the desired output width).
 * The caller (the prep controller) is responsible for vertical padding.
 *
 * The downsampler may request "context rows" by setting need_context_rows
 * during startup.  In this case, the input arrays will contain at least
 * one row group's worth of pixels above and below the passed-in data;
 * the caller will create dummy rows at image top and bottom by replicating
 * the first or last real pixel row.
 *
 * An excellent reference for image resampling is
 *   Digital Image Warping, George Wolberg, 1990.
 *   Pub. by IEEE Computer Society Press, Los Alamitos, CA. ISBN 0-8186-8944-7.
 *
 * The downsampling algorithm used here is a simple average of the source
 * pixels covered by the output pixel.  The hi-falutin sampling literature
 * refers to this as a "box filter".  In general the characteristics of a box
 * filter are not very good, but for the specific cases we normally use (1:1
 * and 2:1 ratios) the box is equivalent to a "triangle filter" which is not
 * nearly so bad.  If you intend to use other sampling ratios, you'd be well
 * advised to improve this code.
 *
 * A simple input-smoothing capability is provided.  This is mainly intended
 * for cleaning up color-dithered GIF input files (if you find it inadequate,
 * we suggest using an external filtering program such as pnmconvol).  When
 * enabled, each input pixel P is replaced by a weighted sum of itself and its
 * eight neighbors.  P's weight is 1-8*SF and each neighbor's weight is SF,
 * where SF = (smoothing_factor / 1024).
 * Currently, smoothing is only supported for 2h2v sampling factors.
 */

#define JPEG_INTERNALS
#include "jinclude.h"

#include <th_file.h>

#include "jpeglib.h"


/* Pointer to routine to downsample a single component */
typedef JMETHOD(void, downsample1_ptr,
        (j_compress_ptr cinfo, jpeg_component_info * compptr,
         JSAMPARRAY input_data, JSAMPARRAY output_data));

/* Private subobject */

typedef struct {
  struct jpeg_downsampler pub;    /* public fields */

  /* Downsampling method pointers, one per component */
  downsample1_ptr methods[MAX_COMPONENTS];
} my_downsampler;

typedef my_downsampler * my_downsample_ptr;


/*
 * Initialize for a downsampling pass.
 */

METHODDEF(void)
start_pass_downsample (j_compress_ptr cinfo)
{
  /* no work for now */
}


/*
 * Expand a component horizontally from width input_cols to width output_cols,
 * by duplicating the rightmost samples.
 */

LOCAL(void)
expand_right_edge (JSAMPARRAY image_data, int num_rows,
           JDIMENSION input_cols, JDIMENSION output_cols)
{
  register JSAMPROW ptr;
  register JSAMPLE pixval;
  register int count;
  int row;
  int numcols = (int) (output_cols - input_cols);

  if (numcols > 0) {
    for (row = 0; row < num_rows; row++) {
      ptr = image_data[row] + input_cols;
      pixval = ptr[-1];        /* don't need GETJSAMPLE() here */
      for (count = numcols; count > 0; count--)
    *ptr++ = pixval;
    }
  }
}


/*
 * Do downsampling for a whole row group (all components).
 *
 * In this version we simply downsample each component independently.
 */

METHODDEF(void)
sep_downsample (j_compress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION in_row_index,
        JSAMPIMAGE output_buf, JDIMENSION out_row_group_index)
{
  my_downsample_ptr downsample = (my_downsample_ptr) cinfo->downsample;
  int ci;
  jpeg_component_info * compptr;
  JSAMPARRAY in_ptr, out_ptr;

  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    in_ptr = input_buf[ci] + in_row_index;
    out_ptr = output_buf[ci] + (out_row_group_index * compptr->v_samp_factor);
    (*downsample->methods[ci]) (cinfo, compptr, in_ptr, out_ptr);
  }
}


/*
 * Downsample pixel values of a single component.
 * One row group is processed per call.
 * This version handles arbitrary integral sampling ratios, without smoothing.
 * Note that this version is not actually used for customary sampling ratios.
 */

METHODDEF(void)
int_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
        JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow, outrow, h_expand, v_expand, numpix, numpix2, h, v;
  JDIMENSION outcol, outcol_h;    /* outcol_h == outcol*h_expand */
  JDIMENSION output_cols = compptr->width_in_blocks * DCTSIZE;
  JSAMPROW inptr, outptr;
  e_s32 outvalue;

  h_expand = cinfo->max_h_samp_factor / compptr->h_samp_factor;
  v_expand = cinfo->max_v_samp_factor / compptr->v_samp_factor;
  numpix = h_expand * v_expand;
  numpix2 = numpix/2;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expand_right_edge(input_data, cinfo->max_v_samp_factor,
            cinfo->image_width, output_cols * h_expand);

  inrow = 0;
  for (outrow = 0; outrow < compptr->v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    for (outcol = 0, outcol_h = 0; outcol < output_cols;
     outcol++, outcol_h += h_expand) {
      outvalue = 0;
      for (v = 0; v < v_expand; v++) {
    inptr = input_data[inrow+v] + outcol_h;
    for (h = 0; h < h_expand; h++) {
      outvalue += (e_s32) GETJSAMPLE(*inptr++);
    }
      }
      *outptr++ = (JSAMPLE) ((outvalue + numpix2) / numpix);
    }
    inrow += v_expand;
  }
}


/*
 * Downsample pixel values of a single component.
 * This version handles the special case of a full-size component,
 * without smoothing.
 */

METHODDEF(void)
fullsize_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  /* Copy the data */
  cjcopy_sample_rows(input_data, 0, output_data, 0,
            cinfo->max_v_samp_factor, cinfo->image_width);
  /* Edge-expand */
  expand_right_edge(output_data, cinfo->max_v_samp_factor,
            cinfo->image_width, compptr->width_in_blocks * DCTSIZE);
}


/*
 * Downsample pixel values of a single component.
 * This version handles the common case of 2:1 horizontal and 1:1 vertical,
 * without smoothing.
 *
 * A note about the "bias" calculations: when rounding fractional values to
 * integer, we do not want to always round 0.5 up to the next integer.
 * If we did that, we'd introduce a noticeable bias towards larger values.
 * Instead, this code is arranged so that 0.5 will be rounded up or down at
 * alternate pixel locations (a simple ordered dither pattern).
 */

METHODDEF(void)
h2v1_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
         JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int outrow;
  JDIMENSION outcol;
  JDIMENSION output_cols = compptr->width_in_blocks * DCTSIZE;
  register JSAMPROW inptr, outptr;
  register int bias;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expand_right_edge(input_data, cinfo->max_v_samp_factor,
            cinfo->image_width, output_cols * 2);

  for (outrow = 0; outrow < compptr->v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr = input_data[outrow];
    bias = 0;            /* bias = 0,1,0,1,... for successive samples */
    for (outcol = 0; outcol < output_cols; outcol++) {
      *outptr++ = (JSAMPLE) ((GETJSAMPLE(*inptr) + GETJSAMPLE(inptr[1])
                  + bias) >> 1);
      bias ^= 1;        /* 0=>1, 1=>0 */
      inptr += 2;
    }
  }
}


/*
 * Downsample pixel values of a single component.
 * This version handles the standard case of 2:1 horizontal and 2:1 vertical,
 * without smoothing.
 */

METHODDEF(void)
h2v2_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
         JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  int inrow, outrow;
  JDIMENSION outcol;
  JDIMENSION output_cols = compptr->width_in_blocks * DCTSIZE;
  register JSAMPROW inptr0, inptr1, outptr;
  register int bias;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expand_right_edge(input_data, cinfo->max_v_samp_factor,
            cinfo->image_width, output_cols * 2);

  inrow = 0;
  for (outrow = 0; outrow < compptr->v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow+1];
    bias = 1;            /* bias = 1,2,1,2,... for successive samples */

#if USE_RVV
    size_t vl;
    __asm__ volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(output_cols));
    __asm__ volatile("vmv.v.x v0, %0" : : "r"((uint16_t)0x0201));
    for (outcol = 0; outcol < output_cols; ) {
      __asm__ volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(output_cols - outcol));
      __asm__ volatile("vlseg2e8.v  v8, (%0)" : : "r"(&inptr0[outcol << 1]));
      __asm__ volatile("vlseg2e8.v v16, (%0)" : : "r"(&inptr1[outcol << 1]));
      __asm__ volatile("vwaddu.vv  v24, v8 , v12");
      __asm__ volatile("vwaddu.wv  v24, v24, v16");
      __asm__ volatile("vwaddu.wv  v24, v24, v20");
      __asm__ volatile("vwaddu.wv  v24, v24, v0");
      __asm__ volatile("vnsra.wi   v20, v24, 2");
      __asm__ volatile("vse8.v     v20, (%0)" : : "r"(&outptr[outcol]));
      outcol += vl;
    }
#else
    for (outcol = 0; outcol < output_cols; outcol++) {
      *outptr++ = (JSAMPLE) ((GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[1]) +
                  GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[1])
                  + bias) >> 2);
      bias ^= 3;        /* 1=>2, 2=>1 */
      inptr0 += 2; inptr1 += 2;
    }
#endif
    inrow += 2;
  }
}



/*
 * Module initialization routine for downsampling.
 * Note that we must select a routine for each component.
 */

GLOBAL(void)
jinit_downsampler (j_compress_ptr cinfo)
{
  my_downsample_ptr downsample;
  int ci;
  jpeg_component_info * compptr;
  boolean smoothok = TRUE;

  downsample = (my_downsample_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                SIZEOF(my_downsampler));
  cinfo->downsample = (struct jpeg_downsampler *) downsample;
  downsample->pub.start_pass = start_pass_downsample;
  downsample->pub.downsample = sep_downsample;
  downsample->pub.need_context_rows = FALSE;

  if (cinfo->CCIR601_sampling)
    ERREXIT(cinfo, JERR_CCIR601_NOTIMPL);

  /* Verify we can handle the sampling factors, and set up method pointers */
  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    if (compptr->h_samp_factor == cinfo->max_h_samp_factor &&
    compptr->v_samp_factor == cinfo->max_v_samp_factor) {
    downsample->methods[ci] = fullsize_downsample;
    } else if (compptr->h_samp_factor * 2 == cinfo->max_h_samp_factor &&
           compptr->v_samp_factor == cinfo->max_v_samp_factor) {
      smoothok = FALSE;
      downsample->methods[ci] = h2v1_downsample;
    } else if (compptr->h_samp_factor * 2 == cinfo->max_h_samp_factor &&
           compptr->v_samp_factor * 2 == cinfo->max_v_samp_factor) {
    downsample->methods[ci] = h2v2_downsample;
    } else if ((cinfo->max_h_samp_factor % compptr->h_samp_factor) == 0 &&
           (cinfo->max_v_samp_factor % compptr->v_samp_factor) == 0) {
      smoothok = FALSE;
      downsample->methods[ci] = int_downsample;
    } else
      ERREXIT(cinfo, JERR_FRACT_SAMPLE_NOTIMPL);
  }

}

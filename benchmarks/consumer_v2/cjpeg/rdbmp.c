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
 *$RCSfile: rdbmp.c,v $
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
 *          $Date: 2004/05/10 22:36:05 $
 *          $Author: rick $
 *          $Source: D:/cvs/eembc2/consumer/cjpegv2/rdbmp.c,v $
 *          
 * NOTE   :
 *
 *------------------------------------------------------------------------------
 *
 * HISTORY :
 *
 * $Log: rdbmp.c,v $
 * Revision 1.1  2004/05/10 22:36:05  rick
 * Rename Consumer V1.1 benchmarks used in V2
 *
 * Revision 1.5  2004/02/21 00:05:51  rick
 * Upgrade cjpeg to V2
 *
 * Revision 1.4  2004/02/06 18:16:23  rick
 * Fix errno conflicts
 *
 * Revision 1.3  2004/01/22 20:16:55  rick
 * Copyright update and cleanup
 *
 * Revision 1.2  2002/04/22 22:54:53  rick
 * Standard Comment blocks
 *
 *
 *------------------------------------------------------------------------------
 * Other Copyright Notice (if any): 
 * 
 * For conditions of distribution and use, see the accompanying README file.
 * ===========================================================================*/
/*
 * rdbmp.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to read input images in Microsoft "BMP"
 * format (MS Windows 3.x, OS/2 1.x, and OS/2 2.x flavors).
 * Currently, only 8-bit and 24-bit images are supported, not 1-bit or
 * 4-bit (feeding such low-depth images into JPEG would be silly anyway).
 * Also, we don't support RLE-compressed files.
 *
 * These routines may need modification for non-Unix environments or
 * specialized applications.  As they stand, they assume input from
 * an ordinary stdio stream.  They further assume that reading begins
 * at the start of the file; start_input may need work if the
 * user interface has already read some data (e.g., to determine that
 * the file is indeed BMP format).
 *
 * This code contributed by James Arthur Boucher.
 */

#include "cdjpeg.h"        /* Common decls for cjpeg/djpeg applications */
#include <th_file.h>

/* Macros to deal with unsigned chars as efficiently as compiler allows */

#ifdef HAVE_UNSIGNED_CHAR
#define UCH(x)    ((int) (x))
#else /* !HAVE_UNSIGNED_CHAR */
#ifdef CHAR_IS_UNSIGNED
#define UCH(x)    ((int) (x))
#else
#define UCH(x)    ((int) (x) & 0xFF)
#endif
#endif /* HAVE_UNSIGNED_CHAR */


#define		ReadOK(file,buffer,len)    (JFREAD(file,buffer,len) == ((size_t) (len)))
size_t		fread_bmp_line (JSAMPROW ptr, size_t size, size_t nelem, cjpparam_t *params);


/* Private version of data source object */

typedef struct _bmp_source_struct * bmp_source_ptr;

typedef struct _bmp_source_struct {
  struct cjpeg_source_struct pub; /* public fields */

  j_compress_ptr cinfo;        /* back link saves passing separate parm */

  JSAMPARRAY colormap;        /* BMP colormap (converted to my format) */

  jvirt_sarray_ptr whole_image;    /* Needed to reverse row order */
  JDIMENSION source_row;    /* Current source row number */
  JDIMENSION row_width;        /* Physical width of scanlines in file */

  int bits_per_pixel;        /* remembers 8- or 24-bit format */
} bmp_source_struct;

LOCAL(int)
read_byte (bmp_source_ptr sinfo)
/* Read next byte from BMP file */
{
  cjpparam_t *params = sinfo->pub.input_file;
  register int c;

  if (params->inFile_idx >= params->inFile_size) {
      c = EOF;
      ERREXIT(sinfo->cinfo, JERR_INPUT_EOF);
  } else {
      c = params->inFile_p[params->inFile_idx++];
  }
  return c;
}


LOCAL(void)
read_colormap (bmp_source_ptr sinfo, int cmaplen, int mapentrysize)
/* Read the colormap from a BMP file */
{
  int i;

  switch (mapentrysize) {
  case 3:
    /* BGR format (occurs in OS/2 files) */
    for (i = 0; i < cmaplen; i++) {
      sinfo->colormap[2][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[1][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[0][i] = (JSAMPLE) read_byte(sinfo);
    }
    break;
  case 4:
    /* BGR0 format (occurs in MS Windows files) */
    for (i = 0; i < cmaplen; i++) {
      sinfo->colormap[2][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[1][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[0][i] = (JSAMPLE) read_byte(sinfo);
      (void) read_byte(sinfo);
    }
    break;
  default:
    ERREXIT(sinfo->cinfo, JERR_BMP_BADCMAP);
    break;
  }
}


/*
 * Read one row of pixels.
 * The image has been read into the whole_image array, but is otherwise
 * unprocessed.  We must read it out in top-to-bottom row order, and if
 * it is an 8-bit image, we must expand colormapped pixels to 24bit format.
 */

METHODDEF(JDIMENSION)
get_8bit_row (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit colormap indexes */
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  register JSAMPARRAY colormap = source->colormap;
  JSAMPARRAY image_ptr;
  register int t;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;

  /* Fetch next row from virtual array */
  source->source_row--;
  image_ptr = (*cinfo->mem->access_virt_sarray)
    ((j_common_ptr) cinfo, source->whole_image,
     source->source_row, (JDIMENSION) 1, FALSE);

  /* Expand the colormap indexes to real data */
  inptr = image_ptr[0];
  outptr = source->pub.buffer[0];

#if USE_RVV
  size_t n = cinfo->image_width;
  for (size_t c = 0; c < n;) {
    size_t vl;
    __asm__ volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(n - c));
    __asm__ volatile("vle8.v v0, (%0)" : : "r"(&inptr[c]));
    __asm__ volatile("vluxei8.v  v8, (%0), v0" : : "r"(colormap[0]));
    __asm__ volatile("vluxei8.v v10, (%0), v0" : : "r"(colormap[1]));
    __asm__ volatile("vluxei8.v v12, (%0), v0" : : "r"(colormap[2]));
    __asm__ volatile("vsseg3e8.v v8, (%0)" : : "r"(&outptr[c*3]));
    c += vl;
  }
#else
  for (col = cinfo->image_width; col > 0; col--) {
    t = GETJSAMPLE(*inptr++);
    *outptr++ = colormap[0][t];    /* can omit GETJSAMPLE() safely */
    *outptr++ = colormap[1][t];
    *outptr++ = colormap[2][t];
  }
#endif
  return 1;
}


METHODDEF(JDIMENSION)
get_24bit_row (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 24-bit pixels */
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  JSAMPARRAY image_ptr;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;

  /* Fetch next row from virtual array */
  source->source_row--;
  image_ptr = (*cinfo->mem->access_virt_sarray)
    ((j_common_ptr) cinfo, source->whole_image,
     source->source_row, (JDIMENSION) 1, FALSE);

  /* Transfer data.  Note source values are in BGR order
   * (even though Microsoft's own documents say the opposite).
   */
  inptr = image_ptr[0];
  outptr = source->pub.buffer[0];
  for (col = cinfo->image_width; col > 0; col--) {
    outptr[2] = *inptr++;    /* can omit GETJSAMPLE() safely */
    outptr[1] = *inptr++;
    outptr[0] = *inptr++;
    outptr += 3;
  }

  return 1;
}


/*
 * This method loads the image into whole_image during the first call on
 * get_pixel_rows.  The get_pixel_rows pointer is then adjusted to call
 * get_8bit_row or get_24bit_row on subsequent calls.
 */

METHODDEF(JDIMENSION)
preload_image (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  register cjpparam_t *infile = source->pub.input_file;
  register JSAMPROW out_ptr;
  JSAMPARRAY image_ptr;
  JDIMENSION row;
  cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;

  /* Read the data into a virtual array in input-file row order. */
  for (row = 0; row < cinfo->image_height; row++) 
  {
    if (progress != NULL) 
	{
      progress->pub.pass_counter = (long) row;
      progress->pub.pass_limit = (long) cinfo->image_height;
      (*progress->pub.progress_monitor) ((j_common_ptr) cinfo);
    }

    image_ptr = (*cinfo->mem->access_virt_sarray)
      ((j_common_ptr) cinfo, source->whole_image,
       row, (JDIMENSION) 1, TRUE);

    out_ptr = image_ptr[0];

	if( !fread_bmp_line( out_ptr, source->row_width, 1, infile ) )
		  ERREXIT(cinfo, JERR_INPUT_EOF);
  }

  if (progress != NULL)
    progress->completed_extra_passes++;

  /* Set up to read from the virtual array in top-to-bottom order */
  switch (source->bits_per_pixel) 
  {
	case 8:
		source->pub.get_pixel_rows = get_8bit_row;
		break;
	
	case 24:
		source->pub.get_pixel_rows = get_24bit_row;
		break;
	
	default:
		ERREXIT(cinfo, JERR_BMP_BADDEPTH);
  }
  source->source_row = cinfo->image_height;

  /* And read the first row */
  return (*source->pub.get_pixel_rows) (cinfo, sinfo);
}


/*
 * Read the file header; return image size and component count.
 */

METHODDEF(void)
start_input_bmp (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  e_u8 bmpfileheader[14];
  e_u8 bmpinfoheader[64];
#define GET_2B(array,offset)  ((unsigned int) UCH(array[offset]) + \
                   (((unsigned int) UCH(array[offset+1])) << 8))
#define GET_4B(array,offset)  ((e_s32) UCH(array[offset]) + \
                   (((e_s32) UCH(array[offset+1])) << 8) + \
                   (((e_s32) UCH(array[offset+2])) << 16) + \
                   (((e_s32) UCH(array[offset+3])) << 24))
  e_s32 bfOffBits;
  e_s32 headerSize;
  e_s32 biWidth = 0;        /* initialize to avoid compiler warning */
  e_s32 biHeight = 0;
  unsigned int biPlanes;
  e_s32 biCompression;
  e_s32 biXPelsPerMeter,biYPelsPerMeter;
  e_s32 biClrUsed = 0;
  int mapentrysize = 0;        /* 0 indicates no colormap */
  e_s32 bPad;
  JDIMENSION row_width;

  /* Read and verify the bitmap file header */
  if (! ReadOK(source->pub.input_file, bmpfileheader, 14))
    ERREXIT(cinfo, JERR_INPUT_EOF);
  if (GET_2B(bmpfileheader,0) != 0x4D42) /* 'BM' */
    ERREXIT(cinfo, JERR_BMP_NOT);
  bfOffBits = (e_s32) GET_4B(bmpfileheader,10);
  /* We ignore the remaining fileheader fields */

  /* The infoheader might be 12 bytes (OS/2 1.x), 40 bytes (Windows),
   * or 64 bytes (OS/2 2.x).  Check the first 4 bytes to find out which.
   */
  if (! ReadOK(source->pub.input_file, bmpinfoheader, 4))
    ERREXIT(cinfo, JERR_INPUT_EOF);
  headerSize = (e_s32) GET_4B(bmpinfoheader,0);
  if (headerSize < 12 || headerSize > 64)
    ERREXIT(cinfo, JERR_BMP_BADHEADER);
  if (! ReadOK(source->pub.input_file, bmpinfoheader+4, headerSize-4))
    ERREXIT(cinfo, JERR_INPUT_EOF);

  switch ((int) headerSize) {
  case 12:
    /* Decode OS/2 1.x header (Microsoft calls this a BITMAPCOREHEADER) */
    biWidth = (e_s32) GET_2B(bmpinfoheader,4);
    biHeight = (e_s32) GET_2B(bmpinfoheader,6);
    biPlanes = GET_2B(bmpinfoheader,8);
    source->bits_per_pixel = (int) GET_2B(bmpinfoheader,10);

    switch (source->bits_per_pixel) {
    case 8:            /* colormapped image */
      mapentrysize = 3;        /* OS/2 uses RGBTRIPLE colormap */
      TRACEMS2(cinfo, 1, JTRC_BMP_OS2_MAPPED, (int) biWidth, (int) biHeight);
      break;
    case 24:            /* RGB image */
      TRACEMS2(cinfo, 1, JTRC_BMP_OS2, (int) biWidth, (int) biHeight);
      break;
    default:
      ERREXIT(cinfo, JERR_BMP_BADDEPTH);
      break;
    }
    if (biPlanes != 1)
      ERREXIT(cinfo, JERR_BMP_BADPLANES);
    break;
  case 40:
  case 64:
    /* Decode Windows 3.x header (Microsoft calls this a BITMAPINFOHEADER) */
    /* or OS/2 2.x header, which has additional fields that we ignore */
    biWidth = GET_4B(bmpinfoheader,4);
    biHeight = GET_4B(bmpinfoheader,8);
    biPlanes = GET_2B(bmpinfoheader,12);
    source->bits_per_pixel = (int) GET_2B(bmpinfoheader,14);
    biCompression = GET_4B(bmpinfoheader,16);
    biXPelsPerMeter = GET_4B(bmpinfoheader,24);
    biYPelsPerMeter = GET_4B(bmpinfoheader,28);
    biClrUsed = GET_4B(bmpinfoheader,32);
    /* biSizeImage, biClrImportant fields are ignored */

    switch (source->bits_per_pixel) {
    case 8:            /* colormapped image */
      mapentrysize = 4;        /* Windows uses RGBQUAD colormap */
      TRACEMS2(cinfo, 1, JTRC_BMP_MAPPED, (int) biWidth, (int) biHeight);
      break;
    case 24:            /* RGB image */
      TRACEMS2(cinfo, 1, JTRC_BMP, (int) biWidth, (int) biHeight);
      break;
    default:
      ERREXIT(cinfo, JERR_BMP_BADDEPTH);
      break;
    }
    if (biPlanes != 1)
      ERREXIT(cinfo, JERR_BMP_BADPLANES);
    if (biCompression != 0)
      ERREXIT(cinfo, JERR_BMP_COMPRESSED);

    if (biXPelsPerMeter > 0 && biYPelsPerMeter > 0) {
      /* Set JFIF density parameters from the BMP data */
      cinfo->X_density = (e_u16) (biXPelsPerMeter/100); /* 100 cm per meter */
      cinfo->Y_density = (e_u16) (biYPelsPerMeter/100);
      cinfo->density_unit = 2;    /* dots/cm */
    }
    break;
  default:
    ERREXIT(cinfo, JERR_BMP_BADHEADER);
    break;
  }

  /* Compute distance to bitmap data --- will adjust for colormap below */
  bPad = bfOffBits - (headerSize + 14);

  /* Read the colormap, if any */
  if (mapentrysize > 0) {
    if (biClrUsed <= 0)
      biClrUsed = 256;        /* assume it's 256 */
    else if (biClrUsed > 256)
      ERREXIT(cinfo, JERR_BMP_BADCMAP);
    /* Allocate space to store the colormap */
    source->colormap = (*cinfo->mem->alloc_sarray)
      ((j_common_ptr) cinfo, JPOOL_IMAGE,
       (JDIMENSION) biClrUsed, (JDIMENSION) 3);
    /* and read it from the file */
    read_colormap(source, (int) biClrUsed, mapentrysize);
    /* account for size of colormap */
    bPad -= biClrUsed * mapentrysize;
  }

  /* Skip any remaining pad bytes */
  if (bPad < 0)            /* incorrect bfOffBits value? */
    ERREXIT(cinfo, JERR_BMP_BADHEADER);
  while (--bPad >= 0) {
    (void) read_byte(source);
  }

  /* Compute row width in file, including padding to 4-byte boundary */
  if (source->bits_per_pixel == 24)
    row_width = (JDIMENSION) (biWidth * 3);
  else
    row_width = (JDIMENSION) biWidth;
  while ((row_width & 3) != 0) row_width++;
  source->row_width = row_width;

  /* Allocate space for inversion array, prepare for preload pass */
  source->whole_image = (*cinfo->mem->request_virt_sarray)
    ((j_common_ptr) cinfo, JPOOL_IMAGE, FALSE,
     row_width, (JDIMENSION) biHeight, (JDIMENSION) 1);
  source->pub.get_pixel_rows = preload_image;
  if (cinfo->progress != NULL) {
    cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;
    progress->total_extra_passes++; /* count file input as separate pass */
  }

  /* Allocate one-row buffer for returned data */
  source->pub.buffer = (*cinfo->mem->alloc_sarray)
    ((j_common_ptr) cinfo, JPOOL_IMAGE,
     (JDIMENSION) (biWidth * 3), (JDIMENSION) 1);
  source->pub.buffer_height = 1;

  cinfo->in_color_space = JCS_RGB;
  cinfo->input_components = 3;
  cinfo->data_precision = 8;
  cinfo->image_width = (JDIMENSION) biWidth;
  cinfo->image_height = (JDIMENSION) biHeight;
}


/*
 * Finish up at the end of the file.
 */

METHODDEF(void)
finish_input_bmp (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  /* no work */
}


/*
 * The module selection routine for BMP format input.
 */

GLOBAL(cjpeg_source_ptr)
jinit_read_bmp (j_compress_ptr cinfo)
{
  bmp_source_ptr source;

  /* Create module interface object */
  source = (bmp_source_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                  SIZEOF(bmp_source_struct));
  source->cinfo = cinfo;    /* make back link for subroutines */
  /* Fill in method ptrs, except get_pixel_rows which start_input sets */
  source->pub.start_input = start_input_bmp;
  source->pub.finish_input = finish_input_bmp;

  return (cjpeg_source_ptr) source;
}


/*															*/
/*   fread_bmp_line: fread customized to get a range of		*/
/*   pixels at a time into work array                       */
/*															*/
size_t fread_bmp_line (JSAMPROW ptr, size_t size, size_t count, cjpparam_t *params)
{
    return cjpeg_fread(ptr, (size*count), params);
}


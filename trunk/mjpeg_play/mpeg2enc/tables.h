/* tables.h, Tables for MPEG syntax                        */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */


/* global syntax look-up tables */


#ifdef __cplusplus
#define CLINKAGE "C"
#else
#define CLINKAGE
#endif

#define EXTERNTBL extern CLINKAGE


EXTERNTBL const char version[]
#ifdef GLOBAL
  ="MSSG+ 1.3 2001/6/10 (development of mpeg2encode V1.2, 96/07/19)"
#endif
;

EXTERNTBL const char author[]
#ifdef GLOBAL
  ="(C) 1996, MPEG Software Simulation Group, (C) 2000, 2001,2002 Andrew Stevens, Rainer Johanni"
#endif
;

/* zig-zag scan */
EXTERNTBL const uint8_t zig_zag_scan[64]
#ifdef GLOBAL
=
{
  0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
  12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
  58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
}
#endif
;

/* alternate scan */
EXTERNTBL const uint8_t alternate_scan[64]
#ifdef GLOBAL
=
{
  0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
  41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
  51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
  53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
}
#endif
;

/* default intra quantization matrix */
EXTERNTBL const uint16_t default_intra_quantizer_matrix[64]
#ifdef GLOBAL
=
{
   8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83
}
#endif
;

EXTERNTBL const uint16_t hires_intra_quantizer_matrix[64]
#ifdef GLOBAL
=
{
   8, 16, 18, 20, 24, 25, 26, 30,
  16, 16, 20, 23, 25, 26, 30, 30,
  18, 20, 22, 24, 26, 28, 29, 31,
  20, 21, 23, 24, 26, 28, 31, 31,
  21, 23, 24, 25, 28, 30, 30, 33,
  23, 24, 25, 28, 30, 30, 33, 36,
  24, 25, 26, 29, 29, 31, 34, 38,
  25, 26, 28, 29, 31, 34, 38, 42
}
#endif
;

EXTERNTBL const uint16_t default_nonintra_quantizer_matrix[64]
#ifdef GLOBAL
=
{
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16
}
#endif
;


/* Hires non intra quantization matrix.  THis *is*
	the MPEG default...	 */
EXTERNTBL const uint16_t *hires_nonintra_quantizer_matrix
#ifdef GLOBAL
= &default_nonintra_quantizer_matrix[0]
#endif
;


EXTERNTBL const char pict_type_char[6]
#ifdef GLOBAL
= {'X', 'I', 'P', 'B', 'D', 'X'}
#endif
;


/* Support for the picture layer(!) insertion of scan data fieldsas
   MPEG user-data section as part of I-frames.  */

EXTERNTBL const uint8_t dummy_svcd_scan_data[14]
#ifdef GLOBAL
= {
	0x10,                       /* Scan data tag */
	14,                         /* Length */
	0x00, 0x80, 0x81,            /* Dummy data - this will be filled */
	0x00, 0x80, 0x81,            /* By the multiplexor or cd image   */        
	0xff, 0xff, 0xff,            /* creation software                */        
	0xff, 0xff, 0xff

  }
#endif
;


EXTERNTBL const char *statname
#ifdef GLOBAL
 = "mpeg2enc.stat"
#endif
;

EXTERNTBL const uint8_t map_non_linear_mquant[113];
EXTERNTBL const uint8_t non_linear_mquant_table[32];
EXTERNTBL uint16_t intra_q_tbl[113][64], inter_q_tbl[113][64];
EXTERNTBL uint16_t i_intra_q_tbl[113][64], i_inter_q_tbl[113][64];
EXTERNTBL float intra_q_tblf[113][64], inter_q_tblf[113][64];
EXTERNTBL float i_intra_q_tblf[113][64], i_inter_q_tblf[113][64];

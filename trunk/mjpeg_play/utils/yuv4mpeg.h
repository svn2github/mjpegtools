/*
 *  yuv4mpeg.h:  Functions for reading and writing "new" YUV4MPEG2 streams.
 *
 *               Stream format is described at the end of this file.
 *
 *
 *  Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __YUV4MPEG_H__
#define __YUV4MPEG_H__

#include <stdlib.h>
#include <unistd.h>
#include <mjpeg_logging.h>


/************************************************************************
 *  error codes returned by y4m_* functions
 ************************************************************************/
#define Y4M_OK          0
#define Y4M_ERR_RANGE   1  /* argument or tag value out of range */
#define Y4M_ERR_SYSTEM  2  /* failed system call, check errno */
#define Y4M_ERR_HEADER  3  /* illegal/malformed header */
#define Y4M_ERR_BADTAG  4  /* illegal tag character */
#define Y4M_ERR_MAGIC   5  /* bad header magic */
#define Y4M_ERR_EOF     6  /* end-of-file (clean) */
#define Y4M_ERR_XXTAGS  7  /* too many xtags */
#define Y4M_ERR_BADEOF  8  /* unexpected end-of-file */


/* generic 'unknown' value for integer parameters (e.g. interlace, height) */
#define Y4M_UNKNOWN -1



/************************************************************************
 *  'ratio' datatype, for rational numbers
 *                                     (see 'ratio' functions down below)
 ************************************************************************/
typedef struct _y4m_ratio {
  int n;  /* numerator   */
  int d;  /* denominator */
} y4m_ratio_t;

/************************************************************************
 *
 * 'aspect_code' datatype for holding codes for common display (whole image)
 * aspect ratios in the form used by MPEG2.
 *
 ***********************************************************************/

typedef unsigned int mpeg_aspect_code_t;

/************************************************************************
 *  useful standard framerates (as ratios)
 ************************************************************************/
extern const y4m_ratio_t y4m_fps_UNKNOWN;
extern const y4m_ratio_t y4m_fps_NTSC_FILM;  /* 24000/1001 film (in NTSC)  */
extern const y4m_ratio_t y4m_fps_FILM;       /* 24fps film                 */
extern const y4m_ratio_t y4m_fps_PAL;        /* 25fps PAL                  */
extern const y4m_ratio_t y4m_fps_NTSC;       /* 30000/1001 NTSC            */
extern const y4m_ratio_t y4m_fps_30;         /* 30fps                      */
extern const y4m_ratio_t y4m_fps_PAL_FIELD;  /* 50fps PAL field rate       */
extern const y4m_ratio_t y4m_fps_NTSC_FIELD; /* 60000/1001 NTSC field rate */
extern const y4m_ratio_t y4m_fps_60;         /* 60fps                      */

/************************************************************************
 *  useful standard sample (pixel) aspect ratios (W:H)
 ************************************************************************/
extern const y4m_ratio_t y4m_sar_UNKNOWN; 
extern const y4m_ratio_t y4m_sar_SQUARE;        /* square pixels */
extern const y4m_ratio_t y4m_sar_SQANA_16_9;    /* anamorphic 16:9
												 * sampled from 4:3
												 * with square
												 * pixels */
extern const y4m_ratio_t y4m_sar_NTSC_CCIR601;  /* 525-line (NTSC) Rec.601 */
extern const y4m_ratio_t y4m_sar_NTSC_16_9;     /* 16:9 NTSC/Rec.601       */
extern const y4m_ratio_t y4m_sar_NTSC_SVCD_4_3; /* NTSC SVCD 4:3           */
extern const y4m_ratio_t y4m_sar_NTSC_SVCD_16_9;/* NTSC SVCD 16:9          */
extern const y4m_ratio_t y4m_sar_PAL_CCIR601;   /* 625-line (PAL) Rec.601  */
extern const y4m_ratio_t y4m_sar_PAL_16_9;      /* 16:9 PAL/Rec.601        */
extern const y4m_ratio_t y4m_sar_PAL_SVCD_4_3;  /* PAL SVCD 4:3            */
extern const y4m_ratio_t y4m_sar_PAL_SVCD_16_9; /* PAL SVCD 16:9           */


/************************************************************************
 *  'xtag_list' --- list of unparsed and/or meta/X header tags
 *
 *     Do not touch this structure directly!
 *
 *     Use the y4m_xtag_*() functions (see below).
 *     You must initialize/finalize this structure before/after use.
 ************************************************************************/
#define Y4M_MAX_XTAGS 32        /* maximum number of xtags in list       */
#define Y4M_MAX_XTAG_SIZE 32    /* max length of an xtag (including 'X') */
typedef struct _y4m_xtag_list {
  int count;
  char *tags[Y4M_MAX_XTAGS];
} y4m_xtag_list_t;



/************************************************************************
 *  'stream_info' --- stream header information
 *
 *     Do not touch this structure directly!
 *
 *     Use the y4m_si_*() functions (see below).
 *     You must initialize/finalize this structure before/after use.
 ************************************************************************/
typedef struct _y4m_stream_info {
  /* values from header */
  int width;
  int height;
  int interlace;            /* see Y4M_ILACE_* definitions below   */
  y4m_ratio_t framerate;    /* frames-per-second;   0:0 == unknown */
  y4m_ratio_t sampleaspect; /* pixel width/height;  0:0 == unknown */
  /* computed/derivative values */
  int framelength;    /* bytes of data per frame (not including header) */
  /* mystical X tags */
  y4m_xtag_list_t x_tags;
} y4m_stream_info_t;

/* possible options for the interlace parameter */
#define Y4M_ILACE_NONE          0   /* non-interlaced, progressive frame */
#define Y4M_ILACE_TOP_FIRST     1   /* interlaced, top-field first       */
#define Y4M_ILACE_BOTTOM_FIRST  2   /* interlaced, bottom-field first    */


/************************************************************************
 *  'frame_info' --- frame header information
 *
 *     Do not touch this structure directly!
 *
 *     Use the y4m_fi_*() functions (see below).
 *     You must initialize/finalize this structure before/after use.
 ************************************************************************/
typedef struct _y4m_frame_info {
  /* mystical X tags */
  y4m_xtag_list_t x_tags;
} y4m_frame_info_t;



#ifdef __cplusplus
extern "C" {
#else
#endif


/************************************************************************
 *  'ratio' functions
 ************************************************************************/

/* 'normalize' a ratio (remove common factors) */
void y4m_ratio_reduce(y4m_ratio_t *r);

/* parse "nnn:ddd" into a ratio (returns Y4M_OK or Y4M_ERR_RANGE) */
int y4m_parse_ratio(y4m_ratio_t *r, const char *s);

/* Guesses a sample aspect ratio for common image formats based on the
   pixel grid dimensations and the original image aspect ratio
*/

y4m_ratio_t y4m_guess_sample_ratio(int width, int height, 
								   mpeg_aspect_code_t image_aspect_code);

/* quick test of two ratios for equality (i.e. identical components) */
#define Y4M_RATIO_EQL(a,b) ( ((a).n == (b).n) && ((a).d == (b).d) )

/* quick conversion of a ratio to a double (no divide-by-zero check!) */
#define Y4M_RATIO_DBL(r) ((double)(r).n / (double)(r).d)



/************************************************************************
 *  'xtag' functions
 *
 * o Before using an xtag_list (but after the structure/memory has been
 *    allocated), you must initialize it via y4m_init_xtag_list().
 * o After using an xtag_list (but before the structure is released),
 *    call y4m_fini_xtag_list() to free internal memory.
 *
 ************************************************************************/

/* initialize an xtag_list structure */
void y4m_init_xtag_list(y4m_xtag_list_t *xtags);

/* finalize an xtag_list structure */
void y4m_fini_xtag_list(y4m_xtag_list_t *xtags);

/* make one xtag_list into a copy of another */
void y4m_copy_xtag_list(y4m_xtag_list_t *dest, const y4m_xtag_list_t *src);

/* return number of tags in an xtag_list */
int y4m_xtag_count(const y4m_xtag_list_t *xtags);

/* access n'th tag in an xtag_list */
const char *y4m_xtag_get(const y4m_xtag_list_t *xtags, int n);

/* append a new tag to an xtag_list
    returns:          Y4M_OK - success
              Y4M_ERR_XXTAGS - list is already full */
int y4m_xtag_add(y4m_xtag_list_t *xtags, const char *tag);

/* remove a tag from an xtag_list 
    returns:         Y4M_OK - success
              Y4M_ERR_RANGE - n is out of range */
int y4m_xtag_remove(y4m_xtag_list_t *xtags, int n);

/* remove all tags from an xtag_list 
    returns:   Y4M_OK - success       */
int y4m_xtag_clearlist(y4m_xtag_list_t *xtags);

/* append copies of tags from src list to dest list
    returns:          Y4M_OK - success
              Y4M_ERR_XXTAGS - operation would overfill dest list */
int y4m_xtag_addlist(y4m_xtag_list_t *dest, const y4m_xtag_list_t *src);



/************************************************************************
 *  '*_info' functions
 *
 * o Before using a *_info structure (but after the structure/memory has
 *    been allocated), you must initialize it via y4m_init_*_info().
 * o After using a *_info structure (but before the structure is released),
 *    call y4m_fini_*_info() to free internal memory.
 * o Use the 'set' and 'get' accessors to modify or access the fields in
 *    the structures; don't touch the structure directly.  (Ok, so there
 *    is no really convenient C syntax to prevent you from doing this,
 *    but we are all responsible programmers here, so just don't do it!)
 *
 ************************************************************************/

/* initialize a stream_info structure */
void y4m_init_stream_info(y4m_stream_info_t *i);

/* finalize a stream_info structure */
void y4m_fini_stream_info(y4m_stream_info_t *i);

/* make one stream_info into a copy of another */
void y4m_copy_stream_info(y4m_stream_info_t *dest, y4m_stream_info_t *src);

/* access or set stream_info fields */
void y4m_si_set_width(y4m_stream_info_t *si, int width);
int y4m_si_get_width(y4m_stream_info_t *si);
void y4m_si_set_height(y4m_stream_info_t *si, int height);
int y4m_si_get_height(y4m_stream_info_t *si);
void y4m_si_set_interlace(y4m_stream_info_t *si, int interlace);
int y4m_si_get_interlace(y4m_stream_info_t *si);
void y4m_si_set_framerate(y4m_stream_info_t *si, y4m_ratio_t framerate);
y4m_ratio_t y4m_si_get_framerate(y4m_stream_info_t *si);
void y4m_si_set_sampleaspect(y4m_stream_info_t *si, y4m_ratio_t sar);
y4m_ratio_t y4m_si_get_sampleaspect(y4m_stream_info_t *si);
int y4m_si_get_framelength(y4m_stream_info_t *si);

/* access stream_info xtag_list */
y4m_xtag_list_t *y4m_si_xtags(y4m_stream_info_t *si);


/* initialize a frame_info structure */
void y4m_init_frame_info(y4m_frame_info_t *i);

/* finalize a frame_info structure */
void y4m_fini_frame_info(y4m_frame_info_t *i);

/* make one frame_info into a copy of another */
void y4m_copy_frame_info(y4m_frame_info_t *dest, y4m_frame_info_t *src);

/* access frame_info xtag_list */
y4m_xtag_list_t *y4m_fi_xtags(y4m_frame_info_t *fi);



/************************************************************************
 *  blocking read and write functions
 *
 *  o guaranteed to transfer entire payload (or fail)
 *  o return values:
 *                         0 (zero)   complete success
 *          -(# of remaining bytes)   error (and errno left set)
 *          +(# of remaining bytes)   EOF (for y4m_read only)
 *
 ************************************************************************/

/* read len bytes from fd into buf */
ssize_t y4m_read(int fd, char *buf, size_t len);

/* write len bytes from fd into buf */
ssize_t y4m_write(int fd, char *buf, size_t len);



/************************************************************************
 *  stream header processing functions
 *  
 *  o return values:
 *                   Y4M_OK - success
 *                Y4M_ERR_* - error (see y4m_strerr() for descriptions)
 *
 ************************************************************************/

/* parse a string of stream header tags */
int y4m_parse_stream_tags(char *s, y4m_stream_info_t *i);

/* read a stream header from file descriptor fd */
int y4m_read_stream_header(int fd, y4m_stream_info_t *i);

/* write a stream header to file descriptor fd */
int y4m_write_stream_header(int fd,  y4m_stream_info_t *i);



/************************************************************************
 *  frame processing functions
 *  
 *  o return values:
 *                   Y4M_OK - success
 *                Y4M_ERR_* - error (see y4m_strerr() for descriptions)
 *
 ************************************************************************/

/* read a frame header from file descriptor fd */
int y4m_read_frame_header(int fd, y4m_frame_info_t *i);

/* write a frame header to file descriptor fd */
int y4m_write_frame_header(int fd, y4m_frame_info_t *i);

/* read a complete frame (header + data)
   o yuv[3] points to three buffers, one each for Y, U, V planes */
int y4m_read_frame(int fd, y4m_stream_info_t *si, 
		   y4m_frame_info_t *fi, unsigned char *yuv[3]);

/* write a complete frame (header + data)
   o yuv[3] points to three buffers, one each for Y, U, V planes */
int y4m_write_frame(int fd, y4m_stream_info_t *si, 
		    y4m_frame_info_t *fi, unsigned char *yuv[3]);


/* read a complete frame (header + data), but de-interleave fields
    into two separate buffers
   o upper_field[3] same as yuv[3] above, but for upper field
   o lower_field[3] same as yuv[3] above, but for lower field
*/
int y4m_read_fields(int fd, y4m_stream_info_t *si, y4m_frame_info_t *fi,
		    unsigned char *upper_field[3], 
		    unsigned char *lower_field[3]);

/* write a complete frame (header + data), but interleave fields
    from two separate buffers
   o upper_field[3] same as yuv[3] above, but for upper field
   o lower_field[3] same as yuv[3] above, but for lower field
*/
int y4m_write_fields(int fd, y4m_stream_info_t *si, y4m_frame_info_t *fi,
		     unsigned char *upper_field[3], 
		     unsigned char *lower_field[3]);



/************************************************************************
 *  miscellaneous functions
 ************************************************************************/

/* convenient dump of stream header info via mjpeg_log facility
 *  - each logged/printed line is prefixed by 'prefix'
 */
void y4m_log_stream_info(log_level_t level, const char *prefix,
			 y4m_stream_info_t *i);

/* convert a Y4M_ERR_* error code into mildly explanatory string */
const char *y4m_strerr(int err);

/* set 'allow_unknown_tag' flag for library...
    o yn = 0 :  unknown header tags will produce a parsing error
    o yn = 1 :  unknown header tags/values will produce a warning, but
                 are otherwise passed along via the xtags list
    o yn = -1:  don't change, just return current setting

   return value:  previous setting of flag
*/
int y4m_allow_unknown_tags(int yn);


#ifdef __cplusplus
}
#endif

/************************************************************************
 ************************************************************************

  Description of the (new!, forever?) YUV4MPEG2 stream format:

  STREAM consists of
    o one '\n' terminated STREAM-HEADER
    o unlimited number of FRAMEs

  FRAME consists of
    o one '\n' terminated FRAME-HEADER
    o "length" octets of planar YCrCb 4:2:0 image data
        (if frame is interlaced, then the two fields are interleaved)


  STREAM-HEADER consists of
     o string "YUV4MPEG2 "  (note the space after the '2')
     o unlimited number of ' ' separated TAGGED-FIELDs
     o '\n' line terminator

  FRAME-HEADER consists of
     o string "FRAME "  (note the space after the 'E')
     o unlimited number of ' ' separated TAGGED-FIELDs
     o '\n' line terminator


  TAGGED-FIELD consists of
     o single ascii character tag
     o VALUE (which does not contain whitespace)

  VALUE consists of
     o integer (base 10 ascii representation)
  or o RATIO
  or o single ascii character
  or o generic ascii string

  RATIO consists of
     o numerator (integer)
     o ':' (a colon)
     o denominator (integer)


  The currently supported tags for the STREAM-HEADER:
     W - [integer] frame width, pixels, should be > 0
     H - [integer] frame height, pixels, should be > 0
     I - [char] interlacing:  p - progressive (none)
                            t - top-field-first
                            b - bottom-field-first
		            ? - unknown
     F - [ratio] frame-rate, 0:0 == unknown
     A - [ratio] sample (pixel) aspect ratio, 0:0 == unknown
     X - [character string] 'metadata' (unparsed, but passed around)

  The currently supported tags for the FRAME-HEADER:
     X - character string 'metadata' (unparsed, but passed around)

 ************************************************************************
 ************************************************************************/

#endif /* __YUV4MPEG_H__ */



/*
  *  yuvscaler.c
  *  Copyright (C) 2001 Xavier Biquard <xbiquard@free.fr>
  * 
  *  
  *  Downscales arbitrary sized yuv frame to yuv frames suitable for VCD, SVCD or specified
  * 
  *  yuvscaler is distributed in the hope that it will be useful, but
  *  WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  * 
  *  yuvscaler is distributed under the terms of the GNU General Public Licence
  *  as discribed in the COPYING file
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "lav_io.h"
#include "editlist.h"
#include "jpegutils.c"
#include "mjpeg_logging.h"
#include "inttypes.h"
#include "yuvscaler.h"


// For input
unsigned int input_width;
unsigned int input_height;
unsigned int input_useful_width = 0;
unsigned int input_useful_height = 0;
unsigned int input_offset_width = 0;
unsigned int input_offset_height = 0;
int input_interlaced = -1; //=0 for not interlaced, =1 for odd interlaced, =2 for even interlaced

// Downscaling ratios
unsigned int input_height_slice;
unsigned int output_height_slice;
unsigned int input_width_slice;
unsigned int output_width_slice;

// For output
unsigned int display_width;
unsigned int display_height;
unsigned int output_width;
unsigned int output_height;
unsigned int output_active_width;
unsigned int output_active_height;
unsigned int output_black_line_above = 0;
unsigned int output_black_line_under = 0;
unsigned int output_black_col_right = 0;
unsigned int output_black_col_left = 0;
unsigned int output_skip_line_above = 0;
unsigned int output_skip_line_under = 0;
unsigned int output_skip_col_right = 0;
unsigned int output_skip_col_left = 0;
unsigned int black = 0, black_line = 0, black_col = 0;	// =1 if black lines must be generated on output
unsigned int skip = 0, skip_line = 0, skip_col = 0;	// =1 if lines or columns from the active output will not be displayed on output frames
// NB: as these number may not be multiple of output_[height,width]_slice, it is not possible to remove the corresponding pixels in
// the input frame, a solution that could speed up things. 
int output_interlaced = -1; //=0 for not interlaced, =1 for odd interlaced, =2 for even interlaced
unsigned int vcd = 0;  //=1 if vcd output was selected
unsigned int svcd = 0; //=1 if svcd output was selected

// Global variables
int norm = -1;			// =0 for PAL and =1 for NTSC
int valid_output = 0;		// =0 for unvalid output specification or =1 for valid specification
int wide = 0;			// =1 for wide (16:9) input to standard (4:3) output conversion
int ratio = 0;
int infile = 0;			// =0 for stdin (default) =1 for file

// taken from lav2yuv
char **filename;
EditList el;
#define MAX_EDIT_LIST_FILES 256
#define MAX_JPEG_LEN (512*1024)
int chroma_format = CHROMA422;
static unsigned char jpeg_data[MAX_JPEG_LEN];


// Keywords for argument passing 
const char VCD_KEYWORD[] = "VCD";
const char SVCD_KEYWORD[] = "SVCD";
const char SIZE_KEYWORD[] = "SIZE_";
const char USE_KEYWORD[] = "USE_";
const char WIDE2STD_KEYWORD[] = "WIDE2STD";
const char INFILE_KEYWORD[] = "INFILE_";
const char RATIO_KEYWORD[] = "RATIO_";
const char ODD_FIRST[] = "INTERLACED_ODD_FIRST";
const char EVEN_FIRST[] = "INTERLACED_EVEN_FIRST";
const char NOT_INTER[] = "NOT_INTERLACED";

// Unclassified
unsigned long int diviseur;
unsigned short int *divide, *u_i_p;
int out_nb_col_slice_y, out_nb_line_slice_y, out_nb_col_slice_uv,
  out_nb_line_slice_uv;
static char *legal_opt_flags = "k:I:d:n:v:M:O:whtg";
int verbose = 1;
#define PARAM_LINE_MAX 256
float framerates[] =
  { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };


// *************************************************************************************
void
print_usage (char *argv[])
{
  fprintf (stderr,
	   "usage: %s -I [input_keyword] -M [mode_keyword] -O [output_keyword] [-n p|s|n] |-v 0-2] [-h] [inputfiles]\n"
	   "%s downscales arbitrary sized yuv frames (stdin by default) to a specified format (to stdout)\n\n"
	   "%s is keyword driven :\n"
	   "\t inputfiles selects yuv frames coming from Editlist files\n"
	   "\t -I for keyword concerning INPUT  frame characteristics\n"
	   "\t -M for keyword concerning the downscaling MODE of yuvscaler\n"
	   "\t -O for keyword concerning OUTPUT frame characteristics\n"
	   "\t ((the former syntax with -k and -w is still supported but depreciated))\n"
	   "\n" 
	   "Possible input keyword are:\n"
	   "\t USE_WidthxHeight+WidthOffset+HeightOffset to select a useful area of the input frame (multiple of 4)\n"
	   "\t    the rest of the image being discarded\n"
	   "\t if frames come from stdin, interlacing type is not known from header.\n"
	   "\t Therefore, they are taken as non-interlaced by default. To change this, use:\n"
	   "\t INTERLACED_ODD_FIRST  to select an interlaced, odd  first frame input stream from stdin\n"
	   "\t INTERLACED_EVEN_FIRST to select an interlaced, even first frame input stream from stdin\n"
	   "\n"
	   "Possible mode keyword are:\n"
	   "\t WIDE2STD to converts widescreen (16:9) input frames into standard output (4:3),\n"
	   "\t     generating necessary black lines\n"
	   "\t RATIO_WidthIn_WidthOut_HeightIn_HeightOut to specified downscaling ratios of\n"
	   "\t     WidthIn/WidthOut for width and HeightIN/HeightOut for height to be applied to the useful area.\n"
	   "\t     The output active area that results from dowscaling the input useful area might be different\n"
	   "\t     from the display area specified thereafter using the -O KEYWORD syntax\n"
	   "\t     In that case, %s will automatically generate necessary black lines and columns and/or skip necessary\n"
	   "\t     lines and columns to get an active output centered with the display size\n"
	   "\n"
	   "Possible output keywords are:\n"
	   "\t  VCD to generate  VCD compliant frames on output\n"
	   "\t     (taking care of PAL and NTSC standards, not-interlaced frames)\n"
	   "\t SVCD to generate SVCD compliant frames on output\n"
	   "\t     (taking care of PAL and NTSC standards, even interlaced frames)\n"
	   "\t SIZE_WidthxHeight to generate frames of size WidthxHeight on output (multiple of 4)\n"
	   "\t If you wish to specify interlacing of output frames, use:\n"
	   "\t INTERLACED_ODD_FIRST  to select an interlaced, odd  first frame output stream\n"
	   "\t INTERLACED_EVEN_FIRST to select an interlaced, even first frame output stream\n"
	   "\t NON_INTERLACED to select not interlaced output frames\n"
	   "\t by default, regarding interlacing, output frames are considered of the same kind as input frames\n"
	   "\n"
	   "-n  Specifies the OUTPUT norm for VCD/SVCD p=pal,s=secam,n=ntsc\n"
	   "-v  Specifies the degree of verbosity: 0=quiet, 1=normal, 2=verbose/debug\n"
	   "-h : print this lot!\n", argv[0], argv[0], argv[0], argv[0]);
  exit (1);
}

// *************************************************************************************

// *************************************************************************************
void
handle_args_global (int argc, char *argv[])
{
  // This function takes care of the global variables 
  // initialisation that are independent of the input stream
  unsigned int ui1, ui2, ui3, ui4;
  int c;

  // Default values 
  //   output_black_line_above=output_black_line_under=skip_lines=0;
  // End of default values

  while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{
	case 'v':
	  verbose = atoi (optarg);
	  if (verbose < 0 || verbose > 2)
	    {
	      mjpeg_error_exit1 ("Verbose level must be [0..2]\n");
	    }
	  break;


	case 'n':		// TV norm for SVCD/VCD output
	  switch (*optarg)
	    {
	    case 'p':
	    case 's':
	      norm = 1;
	      break;
	    case 'n':
	      norm = 0;
	      break;
	    default:
	      mjpeg_error_exit1 ("Illegal norm letter specified: %c\n",
				 *optarg);
	    }
	case 'M':
	  if (strncmp (optarg, RATIO_KEYWORD, 6) == 0)
	    {
	      if (sscanf (optarg, "RATIO_%u_%u_%u_%u", &ui1, &ui2, &ui3, &ui4)
		  == 4)
		{
		  // Coherence check : this must be downscaling ratios. An other coherence check will be done 
		  // when the useful input sizes are known
		  if ((ui1 > ui2) && (ui3 > ui4))
		    {
		      ratio = 1;
		      input_width_slice = ui1;
		      output_width_slice = ui2;
		      input_height_slice = ui3;
		      output_height_slice = ui4;
		    }
		  else
		    mjpeg_error_exit1 ("Unconsistent RATIO keyword: %s\n",
				       optarg);
		}
	      else
		mjpeg_error_exit1 ("Uncorrect mode flag argument: %s\n",
				   optarg);
	    }
	  break;
	   
	case 'h':
	  print_usage (argv);
	  break;

	default:
	  break;
	}
    }
//   fprintf(stderr,"optind=%d,argc=%d\n",optind,argc);
   if (optind>argc)
     print_usage (argv);
   if (optind==argc)
     infile=0;
     else {
	infile=argc-optind;
	filename=argv+optind;
     }
	
}


// *************************************************************************************


// *************************************************************************************
void
handle_args_dependent (int argc, char *argv[])
{
  // This function takes care of the global variables 
  // initialisation that may depend on the input stream
  int c;
  unsigned int ui1, ui2, ui3, ui4;
  int output,input;

  optind = 1;
  while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{
	case 'k':		// Compatibility reasons
	case 'O':		// OUTPUT
	  output = 0;
	  if (strcmp (optarg, VCD_KEYWORD) == 0)
	    {
	      output = 1;
	      valid_output = 1;
	       vcd=1; svcd=0; // if user gives VCD and SVCD keyword, take last one only into account
	      display_width = 352;
	      if (norm == 0)
		{
		  mjpeg_info ("VCD output format requested in PAL norm\n");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm\n");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified cannot set VCD output!\n");

	    }
	  if (strcmp (optarg, SVCD_KEYWORD) == 0)
	    {
	      output = 1;
	      valid_output = 1;
	       svcd=1; vcd=0; // if user gives VCD and SVCD keyword, take last one only into account
	      display_width = 480;
	      if (norm == 0)
		{
		  mjpeg_info ("SVCD output format requested in PAL norm\n");
		  display_height = 576;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("SVCD output format requested in NTSC norm\n");
		  display_height = 480;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified cannot set VCD output!\n");
	    }
	  if (strncmp (optarg, SIZE_KEYWORD, 5) == 0)
	    {
	      output = 1;
	      if (sscanf (optarg, "SIZE_%ux%u", &ui1, &ui2) == 2)
		{
		  // Coherence check: sizes must be multiple of 4
		  if ((ui1 % 4 == 0) && (ui2 % 4 == 0))
		    {
		      valid_output = 1;
		      display_width = ui1;
		      display_height = ui2;
		    }
		  else
		    mjpeg_error_exit1
		      ("Unconsistent SIZE keyword, not multiple of 4: %s\n",
		       optarg);
		}
	      else
		mjpeg_error_exit1 ("Uncorrect SIZE keyword: %s\n", optarg);
	    }
	  if (strcmp(optarg, ODD_FIRST)==0) 
	     {
		output=1;
		output_interlaced=1;
	     }
	   
	  if (strcmp(optarg, EVEN_FIRST)==0) 
	     {
		output=1;
	     output_interlaced=2;
	     }
	   if (strcmp(optarg, NOT_INTER)==0) 
	     {
		output=1;
	     output_interlaced=0;
	     }
	   
	  if (output == 0)
	    mjpeg_error_exit1 ("Uncorrect output keyword: %s\n", optarg);
	  break;

	case 'w':		// Compatibility reasons
	  wide = 1;
	  break;

	case 'M':		// MODE
	  if (strcmp (optarg, WIDE2STD_KEYWORD) == 0)
	    wide = 1;
	  else if (ratio == 0)
	    mjpeg_error_exit1 ("Uncorrect mode keyword: %s\n", optarg);

	  break;


	case 'I':		// INPUT
	   input=0;
	  if (strncmp (optarg, USE_KEYWORD, 4) == 0)
	    {
	       input=1;
	      if (sscanf (optarg, "USE_%ux%u+%u+%u", &ui1, &ui2, &ui3, &ui4)
		  == 4)
		{
		  // Coherence check : offsets must be multiple of 4 since U and V have half Y resolution and are interlaced
		  // and the required zone must be inside the input size
		  if ((ui1 % 4 == 0) && (ui2 % 4 == 0) && (ui3 % 4 == 0)
		      && (ui4 % 4 == 0) && (ui1 + ui3 <= input_width)
		      && (ui2 + ui4 <= input_height))
		    {
		      input_useful_width = ui1;
		      input_useful_height = ui2;
		      input_offset_width = ui3;
		      input_offset_height = ui4;
		    }
		  else
		    mjpeg_error_exit1 ("Unconsistent USE keyword: %s\n",
				       optarg);
		}
	      else
		mjpeg_error_exit1 ("Uncorrect input flag argument: %s\n",
				   optarg);
	    }
	  else
	    mjpeg_error_exit1 ("Uncorrect input flag argument: %s\n", optarg);
	  break;
	  if (strcmp(optarg, ODD_FIRST)==0) 
	     {
		input=1;
		input_interlaced=1;
	     }
	   
	  if (strcmp(optarg, EVEN_FIRST)==0) 
	     {
		input=1;
		input_interlaced=2;
	     }
	  if (input == 0)
	    mjpeg_error_exit1 ("Uncorrect input keyword: %s\n", optarg);
	  break;

	default:
	  break;
	}
    }

// Interlacing
   if (input_interlaced==-1)
     input_interlaced=0;
   // Yes, I know, i could have put "int input_interlaced=0" at the beginning of the program.
   if (vcd==1) 
     {
	output_interlaced=0;
	if (output_interlaced>0) 
	  mjpeg_warn("VCD requires non-interlaced output frames. Ignoring interlaced specification\n");
     }
   if (svcd==1)
     {
	if (input_interlaced==0) 
	  mjpeg_error_exit1("Input frames are not interlaced => output cannot be interlaced => cannot make SVCD compliant stream!\n");
	if (output_interlaced==-1) {
	   output_interlaced=2;
	} else { 
	   if (output_interlaced==0) {
	      output_interlaced=2;
	      mjpeg_warn("SVCD requires interlaced output frames. Ignoring non-interlaced specification\n");
	   } 
	}
	
     }
   
   if ((input_interlaced==0)&&(output_interlaced!=0))
       mjpeg_error_exit1("Input frames are not interlaced but output is specified as interlaced => uncoherence, STOP!\n");
   // By default
   if (output_interlaced==-1)
     output_interlaced=input_interlaced;

   
   // Unspecified input variables specification
  if (input_useful_width == 0)
    input_useful_width = input_width;
  if (input_useful_height == 0)
    input_useful_height = input_height;

  // Ratio coherence check against input_useful size
  if (ratio == 1)
    {
      if ((input_useful_width % input_width_slice == 0)
	  && (input_useful_height % input_height_slice == 0))
	{
	  valid_output = 1;
	  output_active_width =
	    (input_useful_width / input_width_slice) * output_width_slice;
	  output_active_height =
	    (input_useful_height / input_height_slice) * output_height_slice;
	}
      else
	mjpeg_error_exit1
	  ("Specified input ratios (%u and %u) does not divide input useful size (%u and %u)!\n",
	   input_width_slice, input_height_slice, input_useful_width,
	   input_useful_height);
    }

  if (valid_output == 0)
    mjpeg_error_exit1
      ("Output size could not be detrmined from command line!\n");

  // Unspecified output variables specification
  if (output_active_width == 0)
    output_active_width = display_width;
  if (output_active_height == 0)
    output_active_height = display_height;
  if (display_width == 0)
    display_width = output_active_width;
  if (display_height == 0)
    display_height = output_active_height;

  if (wide == 1)
    output_active_height = (output_active_height * 3) / 4;
  // Common pitfall! it is 3/4 not 9/16!
  // Indeed, Standard ratio is 4:3, so 16:9 has an height that is 3/4 smaller than the display_height

  // At this point, input size, input_useful size, output_active size and display size are specified
  // Time for the final coherence check and black and skip initialisations
  // Final check
  output_width =
    output_active_width > display_width ? output_active_width : display_width;
  output_height =
    output_active_height >
    display_height ? output_active_height : display_height;
  if ((output_active_width % 4 != 0) || (output_active_height % 4 != 0)
      || (display_width % 4 != 0) || (display_height % 4 != 0))
    mjpeg_error_exit1
      ("Output sizes are not multiple of 4! %ux%u, %ux%u being displayed\n",
       output_active_width, output_active_height, display_width,
       display_height);
  // Skip and black initialisations
  if (output_active_width > display_width)
    {
      skip = 1;
      skip_col = 1;
      output_skip_col_right = (output_active_width - display_width) / 2;
      output_skip_col_left =
	output_active_width - display_width - output_skip_col_right;
    }
  if (output_active_width < display_width)
    {
      black = 1;
      black_col = 1;
      output_black_col_right = (display_width - output_active_width) / 2;
      output_black_col_left =
	display_width - output_active_width - output_black_col_right;
    }
  if (output_active_height > display_height)
    {
      skip = 1;
      skip_line = 1;
      output_skip_line_above = (output_active_height - display_height) / 2;
      output_skip_line_under =
	output_active_height - display_height - output_skip_line_above;
    }
  if (output_active_height < display_height)
    {
      black = 1;
      black_line = 1;
      output_black_line_above = (display_height - output_active_height) / 2;
      output_black_line_under =
	display_height - output_active_height - output_black_line_above;
    }
}

// *************************************************************************************

// *************************************************************************************
static __inline__ int
fwrite_complete (const uint8_t * buf, const int buflen, FILE * file)
{
  return fwrite (buf, 1, buflen, file) == buflen;
}

// *************************************************************************************

// *************************************************************************************
int
main (int argc, char *argv[])
{
  char param_line[PARAM_LINE_MAX];
  int n, nerr = 0, res, len;
  unsigned long int i, j;
  int frame_rate_code = 0;
  float frame_rate;

  int input_fd = 0;
  long int frame_num = 0;
  uint8_t magic[] = "123456";	// : the last character of magic is the string termination character 
  const uint8_t key[] = "FRAME\n";
  unsigned int *height_coeff, *width_coeff;
  uint8_t *input, *output;
  uint8_t *input_y, *input_u, *input_v;
  uint8_t *input_y_infile, *input_u_infile, *input_v_infile;	// when input frames come from files
  uint8_t *output_y, *output_u, *output_v;
  uint8_t *frame_y, *frame_u, *frame_v;
  uint8_t **frame_y_p, **frame_u_p, **frame_v_p;	// size is not yet known
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  unsigned int divider;
  FILE *in_file = stdin;
  FILE *out_file = stdout;


  mjpeg_info ("yuvscaler is a general downscaling utility for yuv frames\n");
  mjpeg_info ("(C) 2001 Xavier Biquard <xbiquard@free.fr>\n");
  mjpeg_info ("%s -h for help\n", argv[0]);

  handle_args_global (argc, argv);
  mjpeg_default_handler_verbosity (verbose);

  if (infile == 0)
    {
      // INPUT comes from stdin
      // Check for correct file header : taken from mpeg2enc
      for (n = 0; n < PARAM_LINE_MAX; n++)
	{
	  if (!read (input_fd, param_line + n, 1))
	    {
	      mjpeg_error ("yuvscaler Unable to read header from stdin\n");

	      exit (1);
	    }
	  if (param_line[n] == '\n')
	    break;
	}

      if (n == PARAM_LINE_MAX)
	{
	  mjpeg_error
	    ("yuvscaler Didn't get linefeed in first %d characters of data\n",
	     PARAM_LINE_MAX);
	  exit (1);
	}

      //   param_line[n] = 0; /* Replace linefeed by end of string */
      // 
      if (strncmp (param_line, "YUV4MPEG", 8))
	{
	  mjpeg_error ("yuvscaler Input starts not with \"YUV4MPEG\"\n");
	  mjpeg_error ("yuvscaler This is not a valid input for me\n");
	  exit (1);
	}


      sscanf (param_line + 8, "%d %d %d", &input_width, &input_height,
	      &frame_rate_code);
      nerr = 0;
      if (input_width <= 0)
	{
	  mjpeg_error ("yuvscaler Horizontal size illegal\n");
	  nerr++;
	}
      if (input_height <= 0)
	{
	  mjpeg_error ("yuvscaler Vertical size illegal\n");
	  nerr++;
	}
      if (frame_rate_code < 1 || frame_rate_code > 8)
	{
	  mjpeg_error ("yuvscaler frame rate code illegal !!!!\n");
	  nerr++;
	}

      if (nerr)
	exit (1);

    }
  else
    {
      // INPUT comes from FILES
      read_video_files (filename, infile, &el);
      chroma_format = el.MJPG_chroma;
      if (chroma_format != CHROMA422)
	{
	  fprintf (stderr, "Editlist not in chroma 422 format, exiting...\n");
	  exit (1);
	}
      input_width = el.video_width;
      input_height = el.video_height;
      frame_rate = el.video_fps;
      input_interlaced=el.video_inter; // we eventually ooverride user's specification
      if (frame_rate == 25.0)
	{
	  frame_rate_code = 3;
	  norm = 0;
	}
      if (frame_rate == 29.97)
	{
	  frame_rate_code = 4;
	  norm = 1;
	}
      if ((frame_rate_code != 3) && (frame_rate_code != 4))
	mjpeg_warn
	  ("Cannot infer TV norm from frame rate : rate=%.3f fps!!\n",
	   frame_rate);
    }

   // INITIALISATIONS

   
   
  // Are frame PAL or NTSC?
  if (norm < 0 && frame_rate_code == 3)
    {
      norm = 0;
    }
  else if (norm < 0 && frame_rate_code == 4)
    {
      norm = 1;
    }

  if (norm < 0)
    {
      mjpeg_warn
	("Cannot infer TV norm from frame rate : frame size=%dx%d and rate=%.3f fps!!\n",
	 input_width, input_height, framerates[frame_rate_code]);
    }



  // Deal with args that depend on input stream
  handle_args_dependent (argc, argv);

  mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, interlaced = %u\n",
	      input_width, input_height,
	      input_useful_width, input_useful_height, input_offset_width,
	      input_offset_height,input_interlaced);
  mjpeg_info ("downscale to %ux%u, %ux%u being displayed, interlaced = %u\n",
	      output_active_width, output_active_height, display_width,
	      display_height,output_interlaced);
  if (black == 1)
    {
      mjpeg_info ("black lines: %u above and %u under\n",
		  output_black_line_above, output_black_line_under);
      mjpeg_info ("black columns: %u left and %u right\n",
		  output_black_col_left, output_black_col_right);
    }
  if (skip == 1)
    {
      mjpeg_info ("skipped lines: %u above and %u under\n",
		  output_skip_line_above, output_skip_line_under);
      mjpeg_info ("skipped columns: %u left and %u right\n",
		  output_skip_col_left, output_skip_col_right);
    }
  mjpeg_info ("yuvscaler Frame rate code: %d\n", frame_rate_code);
   

  // Coherences check: downscale not upscale
  if ((input_useful_width < output_active_width)
      || (input_useful_height < output_active_height))
    {
      mjpeg_error ("yuvscaler can only downscale, not upscale!!!!! STOP\n");
      exit (1);
    }
   // Coherence check: useful size should be multiple of 4 x 4, as well as display sizes

/*   
   
  if ((input_width == input_useful_width)
      && (input_height == input_useful_height)
      && (input_width == output_active_width)
      && (input_height == output_active_height))
    {
      mjpeg_error ("Frame size are OK! Nothing to do!!!!! STOP\n");
      exit (0);
    }
*/
   
   

  divider = pgcd (input_useful_width, output_active_width);
  input_width_slice = input_useful_width / divider;
  output_width_slice = output_active_width / divider;
#ifdef DEBUG
  mjpeg_debug ("divider,i_w_s,o_w_s = %d,%d,%d\n",
	       divider, input_width_slice, output_width_slice);
#endif

  divider = pgcd (input_useful_height, output_active_height);
  input_height_slice = input_useful_height / divider;
  output_height_slice = output_active_height / divider;
#ifdef DEBUG
  mjpeg_debug ("divider,i_w_s,o_w_s = %d,%d,%d\n",
	       divider, input_height_slice, output_height_slice);
#endif

  diviseur = input_height_slice * input_width_slice;
#ifdef DEBUG
  mjpeg_debug ("Diviseur=%ld\n", diviseur);
#endif

  mjpeg_info("Downscaling ratio for width is %u to %u\n",input_width_slice,output_width_slice);
  mjpeg_info("and is %u to %u for height\n",input_height_slice,output_height_slice);
   
   // Coherence check: if input frames are interlaced but output frames are not, input height slice must be a multiple of 2.
   if ((input_interlaced>0)&&(output_interlaced==0))
     if ((input_height_slice%2)!=0)
       mjpeg_error_exit1 ("As input frames are interlaced but output frames are not, height lines must be average 2 by 2, which is not possible here!\n");
       

  // To speed up downscaling, we tabulate the integral part of the division by "diviseur" which is inside [0;255] integral => unsigned short int storage class
  // but we do not tabulate the rest of this division since it may range up to diviseur-1 => unsigned long int storage class => too big in memory
  divide =
    (unsigned short int *) alloca (256 * diviseur *
				   sizeof (unsigned short int));

  u_i_p = divide;
  for (i = 0; i < 256 * diviseur; i++)
    {
      *(u_i_p++) = i / diviseur;
      //      mjpeg_error("%ld: %d\r",i,(unsigned short int)(i/diviseur));
    }

  // Calculate averaging coefficient
  // For the height
  height_coeff =
    alloca ((input_height_slice + 1) * output_height_slice *
	    sizeof (unsigned int));
  average_coeff (input_height_slice, output_height_slice, height_coeff);

  // For the width
  width_coeff =
    alloca ((input_width_slice + 1) * output_width_slice *
	    sizeof (unsigned int));
  average_coeff (input_width_slice, output_width_slice, width_coeff);


  // Pointers allocations
  input = alloca ((input_width * input_height * 3) / 2);
  output = alloca ((output_width * output_height * 3) / 2);
  // if skip_col==1
  frame_y_p = (uint8_t **) alloca (display_height * sizeof (uint8_t *));
  frame_u_p = (uint8_t **) alloca (display_height / 2 * sizeof (uint8_t *));
  frame_v_p = (uint8_t **) alloca (display_height / 2 * sizeof (uint8_t *));

  // Incorporate blacks lines and columns directly into output matrix since this will never change. 
  // BLACK pixel in YUV = (16,128,128)
  if (black == 1)
    {
      u_c_p = output;
      // Y component
      for (i = 0; i < output_black_line_above * output_width; i++)
	*(u_c_p++) = 16;
      if (black_col == 0)
	u_c_p += output_active_height * output_width;
      else
	{
	  for (i = 0; i < output_active_height; i++)
	    {
	      for (j = 0; j < output_black_col_left; j++)
		*(u_c_p++) = 16;
	      u_c_p += output_active_width;
	      for (j = 0; j < output_black_col_right; j++)
		*(u_c_p++) = 16;
	    }
	}
      for (i = 0; i < output_black_line_under * output_width; i++)
	*(u_c_p++) = 16;

      // U component
      //   u_c_p=output+output_width*output_height;
      for (i = 0; i < output_black_line_above / 2 * output_width / 2; i++)
	*(u_c_p++) = 128;
      if (black_col == 0)
	u_c_p += output_active_height / 2 * output_width / 2;
      else
	{
	  for (i = 0; i < output_active_height / 2; i++)
	    {
	      for (j = 0; j < output_black_col_left / 2; j++)
		*(u_c_p++) = 128;
	      u_c_p += output_active_width / 2;
	      for (j = 0; j < output_black_col_right / 2; j++)
		*(u_c_p++) = 128;
	    }
	}
      for (i = 0; i < output_black_line_under / 2 * output_width / 2; i++)
	*(u_c_p++) = 128;

      // V component
      //   u_c_p=output+(output_width*output_height*5)/4;
      for (i = 0; i < output_black_line_above / 2 * output_width / 2; i++)
	*(u_c_p++) = 128;
      if (black_col == 0)
	u_c_p += output_active_height / 2 * output_width / 2;
      else
	{
	  for (i = 0; i < output_active_height / 2; i++)
	    {
	      for (j = 0; j < output_black_col_left / 2; j++)
		*(u_c_p++) = 128;
	      u_c_p += output_active_width / 2;
	      for (j = 0; j < output_black_col_right / 2; j++)
		*(u_c_p++) = 128;
	    }
	}
      for (i = 0; i < output_black_line_under / 2 * output_width / 2; i++)
	*(u_c_p++) = 128;
    }

  // Various initialisatiosn for functions average_y and average_uv   
  out_nb_col_slice_y = output_active_width / output_width_slice;
  out_nb_line_slice_y = output_active_height / output_height_slice;
  input_y = input + input_offset_height * input_width + input_offset_width;
  out_nb_line_slice_uv = output_active_height / 2 / output_height_slice;
  out_nb_col_slice_uv = output_active_width / 2 / output_width_slice;
  input_u =
    input + input_width * input_height +
    input_offset_height / 2 * input_width / 2 + input_offset_width / 2;
  input_v =
    input + (input_height * input_width * 5) / 4 +
    input_offset_height / 2 * input_width / 2 + input_offset_width / 2;
  output_y =
    output + output_black_line_above * output_width + output_black_col_left;
  output_u =
    output + output_width * output_height +
    output_black_line_above / 2 * output_width / 2 +
    output_black_col_left / 2;
  output_v =
    output + (output_width * output_height * 5) / 4 +
    output_black_line_above / 2 * output_width / 2 +
    output_black_col_left / 2;

  // Other initialisations for frame output
  frame_y =
    output + output_skip_line_above * output_width + output_skip_col_left;
  frame_u =
    output + output_width * output_height +
    output_skip_line_above / 2 * output_width / 2 + output_skip_col_left / 2;
  frame_v =
    output + (output_width * output_height * 5) / 4 +
    output_skip_line_above / 2 * output_width / 2 + output_skip_col_left / 2;
  if (skip_col == 1)
    {
      for (i = 0; i < display_height; i++)
	frame_y_p[i] = frame_y + i * output_width;
      for (i = 0; i < display_height / 2; i++)
	{
	  frame_u_p[i] = frame_u + i * output_width / 2;
	  frame_v_p[i] = frame_v + i * output_width / 2;
	}
    }

  mjpeg_debug ("End of Initialisation\n");
  // END OF INITIALISATION

   // Output file header
   if (fprintf
       (stdout, "YUV4MPEG %3d %3d %1d\n", display_width, display_height,
	frame_rate_code) != 19)
     {
	mjpeg_error ("Error writing output header. Stop\n");
	exit (1);
     }

	 
  if (infile == 0)
    {
      // input comes from stdin
      // Master loop : continue until there is no next frame in stdin
      while ((fread (magic, 6, 1, in_file) == 1)
	     && (strcmp (magic, key) == 0))
	{
	  // Output key word
	  if (!fwrite_complete (key, 6, out_file))
	    goto out_error;

	  // Frame by Frame read and down-scale
	  frame_num++;
	  if (fread (input, (input_height * input_width * 3) / 2, 1, in_file)
	      != 1)
	    {
	      mjpeg_error_exit1 ("Frame %ld read failed\n", frame_num);
	    }
	  average_Y (input_y, output_y, height_coeff, width_coeff);
	  average_UV (input_u, output_u, height_coeff, width_coeff);
	  average_UV (input_v, output_v, height_coeff, width_coeff);

	  if (skip == 0)
	    {
	      // Here, display=output_active 
	      if (!fwrite_complete
		  (output, (display_width * display_height * 3) / 2,
		   out_file))
		goto out_error;
	    }
	  else
	    {
	      // skip == 1
	      if (skip_col == 0)
		{
		  // output_active_width==display_width, component per component frame output
		  if (!fwrite_complete
		      (frame_y, display_width * display_height, out_file))
		    goto out_error;
		  if (!fwrite_complete
		      (frame_u, display_width / 2 * display_height / 2,
		       out_file))
		    goto out_error;
		  if (!fwrite_complete
		      (frame_v, display_width / 2 * display_height / 2,
		       out_file))
		    goto out_error;
		}
	      else
		{
		  // output_active_width > display_width, line per line frame output for each component
		  for (i = 0; i < display_height; i++)
		    {
		      if (!fwrite_complete
			  (frame_y_p[i], display_width, out_file))
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (!fwrite_complete
			  (frame_u_p[i], display_width / 2, out_file))
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (!fwrite_complete
			  (frame_v_p[i], display_width / 2, out_file))
			goto out_error;

		    }
		}
	    }
	  mjpeg_debug ("yuvscaler Frame number %ld\r", frame_num);
	}
      // End of master loop
    }
  else
    {
      // input comes from files: frame by frame read 
      input_y_infile = input;
      input_u_infile = input + input_width * input_height;
      input_v_infile = input + (input_width * input_height * 5) / 4;
      for (frame_num = 0; frame_num < el.video_frames; frame_num++)
	{
	  // taken from lav2yuv
	  len = el_get_video_frame (jpeg_data, frame_num, &el);
	  if (
	      (res =
	       decode_jpeg_raw (jpeg_data, len, el.video_inter, chroma_format,
				input_width, input_height, input_y_infile,
				input_u_infile, input_v_infile)))
	    mjpeg_error_exit1 ("Frame %ld read failed\n", frame_num);
	  // Output key word
	  if (!fwrite_complete (key, 6, out_file))
	    goto out_error;

	  average_Y (input_y, output_y, height_coeff, width_coeff);
	  average_UV (input_u, output_u, height_coeff, width_coeff);
	  average_UV (input_v, output_v, height_coeff, width_coeff);

	  if (skip == 0)
	    {
	      // Here, display=output_active 
	      if (!fwrite_complete
		  (output, (display_width * display_height * 3) / 2,
		   out_file))
		goto out_error;
	    }
	  else
	    {
	      // skip == 1
	      if (skip_col == 0)
		{
		  // output_active_width==display_width, component per component frame output
		  if (!fwrite_complete
		      (frame_y, display_width * display_height, out_file))
		    goto out_error;
		  if (!fwrite_complete
		      (frame_u, display_width / 2 * display_height / 2,
		       out_file))
		    goto out_error;
		  if (!fwrite_complete
		      (frame_v, display_width / 2 * display_height / 2,
		       out_file))
		    goto out_error;
		}
	      else
		{
		  // output_active_width > display_width, line per line frame output for each component
		  for (i = 0; i < display_height; i++)
		    {
		      if (!fwrite_complete
			  (frame_y_p[i], display_width, out_file))
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (!fwrite_complete
			  (frame_u_p[i], display_width / 2, out_file))
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (!fwrite_complete
			  (frame_v_p[i], display_width / 2, out_file))
			goto out_error;

		    }
		}
	    }
	  mjpeg_debug ("yuvscaler Frame number %ld\r", frame_num);
	}
      // End of master loop
    }

  return 0;

out_error:
  mjpeg_error_exit1 ("Unable to write to output - aborting!\n");
  return 1;
}

// *************************************************************************************
int
average_coeff (unsigned int input_length, unsigned int output_length,
	       unsigned int *coeff)
{
  // This function calculates multiplicative coeeficients to average an input (vector of) length
  // input_length into an output (vector of) length output_length;
  // We sequentially store the number-of-non-zero-coefficients, followed by the coefficients
  // themselvesc, and that, output_length time
  int last_coeff = 0, remaining_coeff, still_to_go = 0, in, out, non_zero =
    0, nb;
  unsigned int *non_zero_p = NULL;
  unsigned int *pointer;

  if ((output_length > input_length) || (input_length <= 0)
      || (output_length <= 0) || (coeff == 0))
    {
      mjpeg_error ("Function average_coeff : arguments are wrong\n");
      mjpeg_error ("input length = %d, output length = %d, input = %p\n",
		   input_length, output_length, coeff);
      exit (1);
    }
#ifdef DEBUG
  mjpeg_debug
    ("Function average_coeff : input length = %d, output length = %d, input = %p\n",
     input_length, output_length, coeff);
#endif

  pointer = coeff;

  if (output_length == 1)
    {
      *pointer = input_length;
      pointer++;
      for (in = 0; in < input_length; in++)
	{
	  *pointer = 1;
	  pointer++;
	}
    }
  else
    {
      for (in = 0; in < output_length; in++)
	{
	  non_zero = 0;
	  non_zero_p = pointer;
	  pointer++;
	  still_to_go = input_length;
	  if (last_coeff > 0)
	    {
	      remaining_coeff = output_length - last_coeff;
	      *pointer = remaining_coeff;
	      pointer++;
	      non_zero++;
	      still_to_go -= remaining_coeff;
	    }
	  nb = (still_to_go / output_length);
#ifdef DEBUG
	  mjpeg_debug ("in=%d,nb=%d,stgo=%d ol=%d\n", in, nb, still_to_go,
		       output_length);
#endif
	  for (out = 0; out < nb; out++)
	    {
	      *pointer = output_length;
	      pointer++;
	    }
	  still_to_go -= nb * output_length;
	  non_zero += nb;

	  if ((last_coeff = still_to_go) != 0)
	    {
	      *pointer = last_coeff;
#ifdef DEBUG
	      mjpeg_debug ("non_zero=%d,last_coeff=%d\n", non_zero,
			   last_coeff);
#endif
	      pointer++;	// now pointer points onto the next number-of-non_zero-coefficients
	      non_zero++;
	      *non_zero_p = non_zero;
	    }
	  else
	    {
	      if (in != output_length - 1)
		{
		  mjpeg_error
		    ("There is a common divider between %d and %d\n This should not be the case\n",
		     input_length, output_length);
		  exit (1);
		}
	    }

	}
      *non_zero_p = non_zero;

      if (still_to_go != 0)
	{
	  mjpeg_error
	    ("Function average_coeff : calculus doesn't stop right : %d\n",
	     still_to_go);
	}
    }
#ifdef DEBUG
  if (verbose == 2)
    {
      int i, j;
      for (i = 0; i < output_length; i++)
	{
	  mjpeg_debug ("line=%d\n", i);
	  non_zero = *coeff;
	  coeff++;
	  mjpeg_debug (" ");
	  for (j = 0; j < non_zero; j++)
	    {
	      fprintf (stderr, "%d : %d ", j, *coeff);
	      coeff++;
	    }
	  fprintf (stderr, "\n");
	}
    }
#endif
  return (0);
}

// *************************************************************************************



// *************************************************************************************
int
average_Y (uint8_t * input, uint8_t * output, unsigned int *height_coeff,
	   unsigned int *width_coeff)
{
  // This function average an input matrix of name input that contains the Y component (size input_width*input_height, useful size input_useful_width*input_useful_height)
  // into an output matrix of name output of size output_active_width*output_active_height
  // input and output images are interlaced
  //Init
  mjpeg_debug ("Start of average_Y\n");
  //End of INIT
  if ((output_height_slice != 1) || (input_height_slice != 1))
    {
      uint8_t *input_line_p[input_height_slice];
      uint8_t *output_line_p[output_height_slice];
      unsigned int *H_var, *W_var, *H, *W;
      uint8_t *u_c_p;
      int j, nb_H, nb_W, in_line, out_line;
      int out_col_slice, out_col;
      int out_line_slice;
      int current_line, last_line;
      unsigned long int round_off_error = 0, value = 0;
      unsigned short int divi;


       if (output_interlaced==0) 
	 {
	    mjpeg_debug("Non-interlaced downscaling\n");
	    // Output frames are not interlaced, input may be interlaced or not.
	    // If they are interlaced, coherence check insures that input_height_slice is multiple of 2 => 
	    // input lines will be averaged two by two
	    // So, we average input_height_slice following lines into output_height_slice lines
	    for (out_line_slice = 0; out_line_slice < out_nb_line_slice_y;
		 out_line_slice++)
	      {
		 u_c_p = input+out_line_slice*input_height_slice*input_width;
		 for (in_line = 0; in_line < input_height_slice; in_line++)
		   {
		      input_line_p[in_line] = u_c_p;
		      u_c_p += input_width;
		   }
		 u_c_p = output+out_line_slice*output_height_slice*output_width;
		 for (out_line = 0; out_line < output_height_slice; out_line++)
		   {
		      output_line_p[out_line] = u_c_p;
		      u_c_p += output_width;
		   }
		 for (out_col_slice = 0; out_col_slice < out_nb_col_slice_y;
		      out_col_slice++)
		   {
		      H = height_coeff;
		      in_line = 0;
		      for (out_line = 0; out_line < output_height_slice; out_line++)
			{
			   nb_H = *H;
			   W = width_coeff;
			   for (out_col = 0; out_col < output_width_slice; out_col++)
			     {
				H_var = H + 1;
				nb_W = *W;
				value = round_off_error;
				value=0;
				last_line = in_line + nb_H;
				for (current_line = in_line; current_line < last_line;
				     current_line++)
				  {
				     W_var = W + 1;
				     // we average nb_W columns of input : we increment input_line_p[current_line] and W_var each time, except for the last value where 
				     // input_line_p[current_line] and W_var do not need to be incremented, but H_var does
				     for (j = 0; j < nb_W - 1; j++)
				       value +=
				       (*H_var) * (*W_var++) *
				       (*input_line_p[current_line]++);
				     value +=
				       (*H_var++) * (*W_var) *
				       (*input_line_p[current_line]);
				  }
				//                Straiforward implementation is 
				//                *(output_line_p[out_line]++)=value/diviseur;
				//                round_off_error=value%diviseur;
				//                Here, we speed up things but using the pretabulated integral parts
				divi = divide[value];
				*(output_line_p[out_line]++) = divi;
				round_off_error = value - divi * diviseur;
				W += nb_W + 1;
			     }
			   H += nb_H + 1;
			   in_line += nb_H - 1;
			   input_line_p[in_line] -= input_width_slice - 1;
			   // If last line of input is to be reused in next loop, 
			   // make the pointer points at the correct place
			}
		      if (in_line != input_height_slice - 1)
			{
			   mjpeg_error_exit1 ("INTERNAL: Error line conversion\n");
			}
		      else
			input_line_p[in_line] += input_width_slice - 1;
		      for (in_line = 0; in_line < input_height_slice; in_line++)
			input_line_p[in_line]++;
		   }
	      }
	 } else 
	 {
	    // Output frames are interlaced, so coherence check insures that input frames are also interlaced.
	    // Therefore, downscaling is done between odd lines, then between even lines, but we do not mix odd and even lines 
	    // the kind of interlacing (odd or even) may lead to line switching
	    mjpeg_debug("Interlaced downscaling\n");
       
	    for (out_line_slice = 0; out_line_slice < out_nb_line_slice_y;
		 out_line_slice++)
	      {
		 u_c_p =input+(2*(out_line_slice/2)*input_height_slice+((input_interlaced+out_line_slice)%2))*input_width;
		 for (in_line = 0; in_line < input_height_slice; in_line++)
		   {
		      input_line_p[in_line] = u_c_p;
		      u_c_p += (2*input_width);
		   }
		 u_c_p =output+(2*(out_line_slice/2)*output_height_slice+((output_interlaced+out_line_slice)%2))*output_width;
		 for (out_line = 0; out_line < output_height_slice; out_line++)
		   {
		      output_line_p[out_line] = u_c_p;
		      u_c_p += (2*output_width);
		   }
		 
		 for (out_col_slice = 0; out_col_slice < out_nb_col_slice_y;
		      out_col_slice++)
		   {
		      H = height_coeff;
		      in_line = 0;
		      for (out_line = 0; out_line < output_height_slice; out_line++)
			{
			   nb_H = *H;
			   W = width_coeff;
			   for (out_col = 0; out_col < output_width_slice; out_col++)
			     {
				H_var = H + 1;
				nb_W = *W;
				value = round_off_error;
				last_line = in_line + nb_H;
				for (current_line = in_line; current_line < last_line;
				     current_line++)
				  {
				     W_var = W + 1;
				     // we average nb_W columns of input : we increment input_line_p[current_line] and W_var each time, except for the last value where 
				     // input_line_p[current_line] and W_var do not need to be incremented, but H_var does
				     for (j = 0; j < nb_W - 1; j++)
				       value +=
				       (*H_var) * (*W_var++) *
				       (*input_line_p[current_line]++);
				     value +=
				       (*H_var++) * (*W_var) *
				       (*input_line_p[current_line]);
				  }
				//                Straiforward implementation is 
				//                *(output_line_p[out_line]++)=value/diviseur;
				//                round_off_error=value%diviseur;
				//                Here, we speed up things but using the pretabulated integral parts
				divi = divide[value];
				*(output_line_p[out_line]++) = divi;
				round_off_error = value - divi * diviseur;
				W += nb_W + 1;
			     }
			   H += nb_H + 1;
			   in_line += nb_H - 1;
			   input_line_p[in_line] -= input_width_slice - 1;
			   // If last line of input is to be reused in next loop, 
			   // make the pointer points at the correct place
			}
		      if (in_line != input_height_slice - 1)
			{
			   mjpeg_error_exit1 ("INTERNAL: Error line conversion\n");
			}
		      else
			input_line_p[in_line] += input_width_slice - 1;
		      for (in_line = 0; in_line < input_height_slice; in_line++)
			input_line_p[in_line]++;
		   }
	      }
	    
	    
	 }
    }
   else
     {
	average_Y_specific (input, output, height_coeff, width_coeff);
     }
   mjpeg_debug ("End of average_Y\n");
   return (0);
}

// *************************************************************************************
// 


// *************************************************************************************
int
average_UV (uint8_t * input, uint8_t * output,
	    unsigned int *height_coeff, unsigned int *width_coeff)
{
  // This function average an input matrix of name input that contains
  // either the U or the V component (size input_height/2*input_width/2)
  // into an output matrix of name output of size output_active_height/2*output_active_width/2

  //Init
  mjpeg_debug ("Start of average_UV\n");
  //End of INIT

  if ((output_height_slice != 1) || (input_height_slice != 1))
    {
      uint8_t *input_line_p[input_height_slice];
      uint8_t *output_line_p[output_height_slice];
      unsigned int *H_var, *W_var, *H, *W;
      uint8_t *u_c_p;
      int j, nb_H, nb_W, in_line, out_line, out_col_slice;
      int out_col, out_line_slice;
      int current_line, last_line;
      unsigned long int round_off_error = 0, value;
      unsigned short int divi;

       if (output_interlaced==0) 
	 {
 	    mjpeg_debug("Non-interlaced downscaling\n");
	    for (out_line_slice = 0; out_line_slice < out_nb_line_slice_uv;
		 out_line_slice++)
	      {
		 u_c_p =
		   input + out_line_slice * input_height_slice * (input_width / 2);
		 for (in_line = 0; in_line < input_height_slice; in_line++)
		   {
		      input_line_p[in_line] = u_c_p;
		      u_c_p += input_width/2;
		   }
		 u_c_p =
		   output + out_line_slice * output_height_slice * (output_width / 2);
		 for (out_line = 0; out_line < output_height_slice; out_line++)
		   {
		      output_line_p[out_line] = u_c_p;
		      u_c_p += output_width/2;
		   }
		 
		 for (out_col_slice = 0; out_col_slice < out_nb_col_slice_uv;
		      out_col_slice++)
		   {
		      H = height_coeff;
		      in_line = 0;
		      for (out_line = 0; out_line < output_height_slice; out_line++)
			{
			   nb_H = *H;
			   W = width_coeff;
			   for (out_col = 0; out_col < output_width_slice; out_col++)
			     {
				H_var = H + 1;
				nb_W = *W;
				value = round_off_error;
				last_line = in_line + nb_H;
				for (current_line = in_line; current_line < last_line;
				     current_line++)
				  {
				     W_var = W + 1;
				     for (j = 0; j < nb_W - 1; j++)
				       value +=
				       (*H_var) * (*W_var++) *
				       (*input_line_p[current_line]++);
				     value +=
				       (*H_var++) * (*W_var) *
				       (*input_line_p[current_line]);
				  }
				u_i_p = divide + 2 * value;
				divi = *(divide + value);
				*(output_line_p[out_line]++) = divi;
				round_off_error = value - divi * diviseur;
				W += nb_W + 1;
			     }
			   H += nb_H + 1;
			   in_line += nb_H - 1;
			   input_line_p[in_line] -= input_width_slice - 1;	// If last line of input is to be reused in next loop, 
			   // make the pointer points at the correct place
			}
		      if (in_line != input_height_slice - 1)
			{
			   mjpeg_error_exit1 ("Error line conversion\n");
			}
		      else
			input_line_p[in_line] += input_width_slice - 1;
		      for (in_line = 0; in_line < input_height_slice; in_line++)
			input_line_p[in_line]++;
		   }
	      }
	 }
       else 
	 {
	    mjpeg_debug("Interlaced downscaling\n");
       
	    for (out_line_slice = 0; out_line_slice < out_nb_line_slice_uv;
		 out_line_slice++)
	      {
		 u_c_p =
		   input + (2*(out_line_slice/2)*input_height_slice+((input_interlaced+out_line_slice)%2))*(input_width/2);
		 for (in_line = 0; in_line < input_height_slice; in_line++)
		   {
		      input_line_p[in_line] = u_c_p;
		      u_c_p += input_width;
		   }
		 u_c_p =
		   output + (2*(out_line_slice/2)*output_height_slice+((output_interlaced+out_line_slice)%2))*(output_width/2);
		 for (out_line = 0; out_line < output_height_slice; out_line++)
		   {
		      output_line_p[out_line] = u_c_p;
		      u_c_p += output_width;
		   }
		 for (out_col_slice = 0; out_col_slice < out_nb_col_slice_uv;
		      out_col_slice++)
		   {
		      H = height_coeff;
		      in_line = 0;
		      for (out_line = 0; out_line < output_height_slice; out_line++)
			{
			   nb_H = *H;
			   W = width_coeff;
			   for (out_col = 0; out_col < output_width_slice; out_col++)
			     {
				H_var = H + 1;
				nb_W = *W;
				value = round_off_error;
				last_line = in_line + nb_H;
				for (current_line = in_line; current_line < last_line;
				     current_line++)
				  {
				     W_var = W + 1;
				     for (j = 0; j < nb_W - 1; j++)
				       value +=
				       (*H_var) * (*W_var++) *
				       (*input_line_p[current_line]++);
				     value +=
				       (*H_var++) * (*W_var) *
				       (*input_line_p[current_line]);
				  }
				u_i_p = divide + 2 * value;
				divi = *(divide + value);
				*(output_line_p[out_line]++) = divi;
				round_off_error = value - divi * diviseur;
				W += nb_W + 1;
			     }
			   H += nb_H + 1;
			   in_line += nb_H - 1;
			   input_line_p[in_line] -= input_width_slice - 1;	// If last line of input is to be reused in next loop, 
			   // make the pointer points at the correct place
			}
		      if (in_line != input_height_slice - 1)
			{
			   mjpeg_error_exit1 ("Error line conversion\n");
			}
		      else
			input_line_p[in_line] += input_width_slice - 1;
		      for (in_line = 0; in_line < input_height_slice; in_line++)
			input_line_p[in_line]++;
		   }
	      }
	 }
    }
   else
     average_UV_specific (input, output, height_coeff, width_coeff);
   mjpeg_debug ("End of average_UV\n");
   return (0);
}

// *************************************************************************************


// 
// *************************************************************************************
int
average_Y_specific (uint8_t * input, uint8_t * output,
		    unsigned int *height_coeff, unsigned int *width_coeff)
{
  // This function gathers code that are speed enhanced due to specific downscaling ratios     
  // 
  uint8_t *input_line_p[input_height_slice];
  uint8_t *output_line_p[output_height_slice];
  unsigned int *W_var, *W;
  int j, nb_W;
  int out_col_slice, out_col;
  int out_line_slice;
  unsigned long int round_off_error = 0, value = 0;
  unsigned short int divi;

  //Init
  mjpeg_debug ("Start of average_Y_specific\n");
  //End of INIT

   // (SPECIAL SVCD) We just take the average along the width, not the height, line per line
   // If input and output are interlaced, but not of the same type (one odd, the other even),
   // we must switch lines

   if (output_interlaced==input_interlaced) 
     {
	for (out_line_slice = 0; out_line_slice < output_active_height;
	     out_line_slice++)
	  {
	     input_line_p[0] = input + out_line_slice * input_width;
	     output_line_p[0] = output + out_line_slice * output_width;
	     for (out_col_slice = 0; out_col_slice < out_nb_col_slice_y;
		  out_col_slice++)
	       {
		  W = width_coeff;
		  for (out_col = 0; out_col < output_width_slice; out_col++)
		    {
		       nb_W = *W;
		       value = round_off_error;
		       W_var = W + 1;
		       for (j = 0; j < nb_W - 1; j++)
			 value += (*W_var++) * (*input_line_p[0]++);
		       value += (*W_var) * (*input_line_p[0]);
		       divi = *(divide + value);
		       *(output_line_p[0]++) = divi;
		       round_off_error = value - divi * diviseur;
		       W += nb_W + 1;
		    }
		  input_line_p[0]++;
	       }
	  }
     }
   else 
     {
	for (out_line_slice = 0; out_line_slice < output_active_height;
	     out_line_slice++)
	  {
	     input_line_p[0] = input + out_line_slice * input_width;
	     if (out_line_slice%2==0)
	       output_line_p[0] = output + (out_line_slice + 1) * output_width;
	     else 
	       output_line_p[0] = output + (out_line_slice - 1) * output_width;
	     for (out_col_slice = 0; out_col_slice < out_nb_col_slice_y;
		  out_col_slice++)
	       {
		  W = width_coeff;
		  for (out_col = 0; out_col < output_width_slice; out_col++)
		    {
		       nb_W = *W;
		       value = round_off_error;
		       W_var = W + 1;
		       for (j = 0; j < nb_W - 1; j++)
			 value += (*W_var++) * (*input_line_p[0]++);
		       value += (*W_var) * (*input_line_p[0]);
		       divi = *(divide + value);
		       *(output_line_p[0]++) = divi;
		       round_off_error = value - divi * diviseur;
		       W += nb_W + 1;
		    }
		  input_line_p[0]++;
	       }
	  }
     }
   
   mjpeg_debug ("End of average_Y_specific\n");
   return (0);
}

// *************************************************************************************


// *************************************************************************************
int
average_UV_specific (uint8_t * input, uint8_t * output,
		     unsigned int *height_coeff, unsigned int *width_coeff)
{
// This function gathers code that are speed enhanced due to specific downscaling ratios     
  uint8_t *input_line_p[input_height_slice];
  uint8_t *output_line_p[output_height_slice];
  unsigned int *W_var, *W;
  int j, nb_W, out_col_slice;
  int out_col, out_line_slice;
  unsigned long int round_off_error = 0, value;
  unsigned short int divi;

  //Init
  mjpeg_debug ("Start of average_UV_specific\n");
  //End of INIT

   if (output_interlaced==input_interlaced) 
     {
	for (out_line_slice = 0; out_line_slice < output_active_height / 2;
	     out_line_slice++)
	  {
	     input_line_p[0] = input + out_line_slice * input_width / 2;
	     output_line_p[0] = output + out_line_slice * output_width / 2;
	     for (out_col_slice = 0; out_col_slice < out_nb_col_slice_uv;
		  out_col_slice++)
	       {
		  W = width_coeff;
		  for (out_col = 0; out_col < output_width_slice; out_col++)
		    {
		       nb_W = *W;
		       value = round_off_error;
		       W_var = W + 1;
		       for (j = 0; j < nb_W - 1; j++)
			 value += (*W_var++) * (*input_line_p[0]++);
		       value += (*W_var) * (*input_line_p[0]);
		       divi = *(divide + value);
		       *(output_line_p[0]++) = divi;
		       round_off_error = value - divi * diviseur;
		       W += nb_W + 1;
		    }
		  input_line_p[0]++;
	       }
	  }
     }
   else 
     {
	for (out_line_slice = 0; out_line_slice < output_active_height / 2;
	     out_line_slice++)
	  {
	     input_line_p[0] = input + out_line_slice * input_width / 2;
	     if ((out_line_slice%2)==0)
	       output_line_p[0] = output + (out_line_slice + 1) * output_width / 2;
	     else 
	       output_line_p[0] = output + (out_line_slice - 1 ) * output_width / 2;
	     for (out_col_slice = 0; out_col_slice < out_nb_col_slice_uv;
		  out_col_slice++)
	       {
		  W = width_coeff;
		  for (out_col = 0; out_col < output_width_slice; out_col++)
		    {
		       nb_W = *W;
		       value = round_off_error;
		       W_var = W + 1;
		       for (j = 0; j < nb_W - 1; j++)
			 value += (*W_var++) * (*input_line_p[0]++);
		       value += (*W_var) * (*input_line_p[0]);
		       divi = *(divide + value);
		       *(output_line_p[0]++) = divi;
		       round_off_error = value - divi * diviseur;
		       W += nb_W + 1;
		    }
		  input_line_p[0]++;
	       }
	  }
     }
   mjpeg_debug ("End of average_UV_specific\n");
   return (0);
}


// *************************************************************************************
unsigned int
pgcd (unsigned int num1, unsigned int num2)
{
  // Calculates the biggest common divider between num1 and num2, with num2<=num1
  unsigned int i;
  for (i = num2; i >= 2; i--)
    {
      if (((num2 % i) == 0) && ((num1 % i) == 0))
	return (i);
    }
  return (1);
}

// *************************************************************************************

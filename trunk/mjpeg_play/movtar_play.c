#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "movtar.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_timer.h>

#define JPEG_INTERNALS
#include "jpeg-6b-mmx/jinclude.h"
#include "jpeg-6b-mmx/jpeglib.h"

//#include <Hermes/Hermes.h>

#define readbuffsize 200000

#define dprintf(x) if (debug) fprintf(stderr, x);

/* the usual code continuing here */
char *img=NULL;
int height;
int width;
FILE *movtarfile;
FILE *tempfile;
struct tarinfotype frameinfo;
char *readbufferljud;
char *readbuffer;
char **imglines;
struct movtarinfotype movtarinfo;
SDL_Surface *screen;
SDL_Surface *jpeg;
SDL_Rect jpegdims;
struct jpeg_decompress_struct cinfo; 

int debug = 1;

/* definition of srcmanager from memory to I/O with libjpeg */
typedef struct {
  struct jpeg_source_mgr pub;
  JOCTET * buffer;
  boolean start_of_file;
} mem_src_mgr;
typedef mem_src_mgr * mem_src_ptr;

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  mem_src_ptr src= (mem_src_ptr) cinfo->src;
  src->start_of_file = TRUE;
}

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
  printf("Error in the JPEG buffer !\n");
  return TRUE;
}

METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  mem_src_ptr src = (mem_src_ptr) cinfo->src;
  src->pub.next_input_byte += (size_t) num_bytes;
  src->pub.bytes_in_buffer -= (size_t) num_bytes; 
}

METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

GLOBAL(void)
jpeg_mem_src (j_decompress_ptr cinfo,void * buff, int size)
{
  mem_src_ptr src;

   cinfo->src = (struct jpeg_source_mgr *)
     (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				 sizeof(mem_src_mgr));

  src = (mem_src_ptr) cinfo->src;
  src->buffer = (JOCTET *) buff;
  
  src = (mem_src_ptr) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;
  src->pub.term_source = term_source;
  src->pub.bytes_in_buffer = size;
  src->pub.next_input_byte = src->buffer;
}

GLOBAL(void)
jpeg_mem_src_reset (j_decompress_ptr cinfo, int size)
{
  mem_src_ptr src;

  src = (mem_src_ptr) cinfo->src;
  src->pub.bytes_in_buffer = size;
  src->pub.next_input_byte = src->buffer;
}

/* end of data source manager */

/* Colorspace conversion */
/* RGB, 32 bits, 8bits each: (Junk), R, G, B */ 
#if defined(__GNUC__)
#define int64 unsigned long long
#endif
static int64 te0 = 0x0080008000800080; // -128
static int64 te1 = 0xd8186aa1d8186aa1; // for cb 
static int64 te2 = 0x59ba308159ba3081; // for cr

METHODDEF(void)
ycc_rgb32_convert_mmx (j_decompress_ptr cinfo,
		     JSAMPIMAGE input_buf, JDIMENSION input_row,
		     JSAMPARRAY output_buf, int num_rows)
{
  JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    num_cols/=4;    for (col = 0; col < num_cols; col++) {
#if defined(HAVE_MMX_INTEL_MNEMONICS)
#error "MMX routines haven't been converted to INTEL assembler yet - contact JPEGlib/MMX"
#endif
#if defined(HAVE_MMX_ATT_MNEMONICS)
      asm("movd (%%eax),%%mm0\n"   // mm0: 0 0 0 0 y3 y2 y1 y0 - 8 bit
	  "movd (%%ebx),%%mm1\n"   // mm1: 0 0 0 0 cb3 cb2 cb1 cb0
	  "movd (%%ecx),%%mm2\n"   // mm2: 0 0 0 0 cr3 cr2 cr1 cr0
	  "pxor %%mm7,%%mm7\n"     // mm7 = 0
	  "punpcklbw %%mm7,%%mm0\n"// mm0: y3 y2 y1 y0 - expand to 16 bit
	  "punpcklbw %%mm7,%%mm1\n"// mm1: cb3 cb2 cb1 cb0
	  "punpcklbw %%mm7,%%mm2\n"// mm2: cr3 cr2 cr1 cr0
	  "psubw te0,%%mm1\n"  //minus 128 for cb and cr
	  "psubw te0,%%mm2\n"
	  "psllw $2,%%mm1\n"       // shift left 2 bits for Cr and Cb to fit the mult constants
	  "psllw $2,%%mm2\n"

	  // prepare for RGB 1 & 0
	  "movq %%mm1,%%mm3\n"     // mm3_16: cb3 cb2 cb1 cb0
	  "movq %%mm2,%%mm4\n"     // mm4_16: cr3 cr2 cr1 cr0
	  "punpcklwd %%mm3,%%mm3\n"// expand to 32 bit: mm3: cb1 cb1 cb0 cb0
	  "punpcklwd %%mm4,%%mm4\n"// mm4: cr1 cr1 cr0 cr0
	  
	  // Y    Y     Y    Y 
	  // 0    CB*g  CB*b 0
	  // CR*r CR*g  0    0
	  //------------------
	  // R    G     B  

	  "pmulhw te1,%%mm3\n"// multiplicate in the constants: mm3: cb1/green cb1/blue cb0/green cb0/blue
	  "pmulhw te2,%%mm4\n"// mm4: cr1/red cb1/green cr0/red cr0/green

	  "movq %%mm0,%%mm5\n"      // mm5: y3 y2 y1 y0
	  "punpcklwd %%mm5,%%mm5\n" // expand to 32 bit: y1 y1 y0 y0
	  "movq %%mm5,%%mm6\n"      // mm6: y1 y1 y0 y0
	  "punpcklwd %%mm5,%%mm5\n" // mm5: y0 y0 y0 y0
	  "punpckhwd %%mm6,%%mm6\n" // mm6: y1 y1 y1 y1

	  // RGB 0
	  "movq %%mm3,%%mm7\n"      // mm7: cb1 cb1 cb0 cb0
	  "psllq $32,%%mm7\n"       // shift left 32 bits: mm7: cb0 cb0 0 0
	  "psrlq $16,%%mm7\n"       // mm7 = 0 cb0 cb0 0
	  "paddw %%mm7,%%mm5\n"     // add: mm7: y+cb
	  "movq %%mm4,%%mm7\n"      // mm7 = cr1 cr1 cr0 cr0
	  "psllq $32,%%mm7\n"       // shift left 32 bits: mm7: cr0 cr0 0 0
	  "paddw %%mm7,%%mm5\n"     // y+cb+cr r g b ?
	  
	  // RGB 1
	  "psrlq $32,%%mm4\n"       // mm4: 0 0 cr1 cr1 
	  "psllq $16,%%mm4\n"       // mm4: 0 cr1 cr1 0 
	  "paddw %%mm4,%%mm6\n"     //y+cr
	  "psrlq $32,%%mm3\n"       // mm3: 0 0 cb1 cb1
	  "paddw %%mm3,%%mm6\n"     //y+cr+cb: mm6 = r g b

	  "psrlq $16,%%mm5\n"        // mm5: 0 r0 g0 b0
	  "packuswb %%mm6,%%mm5\n"   //mm5 = ? r1 g1 b1 0 r0 g0 b0
	  "movq %%mm5,%0\n"         // store mm5

	  // prepare for RGB 2 & 3
	  "punpckhwd %%mm0,%%mm0\n" //mm0 = y3 y3 y2 y2
	  "punpckhwd %%mm1,%%mm1\n" //mm1 = cb3 cb3 cb2 cb2
	  "punpckhwd %%mm2,%%mm2\n" //mm2 = cr3 cr3 cr2 cr2
	  "pmulhw te1,%%mm1\n"      //mm1 = cb * ?
	  "pmulhw te2,%%mm2\n"      //mm2 = cr * ?
	  "movq %%mm0,%%mm3\n"      //mm3 = y3 y3 y2 y2
	  "punpcklwd %%mm3,%%mm3\n" //mm3 = y2 y2 y2 y2
	  "punpckhwd %%mm0,%%mm0\n" //mm0 = y3 y3 y3 y3

	  // RGB 2
	  "movq %%mm1,%%mm4\n"      //mm4 = cb3 cb3 cb2 cb2
	  "movq %%mm2,%%mm5\n"      //mm5 = cr3 cr3 cr2 cr2
	  "psllq $32,%%mm4\n"       //mm4 = cb2 cb2 0 0
 	  "psllq $32,%%mm5\n"       //mm5 = cr2 cr2 0 0
	  "psrlq $16,%%mm4\n"       //mm4 = 0 cb2 cb2 0
	  "paddw %%mm4,%%mm3\n"     // y+cb
	  "paddw %%mm5,%%mm3\n"     //mm3 = y+cb+cr

	  // RGB 3
	  "psrlq $32,%%mm2\n"       //mm2 = 0 0 cr3 cr3
	  "psrlq $32,%%mm1\n"       //mm1 = 0 0 cb3 cb3
	  "psllq $16,%%mm2\n"       //mm1 = 0 cr3 cr3 0
	  "paddw %%mm2,%%mm0\n"     //y+cr
	  "paddw %%mm1,%%mm0\n"     //y+cb+cr

	  "psrlq $16,%%mm3\n"        // shift to the right corner
	  "packuswb %%mm0,%%mm3\n"  // pack in a quadword
	  "movq %%mm3,8%0\n"       //  save two more RGB pixels

	  :"=m"(outptr[0])
	  :"eax"(inptr0),"ebx"(inptr1),"ecx"(inptr2) //y cb cr
	  :"eax","ebx","ecx","edx", "st");
#endif
      outptr+=16;
      inptr0+=4;
      inptr1+=4;
      inptr2+=4;
    }
  }
}

/* end of custom color deconverter */
void ComplainAndExit(void)
{
  fprintf(stderr, "Problem: %s\n", SDL_GetError());
  exit(1);
}

void callback_AbortProg(int num)
{
  SDL_Quit();
}

/* forward reference */
void inline readpicfrommem(void * inbuffer,int size);

void initmovtar(char *filename)
{
  int datasize;

  /* allocate memory for readbuffer */
  readbuffer=(char *)malloc(readbuffsize);
  readbufferljud=(char *)malloc(readbuffsize);
  
  /* open file */
  movtarfile = (FILE *)movtar_open(filename);
  if(movtarfile == NULL)
    {
      fprintf(stderr,"The movtarfile %s couldn't be opened.\n", filename);
      exit(0);
    }

  /* read data from INFO file */
  datasize=movtar_read_tarblock(movtarfile,&frameinfo);
  movtar_read_data(movtarfile,readbuffer,datasize);
  movtar_parse_info(readbuffer,&movtarinfo);
  /* adjust parameters */
  height=movtarinfo.mov_height;
  width=movtarinfo.mov_width;
  /* start sound */
  //audio_init(0,movtarinfo.sound_stereo,movtarinfo.sound_size,movtarinfo.sound_rate);
  //audio_start();

}

void inline readnext()
{
  static int played=0;
  static int viewed=0; 

  int datasize; 
  int datatype;

  do
    {
      //reads until jpegfile found
      datasize=movtar_read_tarblock(movtarfile,&frameinfo);
      datatype=movtar_datatype(&frameinfo);

      /* check if it read video or audio data */
      /* play sound */
      if(datatype & MOVTAR_DATA_AUDIO)
	{
	  movtar_read_data(movtarfile,readbufferljud,datasize);
	  // write audio over SDL 
	  //audio_write(readbufferljud,datasize,0);
	}

      /* test to increase the speed just play some of the frames */
      else if((datatype & MOVTAR_DATA_VIDEO))
      //else if((datatype & MOVTAR_DATA_VIDEO)&&( (shmemptr->buffers) >1))
	{
	  viewed++;
	  movtar_read_data(movtarfile,readbuffer,datasize);
	  readpicfrommem(readbuffer,datasize);
	}
    }
  while(!(datatype & MOVTAR_DATA_VIDEO));
}

void inline readpicfrommem(void *inbuffer,int size)
{
  static struct jpeg_color_deconverter *cconvert;
  int i;

  jpeg_mem_src_reset(&cinfo, size);
  jpeg_read_header(&cinfo, TRUE);

  jpeg_start_decompress(&cinfo);

  if (screen->format->BytesPerPixel == 4)
    {
      cconvert = cinfo.cconvert;
      cconvert->color_convert = ycc_rgb32_convert_mmx;
    }

  /* lock the screen for current decompression */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      if ( SDL_LockSurface(screen) < 0 )
	ComplainAndExit();
    }

  if(img == NULL)
    {
      img = screen->pixels;

      if((imglines = (char **)calloc(cinfo.output_height, sizeof(char *)))==NULL)
	{
	  fprintf(stderr, "couldn't allocate memory for imglines\n");
	  exit(0);
	}
      for(i=0;i < cinfo.output_height;i++)
	imglines[i]= screen->pixels + i * screen->format->BytesPerPixel * screen->w;

      jpegdims.x = 0; // This is not going to work with interlaced pics !!
      jpegdims.y = 0;
      jpegdims.w = cinfo.output_width;
      jpegdims.h = cinfo.output_height;
    }
  
  while (cinfo.output_scanline < cinfo.output_height) 
    {       
      /* try to save the picture directly */
      jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &imglines[cinfo.output_scanline], 100);
    }

  /* unlock it again */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      SDL_UnlockSurface(screen);
    }

  SDL_UpdateRect(screen, 0, 0, jpegdims.w, jpegdims.h);
                          
  jpeg_finish_decompress(&cinfo);
}


void dump_pixel_format(struct SDL_PixelFormat *format)
{
  printf("Dumping format content\n");

  printf("BitsPerPixel: %d\n", format->BitsPerPixel);
  printf("BytesPerPixel: %d\n", format->BytesPerPixel);
  printf("Rloss: %d\n", format->Rloss);
  printf("Gloss: %d\n", format->Gloss);
  printf("Bloss: %d\n", format->Bloss);
  printf("Aloss: %d\n", format->Aloss);
  printf("Rshift: %d\n", format->Rshift);
  printf("Gshift: %d\n", format->Gshift);
  printf("Bshift: %d\n", format->Bshift);
  printf("Rmask: 0x%x\n", format->Rmask);
  printf("Gmask: 0x%x\n", format->Gmask);
  printf("Bmask: 0x%x\n", format->Bmask);
  printf("Amask: 0x%x\n", format->Amask);
}

int main(int argc,char** argv)
{ 
  int i;
  unsigned char *buffer;
  char wintitle[255];
  int frame =0;
  SDL_Event event;
  struct jpeg_error_mgr jerr;

  /* Initialize SDL library */
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
    ComplainAndExit();
  
  signal(SIGINT, callback_AbortProg);
  atexit(SDL_Quit);
  
  /* Set the video mode (800x600 at native depth) */
  screen = SDL_SetVideoMode(800, 600, 0, SDL_HWSURFACE /*| SDL_FULLSCREEN */);
  SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

  if ( screen == NULL )  
    ComplainAndExit(); 

  /* init the movtar library */
  initmovtar(argv[1]);

  cinfo.err = jpeg_std_error(&jerr);	
  jpeg_create_decompress(&cinfo);
  cinfo.out_color_space = JCS_RGB;
  cinfo.dct_method = JDCT_IFAST;
  jpeg_mem_src(&cinfo, readbuffer, 200000);

  sprintf(wintitle, "movtar_play %s", argv[1]);
  SDL_WM_SetCaption(wintitle, "0000000");  

  /* Draw bands of color on the raw surface */
#if 1
    buffer=(unsigned char *)screen->pixels;
  for ( i=0; i < screen->h; ++i ) 
    {
      memset(buffer,(i*255)/screen->h,
	     screen->w*screen->format->BytesPerPixel);
      buffer += screen->pitch;
    }
#endif

  do
    {
      readnext();
      frame++;
    }
  while((frame < 250) && !movtar_eof(movtarfile) && !SDL_PollEvent(&event));

  return 0;
}










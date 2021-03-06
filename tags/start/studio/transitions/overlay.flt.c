/*
 *  overlay.flt - reads two frame-interlaced YUV4MPEG streams from stdin
 *                and writes out a transistion from one to the other
 *
 *  Copyright (C) 2001, Ronald Bultje
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "yuv4mpeg.h"

int pid, to_yuvscaler, from_yuvscaler_e, from_yuvscaler_o;

static void usage () {

   fprintf (stderr, "usage:  overlay.flt [params]\n"
                    "params: -l [num]       - duration of transistion in frames (REQUIRED!)\n"
                    "        -p [num],[num] - Second stream's starting position (x,y) (default: 0,0)\n"
                    "        -s             - Scale second stream through yuvscaler\n"
                    "        -i             - Make 1st stream smaller rather than enlarge 2nd stream\n"
                    "        -v [num]       - verbosity [0..2]\n"
                    "        -h             - print out this usage information\n");

}

static void create_pipe(int w, int h)
{
   int ipipe[2], opipe[2], epipe[2];
   int n;

   if(pipe(ipipe) || pipe(opipe) || pipe(epipe)) {
      perror("pipe() failed");
      exit(1);
   }
   pid = fork();
   if (pid < 0) {
      perror("fork() failed");
      exit(1);
   }

   if (pid) /* This is the parent process */ {
      from_yuvscaler_o = opipe[0];
      close(opipe[1]);
      from_yuvscaler_e = epipe[0];
      fcntl (from_yuvscaler_e, F_SETFL, O_NONBLOCK);
      close(epipe[1]);

      to_yuvscaler = ipipe[1];
      close(ipipe[0]);
   } else /* This is the child process */ {
      char *command[6];
      char temp[128];
      command[0] = "yuvscaler";
      command[1] = "-O";
      sprintf(temp, "SIZE_%dx%d", w,h);
      command[2] = temp;
      command[3] = "-O";
      command[4] = "NOT_INTERLACED";
      command[5] = NULL;

      close(ipipe[1]);
      close(opipe[0]);
      close(epipe[0]);

      n = dup2(ipipe[0],0);
      if(n!=0) exit(1);
      close(ipipe[0]);

      n = dup2(opipe[1],1);
      if(n!=1) exit(1);
      close(opipe[1]);

      n = dup2(epipe[1],2);
      if(n!=2) exit(1);
      close(epipe[1]);

      execvp(command[0], command);
      exit(1);
   }
}

static void rescale(unsigned char *yuv[3], int width, int height, int rate, int dest_width, int dest_height)
{
   if (dest_width > 0 && dest_height > 0) {
      create_pipe(dest_width, dest_height);
      yuv_write_header(to_yuvscaler, width, height, rate);
      yuv_write_frame(to_yuvscaler, yuv, width, height);
      close(to_yuvscaler);
      yuv_read_header(from_yuvscaler_o, &dest_width, &dest_height, &rate);
      yuv_read_frame(from_yuvscaler_o, yuv, dest_width, dest_height);
      close(from_yuvscaler_o);
      close(from_yuvscaler_e);
      kill(pid, SIGINT);
      waitpid (pid, NULL, 0);
   }
}

static void overlay (unsigned char *src0[3], unsigned char *src1[3],
                   int pos_x, int pos_y, int rate,
                   int use_scale, int inverse, double progress_phase,
                   unsigned int width,     unsigned int height,
                   unsigned char *dst[3])
{
   register unsigned int len = width * height;
   register unsigned int i;
   register unsigned second_w;
   register unsigned second_h;
   unsigned char **static_src, **dynamic_src;

   if (inverse) {
      dynamic_src = src0;
      static_src = src1;
      second_w = width * (1-progress_phase);
      second_h = height * (1-progress_phase);
   } else {
      dynamic_src = src1;
      static_src = src0;
      second_w = width * progress_phase;
      second_h = height * progress_phase;
   }

   if (use_scale) {
      if (second_w%4) second_w = (second_w/4)*4;
      if (second_h%4) second_h = (second_h/4)*4;
      rescale(dynamic_src, width, height, rate, second_w, second_h);
   }

   for (i=0;i<len;i++) {
      if (i%width >= pos_x && i%width < pos_x + second_w &&
         i/width >= pos_y && i/width < pos_y + second_h) {
         dst[0][i] = dynamic_src[0][use_scale?(i/width - pos_y)*second_w + i%width - pos_x:i];
      } else {
         dst[0][i] = static_src[0][i];
      }
   }

   len/=4;
   width/=2; height/=2;
   second_w/=2; second_h/=2;
   pos_x/=2; pos_y/=2;

   for (i=0;i<len;i++) {
      if (i%width >= pos_x && i%width < pos_x + second_w &&
         i/width >= pos_y && i/width < pos_y + second_h) {
         dst[1][i] = dynamic_src[1][use_scale?(i/width - pos_y)*second_w + i%width - pos_x:i];
         dst[2][i] = dynamic_src[2][use_scale?(i/width - pos_y)*second_w + i%width - pos_x:i];
      } else {
         dst[1][i] = static_src[1][i];
         dst[2][i] = static_src[2][i];
      }
   }
}

int main (int argc, char *argv[])
{
   int verbose = 1;
   int in_fd  = 0;         /* stdin */
   int out_fd = 1;         /* stdout */
   unsigned char *yuv0[3]; /* input 0 */
   unsigned char *yuv1[3]; /* input 1 */
   unsigned char *yuv[3];  /* output */
   int w, h, rate;
   int i, frame;
   unsigned int param_duration   = 0;     /* duration of transistion effect */
   int param_start_x    = 0;     /* starting position of the second stream */
   int param_start_y    = 0;     /* starting position of the second stream */
   int param_scale      = 0;     /* whether to scale the second stream */
   int param_inverse    = 0;     /* normally, we let the second stream "pop up",
                                    with this, though, we let the first stream disappear */

   while ((i = getopt(argc, argv, "v:l:p:sih")) != -1) {
      switch (i) {
      case 'h':
         usage();
         exit(1);
         break;
      case 'v':
         verbose = atoi (optarg);
         if( verbose < 0 || verbose >2 ) {
            usage ();
            exit (1);
         }
         break;		  
      case 'l':
         param_duration = atoi (optarg);
         break;
      case 'p':
         sscanf(optarg, "%d,%d", &param_start_x, &param_start_y);
         break;
      case 's':
         param_scale = 1;
         break;
      case 'i':
         param_inverse = 1;
         break;
      }
   }

   if (param_duration <= 0) {
      fprintf(stderr, "Error: duration <= 0\n");
      usage();
      exit(1);
   }

   i = yuv_read_header (in_fd, &w, &h, &rate);
   
   yuv[0] = (char *)malloc (w*h);   (char *)yuv0[0] = malloc (w*h);   (char *)yuv1[0] = malloc (w*h);
   yuv[1] = (char *)malloc (w*h/4); (char *)yuv0[1] = malloc (w*h/4); (char *)yuv1[1] = malloc (w*h/4);
   yuv[2] = (char *)malloc (w*h/4); (char *)yuv0[2] = malloc (w*h/4); (char *)yuv1[2] = malloc (w*h/4);

   yuv_write_header (out_fd, w, h, rate);

   for (frame=0;frame<param_duration;frame++)
   {
      i = yuv_read_frame(in_fd, yuv0, w, h);
      if (i<=0) exit (1);
      i = yuv_read_frame(in_fd, yuv1, w, h);
      if (i<=0) exit (1);

      overlay (yuv0, yuv1,
         ((param_inverse?frame:(param_duration-frame))/(double)param_duration)*param_start_x,
         ((param_inverse?frame:(param_duration-frame))/(double)param_duration)*param_start_y,
         rate, param_scale, param_inverse, frame/(double)param_duration,
         w, h, yuv);

      yuv_write_frame (out_fd, yuv, w, h);
   }

   return 0;
}

/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode.c done by Bernhard Praschinger
 *  
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <signal.h>
#include <gdk/gdkx.h>
#include "studio.h"
#include "pipes.h"
#include "gtkfunctions.h"

/* Some variables defined here */
GtkWidget *input_entry, *output_entry, *sound_entry, *video_entry;
GtkWidget *combo_audiobit, *combo_samplerate, *combo_entry_samplebitrate;
GtkWidget *combo_entry_audiobitrate, *combo_entry_videobitrate, *combo_videobit;
GtkWidget *combo_searchradius, *combo_entry_searchradius, *combo_entry_noisefilter;
GtkWidget *combo_noisefilter, *combo_samples, *combo_entry_samples; 
GtkWidget *combo_outputformat, *combo_entry_outputformat, *combo_entry_muxfmt;
GtkWidget *combo_muxfmt, *execute_status, *button_mpeg1, *button_mpeg2;
GtkWidget *check_sequence, *button_force_no, *button_force_vcd, *button_force_stereo;
GtkWidget *button_force_mono, *combo_entry_scaleroutput, *combo_entry_scalermode;
GtkWidget *remove_files_after_completion;
GtkWidget *combo_entry_active, *combo_entry_scalerinput;

/* For progress-meter */
GtkWidget *progress_label;
GtkWidget *progress_button_label;
GtkWidget *progress_status_label;
GtkWidget *progress_bar;
int progress_encoding = 0;
int num_frames = -1;
char standard;
GtkObject *progress_adj;
int go_all_the_way = 0; /* if 1, it will do audio/video/mplex, else only 1 of them */
int use_yuvscaler_pipe; /* tells us whether to use yuvscaler or not */
int use_yuvplay_pipe = 0; /* see preview window while video is being encoded or not */
GtkWidget *progress_window;
GtkWidget *tv_preview;
int error = 0;

/* lists with  encodingoptions */
GList *muxformat = NULL;
GList *samples = NULL;
GList *outputformat = NULL;
GList *yuvscalermode = NULL;
/*-------------------------------------------------------------*/

void lavencode_callback(int number, char *input);
void continue_encoding(void);
int number_of_frames(char *editlist);
void status_progress_window(void);
void stop_encoding_process(GtkWidget *widget, gpointer data);
void show_executing(char *command);
void command2string(char **command, char *string);
void callback_pipes(gpointer data, gint source,GdkInputCondition condition);
void audio_convert(void);
void video_convert(void);
void mplex_convert(void);
void do_vcd(GtkWidget *widget, gpointer data);
void do_svcd(GtkWidget *widget, gpointer data);
void do_defaults(GtkWidget *widget, gpointer data);
void input_callback( GtkWidget *widget, GtkWidget *input_select );
void output_callback( GtkWidget *widget, GtkWidget *output_select );
void sound_callback( GtkWidget *widget, GtkWidget *sound_select );
void file_ok_sound ( GtkWidget *w, GtkFileSelection *fs );
void create_file_sound(GtkWidget *widget, gpointer data);
void file_ok_input( GtkWidget *w, GtkFileSelection *fs );
void create_file_input(GtkWidget *widget, gpointer data);
void file_ok_output( GtkWidget *w, GtkFileSelection *fs );
void create_in_out (GtkWidget *hbox1, GtkWidget *vbox);
void create_vcd_svcd (GtkWidget *hbox1, GtkWidget *vbox);
void set_audiobitrate (GtkWidget *widget, gpointer data);
void set_samplebitrate (GtkWidget *widget, gpointer data);
void force_options (GtkWidget *widget, gpointer data);
void create_sound_encoding (GtkWidget *table);
void set_videobit (GtkWidget *widget, gpointer data);
void set_searchrad (GtkWidget *widget, gpointer data);
void set_sequenceheader(GtkWidget *widget, gpointer data);
void mpeg_option (GtkWidget *widget, gpointer data);
void create_video_layout (GtkWidget *table);
void create_file_output(GtkWidget *widget, gpointer data);
void set_noisefilter (GtkWidget *widget, gpointer data);
void set_drop_samples (GtkWidget *widget, gpointer data);
void set_outputformat (GtkWidget *widget, gpointer data);
void create_yuvscaler_options (GtkWidget *hbox1, GtkWidget *vbox);
void create_video_options_layout (GtkWidget *table);
void video_callback ( GtkWidget *widget, GtkWidget *video_entry );
void file_ok_video ( GtkWidget *w, GtkFileSelection *fs );
void create_file_video(GtkWidget *widget, gpointer data);
void create_fileselect_video (GtkWidget *hbox1, GtkWidget *vbox);
void create_video (GtkWidget *table);
void set_mplex_muxfmt (GtkWidget *widget, gpointer data);
void create_mplex_options (GtkWidget *table);
void encode_all (GtkWidget *widget, gpointer data);
void create_buttons1 (GtkWidget *hbox1);
void create_buttons2 (GtkWidget *hbox1);
void create_mplex (GtkWidget *table);
void create_status (GtkWidget *hbox1, GtkWidget *vbox);
void create_yuvscaler_layout (GtkWidget *table);
void set_activewindow (GtkWidget *widget, gpointer data);
void set_scalerstrings (GtkWidget *widget, gpointer data);


/*************************** Programm starts here **************************/

int number_of_frames(char *editlist)
{
   FILE *fd;
   int n,num,i,j;
   char buffer[256];

   fd = fopen(editlist, "r");
   if (fd == NULL)
   {
      printf("Error opening %s\n",editlist);
      return -1;
   }

   if (NULL != fgets(buffer,255,fd))
   {
      buffer[strlen(buffer)-1] = '\0'; /* get rid of \n */
      if (strcmp(buffer, "LAV Edit List") != 0)
      {
         /* not an editlist! So let's make our own :-) */
         char command_temp[256], file_temp[100];

         fclose(fd);
         sprintf(file_temp, "%s/.studio/.temp.eli", getenv("HOME"));
         /* this is for certainty to prevent circles */
         if (strcmp(editlist, file_temp)==0) return -1;
         sprintf(command_temp, "%s -S %s -T -1 %s >> /dev/null 2>&1",
            LAV2YUV_LOCATION, file_temp, editlist);
         system(command_temp);
         num = number_of_frames(file_temp);
         unlink(file_temp);
         return(num);
      }
   }

   fgets(buffer,255,fd);
   for(i=0;i<strlen(buffer);i++)
   {
      if(buffer[i]=='\n')
      {
         buffer[i] = '\0';
         break;
      }
   }
   if(strcmp(buffer, "PAL")==0)
      standard = 'p';
   else
      standard = 'n';

   fgets(buffer,255,fd); /* number of files */
   sscanf(buffer, "%d", &num);
   for (n=0;n<num;n++)
   {
      fgets(buffer,255,fd); /* skip movie-lines */
   }

   num = 0;
   /* Now, it gets interesting */
   while (NULL != fgets(buffer,255,fd))
   {
      sscanf(buffer, "%d %d %d", &n, &i, &j);
      num += (j-i+1); /* counts frames */
   }

   fclose(fd);

   return num; /* number of frames */
}

void lavencode_callback(int number, char *input) 
{
   if (progress_status_label)
   {
      int n1, n2;
      char c;
      if (number == MPEG2ENC && sscanf(input, "   INFO: Frame %d %c %d", &n1, &c, &n2)==3)
      {
         gtk_label_set_text(GTK_LABEL(progress_status_label), input+9);
         if (num_frames>0) gtk_adjustment_set_value(GTK_ADJUSTMENT(progress_adj), n1);
      }
      else if (number == MP2ENC && sscanf(input, "--DEBUG: %d seconds done", &n1)==1)
      {
         gtk_label_set_text(GTK_LABEL(progress_status_label), input+9);
         if (num_frames>0) gtk_adjustment_set_value(GTK_ADJUSTMENT(progress_adj), standard=='p'?25*n1:30*n1);
      }
   }

   if (strncmp(input, "**ERROR:", 8) == 0)
   {
      char temp[256];

      /* Error handling */
      if (!error) stop_encoding_process(NULL, (gpointer)progress_window);
      sprintf(temp, "%s returned an error:", app_name(number));
      gtk_show_text_window(STUDIO_ERROR,
		temp, input+9);
      error++;
   }
}

void continue_encoding() 
{
   if (go_all_the_way) switch (progress_encoding ) {
      case 1:
         video_convert();
         progress_encoding = 2;
         break;
      case 2:
         mplex_convert();
         progress_encoding = 3;
         break;
      case 3:
         progress_encoding = 0;
         if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label), "Finished!");
         if (progress_button_label) gtk_label_set_text(GTK_LABEL(progress_button_label), " Close ");
         if (progress_bar) gtk_progress_bar_update(GTK_PROGRESS_BAR(progress_bar), 1.0);
         show_executing("");

         if (GTK_TOGGLE_BUTTON(remove_files_after_completion)->active) {
            /* remove temp files */
            unlink(enc_audiofile);
            unlink(enc_videofile);
         }

         break;
   }
   else {
      progress_encoding = 0;
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label), "Finished!");
      if (progress_button_label) gtk_label_set_text(GTK_LABEL(progress_button_label), " Close ");
      if (progress_bar) gtk_progress_bar_update(GTK_PROGRESS_BAR(progress_bar),
1.0);

      if (GTK_TOGGLE_BUTTON(remove_files_after_completion)->active) {
         /* remove temp files */
         unlink(enc_audiofile);
         unlink(enc_videofile);
      }
   }
}


void stop_encoding_process(GtkWidget *widget, gpointer data) {
   GtkWidget *window = (GtkWidget*)data;

   go_all_the_way = 0;

   if (progress_encoding) {
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label), "Cancelled");
      show_executing("Cancelled");
   }

   switch(progress_encoding) {
   case 1:
      close_pipe(LAV2WAV);
      close_pipe(MP2ENC);
      break;
   case 2:
      close_pipe(LAV2YUV);
      if (use_yuvscaler_pipe) close_pipe(YUVSCALER);
      close_pipe(MPEG2ENC);
      break;
   case 3:
      close_pipe(MPLEX);
      break;
   }

   progress_encoding = 0;

   progress_label = NULL;
   progress_button_label = NULL;
   progress_status_label = NULL;
   progress_bar = NULL;
   progress_adj = NULL;
   progress_encoding = 0;
   gtk_widget_destroy(window);

   if (GTK_TOGGLE_BUTTON(remove_files_after_completion)->active) {
      /* remove temp files */
      unlink(enc_audiofile);
      unlink(enc_videofile);
   }
}

void status_progress_window() {
   GtkWidget *vbox, *hbox, *label, *button;

   num_frames = number_of_frames(enc_inputfile);

   progress_window = gtk_window_new(GTK_WINDOW_DIALOG);
   vbox = gtk_vbox_new(FALSE, 5);

   gtk_window_set_title (GTK_WINDOW(progress_window), "Linux Video Studio - MPEG Encoding status");
   gtk_container_set_border_width (GTK_CONTAINER (progress_window), 20);

   if (use_yuvplay_pipe)
   {
      hbox = gtk_hbox_new(TRUE, 0);

      tv_preview = gtk_event_box_new();
      gtk_box_pack_start (GTK_BOX (hbox), tv_preview, TRUE, FALSE, 0);
      gtk_widget_set_usize(GTK_WIDGET(tv_preview), 240, 180); /* TODO: make sizes configurable */
      gtk_widget_show(tv_preview);
      set_background_color(tv_preview,0,0,0);

      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      gtk_widget_show(hbox);
   }

   hbox = gtk_hbox_new(FALSE, 0);
   label = gtk_label_new ("Status: ");
   gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE,FALSE, 0);
   gtk_widget_show (label);
   progress_label = gtk_label_new ("Starting up...");
   gtk_misc_set_alignment(GTK_MISC(progress_label), 0.0, GTK_MISC(progress_label)->yalign);
   gtk_box_pack_start (GTK_BOX (hbox), progress_label, TRUE, TRUE, 0);
   gtk_widget_show (progress_label);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   gtk_widget_show(hbox);

   progress_status_label = gtk_label_new(" ");
   gtk_box_pack_start (GTK_BOX (vbox), progress_status_label, FALSE,FALSE, 10);
   gtk_widget_show (progress_status_label);

   progress_bar = gtk_progress_bar_new();
   gtk_box_pack_start (GTK_BOX (vbox), progress_bar, FALSE, FALSE, 0);
   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(progress_bar), GTK_PROGRESS_LEFT_TO_RIGHT);
   gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(progress_bar), GTK_PROGRESS_CONTINUOUS);
   gtk_progress_set_show_text(GTK_PROGRESS(progress_bar), 1);
   gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "%p\%");
   if (num_frames > 0)
   {
      progress_adj = gtk_adjustment_new(0,0,num_frames,1,1,0);
      gtk_progress_set_adjustment(GTK_PROGRESS(progress_bar), GTK_ADJUSTMENT(progress_adj));
   }
   gtk_widget_show(progress_bar);

   button = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      (GtkSignalFunc)stop_encoding_process, (gpointer)progress_window);
   progress_button_label = gtk_label_new(" Cancel ");
   gtk_container_add (GTK_CONTAINER (button), progress_button_label);
   gtk_widget_show(progress_button_label);
   gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   gtk_container_add (GTK_CONTAINER (progress_window), vbox);
   gtk_widget_show(vbox);

   gtk_grab_add(progress_window);
   gtk_widget_show(progress_window);
}

/* print text */
void show_executing(char command[500])
{
  gtk_label_set_text(GTK_LABEL(execute_status),command);
  gtk_widget_show (execute_status);
}

void command2string(char **command, char *string)
{
   int i;

   string[0] = '\0';
   for (i=0;;i++)
   {
      if(command[i]!=NULL)
         sprintf(string,"%s %s",string, command[i]); 
      else
         return;
   }
}

/* here the audio command is created */
void audio_convert()
{
   int n;
   char *mp2enc_command[256];
   char *lav2wav_command[256];
   char command[500];
   char command_temp[256];
   static char temp1[16], temp2[16];

   n=0;
   error = 0;
   progress_encoding = 1;

   if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
      "Encoding audio: lav2wav | mp2enc");

   mp2enc_command[n] = MP2ENC_LOCATION; n++;
   mp2enc_command[n] = "-v 2"; n++;
   if (encoding.forcevcd[0] == '-')
     {
       mp2enc_command[n] = encoding.forcevcd;
       n++;
     }
   else                   /* If VCD is set no other Options are needed */
     {
        mp2enc_command[n] = "-b"; n++;
        sprintf(temp1,"%i", encoding.audiobitrate);
        mp2enc_command[n] = temp1; n++;
        mp2enc_command[n] = "-r"; n++;
        sprintf(temp2, "%i00", encoding.outputbitrate);
        mp2enc_command[n] = temp2; n++;
       if (encoding.forcemono[0] == '-')
         {
           mp2enc_command[n] = encoding.forcemono;
           n++;
         }
       if (encoding.forcestereo[0] == '-')
         {
           mp2enc_command[n] = encoding.forcestereo;
           n++;
         }
     }
   mp2enc_command[n] = "-o"; n++;         /* Setting output file name */
   mp2enc_command[n] = enc_audiofile; n++;
   mp2enc_command[n] = NULL;
   
   start_pipe_command(mp2enc_command, MP2ENC); /* mp2enc */

   n = 0;
   lav2wav_command[n] = LAV2WAV_LOCATION; n++;
   lav2wav_command[n] = enc_inputfile; n++;
   lav2wav_command[n] = NULL;
 
   start_pipe_command(lav2wav_command, LAV2WAV); /* lav2wav */

   command2string(lav2wav_command, command_temp);
   sprintf(command, "%s |", command_temp);
   command2string(mp2enc_command, command_temp);
   sprintf(command, "%s %s", command, command_temp);
   if (verbose)
      printf("Executing: %s\n", command);
   show_executing(command);
}

/* video encoding */
void video_convert()
{
   int n;
   char *mpeg2enc_command[256];
   char *lav2yuv_command[256];
   char *yuvscaler_command[256];
   char *yuvplay_command[256];
   char command[600];
   char command_temp[256];
   static char temp1[16],temp2[16],temp3[16],temp4[16],temp5[16],temp6[16], temp7[4];

   error = 0;
   progress_encoding = 2;
   if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
      "Encoding video");

   n = 0;
   mpeg2enc_command[n] = MPEG2ENC_LOCATION; n++;
   mpeg2enc_command[n] = "-v1"; n++;
   mpeg2enc_command[n] = "-b"; n++;
   sprintf(temp4, "%i", encoding.bitrate);
   mpeg2enc_command[n] =  temp4; n++;
   if(encoding.mpeglevel != 1) {
      sprintf(temp5, "%i",encoding.mpeglevel);
      mpeg2enc_command[n] =  "-m"; n++;
      mpeg2enc_command[n] =  temp5; n++;
   }
   if(encoding.searchradius != 16) {
      sprintf(temp6, "%i", encoding.searchradius);
      mpeg2enc_command[n] =  "-r"; n++;
      mpeg2enc_command[n] =  temp6; n++;
   }
   if(encoding.sequenceheader[1] == 's') {
      mpeg2enc_command[n] = encoding.sequenceheader; n++;
   }
   mpeg2enc_command[n] = "-o"; n++;
   mpeg2enc_command[n] = enc_videofile; n++;
   mpeg2enc_command[n] = NULL;

   start_pipe_command(mpeg2enc_command, MPEG2ENC); /* mpeg2enc */

   /* let's start yuvplay to show video while it's being encoded */
   if (use_yuvplay_pipe)
   {
      char SDL_windowhack[32];

      sprintf(SDL_windowhack,"%ld",GDK_WINDOW_XWINDOW(tv_preview->window));
      setenv("SDL_WINDOWID", SDL_windowhack, 1);

      n = 0;
      yuvplay_command[n] = "yuvplay"; n++;
      yuvplay_command[n] = "-s"; n++;
      yuvplay_command[n] = "240x180"; n++;
      yuvplay_command[n] = NULL;
      start_pipe_command(yuvplay_command, YUVPLAY); /* yuvplay */
   }

   /* Here, the command for the pipe for yuvscaler may be added */
   if ((strlen(encoding.input_use) > 0) ||
       (strlen(encoding.output_size) > 0) ||
       (strlen(encoding.mode_keyword) > 0) )
   {
      n = 0;
      yuvscaler_command[n] = YUVSCALER_LOCATION; n++;
      if (strlen(encoding.input_use) > 0)
      {
         yuvscaler_command[n] = "-I"; n++;
         yuvscaler_command[n] = encoding.input_use; n++;
      }
      if (strlen(encoding.ininterlace_type) > 0)
      {
         yuvscaler_command[n] = "-I"; n++;
         yuvscaler_command[n] = encoding.ininterlace_type; n++;
      }
      if (strlen(encoding.mode_keyword) > 0)
      {
         yuvscaler_command[n] = "-M"; n++;
         yuvscaler_command[n] = encoding.mode_keyword; n++;
      }
      if (strlen(encoding.output_size) > 0)
      {
         yuvscaler_command[n] = "-O"; n++;
         yuvscaler_command[n] = encoding.output_size; n++;
      }
      if (encoding.addoutputnorm == 1)
      {
         sprintf(temp7, "-n%c", standard);
         yuvscaler_command[n] = temp7; n++;
      }
      yuvscaler_command[n] = NULL;

      start_pipe_command(yuvscaler_command, YUVSCALER); /* yuvscaler */

      /* set variable in pipes.h to tell to use yuvscaler */
      use_yuvscaler_pipe = 1;
   }
   else
   {
      /* set variable in pipes.h to tell not to use yuvscaler */
      use_yuvscaler_pipe = 0;
   }

   n = 0;
   lav2yuv_command[n] = LAV2YUV_LOCATION; n++;
   if (strlen(encoding.notblacksize) > 0 ) 
     {
      lav2yuv_command[n] = "-a"; n++;
      lav2yuv_command[n] = encoding.notblacksize; n++;
     }
   if (encoding.outputformat > 0) 
     {
      sprintf(temp1, "%i", encoding.outputformat);
      lav2yuv_command[n] = "-s"; n++;
      lav2yuv_command[n] = temp1; n++;
     }
   if (encoding.droplsb > 0) 
     {
      sprintf(temp2, "%i", encoding.droplsb);
      lav2yuv_command[n] = "-d"; n++;
      lav2yuv_command[n] = temp2; n++;
     }
   if (encoding.noisefilter > 0) 
     {
      sprintf(temp3, "%i", encoding.noisefilter);
      lav2yuv_command[n] = "-n"; n++;
      lav2yuv_command[n] = temp3; n++;
     }
   lav2yuv_command[n] = enc_inputfile; n++;
   lav2yuv_command[n] = NULL;

   start_pipe_command(lav2yuv_command, LAV2YUV); /* lav2yuv */

   command2string(lav2yuv_command, command_temp);
   sprintf(command, "%s |", command_temp);
   if (use_yuvscaler_pipe)
   {
      command2string(yuvscaler_command, command_temp);
      sprintf(command, "%s %s |", command, command_temp);
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
         "Encoding video: lav2yuv | yuvscaler | mpeg2enc");
   }
   else
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
         "Encoding video: lav2yuv | mpeg2enc");

   command2string(mpeg2enc_command, command_temp);
   sprintf(command, "%s %s", command, command_temp);
   if (verbose)
      printf("Executing : %s\n", command);
   show_executing(command);
}

/* mplexing the encodet streams */
void mplex_convert()
{
   char *mplex_command[256];
   char command[256];
   static char temp1[16];
   int n;

   error = 0;
   progress_encoding = 3;

   if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
      "Multiplexing: mplex");

   n = 0;
   mplex_command[n] = MPLEX_LOCATION; n++;
   if (encoding.muxformat != 0) {
      sprintf(temp1, "%i", encoding.muxformat);
      mplex_command[n] = "-f"; n++;
      mplex_command[n] = temp1; n++;
   }
   mplex_command[n] = enc_audiofile; n++;
   mplex_command[n] = enc_videofile; n++;
   mplex_command[n] = "-o"; n++;
   mplex_command[n] = enc_outputfile; n++;
   mplex_command[n] = NULL;

   start_pipe_command(mplex_command, MPLEX); /* mplex */

   command2string(mplex_command, command);
   if (verbose)
     printf("\nExecuting : %s\n", command);
   show_executing(command);
}

/* set the VCD options */ 
void do_vcd(GtkWidget *widget, gpointer data)
{
  gtk_entry_set_text(GTK_ENTRY(combo_entry_audiobitrate),"224");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_samplebitrate),"44100");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_outputformat), "as is");
/* *** Removed because if noting to scale yuvscaler prevents encoding *** */
/*  gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), "VCD");  */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_force_vcd),TRUE );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_mpeg1),TRUE );
  gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),"1152");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_searchradius),"16");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_sequence),TRUE );
  gtk_entry_set_text(GTK_ENTRY(combo_entry_muxfmt),"VCD");
}

/* set the SVCD options */
void do_svcd(GtkWidget *widget, gpointer data)
{
  gtk_entry_set_text(GTK_ENTRY(combo_entry_audiobitrate),"224");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_samplebitrate),"44100");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_outputformat), "as is");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), "SVCD");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_force_vcd),TRUE );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_mpeg2),TRUE );
  gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),"2500");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_searchradius),"16");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_sequence),TRUE );
  gtk_entry_set_text(GTK_ENTRY(combo_entry_muxfmt),"SVCD");
}

/* set the default options */
void do_defaults(GtkWidget *widget, gpointer data)
{
int i;
char val[LONGOPT];

  if (strcmp((char*)data, "save")==0)
    save_config_encode(); 

  if (strcmp((char*)data, "load")==0)
    {
      load_config_encode(); 

      gtk_entry_set_text(GTK_ENTRY(input_entry), enc_inputfile);
      gtk_entry_set_text(GTK_ENTRY(output_entry),enc_outputfile);
      gtk_entry_set_text(GTK_ENTRY(sound_entry), enc_audiofile);
      gtk_entry_set_text(GTK_ENTRY(video_entry), enc_videofile);

  /* lav2yuv option */
      gtk_entry_set_text(GTK_ENTRY(combo_entry_active), encoding.notblacksize);

  /* yuvscaler options */
      gtk_entry_set_text(GTK_ENTRY(combo_entry_scalerinput),encoding.input_use);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), encoding.output_size);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_scalermode), encoding.mode_keyword);

  /* lav2yuv options */
      outputformat = g_list_first (outputformat);
      for (i = 0; i < encoding.outputformat ;i++)
        outputformat = g_list_next (outputformat);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_outputformat), outputformat->data);

      sprintf(val,"%i",encoding.droplsb);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_samples), val);

      sprintf(val,"%i",encoding.noisefilter);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_noisefilter), val);

  /* Audio Options  */
      sprintf(val,"%i",encoding.audiobitrate);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_audiobitrate), val);
      sprintf(val,"%i00",encoding.outputbitrate);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_samplebitrate), val);

      if (encoding.forcestereo[0] == '-')
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_force_stereo),TRUE);
      if (encoding.forcemono[0] == '-')
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_force_mono),TRUE);
      if (encoding.forcevcd[0] == '-')
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_force_vcd),TRUE);

  /* Video Options */  
      if (encoding.mpeglevel == 1)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_mpeg1),TRUE );
      if (encoding.mpeglevel == 2)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_mpeg2),TRUE );

      sprintf(val,"%i",encoding.bitrate);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),val);

      sprintf(val,"%i",encoding.searchradius);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_searchradius),val);

      /* No need to set use_yuvplay_pipe here */

    }
 
}

/* Changes to the input field are here processed */
void input_callback( GtkWidget *widget, GtkWidget *input_select )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(input_select));
  strcpy(enc_inputfile, file);

  if (verbose)
    printf("\nEntry contents: %s\n", enc_inputfile);
}

/* Changes to the output field are here processed */
void output_callback( GtkWidget *widget, GtkWidget *output_select )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(output_select));
  strcpy(enc_outputfile, file);

  if (verbose)
    printf("\nEntry contents: %s\n", enc_outputfile);
}

/* Changes to the audio file field are here processed */
void sound_callback( GtkWidget *widget, GtkWidget *sound_select )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(sound_select));
  strcpy(enc_audiofile, file);

  if (verbose) 
    printf("\nEntry contents: %s\n", enc_audiofile);
}

/* Set the selected audio filename */
void file_ok_sound ( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(sound_entry),
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a new file selection widget for the output file*/
void create_file_sound(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new ("Linux Video Studio - Audio File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_sound, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Set the selected input filename */
void file_ok_input( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(input_entry),
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a new file selection widget for the input file*/
void create_file_input(GtkWidget *widget, gpointer data)
{ 
GtkWidget *filew;
  
  filew = gtk_file_selection_new ("Linux Video Studio - Input File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_input, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Set the selected output filename */
void file_ok_output( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(output_entry), 
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a new file selection widget for the output file*/
void create_file_output(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new ("Linux Video Studio - Output File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_output, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Create Layout for the input and output fileselection */
void create_in_out (GtkWidget *hbox1, GtkWidget *vbox)
{
GtkWidget *label1, *label2;   
GtkWidget *input_select, *output_select;

  label1 = gtk_label_new ("  Input file : ");
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0); 

  input_entry = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(input_entry), enc_inputfile); 
  gtk_signal_connect(GTK_OBJECT(input_entry), "changed",
                     GTK_SIGNAL_FUNC(input_callback), input_entry);
  gtk_widget_set_usize (input_entry, 200, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), input_entry, FALSE, TRUE, 0);
  gtk_widget_show (input_entry);

  input_select = gtk_button_new_with_label ("Select");
  gtk_widget_show (input_select);
  gtk_signal_connect(GTK_OBJECT(input_select), "clicked", 
                     GTK_SIGNAL_FUNC(create_file_input), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), input_select, FALSE, FALSE, 0);

  label2 = gtk_label_new ("  Output file : ");
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0); 
     
  output_entry = gtk_entry_new (); 
  gtk_entry_set_text(GTK_ENTRY(output_entry), enc_outputfile); 
  gtk_signal_connect(GTK_OBJECT(output_entry), "changed",
                     GTK_SIGNAL_FUNC(output_callback), output_entry);
  gtk_widget_set_usize (output_entry, 200, -2);
  gtk_widget_show (output_entry);
  gtk_box_pack_start (GTK_BOX (hbox1), output_entry, FALSE, TRUE, 0);

  output_select = gtk_button_new_with_label ("Select");
  gtk_signal_connect(GTK_OBJECT(output_select), "clicked", 
                     GTK_SIGNAL_FUNC(create_file_output), NULL);
  gtk_widget_show (output_select);
  gtk_box_pack_start (GTK_BOX (hbox1), output_select, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0); 
  gtk_widget_show (hbox1);
}

/* set output bitrate for the audio */
void set_audiobitrate (GtkWidget *widget, gpointer data)
{
char *test;

  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_audiobitrate));
  encoding.audiobitrate = atoi ( test );

  if ( (encoding.audiobitrate < 32) || (encoding.audiobitrate > 320))
    encoding.audiobitrate = 224;

  if (verbose)
    printf("\n selected audio bitrate: %i \n", encoding.audiobitrate);
}

/* set smaplerate for the audio */
void set_samplebitrate (GtkWidget *widget, gpointer data)
{
gchar *test;
int i;

  i = 0;
  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_samplebitrate));
  samples = g_list_last (samples);

  for (i = (g_list_length (g_list_first(samples))) ; i > 0 ; i--)
    {
      if (strcmp (test,samples->data) == 0)
        {
          encoding.outputbitrate = ( (atoi (samples->data)) / 100);  
          break;
        }
      samples = g_list_previous (samples);
    }

  if (verbose)
    printf("\n selected audio samplerate: %i00\n", encoding.outputbitrate);
}

/* Set the option that should be forced */
void force_options (GtkWidget *widget, gpointer data)
{
  if ( (strcmp((char*)data,"-V") ==0) && encoding.forcevcd[0] == ' ' ) 
    sprintf(encoding.forcevcd, "%s", (char*)data);
  else 
    sprintf(encoding.forcevcd, " ");

  if ( (strcmp((char*)data,"-s") ==0) && encoding.forcestereo[0] == ' ' ) 
    sprintf(encoding.forcestereo, "%s", (char*)data);
  else 
    sprintf(encoding.forcestereo, " ");

  if ( (strcmp((char*)data,"-m") ==0) && encoding.forcemono[0] == ' ' ) 
    sprintf(encoding.forcemono, "%s", (char*)data);
  else 
    sprintf(encoding.forcemono, " ");

  if (verbose) 
    printf("\n in mono select: %s, vcd: %s, stereo %s, mono %s \n", 
     (char*)data, encoding.forcevcd, encoding.forcestereo, encoding.forcemono);
}

/* Create Layout for the sound conversation */
void create_sound_encoding (GtkWidget *table)
{
   GtkWidget *label1;
   GSList *group_force;
   GList *abitrate = NULL;

   abitrate = g_list_append (abitrate, "224");
   abitrate = g_list_append (abitrate, "160");
   abitrate = g_list_append (abitrate, "128");
   abitrate = g_list_append (abitrate, "96");

   samples = g_list_append (samples, "44100");
   samples = g_list_append (samples, "48000");
   samples = g_list_append (samples, "32000");

  label1 = gtk_label_new ("  Bitrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 1, 2);
  gtk_widget_show (label1);

  combo_audiobit = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_audiobit), abitrate);
  combo_entry_audiobitrate = GTK_COMBO (combo_audiobit)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_audiobitrate), "changed",
                      GTK_SIGNAL_FUNC (set_audiobitrate), NULL);
  gtk_widget_set_usize (combo_audiobit, 60, -2);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_audiobit, 1, 2, 1, 2);
  gtk_widget_show (combo_audiobit);

  label1 = gtk_label_new ("  Sampelrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 2, 3);
  gtk_widget_show (label1);

  combo_samplerate = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_samplerate), samples);
  combo_entry_samplebitrate = GTK_COMBO (combo_samplerate)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_samplebitrate), "changed",
                      GTK_SIGNAL_FUNC (set_samplebitrate), NULL);
  gtk_widget_set_usize (combo_samplerate, 80, -2);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_samplerate, 1, 2, 2, 3);
  gtk_widget_show (combo_samplerate);

  label1 = gtk_label_new ("  Force Sound: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 3, 4);
  gtk_widget_show (label1);
 
  button_force_no = gtk_radio_button_new_with_label (NULL, "no change");
  gtk_signal_connect (GTK_OBJECT (button_force_no), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "cl");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_no, 1, 2, 3, 4);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_no));
  gtk_widget_show (button_force_no);
 
  button_force_vcd = gtk_radio_button_new_with_label(group_force, "VCD");
  gtk_signal_connect (GTK_OBJECT (button_force_vcd), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "-V");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_vcd, 1, 2, 4, 5);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_vcd));
  gtk_widget_show (button_force_vcd);

  button_force_stereo = gtk_radio_button_new_with_label(group_force, "Stereo");
  gtk_signal_connect (GTK_OBJECT (button_force_stereo), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "-s");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_stereo, 1, 2, 5, 6);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_stereo));
  gtk_widget_show (button_force_stereo);
  
  button_force_mono = gtk_radio_button_new_with_label(group_force, "Mono");
  gtk_signal_connect (GTK_OBJECT (button_force_mono), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "-m");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_mono, 1, 2, 6, 7);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_mono));
  gtk_widget_show (button_force_mono);
}

/* set output bitrate for the video */
void set_videobit (GtkWidget *widget, gpointer data)
{
char *test;

  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_videobitrate));

  encoding.bitrate = atoi ( test );

  if (verbose)
    printf("\n selected video bitrate: %i \n", encoding.bitrate);
}

/* set output bitrate for the video */
void set_searchrad (GtkWidget *widget, gpointer data)
{
char *test;

  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_searchradius));

  encoding.searchradius = atoi ( test );

  if (verbose)
    printf("\n selected searchradius : %i \n", encoding.searchradius);
}

/* set the sequence header option */
void set_sequenceheader(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    sprintf(encoding.sequenceheader,"-s"); 
  else 
    sprintf(encoding.sequenceheader," "); 

  if (verbose)
    printf("\n Set Sequence header : %s\n ",encoding.sequenceheader);
}

/* Set the mpeg option */
void mpeg_option (GtkWidget *widget, gpointer data)
{
char temp[3];
int i;

for (i = 0; i < 3; i++)
  temp[i]='\0';

  if (strcmp((char*)data,"-m 1") ==0 )
    encoding.mpeglevel = 1;
  else if (strcmp((char*)data,"-m 2") ==0 ) 
    encoding.mpeglevel = 2;

  sprintf(temp,"%c",enc_videofile[strlen(enc_videofile)-2]);

  if ( (strcmp(temp , "1") == 0) || (strcmp(temp , "2") == 0) )
    {
      sprintf(temp,"%i",encoding.mpeglevel);
      enc_videofile[strlen(enc_videofile)-2] = temp[0];
      gtk_entry_set_text(GTK_ENTRY(video_entry), enc_videofile);
    }

  if (verbose)
    printf("\n MPEG Level set to: %i \n",encoding.mpeglevel);
}


/* create the video conversation  the mpeg2enc options */
void create_video_layout (GtkWidget *table)
{
GtkWidget *label1;
GSList *group_mpeg;
GList *vbitrate = NULL;
GList *searchrad = NULL;

   vbitrate = g_list_append (vbitrate, "1152");
   vbitrate = g_list_append (vbitrate, "1300");
   vbitrate = g_list_append (vbitrate, "1500");
   vbitrate = g_list_append (vbitrate, "1800");
   vbitrate = g_list_append (vbitrate, "2000");
   vbitrate = g_list_append (vbitrate, "2500");
   
   searchrad = g_list_append (searchrad, "0");
   searchrad = g_list_append (searchrad, "8");
   searchrad = g_list_append (searchrad, "16");
   searchrad = g_list_append (searchrad, "24");
   searchrad = g_list_append (searchrad, "32");
   
  label1 = gtk_label_new ("  Bitrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 8, 9);
  gtk_widget_show (label1);
  
  combo_videobit = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_videobit), vbitrate);
  combo_entry_videobitrate = GTK_COMBO (combo_videobit)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_videobitrate), "changed",
                      GTK_SIGNAL_FUNC (set_videobit), NULL);
  gtk_widget_set_usize (combo_videobit, 80, -2 );
  gtk_table_attach_defaults (GTK_TABLE (table), combo_videobit, 1, 2, 8, 9);
  gtk_widget_show (combo_videobit);

  label1 = gtk_label_new ("  Searchradius: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 9, 10);
  gtk_widget_show (label1);

  combo_searchradius = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_searchradius), searchrad);
  combo_entry_searchradius = GTK_COMBO (combo_searchradius)->entry;
  gtk_widget_set_usize (combo_searchradius, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_searchradius), "changed",
                      GTK_SIGNAL_FUNC (set_searchrad), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_searchradius, 1, 2, 9, 10);
  gtk_widget_show (combo_searchradius);

  label1 = gtk_label_new ("  MPEG level: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 10, 11);
  gtk_widget_show (label1);

  button_mpeg1 = gtk_radio_button_new_with_label (NULL, "MPEG 1");
  gtk_signal_connect (GTK_OBJECT (button_mpeg1), "toggled",
                      GTK_SIGNAL_FUNC (mpeg_option), (gpointer) "-m 1");
  gtk_table_attach_defaults (GTK_TABLE (table), button_mpeg1, 1, 2, 10, 11);
  gtk_widget_show (button_mpeg1);

  group_mpeg = gtk_radio_button_group (GTK_RADIO_BUTTON (button_mpeg1));
  button_mpeg2 = gtk_radio_button_new_with_label(group_mpeg, "MPEG 2");
  gtk_signal_connect (GTK_OBJECT (button_mpeg2), "toggled",
                      GTK_SIGNAL_FUNC (mpeg_option), (gpointer) "-m 2");
  gtk_table_attach_defaults (GTK_TABLE (table), button_mpeg2, 1, 2, 11, 12);
  group_mpeg = gtk_radio_button_group (GTK_RADIO_BUTTON (button_mpeg2));
  gtk_widget_show (button_mpeg2);

  check_sequence=gtk_check_button_new_with_label("Sequence Header\nfor every GOP");
  gtk_widget_ref (check_sequence);
  gtk_signal_connect (GTK_OBJECT (check_sequence), "toggled",
                      GTK_SIGNAL_FUNC (set_sequenceheader), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), check_sequence, 1, 2, 12, 13);
  gtk_widget_show (check_sequence);
}

/* set the noisfilter for lav2yuv */ 
void set_noisefilter (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;
 
  i = 0;
  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_noisefilter));
  i = atoi ( test );

  if ( (i >= 0) && (i <= 2) )
    encoding.noisefilter = i;

  if (verbose)
    printf("\n selected noise filer grade : %i \n", encoding.noisefilter);
}

/* set the number of LSBs dropped */
void set_drop_samples (GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_samples));

  i = atoi ( test );

  if ( (i >= 0) && (i <= 4) )
    encoding.droplsb = i;

  if (verbose)
    printf("\n dropping  as many LSB of each sample : %i \n", encoding.droplsb);
}

/* set the output format of lav2yuv */
void set_outputformat (GtkWidget *widget, gpointer data)
{
char *test;
int i;

  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_outputformat));

  outputformat = g_list_last (outputformat);

  for (i = (g_list_length (g_list_first(outputformat))-1) ; i > 0 ; i--)
    {
      if (strcmp (test,outputformat->data) == 0)
                break;

      outputformat = g_list_previous (outputformat);
    }
    encoding.outputformat = i; 

  if (verbose)
    printf("\n selected output format: %i \n", encoding.outputformat);
}

/* Set the string for the yuvscaler options */
void set_scalerstrings (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;   

  i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if (data == "SCALERINPUT")
  {
    if (strcmp(test,"as is") == 0 )
      for (i=0; i < LONGOPT; i++)
        encoding.input_use[i]='\0';
      else
        sprintf(encoding.input_use,"%s",test);

    if (verbose)
      printf("\n selected yuvscaler input window: %s \n", encoding.input_use);
  }

  if (data == "SCALERMODE")
  {
    if (strcmp(test,"as is") == 0 )
      for (i=0; i < LONGOPT; i++)
        encoding.mode_keyword[i]='\0';
    else
      sprintf(encoding.mode_keyword,"%s",test); 

    if (verbose)
      printf("\n selected yuvscaler scaling mode: %s \n", encoding.mode_keyword);
  }

  if (data == "SCALEROUTPUT")
  {
    if (strcmp(test,"as is") == 0 )
      for (i=0; i < LONGOPT; i++)
        encoding.output_size[i]='\0';
    else if ( strcmp(test,"VCD") == 0)
      sprintf(encoding.output_size,"%s", test);
    else if ( strcmp(test,"SVCD") == 0)
      sprintf(encoding.output_size,"%s", test);
    else if ( strncmp(test,"INTERLACED_",11) == 0)
      sprintf(encoding.output_size,"%s", test);
    else
      sprintf(encoding.output_size,"SIZE_%s", test);

    if (verbose)
      printf("\n selected yuvscaler output size: %s \n", encoding.output_size);
  }
}

/* Set the string for the lav2yuv active window */
void set_activewindow (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;

  i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if (strcmp(test,"as is") == 0)
    for (i=0; i < LONGOPT; i++)
       encoding.notblacksize[i]='\0';
  else
    sprintf(encoding.notblacksize,"%s", test);

  if (verbose)
    printf ("\n Set activ window to : %s\n", test);
}

/* create the layout for yuvscaler programm */
void create_yuvscaler_layout (GtkWidget *table)
{
GtkWidget *label1, *combo_scaler_input,*combo_scaler_output, *combo_scaler_mode; 
GList *input_window = NULL;
GList *output_window = NULL;

  input_window = g_list_append (input_window, "as is");
  input_window = g_list_append (input_window, "352x288+208+144");
  input_window = g_list_append (input_window, "352x240+144+120");

  yuvscalermode = g_list_append (yuvscalermode, "as is");
  yuvscalermode = g_list_append (yuvscalermode, "WIDE2STD");
  yuvscalermode = g_list_append (yuvscalermode, "WIDE2VCD");
  yuvscalermode = g_list_append (yuvscalermode, "FAST_WIDE2VCD");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_388_352_1_1");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_1_1_288_200");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_2_1_2_1");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_352_320_288_200");

  output_window = g_list_append (output_window, "as is");
  output_window = g_list_append (output_window, "VCD");
  output_window = g_list_append (output_window, "SVCD");
  output_window = g_list_append (output_window, "352x240");
  output_window = g_list_append (output_window, "INTERLACED_ODD_FIRST");
  output_window = g_list_append (output_window, "INTERLACED_EVEN_FIRST");

  label1 = gtk_label_new("  Input window: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 5, 6);
  gtk_widget_show (label1);

  combo_scaler_input = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_input),
    input_window);
  combo_entry_scalerinput = GTK_COMBO (combo_scaler_input)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scalerinput), "changed",
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALERINPUT" );
  gtk_widget_set_usize (combo_scaler_input, 130, -2 );
  gtk_table_attach_defaults (GTK_TABLE (table), combo_scaler_input, 1, 2, 5, 6);
  gtk_widget_show (combo_scaler_input);

  label1 = gtk_label_new("  Scaling mode: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 6, 7);
  gtk_widget_show (label1);

  combo_scaler_mode = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_mode), yuvscalermode);
  combo_entry_scalermode = GTK_COMBO (combo_scaler_mode)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scalermode), "changed",
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALERMODE" );
  gtk_widget_set_usize (combo_scaler_mode, 130, -2); 
  gtk_table_attach_defaults (GTK_TABLE (table), combo_scaler_mode, 1, 2, 6, 7);
  gtk_widget_show(combo_scaler_mode);
 
  label1 = gtk_label_new("  Output window: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 7, 8);
  gtk_widget_show (label1);

  combo_scaler_output = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_output),
    output_window);
  combo_entry_scaleroutput = GTK_COMBO (combo_scaler_output)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scaleroutput), "changed",  
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALEROUTPUT");
  gtk_widget_set_usize (combo_scaler_output, 80, -2 );
  gtk_table_attach_defaults (GTK_TABLE (table), combo_scaler_output, 1, 2, 7, 8);
  gtk_widget_show (combo_scaler_output);
}

/* create the lav2yuv encoding options */
void create_video_options_layout (GtkWidget *table)
{
GtkWidget *label1, *combo_active;
GList *noisefilter = NULL;
GList *droplsb = NULL;
GList *input_active_size = NULL;

   noisefilter = g_list_append (noisefilter, "0");
   noisefilter = g_list_append (noisefilter, "1");
   noisefilter = g_list_append (noisefilter, "2");

   droplsb = g_list_append (droplsb, "0");
   droplsb = g_list_append (droplsb, "1");
   droplsb = g_list_append (droplsb, "2");
   droplsb = g_list_append (droplsb, "3");

   outputformat = g_list_append (outputformat, "as is");
   outputformat = g_list_append (outputformat, "half height/width from input");
   outputformat = g_list_append (outputformat, "wide of 720 to 480 for SVCD");
   outputformat = g_list_append (outputformat, "wide of 720 to 352 for VCD");
   outputformat = g_list_append (outputformat, "720x240 to 480x480 for SVCD");

   input_active_size = g_list_append (input_active_size, "as is");
   input_active_size = g_list_append (input_active_size, "348x278+2+2");
   input_active_size = g_list_append (input_active_size, "352x210+0+39");
   input_active_size = g_list_append (input_active_size, "352x168+0+60");

  label1 = gtk_label_new ("  Noise filter: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 1, 2);
  gtk_widget_show (label1);

  combo_noisefilter = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_noisefilter), noisefilter);
  combo_entry_noisefilter = GTK_COMBO (combo_noisefilter)->entry;
  gtk_widget_set_usize (combo_noisefilter, 35, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_noisefilter), "changed",
                      GTK_SIGNAL_FUNC (set_noisefilter), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_noisefilter, 1, 2, 1, 2);
  gtk_widget_show (combo_noisefilter);

  label1 = gtk_label_new ("  Drop LSB of samples: "); 
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 2, 3);
  gtk_widget_show (label1);
  
  combo_samples = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_samples), droplsb);
  combo_entry_samples = GTK_COMBO (combo_samples)->entry;
  gtk_widget_set_usize (combo_samples, 35, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_samples), "changed",
                      GTK_SIGNAL_FUNC (set_drop_samples), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_samples, 1, 2, 2, 3);
  gtk_widget_show (combo_samples);

  label1 = gtk_label_new ("  Set active window: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 3, 4);
  gtk_widget_show (label1);

  combo_active = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_active), input_active_size);
  combo_entry_active = GTK_COMBO (combo_active)->entry;
  gtk_widget_set_usize (combo_active, 200, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_active), "changed",
    GTK_SIGNAL_FUNC (set_activewindow), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_active, 1, 2, 3, 4);
  gtk_widget_show (combo_active);

  label1 = gtk_label_new ("  Lav2yuv output format: "); 
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 4, 5);
  gtk_widget_show (label1);
  
  combo_outputformat = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_outputformat), outputformat);
  combo_entry_outputformat = GTK_COMBO (combo_outputformat)->entry;
  gtk_widget_set_usize (combo_outputformat, 200, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_outputformat), "changed",
                      GTK_SIGNAL_FUNC (set_outputformat), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_outputformat, 1, 2, 4, 5);
  gtk_widget_show (combo_outputformat);
} 

/* */
void video_callback ( GtkWidget *widget, GtkWidget *video_entry )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(video_entry));
  strcpy(enc_videofile, file);

  if (verbose)
    printf("\nEntry contents: %s\n", enc_videofile);
}

/* Set the selected video filename */
void file_ok_video ( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(video_entry),
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Select the video filename */
void create_file_video(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new ("Linux Video Studio - Video File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_video, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Create Layout for the video conversation */
void create_video (GtkWidget *table)
{
  create_video_options_layout(table);
  create_yuvscaler_layout(table);
  create_video_layout(table);
}

/* set the number of LSBs dropped */
void set_mplex_muxfmt (GtkWidget *widget, gpointer data)
{
  gchar *test;
  int i;

  i = 0;
  test = gtk_entry_get_text(GTK_ENTRY(combo_entry_muxfmt));
  muxformat = g_list_last (muxformat);

  for (i = (g_list_length (g_list_first(muxformat))-1) ; i > 0 ; i--)
    {
      if (strcmp (test,muxformat->data) == 0)
                break;

      muxformat = g_list_previous (muxformat);
    }

  encoding.muxformat = i; 

  if (verbose)
    printf("\n selected multiplexing format: %i \n", encoding.muxformat);
}

/* creating the options for mplex */
void create_mplex_options (GtkWidget *table)
{
GtkWidget *label1, *hbox1;
GtkWidget *video_select;
GtkWidget *sound_select;

   muxformat = g_list_append (muxformat, "Auto MPEG 1");
   muxformat = g_list_append (muxformat, "VCD");
   muxformat = g_list_append (muxformat, "user-rate VCD");
   muxformat = g_list_append (muxformat, "Auto MPEG 2");
   muxformat = g_list_append (muxformat, "SVCD");

  label1 = gtk_label_new ("  Multiplex format: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 1, 2);
  gtk_widget_show (label1);

  combo_muxfmt = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_muxfmt), muxformat);
  combo_entry_muxfmt = GTK_COMBO (combo_muxfmt)->entry;
  gtk_widget_set_usize (combo_muxfmt, 120, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_muxfmt), "changed",
                      GTK_SIGNAL_FUNC (set_mplex_muxfmt), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_muxfmt, 1, 2, 1, 2);
  gtk_widget_show (combo_muxfmt);

  /* sound temp file */
  label1 = gtk_label_new ("  Audio file: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 2, 3);
  gtk_widget_show (label1);

  hbox1 = gtk_hbox_new (FALSE, FALSE);

  sound_entry = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(sound_entry), enc_audiofile);
  gtk_signal_connect(GTK_OBJECT(sound_entry), "changed",
                     GTK_SIGNAL_FUNC(sound_callback), sound_entry);
  gtk_box_pack_start (GTK_BOX (hbox1), sound_entry, TRUE, TRUE, 0);
  gtk_widget_show (sound_entry);

  sound_select = gtk_button_new_with_label ("Select");
  gtk_widget_show (sound_select);
  gtk_signal_connect(GTK_OBJECT(sound_select), "clicked",
                     GTK_SIGNAL_FUNC(create_file_sound), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), sound_select, FALSE, FALSE, 0);

  gtk_table_attach_defaults (GTK_TABLE (table), hbox1, 1, 2, 2, 3);
  gtk_widget_show (hbox1);

  label1 = gtk_label_new ("  Video file : ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, 0, 1, 3, 4);
  gtk_widget_show (label1);

  hbox1 = gtk_hbox_new (FALSE, FALSE);

  video_entry = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(video_entry), enc_videofile);
  gtk_signal_connect(GTK_OBJECT(video_entry), "changed",
                     GTK_SIGNAL_FUNC(video_callback), video_entry);
  gtk_box_pack_start (GTK_BOX (hbox1), video_entry, FALSE, FALSE, 0);
  gtk_widget_show (video_entry);

  video_select = gtk_button_new_with_label ("Select");
  gtk_widget_show (video_select);
  gtk_signal_connect(GTK_OBJECT(video_select), "clicked",
                     GTK_SIGNAL_FUNC(create_file_video), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), video_select, FALSE, FALSE, 0);

  gtk_table_attach_defaults (GTK_TABLE (table), hbox1, 1, 2, 3, 4);
  gtk_widget_show (hbox1);

  remove_files_after_completion = gtk_check_button_new_with_label("Delete temp files after encoding");
  gtk_widget_ref (remove_files_after_completion);
  gtk_table_attach_defaults (GTK_TABLE (table), remove_files_after_completion, 0, 2, 4, 5);
  gtk_widget_show (remove_files_after_completion);
}

/* here all steps of the convert are started */
void encode_all (GtkWidget *widget, gpointer data)
{
  status_progress_window();
  go_all_the_way = 1;
  audio_convert();   /* using go_all_the_way only one call is needed */
}

/* the start of the encoding and mplex */
void create_buttons1 (GtkWidget *hbox1)
{
  GtkWidget *mplex_only, *do_all;
  GtkWidget *create_sound, *do_video;

  do_all = gtk_button_new_with_label (" Full create ");
  gtk_signal_connect (GTK_OBJECT (do_all), "clicked",
                      GTK_SIGNAL_FUNC (encode_all), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), do_all, TRUE, TRUE, 0);
  gtk_widget_show(do_all);

  create_sound = gtk_button_new_with_label ("Audio Only");
  gtk_signal_connect (GTK_OBJECT (create_sound), "clicked",
                      GTK_SIGNAL_FUNC (status_progress_window), NULL);
  gtk_signal_connect (GTK_OBJECT (create_sound), "clicked",
                      GTK_SIGNAL_FUNC (audio_convert), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), create_sound, TRUE, TRUE, 0);
  gtk_widget_show(create_sound);

  do_video = gtk_button_new_with_label ("Video Only");
  gtk_signal_connect (GTK_OBJECT (do_video), "clicked",
                      GTK_SIGNAL_FUNC (status_progress_window), NULL);
  gtk_signal_connect (GTK_OBJECT (do_video), "clicked",
                      GTK_SIGNAL_FUNC (video_convert), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), do_video, TRUE, TRUE, 0);
  gtk_widget_show(do_video);

  mplex_only = gtk_button_new_with_label ("Mplex Only");
  gtk_signal_connect (GTK_OBJECT (mplex_only), "clicked",
                      GTK_SIGNAL_FUNC (status_progress_window), NULL);
  gtk_signal_connect (GTK_OBJECT (mplex_only), "clicked",
                      GTK_SIGNAL_FUNC (mplex_convert), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), mplex_only, TRUE, TRUE, 0);
  gtk_widget_show(mplex_only);
}

void create_buttons2 (GtkWidget *hbox1)
{
  GtkWidget *create_vcd, *create_svcd, *set_defaults;

  create_vcd = gtk_button_new_with_label ("Set Options for VCD");
  gtk_signal_connect (GTK_OBJECT (create_vcd), "clicked",
                      GTK_SIGNAL_FUNC (do_vcd), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), create_vcd, TRUE, TRUE, 0);
  gtk_widget_show(create_vcd);

  create_svcd = gtk_button_new_with_label ("Set Options for SVCD");
  gtk_signal_connect (GTK_OBJECT (create_svcd), "clicked",
                      GTK_SIGNAL_FUNC (do_svcd), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), create_svcd, TRUE, TRUE, 0);
  gtk_widget_show(create_svcd);

  set_defaults = gtk_button_new_with_label ("Load Default Options");
  gtk_signal_connect (GTK_OBJECT (set_defaults), "clicked",
                      GTK_SIGNAL_FUNC (do_defaults), "load");
  gtk_box_pack_start (GTK_BOX (hbox1), set_defaults, TRUE, TRUE, 0);
  gtk_widget_show(set_defaults);

  set_defaults = gtk_button_new_with_label ("Save Default Options");
  gtk_signal_connect (GTK_OBJECT (set_defaults), "clicked",
                      GTK_SIGNAL_FUNC (do_defaults), "save");
  gtk_box_pack_start (GTK_BOX (hbox1), set_defaults, TRUE, TRUE, 0);
  gtk_widget_show(set_defaults);
}

/* Here the mplex layout ist created */
void create_mplex (GtkWidget *table)
{
  create_mplex_options (table);
}

/* Here some parts of status layout are done */
void create_status (GtkWidget *hbox1, GtkWidget *vbox)
{
GtkWidget *label1;

  label1 = gtk_label_new (" Executing : ");
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 5);
  gtk_widget_show (label1);

  execute_status = gtk_label_new("");
  gtk_widget_set_usize (execute_status, 550 , 40 );
  gtk_label_set_line_wrap (GTK_LABEL(execute_status), TRUE);
  gtk_label_set_justify (GTK_LABEL(execute_status), GTK_JUSTIFY_LEFT); 
  gtk_box_pack_start (GTK_BOX (hbox1), execute_status, TRUE, TRUE, 5);
  gtk_widget_show (execute_status);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_widget_show(vbox);
  hbox1 = gtk_hbox_new (FALSE, FALSE);

/*  label1 = gtk_label_new (" Progress  : ");
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 5);
  gtk_widget_show (label1);

*/
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);
}

/* Here all the work is disrtibuted, and some basic parts of the layout done */
GtkWidget *create_lavencode_layout()
{
  GtkWidget *vbox, *hbox1, *hbox, *vbox_main, *vbox1;
  GtkWidget *separator, *label, *table; 
 
  /* main box (with borders) */
  hbox = gtk_hbox_new(FALSE,0);
  vbox_main = gtk_vbox_new(FALSE,0);

  vbox = gtk_vbox_new(FALSE,5);
  hbox1 = gtk_hbox_new(FALSE,2);

  /* buttons */
  hbox1 = gtk_hbox_new (TRUE, 20);
  create_buttons1 (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, TRUE, TRUE, 0);
  gtk_widget_show (hbox1);

  hbox1 = gtk_hbox_new (TRUE, 20);
  create_buttons2 (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, TRUE, TRUE, 0);
  gtk_widget_show (hbox1);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  /* in/output layout */
  label = gtk_label_new (" Input and Output selection : ");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox1 = gtk_hbox_new (FALSE, 0);
  create_in_out (hbox1,vbox);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  /* the table containing sound/mplex layout */
  hbox1 = gtk_hbox_new (FALSE, 0);
  vbox1 = gtk_vbox_new (FALSE, 0);

  /* sound layout */
  table = gtk_table_new (2, 7, FALSE);
  label = gtk_label_new ("Manual Mode Sound encoding:");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 2, 0, 1);
  gtk_widget_show (label);
  create_sound_encoding (table);
  gtk_box_pack_start (GTK_BOX (vbox1), table, FALSE, TRUE, 5);
  gtk_widget_show(table);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox1), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  /* mplex layout */
  table = gtk_table_new (2, 5, FALSE);
  label = gtk_label_new ("Manual Mplex:");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 2, 0, 1);
  gtk_widget_show (label);

  create_mplex (table);
  gtk_box_pack_start (GTK_BOX (vbox1), table, FALSE, TRUE, 5);
  gtk_widget_show(table);

  gtk_box_pack_start (GTK_BOX (hbox1), vbox1, FALSE, FALSE, 0);
  gtk_widget_show(vbox1);

  /* separator */
  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  /* the table containing the video layout */
  table = gtk_table_new (2, 13, FALSE);

  /* video layout */
  label = gtk_label_new ("Manual Mode Video encoding:");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 2, 0, 1);
  gtk_widget_show (label);
  
  create_video (table);
  gtk_box_pack_start (GTK_BOX (hbox1), table, FALSE, FALSE, 0);
  gtk_widget_show(table);

  /* and finaly, show the hbox in the vbox */
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, TRUE, 5);
  gtk_widget_show (hbox1);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  /* the status */
  hbox1 = gtk_hbox_new (FALSE, TRUE);
  create_status (hbox1,vbox);

  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 15);
  gtk_widget_show(vbox);
  gtk_box_pack_start (GTK_BOX (vbox_main), hbox, FALSE, TRUE, 15);
  gtk_widget_show(hbox);

  /* loading the configuration */
  do_defaults(NULL, "load");

  return (vbox_main);
}



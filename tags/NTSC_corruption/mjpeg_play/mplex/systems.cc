#include "main.hh"
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

static int segment_num = 1;

FILE *system_open_init( const char *filename_pat )
{
char filename[MAXPATHLEN];

	snprintf( filename, MAXPATHLEN, filename_pat, segment_num );
	return fopen( filename, "wb" );
}

int system_file_lim_reached( FILE *cur_system_strm )
{
	struct stat stb;
    fstat(fileno(cur_system_strm), &stb);
	off_t written = stb.st_size;
	return max_system_segment_size != 0 && written > max_system_segment_size;
}

FILE *system_next_file( FILE *cur_system_strm, const char *filename_pat )
{
char filename[MAXPATHLEN];

	if( strstr( filename_pat, "%d" ) == NULL )
	{
		fprintf( stderr, 
			"Need to start new file but there is no %%d in filename pattern %s\n", filename_pat );
		exit(1); 
	}
		
	fclose(cur_system_strm);
	++segment_num;
	snprintf( filename, MAXPATHLEN, filename_pat, segment_num );
	return fopen( filename, "wb" );
}

/**************************************************************

 	 Packet payload compute how much payload a sector-sized packet with the 
	 specified headers can carry...
**************************************************************/
	
		
unsigned int packet_payload( Sys_header_struc *sys_header, 
							 Pack_struc *pack_header, 
							 int buffers, int PTSstamp, int DTSstamp )
{
  int payload = sector_size - PACKET_HEADER_SIZE ;
	if( sys_header != NULL )
		payload -= sys_header->length;
	if( opt_mpeg == 2 )
	{
	  if( buffers )
	    payload -= MPEG2_BUFFERINFO_LENGTH;

	  payload -= MPEG2_AFTER_PACKET_LENGTH_MIN;
	  if( pack_header != NULL )
	    payload -= pack_header->length;
	  if( DTSstamp )
	    payload -= DTS_PTS_TIMESTAMP_LENGTH;
	  if ( PTSstamp )
	    payload -= DTS_PTS_TIMESTAMP_LENGTH;
	}
	else
	{
		if( buffers )
			payload -= MPEG1_BUFFERINFO_LENGTH;

		payload -= MPEG1_AFTER_PACKET_LENGTH_MIN;
		if( pack_header != NULL )
			payload -= pack_header->length;
		if( DTSstamp )
			payload -= DTS_PTS_TIMESTAMP_LENGTH;
		if (PTSstamp )
			payload -= DTS_PTS_TIMESTAMP_LENGTH;
		if( DTSstamp || PTSstamp )
			payload += 1;  /* No need for nostamp marker ... */

	}
	
	return payload;
}



/*************************************************************************
    Kopiert einen TimeCode in einen Bytebuffer. Dabei wird er nach
    MPEG-Verfahren in bits aufgesplittet.

    Makes a Copy of a TimeCode in a Buffer, splitting it into bitfields
    for MPEG-1/2 DTS/PTS fields and MPEG-1 pack scr fields
*************************************************************************/

void buffer_dtspts_mpeg1scr_timecode (clockticks    timecode,
									 uint8_t  marker,
									 uint8_t **buffer)

{
	clockticks thetime_base;
    uint8_t temp;
    unsigned int msb, lsb;
     
 	/* MPEG-1 uses a 90KHz clock, extended to 300*90KHz = 27Mhz in MPEG-2 */
	/* For these fields we only encode to MPEG-1 90Khz resolution... */
	
	thetime_base = timecode /300;
	msb = (thetime_base >> 32) & 1;
	lsb = (thetime_base & 0xFFFFFFFFLL);
		
    temp = (marker << 4) | (msb <<3) |
		((lsb >> 29) & 0x6) | 1;
    *((*buffer)++)=temp;
    temp = (lsb & 0x3fc00000) >> 22;
    *((*buffer)++)=temp;
    temp = ((lsb & 0x003f8000) >> 14) | 1;
    *((*buffer)++)=temp;
    temp = (lsb & 0x7f80) >> 7;
    *((*buffer)++)=temp;
    temp = ((lsb & 0x007f) << 1) | 1;
    *((*buffer)++)=temp;

}

/*************************************************************************
    Makes a Copy of a TimeCode in a Buffer, splitting it into bitfields
    for MPEG-2 pack scr fields  which use the full 27Mhz resolution
    
    Did they *really* need to put a 27Mhz
    clock source into the system stream.  Does anyone really need it
    for their decoders?  Get real... I guess they thought it might allow
    someone somewhere to save on a proper clock circuit.
*************************************************************************/


void buffer_mpeg2scr_timecode( clockticks    timecode,
								uint8_t **buffer
							 )
{
 	clockticks thetime_base;
	unsigned int thetime_ext;
    uint8_t temp;
    unsigned int msb, lsb;
     
	thetime_base = timecode /300;
	thetime_ext =  timecode % 300;
	msb = (thetime_base>> 32) & 1;
	lsb = thetime_base & 0xFFFFFFFFLL;


      temp = (MARKER_MPEG2_SCR << 6) | (msb << 5) |
		  ((lsb >> 27) & 0x18) | 0x4 | ((lsb >> 28) & 0x3);
      *((*buffer)++)=temp;
      temp = (lsb & 0x0ff00000) >> 20;
      *((*buffer)++)=temp;
      temp = ((lsb & 0x000f8000) >> 12) | 0x4 |
             ((lsb & 0x00006000) >> 13);
      *((*buffer)++)=temp;
      temp = (lsb & 0x00001fe0) >> 5;
      *((*buffer)++)=temp;
      temp = ((lsb & 0x0000001f) << 3) | 0x4 |
             ((thetime_ext & 0x00000180) >> 7);
      *((*buffer)++)=temp;
      temp = ((thetime_ext & 0x0000007F) << 1) | 1;
      *((*buffer)++)=temp;
}



/*************************************************************************
	Create_Sector
	erstellt einen gesamten Sektor.
	Kopiert in dem Sektorbuffer auch die eventuell vorhandenen
	Pack und Sys_Header Informationen und holt dann aus der
	Inputstreamdatei einen Packet voll von Daten, die im
	Sektorbuffer abgelegt werden.

	creates a complete sector.
	Also copies Pack and Sys_Header informations into the
	sector buffer, then reads a packet full of data from
	the input stream into the sector buffer.
	
	N.b. note that we allow for situations where want to
	deliberately reduce the payload carried by stuffing.
	This allows us to deal with tricky situations where the
	header overhead of adding in additional information
	would exceed the remaining payload capacity.
*************************************************************************/

void create_sector (Sector_struc 	 *sector,
					Pack_struc	 	 *pack,
					Sys_header_struc *sys_header,
					unsigned int     max_packet_data_size,
					FILE		 	 *inputstream,
					MuxStream        &strm,
#ifdef REDUNDANT

					uint8_t 	 _type,
					uint8_t 	 _buffer_scale,
					unsigned int 	 _buffer_size,
#endif
					bool 	 buffers,
					clockticks   	 PTS,
					clockticks   	 DTS,
					uint8_t 	 timestamps
	)

{
    int i,j;
    uint8_t *index;
    uint8_t *size_offset;
	uint8_t *fixed_packet_header_end;
	uint8_t *pes_header_len_offset = 0; /* Silence compiler... */
	unsigned int target_packet_data_size;
	unsigned int actual_packet_data_size;
	int packet_data_to_read;
	int bytes_short;
	uint8_t 	 type = strm.stream_id;
	uint8_t 	 buffer_scale = strm.buffer_scale;
	unsigned int buffer_size = strm.buffer_size_code();

    index = sector->buf;

    /* soll ein Pack Header mit auf dem Sektor gespeichert werden? */
    /* Should we copy Pack Header information ? */

    if (pack != NULL)
    {
		bcopy (pack->buf, index, pack->length);
		index += pack->length;
    }

    /* soll ein System Header mit auf dem Sektor gespeichert werden? */
    /* Should we copy System Header information ? */

    if (sys_header != NULL)
    {
		bcopy (sys_header->buf, index, sys_header->length);
		index += sys_header->length;
    }

    /* konstante Packet Headerwerte eintragen */
    /* write constant packet header data */

    *(index++) = static_cast<uint8_t>(PACKET_START)>>16;
    *(index++) = static_cast<uint8_t>(PACKET_START & 0x00ffff)>>8;
    *(index++) = static_cast<uint8_t>(PACKET_START & 0x0000ff);
    *(index++) = type;	


    /* we remember this offset so we can fill in the packet size field once
	   we know the actual size... */
    size_offset = index;   
	index += 2;
 	fixed_packet_header_end = index;

	if( opt_mpeg == 1 )
	{
		/* MPEG-1: buffer information */
		if (buffers)
		{
			*(index++) = static_cast<uint8_t> (0x40 |
										  (buffer_scale << 5) | (buffer_size >> 8));
			*(index++) = static_cast<uint8_t> (buffer_size & 0xff);
		}

		/* MPEG-1: PTS, PTS & DTS, oder gar nichts? */
		/* should we write PTS, PTS & DTS or nothing at all ? */

		switch (timestamps)
		{
		case TIMESTAMPBITS_NO:
			*(index++) = MARKER_NO_TIMESTAMPS;
			break;
		case TIMESTAMPBITS_PTS:
			buffer_dtspts_mpeg1scr_timecode (PTS, MARKER_JUST_PTS, &index);
			sector->TS = PTS;
			break;
		case TIMESTAMPBITS_PTS_DTS:
			buffer_dtspts_mpeg1scr_timecode (PTS, MARKER_PTS, &index);
			buffer_dtspts_mpeg1scr_timecode (DTS, MARKER_DTS, &index);
			sector->TS = DTS;
			break;
		}
	}
	else
	{
	  	/* MPEG-2 packet syntax header flags. */
		/* First byte:
		   <1,0><PES_scrambling_control:2=0><data_alignment_ind.=0>
		   <copyright=0><original=0> */
		*(index++) = 0x80;
		/* Second byte: PTS PTS_DTS or neither?  Buffer info?
		   <PTS_DTS:2><ESCR=0><ES_rate=0>
		   <DSM_trick_mode:2=0><PES_CRC=0><PES_extension=(!!buffers)>
		*/
		*(index++) = (timestamps << 6) | (!!buffers);
		/* Third byte:
		   <PES_header_length:8> */
		pes_header_len_offset = index;  /* To fill in later! */
		index++;
		/* MPEG-2: the timecodes if required */
		switch (timestamps)
		{
		case TIMESTAMPBITS_PTS:
			buffer_dtspts_mpeg1scr_timecode(PTS, MARKER_JUST_PTS, &index);
			sector->TS = PTS;
			break;

		case TIMESTAMPBITS_PTS_DTS:
			buffer_dtspts_mpeg1scr_timecode(PTS, MARKER_PTS, &index);
			buffer_dtspts_mpeg1scr_timecode(DTS, MARKER_DTS, &index);
			sector->TS = DTS;
			break;
		}

		/* MPEG-2 The buffer information in a PES_extension */
		if( buffers )
		{
			/* MPEG-2 PES extension header
			   <PES_private_data:1=0><pack_header_field=0>
			   <program_packet_sequence_counter=0>
			   <P-STD_buffer=1><reserved:3=1><{PES_extension_flag_2=0> */
			*(index++) = static_cast<uint8_t>(0x1e);
			*(index++) = static_cast<uint8_t> (0x40 | (buffer_scale << 5) | 
										  (buffer_size >> 8));
			*(index++) = static_cast<uint8_t> (buffer_size & 0xff);
		}
	}

	/* MPEG-1, MPEG-2: data available to be filled is packet_size less header and MPEG-1 trailer... */

	target_packet_data_size = sector_size - (index - sector->buf);
	
		
	/* DEBUG: A handy consistency check when we're messing around */
#ifndef NDEBUG		
	if( target_packet_data_size != packet_payload( sys_header, pack, buffers,
												   timestamps & TIMESTAMPBITS_PTS, timestamps & TIMESTAMPBITS_DTS) )
	{ 
		printf("\nPacket size calculation error %d S%d P%d B%d %d %d!\n ", timestamps,
			   sys_header!=0, pack!=0, buffers,
			   target_packet_data_size , 
			   packet_payload( sys_header, pack, buffers,
							   timestamps & TIMESTAMPBITS_PTS, timestamps & TIMESTAMPBITS_DTS));
		exit(1);
	}
#endif	
	/* If a maximum payload data size is specified (!=0) and is smaller than the space available
	   thats all we read (the remaining space is stuffed) */
	if( max_packet_data_size != 0 && max_packet_data_size < target_packet_data_size )
	{
		packet_data_to_read = max_packet_data_size;
	}
	else
		packet_data_to_read = target_packet_data_size;


	/* MPEG-1, MPEG-2: read in available packet data ... */
    
    if (type == PADDING_STR)
    {
    	/* TODO DEBUG */
		for (j=0; j< packet_data_to_read; j++)
			*(index+j)=static_cast<uint8_t>(STUFFING_BYTE);
		actual_packet_data_size = packet_data_to_read;
    } 
    else
    {   
		actual_packet_data_size = fread (index, sizeof(uint8_t), 
										 packet_data_to_read, 
										 inputstream);
	}
	bytes_short = target_packet_data_size - actual_packet_data_size;
	
	/* Handle the situations where we don't have enough data to fill
	   the packet size fully ...
	   Small shortfalls are dealt with by stuffing, big ones by inserting
	   padding packets.
	   TODO: Certain stream types may not like zero stuffing so we may need
	   to be able to selectively insert padding packets...
	*/


	if(  bytes_short < MINIMUM_PADDING_PACKET_SIZE && bytes_short > 0 )
	{
		if (opt_mpeg == 1 )
		{
			/* MPEG-1 stuffing happens *before* header data fields. */
			memmove( fixed_packet_header_end+bytes_short, 
				     fixed_packet_header_end, 
				     actual_packet_data_size+(index-fixed_packet_header_end)
				     );
			for( j=0; j< bytes_short; ++j)
				fixed_packet_header_end[j] = static_cast<uint8_t>(STUFFING_BYTE);
		}
		else
		{
			memmove( index+bytes_short, index,  actual_packet_data_size );
			for( j=0; j< bytes_short; ++j)
				*(index+j)=static_cast<uint8_t>(STUFFING_BYTE);
		}
		index += bytes_short;
		bytes_short = 0;
	}

	
	/* MPEG-2: we now know the header length... */
	if( opt_mpeg == 2 )
	{
		*pes_header_len_offset = static_cast<uint8_t>(index-(pes_header_len_offset+1));	
	}
	index += actual_packet_data_size;	 
	/* MPEG-1, MPEG-2: Now we know that actual packet size */
	size_offset[0] = static_cast<uint8_t>((index-size_offset-2)>>8);
	size_offset[1] = static_cast<uint8_t>((index-size_offset-2)&0xff);
	
	/* The case where we have fallen short enough to allow it to be dealt with by
	   inserting a stuffing packet... */	
	if ( bytes_short != 0 )
	{
		if( zero_stuffing )
		{
			for (i = 0; i < bytes_short; i++)
				  *(index++) = static_cast<uint8_t>(0);
		}
		else
		{
		  *(index++) = static_cast<uint8_t>(PACKET_START)>>16;
		  *(index++) = static_cast<uint8_t>(PACKET_START & 0x00ffff)>>8;
		  *(index++) = static_cast<uint8_t>(PACKET_START & 0x0000ff);
		  *(index++) = PADDING_STR;
		  *(index++) = static_cast<uint8_t>((bytes_short - 6) >> 8);
		  *(index++) = static_cast<uint8_t>((bytes_short - 6) & 0xff);
		  if (opt_mpeg == 2)
		  {
			  for (i = 0; i < bytes_short - 6; i++)
				  *(index++) = static_cast<uint8_t>(STUFFING_BYTE);
		  }
		  else
		  {
			  *(index++) = 0x0F;  /* TODO: A.Stevens 2000 Why is this here? */
			  for (i = 0; i < bytes_short - 7; i++)
				  *(index++) = static_cast<uint8_t>(STUFFING_BYTE);
		  }
		}
		bytes_short = 0;
	}
	  


	/* At this point padding or stuffing will have ensured the packet
		is filled to target_packet_data_size
	*/ 
    sector->length_of_packet_data = actual_packet_data_size;

}

/*************************************************************************
	Create_Pack
	erstellt in einem Buffer die spezifischen Pack-Informationen.
	Diese werden dann spaeter von der Sector-Routine nochmals
	in dem Sektor kopiert.

	writes specifical pack header information into a buffer
	later this will be copied from the sector routine into
	the sector buffer
*************************************************************************/

void create_pack (
	Pack_struc	 *pack,
	clockticks   SCR,
	unsigned int 	 mux_rate
	)
{
    uint8_t *index;

    index = pack->buf;

    *(index++) = static_cast<uint8_t>((PACK_START)>>24);
    *(index++) = static_cast<uint8_t>((PACK_START & 0x00ff0000)>>16);
    *(index++) = static_cast<uint8_t>((PACK_START & 0x0000ff00)>>8);
    *(index++) = static_cast<uint8_t>(PACK_START & 0x000000ff);
        
	if (opt_mpeg == 2)
    {
    	/* Annoying: MPEG-2's SCR pack header time is different from
		   all the rest... */
		buffer_mpeg2scr_timecode(SCR, &index);
		*(index++) = static_cast<uint8_t>(mux_rate >> 14);
		*(index++) = static_cast<uint8_t>(0xff & (mux_rate >> 6));
		*(index++) = static_cast<uint8_t>(0x03 | ((mux_rate & 0x3f) << 2));
		*(index++) = static_cast<uint8_t>(RESERVED_BYTE << 3 | 0); /* No pack stuffing */
    }
    else
    {
		buffer_dtspts_mpeg1scr_timecode(SCR, MARKER_MPEG1_SCR, &index);
		*(index++) = static_cast<uint8_t>(0x80 | (mux_rate >> 15));
		*(index++) = static_cast<uint8_t>(0xff & (mux_rate >> 7));
		*(index++) = static_cast<uint8_t>(0x01 | ((mux_rate & 0x7f) << 1));
    }
    pack->SCR = SCR;
    pack->length = index-pack->buf;
}


/*************************************************************************
	Create_Sys_Header
	erstelle in einem Buffer die spezifischen Sys_Header
	Informationen. Diese werden spaeter von der Sector-Routine
	nochmals zum Sectorbuffer kopiert.

	writes specifical system header information into a buffer
	later this will be copied from the sector routine into
	the sector buffer
	RETURN: Length of header created...
*************************************************************************/

void create_sys_header (
	Sys_header_struc *sys_header,
	unsigned int	 rate_bound,
	int	 fixed,
	int	 CSPS,
	bool	 audio_lock,
	bool	 video_lock,
	int video_bound,
	int audio_bound,

	MuxStream      &strm1,					// Usually 1 is audio
	MuxStream      &strm2					// Usually 2 is video
	)

{
    uint8_t *index;
	uint8_t *len_index;
	int system_header_size;
    index = sys_header->buf;
	unsigned int 	 buffer1_scale = strm1.buffer_scale;
	unsigned int 	 buffer1_size = strm1.buffer_size_code();
	unsigned int 	 buffer2_scale = strm2.buffer_scale;
	unsigned int 	 buffer2_size = strm2.buffer_size_code();

    /* if we are not using both streams, we should clear some
       options here */

    *(index++) = static_cast<uint8_t>((SYS_HEADER_START)>>24);
    *(index++) = static_cast<uint8_t>((SYS_HEADER_START & 0x00ff0000)>>16);
    *(index++) = static_cast<uint8_t>((SYS_HEADER_START & 0x0000ff00)>>8);
    *(index++) = static_cast<uint8_t>(SYS_HEADER_START & 0x000000ff);

	len_index = index;	/* Skip length field for now... */
	index +=2;

    *(index++) = static_cast<uint8_t>(0x80 | (rate_bound >>15));
    *(index++) = static_cast<uint8_t>(0xff & (rate_bound >> 7));
    *(index++) = static_cast<uint8_t>(0x01 | ((rate_bound & 0x7f)<<1));
    *(index++) = static_cast<uint8_t>((audio_bound << 2)|(fixed << 1)|CSPS);
    *(index++) = static_cast<uint8_t>((audio_lock << 7)|
								 (video_lock << 6)|0x20|video_bound);

    *(index++) = static_cast<uint8_t>(RESERVED_BYTE);

    if (strm1.init) {
		*(index++) = strm1.stream_id;
		*(index++) = static_cast<uint8_t> (0xc0 |
									  (buffer1_scale << 5) | (buffer1_size >> 8));
		*(index++) = static_cast<uint8_t> (buffer1_size & 0xff);
    }

    if ( strm2.init ) {
		*(index++) = strm2.stream_id;
		*(index++) = static_cast<uint8_t> (0xc0 |
									  (buffer2_scale << 5) | (buffer2_size >> 8));
		*(index++) = static_cast<uint8_t> (buffer2_size & 0xff);
    }

	system_header_size = (index - sys_header->buf);
	len_index[0] = static_cast<uint8_t>((system_header_size-6) >> 8);
	len_index[1] = static_cast<uint8_t>((system_header_size-6) & 0xff);
	sys_header->length = system_header_size;
}

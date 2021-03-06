
/*
 *  inputstrm.c:  Base classes related to muxing out input streams into
 *                the output stream.
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
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


#include <config.h>
#include <assert.h>
#include "fastintfns.h"
#include "inputstrm.hh"
#include "outputstream.hh"

MuxStream::MuxStream() : init(false) 
{
}


void MuxStream::Init( const int strm_id, 
					  const unsigned int _buf_scale,
					  const unsigned int buf_size,
					  const unsigned int _zero_stuffing,
					  bool bufs_in_first, 
					  bool always_bufs
	) 
{
	stream_id = strm_id;
	nsec = 0;
	zero_stuffing = _zero_stuffing;
	buffer_scale = _buf_scale;
	buffer_size = buf_size;
	bufmodel.Init( buf_size );
	buffers_in_header = bufs_in_first;
	always_buffers_in_header = always_bufs;
	new_au_next_sec = true;
	init = true;
}



unsigned int 
MuxStream::BufferSizeCode()
{
	if( buffer_scale == 1 )
		return buffer_size / 1024;
	else if( buffer_scale == 0 )
		return buffer_size / 128;
	else
		assert(false);
}



ElementaryStream::ElementaryStream( IBitStream &ibs,
                                    OutputStream &into, 
									stream_kind _kind) : 
    InputStream( ibs ),
	muxinto( into ),
	kind(_kind),
    buffer_min(INT_MAX),
    buffer_max(1)
{
}


bool 
ElementaryStream::NextAU()
{
	Aunit *p_au = next();
	if( p_au != NULL )
	{
		au = p_au;
		au_unsent = p_au->length;
		return true;
	}
	else
	{
		au_unsent = 0;
		return false;
	}
}


Aunit *
ElementaryStream::Lookahead( )
{
    return aunits.lookahead();
}

unsigned int 
ElementaryStream::BytesToMuxAUEnd(unsigned int sector_transport_size)
{
	return (au_unsent/min_packet_data)*sector_transport_size +
		(au_unsent%min_packet_data)+(sector_transport_size-min_packet_data);
}

//
//  Read the (parsed and spliced) stream data from the stream
//  buffer.
//
unsigned int 
ElementaryStream::ReadStrm(uint8_t *dst, unsigned int to_read)
{
	return bs.read_buffered_bytes( dst, to_read );
}


bool ElementaryStream::MuxPossible(clockticks currentSCR)
{
	return (!RunOutComplete() &&
			bufmodel.Space() > max_packet_data);
}


void ElementaryStream::UpdateBufferMinMax()
{
    buffer_min =  buffer_min < bufmodel.Space() ? 
        buffer_min : bufmodel.Space();
    buffer_max = buffer_max > bufmodel.Space() ? 
        buffer_max : bufmodel.Space();
}



void ElementaryStream::AllDemuxed()
{
	bufmodel.Flushed();
}

void ElementaryStream::DemuxedTo( clockticks SCR )
{
	bufmodel.Cleaned( SCR );
}

bool ElementaryStream::MuxCompleted()
{
	return au_unsent == 0;
}

void 
ElementaryStream::SetSyncOffset( clockticks sync_offset )
{
	timestamp_delay = sync_offset;
}

Aunit *ElementaryStream::next()
{
    Aunit *res;
    while( AUBufferNeedsRefill() )
	{
        FillAUbuffer(FRAME_CHUNK);
	}
    res = aunits.next();
	return res;
}




/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */

#ifndef _MPEG2CODER_HH
#define _MPEG2CODER_HH

/* mpeg2coder.hh - MPEG2 packed bit / VLC syntax coding engine */

/*  (C) 2003 Andrew Stevens */

/*  This Software is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#include <config.h>
#include "mjpeg_types.h"
#include "synchrolib.h"
#include "elemstrmwriter.hh"

class Picture;

class MPEG2CoderState
{
    friend class MPEG2Coder;
private:
    ElemStrmBufferState writerState;
};    

class MPEG2Coder
{
public:
	MPEG2Coder( EncoderParams &encoder, ElemStrmWriter &writer );

	void PutUserData( const uint8_t *userdata, int len);
	void PutGopHdr(int frame, int closed_gop );
	void PutSeqEnd();
	void PutSeqHdr();
    void PutIntraBlk(Picture *picture, int16_t *blk, int cc);
    void PutNonIntraBlk(Picture *picture, int16_t *blk);
    void PutMV(int dmv, int f_code);
    void PutDMV(int dmv);
    void PutAddrInc(int addrinc);
    void PutMBType(int pict_type, int mb_type);
    void PutMotionCode(int motion_code);
    void PutCPB(int cbp);


    inline void PutBits( uint32_t val, int n) { writer.PutBits( val, n ); }
    inline int64_t BitCount() { return writer.BitCount(); }
    inline void AlignBits() { writer.AlignBits(); }
    inline bool Aligned() { return writer.Aligned(); }

    //
    // Eventually these may actually do something coding state manipulation...
    //
    inline void EmitCoded() { writer.FlushBuffer(); }
    inline MPEG2CoderState CurrentState() 
        {
            MPEG2CoderState state;
            state.writerState = writer.CurrentState();
            return state;
        }

    inline void RestoreCodingState( const MPEG2CoderState &restore) 
        {
            writer.RestoreState( restore.writerState );
        }

private:
	void PutSeqExt();
	void PutSeqDispExt();
	int FrameToTimeCode( int gop_timecode0_frame );


    inline void PutDClum(int val)
        {
            PutDC(DClumtab,val);
        }

    inline int DClum_bits( int val )
        {
            return DC_bits(DClumtab,val);
        }

    /* generate variable length code for chrominance DC coefficient */
    inline void PutDCchrom(int val)
        {
            PutDC(DCchromtab,val);
        }

    inline int DCchrom_bits(int val)
        {
            return DC_bits(DClumtab,val);
        }

    /* type definitions for variable length code table entries */
    
    typedef struct
    {
        unsigned char code; /* right justified */
        char len;
    } VLCtable;
    
    /* for codes longer than 8 bits (excluding leading zeroes) */
    typedef struct
    {
        unsigned short code; /* right justified */
        char len;
    } sVLCtable;


    void PutDC(const sVLCtable *tab, int val);
    int DC_bits(const sVLCtable *tab, int val);
    void PutACfirst(int run, int val);
    void PutAC(int run, int signed_level, int vlcformat);
    int AC_bits(int run, int signed_level, int vlcformat);
    int AddrInc_bits(int addrinc);
    int MBType_bits( int pict_type, int mb_type);
    int MotionCode_bits( int motion_code );
    int DMV_bits(int dmv);
    int CBP_bits(int cbp);

private:
	EncoderParams &encparams;
    ElemStrmWriter &writer;



    const static VLCtable addrinctab[33];
    const static VLCtable mbtypetab[3][32];
    const static VLCtable cbptable[64];
    const static VLCtable motionvectab[17];
    const static sVLCtable DClumtab[12];
    const static sVLCtable DCchromtab[12];
    const static VLCtable dct_code_tab1[2][40];
    const static VLCtable dct_code_tab2[30][5];
    const static VLCtable dct_code_tab1a[2][40];
    const static VLCtable dct_code_tab2a[30][5];

};

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
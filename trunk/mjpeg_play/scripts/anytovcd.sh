#!/bin/sh
#
#  anytovcd.sh
#
#  Copyright (C) 2004 Nicolas Boos <nicolaxx@free.fr>
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

AUD_TRACK="1"
FILTERING="none"
FILTER_TYPE="median"
PREFIX_OUT="out"
VCD_TYPE="dvd"
ENC_TOOL="mpeg2enc"
QUALITY="best"
VERSION="5"

BFR="bfr"
FFMPEG="ffmpeg"
MPEG2DEC="mpeg2dec"
MPEG2ENC="mpeg2enc"
MPLEX="mplex"
PGMTOY4M="pgmtoy4m"
SOX="sox"
TRANSCODE="transcode"
Y4MSCALER="y4mscaler"
Y4MSPATIALFILTER="y4mspatialfilter"
YUVDENOISE="yuvdenoise"
YUVFPS="yuvfps"
YUVMEDIANFILTER="yuvmedianfilter"

probe_ffmpeg_version ()
{
    echo "`${FFMPEG} 2>&1 | \
    awk '$4 == "build" {print $5}' | sed s/,// | head -1`"
}

probe_aud_fmt ()
{
    # $1 = audio input
    # $2 = audio track
    echo "`${FFMPEG} -map 0:${2} -i "$1" 2>&1 | \
    awk '/Audio:/ {print $4}' | sed s/,// | head -${2} | tail -1`"
}

probe_aud_freq ()
{
    # $1 = audio input
    # $2 = audio track
    echo "`${FFMPEG} -map 0:${2} -i "$1" 2>&1 | \
    awk '/Audio:/ {print $5}' | sed s/,// | head -${2} | tail -1`"
}

probe_vid_fps ()
{
    ${TRANSCODE} -i "$1" -c 0-1 -y yuv4mpeg -o /tmp/tmp.y4m >/dev/null 2>&1
    echo "`head -1 /tmp/tmp.y4m | awk '{print $4}' | cut -c 2-`"
    rm -f /tmp/tmp.y4m
}

probe_vid_ilace ()
{
    ${FFMPEG} -i "$1" -f yuv4mpegpipe -t 1 -y /tmp/tmp.y4m >/dev/null 2>&1
    echo "`head -1 /tmp/tmp.y4m | awk '{print $5}' | cut -c 2-`"
    rm -f /tmp/tmp.y4m
}

probe_vid_sar ()
{
    ${TRANSCODE} -i "$1" -c 0-1 -y yuv4mpeg -o /tmp/tmp.y4m >/dev/null 2>&1
    echo "`head -1 /tmp/tmp.y4m | awk '{print $6}' | cut -c 2-`"
    rm -f /tmp/tmp.y4m
}

probe_vid_size ()
{
    echo "`${FFMPEG} -i "$1" 2>&1 | \
    awk '/Video:/ {print $5}' | sed s/,// | head -1`"
}

probe_vid_fmt ()
{
    echo "`${FFMPEG} -i "$1" 2>&1 | \
    awk '/Video:/ {print $4}' | sed s/,// | head -1`"
}

show_help ()
{
    echo "----------------------------------"
    echo "-a    audio track number (1)      "
    echo "-A    force input pixel aspect ratio (X:Y)"
    echo "-b    blind mode (no video)       "
    echo "-e    video encoder tool          "
    echo "      avail. : mpeg2enc (default) "
    echo "      or ffmpeg (experimental)    "
    echo "-i    input file                  "
    echo "-I    force input ilace flag      "
    echo "      avail. : none, top_first and"
    echo "               bottom_first       "
    echo "-f    filtering method (*none*)   "
    echo "      avail.: light, medium, heavy"
    echo "-m    mute mode (no audio)        "
    echo "-n    output norm in case of input"
    echo "      with non-standard framerate "
    echo "      pal (default), ntsc or ntsc_film"
    echo "-o    output files prefix (default = \"out\")"
    echo "-p    output type (default = dvd) "
    echo "      avail. : cvd, dvd, dvd_wide, mvcd,"
    echo "               svcd and vcd       "
    echo "-q    quality (default = best)    "
    echo "      avail. : best, good, fair   "
    echo "-R    force input framerate (X:Y) "
    echo "-t    filter type to use with     "
    echo "      the chosen filter method    "
    echo "      (default = median)          "
    echo "      avail. : mean (yuvmedianfilter -f) "
    echo "               median (yuvmedianfilter)  "
    echo "               spatial (y4mspatialfilter)"
    echo "               temporal (yuvdenoise)     "
    echo "-v    script version              "
}

test_bin ()
{
    if ! which ${1} >/dev/null; then

        echo "Error : ${1} not present."
        exit 2

    fi
}

for BIN in ${FFMPEG} ${MPLEX} ${PGMTOY4M} ${TRANSCODE}; do

    test_bin ${BIN}

done

while getopts a:A:be:f:i:I:mn:o:p:q:R:t:v OPT
do
    case ${OPT} in
    a)    AUD_TRACK="${OPTARG}";;
    A)    VID_SAR_SRC="${OPTARG}";;
    b)    BLIND_MODE="1";;
    e)    ENC_TOOL="${OPTARG}";;
    i)    VIDEO_SRC="${OPTARG}"; AUDIO_SRC="${VIDEO_SRC}";;
    I)    INTERLACING="${OPTARG}";;
    f)    FILTERING="${OPTARG}";;
    m)    MUTE_MODE="1";;
    n)    OUT_NORM="${OPTARG}";;
    o)    PREFIX_OUT="${OPTARG}";;
    p)    VCD_TYPE="${OPTARG}";;
    q)    QUALITY="${OPTARG}";;
    R)    VID_FPS_SRC="${OPTARG}";;
    t)    FILTER_TYPE="${OPTARG}";;
    v)    echo "$0 version ${VERSION}"; exit 0;;
    \?)   show_help; exit 0;;
    esac
done

if test "${VIDEO_SRC}" == "" || ! test -r "${VIDEO_SRC}"; then

    echo "Error : input file not specified or not present."
    show_help
    exit 2

fi

FFMPEG_VERSION="`probe_ffmpeg_version`"

AUD_FMT_SRC="`probe_aud_fmt "${AUDIO_SRC}" ${AUD_TRACK}`"
AUD_FREQ_SRC="`probe_aud_freq "${AUDIO_SRC}" ${AUD_TRACK}`"
VID_FMT_SRC="`probe_vid_fmt "${VIDEO_SRC}"`"
VID_SIZE_SRC="`probe_vid_size "${VIDEO_SRC}"`"

# input interlacing
if test "${INTERLACING}" == "none"; then

    VID_ILACE_SRC="p"

elif test "${INTERLACING}" == "top_first"; then

    VID_ILACE_SRC="t"

elif test "${INTERLACING}" == "bottom_first"; then

    VID_ILACE_SRC="b"

else

    VID_ILACE_SRC="`probe_vid_ilace "${VIDEO_SRC}"`"

fi

# input framerate
if test "${VID_FPS_SRC}" == ""; then

    VID_FPS_SRC="`probe_vid_fps "${VIDEO_SRC}"`"

fi

# input pixel/sample aspect ratio
if test "${VID_SAR_SRC}" == ""; then

    VID_SAR_SRC="`probe_vid_sar "${VIDEO_SRC}"`"

fi

VID_SAR_N_SRC="`echo ${VID_SAR_SRC} | cut -d: -f1`"
VID_SAR_D_SRC="`echo ${VID_SAR_SRC} | cut -d: -f2`"

if test "${VID_SAR_N_SRC}" == "0" || test "${VID_SAR_D_SRC}" == "0"; then

    VID_SAR_SRC="1:1"

fi

FF_ENC="${FFMPEG} -v 0 -f yuv4mpegpipe -i /dev/stdin -bf 2"
MPEG2ENC="${MPEG2ENC} -v 0 -M 0 -R 2 -P -s -2 1 -E -5"
MPLEX="${MPLEX} -v 1"
PGMTOY4M="${PGMTOY4M} -r ${VID_FPS_SRC} -i ${VID_ILACE_SRC} -a ${VID_SAR_SRC}"
Y4MSCALER="${Y4MSCALER} -v 0 -S option=cubicCR"

# filtering method(s)
if test "${FILTERING}" == "none"; then

    YUVMEDIANFILTER=""
    YUVDENOISE=""
    Y4MSPATIALFILTER=""
    MEAN_FILTER=""

elif test "${FILTERING}" == "light"; then

    YUVMEDIANFILTER="${YUVMEDIANFILTER} -t 0"
    YUVDENOISE="${YUVDENOISE} -l 1 -t 4 -S 0"
    Y4MSPATIALFILTER="${Y4MSPATIALFILTER} -x 0 -y 0"
    MEAN_FILTER="${YUVMEDIANFILTER} -f -r 1 -R 1"

elif test "${FILTERING}" == "medium"; then

    YUVMEDIANFILTER="${YUVMEDIANFILTER} -t 1 -T 1"
    YUVDENOISE="${YUVDENOISE} -l 2 -t 6 -S 0"
    Y4MSPATIALFILTER="${Y4MSPATIALFILTER} -X 4,0.64 -Y 4,0.8"
    MEAN_FILTER="${YUVMEDIANFILTER} -f -r 1 -R 1 -w 2.667"

elif test "${FILTERING}" == "heavy"; then

    YUVMEDIANFILTER="${YUVMEDIANFILTER}"
    YUVDENOISE="${YUVDENOISE} -l 3 -t 8 -S 0"
    Y4MSPATIALFILTER="${Y4MSPATIALFILTER} -X 4,0.5 -Y 4,0.5 -x 2,0.5 -y 2,0.5"
    MEAN_FILTER="${YUVMEDIANFILTER} -f"

else

    echo "Error : the specified filtering method is inexistant."
    show_help
    exit 2

fi

# quality presets
if test "${QUALITY}" == "best"; then

    MATRICES="default"
    QUANT="4"

elif test "${QUALITY}" == "good"; then

    MATRICES="tmpgenc"
    QUANT="5"

elif test "${QUALITY}" == "fair"; then

    MATRICES="kvcd"
    QUANT="6"

else

    echo "Error : the specified quality preset is inexistant."
    show_help
    exit 2

fi

# profile(s)
if test "${VCD_TYPE}" == "cvd"; then

    FF_ENC="${FF_ENC} -vcodec mpeg2video -f rawvideo -bufsize 224"
    MPEG2ENC="${MPEG2ENC} -f 8"
    Y4MSCALER="${Y4MSCALER} -O preset=cvd"
    MPLEX="${MPLEX} -f 8"
    
    VID_FMT_OUT="m2v"
    VID_ILACE_OUT="${VID_ILACE_SRC}"
    VID_MINRATE_OUT="0"
    VID_MAXRATE_OUT="7500"

    VID_SAR_525_OUT="20:11"
    VID_SAR_625_OUT="59:27"
    VID_SIZE_525_OUT="352x480"
    VID_SIZE_625_OUT="352x576"
    
    AUD_FMT_OUT="ac3"
    AUD_BITRATE_OUT="192"
    AUD_FREQ_OUT="48000"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "dvd"; then

    FF_ENC="${FF_ENC} -target dvd -f rawvideo -dc 10"
    MPEG2ENC="${MPEG2ENC} -f 8 -D 10"
    Y4MSCALER="${Y4MSCALER} -O preset=dvd"
    MPLEX="${MPLEX} -f 8"

    VID_FMT_OUT="m2v"
    VID_ILACE_OUT="${VID_ILACE_SRC}"
    VID_MINRATE_OUT="0"
    VID_MAXRATE_OUT="8000"
    
    VID_SAR_525_OUT="10:11"
    VID_SAR_625_OUT="59:54"
    VID_SIZE_525_OUT="720x480"
    VID_SIZE_625_OUT="720x576"
    
    AUD_FMT_OUT="ac3"
    AUD_BITRATE_OUT="192"
    AUD_FREQ_OUT="48000"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "dvd_wide"; then

    FF_ENC="${FF_ENC} -target dvd -f rawvideo -dc 10"
    MPEG2ENC="${MPEG2ENC} -f 8 -D 10"
    Y4MSCALER="${Y4MSCALER} -O preset=dvd_wide"
    MPLEX="${MPLEX} -f 8"

    VID_FMT_OUT="m2v"
    VID_ILACE_OUT="${VID_ILACE_SRC}"
    VID_MINRATE_OUT="0"
    VID_MAXRATE_OUT="8000"
    
    VID_SAR_525_OUT="40:33"
    VID_SAR_625_OUT="118:81"
    VID_SIZE_525_OUT="720x480"
    VID_SIZE_625_OUT="720x576"
    
    AUD_FMT_OUT="ac3"
    AUD_BITRATE_OUT="192"
    AUD_FREQ_OUT="48000"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "mvcd"; then

    FF_ENC="${FF_ENC} -vcodec mpeg1video -f rawvideo -bufsize 40 -me_range 64"
    MPEG2ENC="${MPEG2ENC} -f 2 -g 12 -G 24 -B 256 -S 797"
    Y4MSCALER="${Y4MSCALER} -O preset=vcd"
    MPLEX="${MPLEX} -f 2 -r 1400 -V"

    MATRICES="kvcd"
    VID_FMT_OUT="m1v"
    VID_ILACE_OUT="p"
    VID_MINRATE_OUT="0"
    VID_MAXRATE_OUT="1152"
    
    VID_SAR_525_OUT="10:11"
    VID_SAR_625_OUT="59:54"
    VID_SIZE_525_OUT="352x240"
    VID_SIZE_625_OUT="352x288"
    
    AUD_FMT_OUT="mp2"
    AUD_BITRATE_OUT="224"
    AUD_FREQ_OUT="44100"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "svcd"; then

    FF_ENC="${FF_ENC} -target svcd -f rawvideo"
    MPEG2ENC="${MPEG2ENC} -f 4 -B 256 -S 797"
    Y4MSCALER="${Y4MSCALER} -O preset=svcd"
    MPLEX="${MPLEX} -f 4"

    VID_FMT_OUT="m2v"
    VID_ILACE_OUT="${VID_ILACE_SRC}"
    VID_MINRATE_OUT="0"
    VID_MAXRATE_OUT="2500"
    
    VID_SAR_525_OUT="15:11"
    VID_SAR_625_OUT="59:36"
    VID_SIZE_525_OUT="480x480"
    VID_SIZE_625_OUT="480x576"
    
    AUD_FMT_OUT="mp2"
    AUD_BITRATE_OUT="224"
    AUD_FREQ_OUT="44100"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "vcd"; then

    FF_ENC="${FF_ENC} -target vcd -f rawvideo -me_range 64"
    MPEG2ENC="${MPEG2ENC} -f 1 -B 256 -S 797"
    Y4MSCALER="${Y4MSCALER} -O preset=vcd"
    MPLEX="${MPLEX} -f 1"

    VID_FMT_OUT="m1v"
    VID_ILACE_OUT="p"
    
    VID_SAR_525_OUT="10:11"
    VID_SAR_625_OUT="59:54"
    VID_SIZE_525_OUT="352x240"
    VID_SIZE_625_OUT="352x288"
    
    AUD_FMT_OUT="mp2"
    AUD_BITRATE_OUT="224"
    AUD_FREQ_OUT="44100"
    AUD_CHANNELS_OUT="2"

else

    echo "Error : the specified output type/profile is inexistant."
    show_help
    exit 2

fi

# output filenames
VIDEO_OUT="${PREFIX_OUT}.${VID_FMT_OUT}"
AUDIO_OUT="${PREFIX_OUT}.${AUD_FMT_OUT}"
MPEG_OUT="${PREFIX_OUT}-%d.mpg"

# output interlacing
if test "${VID_ILACE_OUT}" == "b"; then

    FF_ENC="${FF_ENC} -ildct -ilme -top 0"

elif test "${VID_ILACE_OUT}" == "t"; then

    FF_ENC="${FF_ENC} -ildct -ilme -top 1"

fi

# output framerate
if test "${VID_FPS_SRC}" == "30000:1001" || test "${VID_FPS_SRC}" == "24000:1001" || test "${VID_FPS_SRC}" == "25:1"; then

    VID_FPS_OUT="${VID_FPS_SRC}"

elif test "${OUT_NORM}" == "ntsc"; then

    VID_FPS_OUT="30000:1001"

elif test "${OUT_NORM}" == "ntsc_film"; then

    VID_FPS_OUT="24000:1001"

else

    VID_FPS_OUT="25:1"

fi

# output sar and framesize for pal/ntsc
if test "${VID_FPS_OUT}" == "25:1"; then

    VID_SAR_OUT="${VID_SAR_625_OUT}"
    VID_SIZE_OUT="${VID_SIZE_625_OUT}"

else

    VID_SAR_OUT="${VID_SAR_525_OUT}"
    VID_SIZE_OUT="${VID_SIZE_525_OUT}"

fi

# pipe buffer
if which ${BFR} >/dev/null; then

    PIPE_BUFFER="${BFR} -b 16m |"

fi

# video decoder
if test "${VID_FMT_SRC}" == "mpeg2video" || test "${VID_FMT_SRC}" == "mpeg1video" && which ${MPEG2DEC} >/dev/null; then

    DECODER="${MPEG2DEC} -s -o pgmpipe \"${VIDEO_SRC}\" | ${PGMTOY4M} |"

elif test "${FFMPEG_VERSION}" -ge "4731"; then

    DECODER="${FFMPEG} -i \"${VIDEO_SRC}\" -f image2pipe -vcodec pgmyuv -y /dev/stdout | ${PGMTOY4M} |"

else

    DECODER="${FFMPEG} -i \"${VIDEO_SRC}\" -f imagepipe -img pgmyuv -y /dev/stdout | ${PGMTOY4M} |"

fi

# video filter
if test "${FILTERING}" == "none"; then

    FILTER=""

elif test "${FILTER_TYPE}" == "mean"; then
    
    test_bin ${YUVMEDIANFILTER}
    FILTER="${MEAN_FILTER} |"

elif test "${FILTER_TYPE}" == "median"; then

    test_bin ${YUVMEDIANFILTER}
    FILTER="${YUVMEDIANFILTER} |"

elif test "${FILTER_TYPE}" == "spatial"; then

    test_bin ${Y4MSPATIALFILTER}
    FILTER="${Y4MSPATIALFILTER} |"

elif test "${FILTER_TYPE}" == "temporal"; then

    test_bin ${YUVDENOISE}
    FILTER="${YUVDENOISE} |"

else

    echo "Error : the specified filter tool is not used by this script."
    show_help
    exit 2

fi

# video framerate correction
if test "${VID_FPS_SRC}" == "${VID_FPS_OUT}"; then

    FRC=""

else

    test_bin ${YUVFPS}
    FRC="${YUVFPS} -r ${VID_FPS_OUT} |"

fi

# video "deinterlacer"
if test "${VID_ILACE_SRC}" != "p" && test "${VID_ILACE_OUT}" == "p"; then

    Y4MSCALER="${Y4MSCALER} -I ilace=top_only"

fi

# video scaler
if test "${VID_SAR_SRC}" == "${VID_SAR_OUT}" && test "${VID_SIZE_SRC}" == "${VID_SIZE_OUT}"; then

    SCALER=""

else

    test_bin ${Y4MSCALER}
    SCALER="${Y4MSCALER} |"

fi

# 3:2 pulldown
if test "${VID_FPS_OUT}" == "24000:1001" && test "${VID_FMT_OUT}" == "m2v"; then

    FF_ENC="${FF_ENC}"
    MPEG2ENC="${MPEG2ENC} -p"

fi

# quantisation matrices
if test "${MATRICES}" == "kvcd"; then

    MPEG2ENC="${MPEG2ENC} -K kvcd"

elif test "${MATRICES}" == "tmpgenc"; then

    MPEG2ENC="${MPEG2ENC} -K tmpgenc"

fi

# video encoding mode
if test "${VCD_TYPE}" == "vcd"; then

    # when using -q with mpeg2enc, variable bitrate is selected.
    # it's not ok for vcds...
    MPEG2ENC="${MPEG2ENC}"

else

    FF_ENC="${FF_ENC} -maxrate ${VID_MAXRATE_OUT} -qscale ${QUANT}"
    MPEG2ENC="${MPEG2ENC} -b ${VID_MAXRATE_OUT} -q ${QUANT}"

fi

# video encoder
if test "${ENC_TOOL}" == "ffmpeg"; then

    test_bin ${FFMPEG}
    ENCODER="${FF_ENC} -y \"${VIDEO_OUT}\""

elif test "${ENC_TOOL}" == "mpeg2enc"; then

    test_bin ${MPEG2ENC}
    ENCODER="${MPEG2ENC} -o \"${VIDEO_OUT}\""

else

    echo "Error : the specified video encoder tool is not used by this script."
    show_help
    exit 2

fi

# audio resampler
if test "${AUD_FREQ_SRC}" == "${AUD_FREQ_OUT}"; then

    AUDIO_RESAMPLER=""

else

    test_bin ${SOX}
    AUDIO_RESAMPLER="${SOX} -t au /dev/stdin -r ${AUD_FREQ_OUT} -t au /dev/stdout polyphase |"

fi

# audio decoder/encoder
if test "${AUD_FMT_SRC}" == "${AUD_FMT_OUT}"; then

    AUDIO_DECODER="${FFMPEG} -map 0:${AUD_TRACK} -i \"${AUDIO_SRC}\""
    AUDIO_ENCODER="-acodec copy -y \"${AUDIO_OUT}\""

elif test "${AUD_FREQ_SRC}" == "${AUD_FREQ_OUT}"; then

    AUDIO_DECODER="${FFMPEG} -map 0:${AUD_TRACK} -i \"${AUDIO_SRC}\""
    AUDIO_ENCODER="-ab ${AUD_BITRATE_OUT} -ac ${AUD_CHANNELS_OUT} -y \"${AUDIO_OUT}\""

else

    AUDIO_DECODER="${FFMPEG} -v 0 -map 0:${AUD_TRACK} -i \"${AUDIO_SRC}\" -f au -y /dev/stdout |"
    AUDIO_ENCODER="${FFMPEG} -f au -i /dev/stdin -ab ${AUD_BITRATE_OUT} -ac ${AUD_CHANNELS_OUT} -y \"${AUDIO_OUT}\""

fi

# video (de)coding
if ! test "${BLIND_MODE}" == "1"; then

    eval "${DECODER} ${FILTER} ${FRC} ${SCALER} ${PIPE_BUFFER} ${ENCODER}"

fi

# audio (de)coding
if ! test "${MUTE_MODE}" == "1"; then

    eval "${AUDIO_DECODER} ${AUDIO_RESAMPLER} ${AUDIO_ENCODER}"

fi

# multiplexing
if ! test "${BLIND_MODE}" == "1" && ! test "${MUTE_MODE}" == "1"; then

    ${MPLEX} -o "${MPEG_OUT}" "${VIDEO_OUT}" "${AUDIO_OUT}"

fi

exit 0

/*
 * Zoran zr36057/zr36067 PCI controller driver, for the
 * Pinnacle/Miro DC10/DC10+/DC30/DC30+, Iomega Buz, Linux
 * Media Labs LML33/LML33R10.
 *
 * This part handles card-specific data and detection
 * 
 * Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
 *
 * Currently maintained by:
 *   Ronald Bultje    <rbultje@ronald.bitfreak.net>
 *   Laurent Pinchart <laurent.pinchart@skynet.be>
 *   Mailinglist      <mjpeg-users@lists.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>

#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/videodev.h>
#include <linux/spinlock.h>
#include <linux/sem.h>
#include <linux/kmod.h>

#include <linux/pci.h>
#include <linux/video_decoder.h>
#include <linux/video_encoder.h>

#include "videocodec.h"
#include "zoran.h"
#include "zoran_card.h"
#include "zoran_device.h"
#include "zoran_procfs.h"

/* temp hack */
#ifndef I2C_DRIVERID_ADV717X
#warning Using temporary hack for missing I2C driver-ID for adv717x
#define I2C_DRIVERID_ADV717X 48	/* same as in 2.5.x */
#endif
#ifndef I2C_DRIVERID_MSE3000
#warning Using temporary hack for missing I2C driver-ID for mse3000
#define I2C_DRIVERID_MSE3000 I2C_DRIVERID_EXP0
#endif
#ifndef I2C_DRIVERID_SAA7114
#warning Using temporary hack for missing I2C driver-ID for saa7114
#define I2C_DRIVERID_SAA7114 I2C_DRIVERID_EXP1
#endif
#ifndef I2C_DRIVERID_ADV7170
#warning Using temporary hack for missing I2C driver-ID for adv7170
#define I2C_DRIVERID_ADV7170 I2C_DRIVERID_EXP2
#endif
#ifndef I2C_HW_B_ZR36067
#warning Using temporary hack for missing I2C adapter-ID for zr36067
#define I2C_HW_B_ZR36067 0x20
#endif
#ifndef PCI_VENDOR_ID_IOMEGA
#warning Using temporary hack for missing Iomega vendor-ID
#define PCI_VENDOR_ID_IOMEGA 0x13ca
#endif
#ifndef PCI_DEVICE_ID_IOMEGA_BUZ
#warning Using temporary hack for missing Iomega Buz device-ID
#define PCI_DEVICE_ID_IOMEGA_BUZ 0x4231
#endif
#ifndef PCI_DEVICE_ID_MIRO_DC10PLUS
#warning Using temporary hack for missing Miro DC10+ device-ID
#define PCI_DEVICE_ID_MIRO_DC10PLUS 0x7efe
#endif
#ifndef PCI_DEVICE_ID_MIRO_DC30PLUS
#warning Using temporary hack for missing Miro DC30+ device-ID
#define PCI_DEVICE_ID_MIRO_DC30PLUS 0xd801
#endif
/* /temp hack */

extern const struct zoran_format zoran_formats[];
extern const int zoran_num_formats;

static int card[BUZ_MAX] = { -1, -1, -1, -1 };
MODULE_PARM(card, "1-4i");
MODULE_PARM_DESC(card, "The type of card");

/*
   The video mem address of the video card.
   The driver has a little database for some videocards
   to determine it from there. If your video card is not in there
   you have either to give it to the driver as a parameter
   or set in in a VIDIOCSFBUF ioctl
 */

static unsigned long vidmem = 0;	/* Video memory base address */
MODULE_PARM(vidmem, "i");

/*
   Default input and video norm at startup of the driver.
*/

static int default_input = 0;	/* 0=Composite, 1=S-Video */
MODULE_PARM(default_input, "i");
MODULE_PARM_DESC(default_input,
		 "Default input (0=Composite, 1=S-Video, 2=Internal)");

static int default_norm = 0;	/* 0=PAL, 1=NTSC 2=SECAM */
MODULE_PARM(default_norm, "i");
MODULE_PARM_DESC(default_norm, "Default norm (0=PAL, 1=NTSC, 2=SECAM)");

static int video_nr = -1;	/* /dev/videoN, -1 for autodetect */
MODULE_PARM(video_nr, "i");
MODULE_PARM_DESC(video_nr, "video device number");

/*
   Number and size of grab buffers for Video 4 Linux
   The vast majority of applications should not need more than 2,
   the very popular BTTV driver actually does ONLY have 2.
   Time sensitive applications might need more, the maximum
   is VIDEO_MAX_FRAME (defined in <linux/videodev.h>).

   The size is set so that the maximum possible request
   can be satisfied. Decrease  it, if bigphys_area alloc'd
   memory is low. If you don't have the bigphys_area patch,
   set it to 128 KB. Will you allow only to grab small
   images with V4L, but that's better than nothing.

   v4l_bufsize has to be given in KB !

*/

int v4l_nbufs = 2;
int v4l_bufsize = 128;		/* Everybody should be able to work with this setting */
MODULE_PARM(v4l_nbufs, "i");
MODULE_PARM_DESC(v4l_nbufs, "Maximum number of V4L buffers to use");
MODULE_PARM(v4l_bufsize, "i");
MODULE_PARM_DESC(v4l_bufsize, "Maximum size per V4L buffer (in kB)");

int jpg_nbufs = 32;
int jpg_bufsize = 512;		/* max size for 100% quality full-PAL frame */
MODULE_PARM(jpg_nbufs, "i");
MODULE_PARM_DESC(jpg_nbufs, "Maximum number of JPG buffers to use");
MODULE_PARM(jpg_bufsize, "i");
MODULE_PARM_DESC(jpg_bufsize, "Maximum size per JPG buffer (in kB)");

int pass_through = 0;		/* 1=Pass through TV signal when device is not used */
				/* 0=Show color bar when device is not used (LML33: only if lml33dpath=1) */
MODULE_PARM(pass_through, "i");
MODULE_PARM_DESC(pass_through,
		 "Pass TV signal through to TV-out when idling");

int debug = 0;
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level (0-4)");

MODULE_DESCRIPTION("Zoran-36057/36067 JPEG codec driver");
MODULE_AUTHOR("Serguei Miridonov");
MODULE_LICENSE("GPL");

static struct pci_device_id zr36067_pci_tbl[] = {
	{PCI_VENDOR_ID_ZORAN, PCI_DEVICE_ID_ZORAN_36057,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0}
};
MODULE_DEVICE_TABLE(pci, zr36067_pci_tbl);

#define dprintk(num, format, args...) \
	do { \
		if (debug >= num) \
			printk(format, ##args); \
	} while (0)

int zoran_num;			/* number of Buzs in use */
struct zoran zoran[BUZ_MAX];

/* videocodec bus functions ZR36060 */
static u32
zr36060_read (struct videocodec *codec,
	      u16                reg)
{
	struct zoran *zr = (struct zoran *) codec->master_data->data;
	__u32 data;

	if (post_office_wait(zr)
	    || post_office_write(zr, 0, 1, reg >> 8)
	    || post_office_write(zr, 0, 2, reg & 0xff)) {
		return -1;
	}

	data = post_office_read(zr, 0, 3) & 0xff;
	return data;
}

static void
zr36060_write (struct videocodec *codec,
	       u16                reg,
	       u32                val)
{
	struct zoran *zr = (struct zoran *) codec->master_data->data;

	if (post_office_wait(zr)
	    || post_office_write(zr, 0, 1, reg >> 8)
	    || post_office_write(zr, 0, 2, reg & 0xff)) {
		return;
	}

	post_office_write(zr, 0, 3, val & 0xff);
}

/* videocodec bus functions ZR36050 */
static u32
zr36050_read (struct videocodec *codec,
	      u16                reg)
{
	struct zoran *zr = (struct zoran *) codec->master_data->data;
	__u32 data;

	if (post_office_wait(zr)
	    || post_office_write(zr, 1, 0, reg >> 2)) {	// reg. HIGHBYTES
		return -1;
	}

	data = post_office_read(zr, 0, reg & 0x03) & 0xff;	// reg. LOWBYTES + read
	return data;
}

static void
zr36050_write (struct videocodec *codec,
	       u16                reg,
	       u32                val)
{
	struct zoran *zr = (struct zoran *) codec->master_data->data;

	if (post_office_wait(zr)
	    || post_office_write(zr, 1, 0, reg >> 2)) {	// reg. HIGHBYTES
		return;
	}

	post_office_write(zr, 0, reg & 0x03, val & 0xff);	// reg. LOWBYTES + wr. data
}

/* videocodec bus functions ZR36016 */
static u32
zr36016_read (struct videocodec *codec,
	      u16                reg)
{
	struct zoran *zr = (struct zoran *) codec->master_data->data;
	__u32 data;

	if (post_office_wait(zr)) {
		return -1;
	}

	data = post_office_read(zr, 2, reg & 0x03) & 0xff;	// read
	return data;
}

/* hack for in zoran_device.c */
void
zr36016_write (struct videocodec *codec,
	       u16                reg,
	       u32                val)
{
	struct zoran *zr = (struct zoran *) codec->master_data->data;

	if (post_office_wait(zr)) {
		return;
	}

	post_office_write(zr, 2, reg & 0x03, val & 0x0ff);	// wr. data
}

/*
 * Board specific information
 */

static void
dc10_init (struct zoran *zr)
{
	dprintk(3, KERN_DEBUG "%s: dc10_init()\n", zr->name);

	/* Pixel clock selection */
	GPIO(zr, 4, 0);
	GPIO(zr, 5, 1);
	/* Enable the video bus sync signals */
	GPIO(zr, 7, 0);
}

static void
dc10plus_init (struct zoran *zr)
{
	dprintk(3, KERN_DEBUG "%s: dc10plus_init()\n", zr->name);
}

static void
buz_init (struct zoran *zr)
{
	dprintk(3, KERN_DEBUG "%s: buz_init()\n", zr->name);
}

static void
lml33_init (struct zoran *zr)
{
	dprintk(3, KERN_DEBUG "%s: lml33_init()\n", zr->name);

	GPIO(zr, 2, 1);		// Set Composite input/output
}

// struct tvnorm {
//      u16 Wt, Wa, HStart, HSyncStart, Ht, Ha, VStart;
// };

/* The DC10 (57/16/50) uses VActive as HSync, so HStart must be 0 */

static struct tvnorm f50sqpixel = { 944, 768, 83, 880, 625, 576, 16 };
static struct tvnorm f60sqpixel = { 780, 640, 51, 716, 525, 480, 12 };
static struct tvnorm f50ccir601 = { 864, 720, 75, 804, 625, 576, 18 };
static struct tvnorm f60ccir601 = { 858, 720, 57, 788, 525, 480, 16 };

static struct tvnorm f50sqpixel_dc10 = { 944, 768, 0, 880, 625, 576, 16 };
static struct tvnorm f60sqpixel_dc10 = { 780, 640, 0, 716, 525, 480, 12 };
//static struct tvnorm f50ccir601_dc10 = { 864, 720, 0, 804, 625, 576, 18 };
//static struct tvnorm f60ccir601_dc10 = { 858, 720, 0, 788, 525, 480, 16 };

// FIXME: I cannot swap U and V in saa7114, so i do one pixel left shift in zoran
//        by Maxim Yevtyushkin <max@linuxmedialabs.com>
static struct tvnorm f50ccir601_lm33r10 =
    { 864, 720, 74, 804, 625, 576, 18 };
static struct tvnorm f60ccir601_lm33r10 =
    { 858, 720, 56, 788, 525, 480, 16 };

static struct card_info zoran_cards[NUM_CARDS] = {
	{
		.type = DC10_old,
		.name = "DC10(old)",
		.vendor_id = -1,	/* apparently, the DC10(old) doesn't */
		.device_id = -1,	/* have subsystem IDs *at all*! */
		.i2c_decoder = I2C_DRIVERID_VPX32XX,
		.i2c_dec_name = "vpx3220",
		.i2c_encoder = I2C_DRIVERID_MSE3000,
		.i2c_enc_name = "mse3000",
		.video_codec = CODEC_TYPE_ZR36050,
		.video_codec_name = "zr36050",
		.video_vfe = CODEC_TYPE_ZR36016,
		.video_vfe_name = "zr36016",
		.audio_chip = -1,

		.inputs = 3,
		.input = {
			{ 1, "Composite" },
			{ 2, "S-Video" },
			{ 0, "Internal/comp" }
		},
		.norms = 3,
#if 0
		.tvn = {
			&f50ccir601_dc10,
			&f60ccir601_dc10,
			&f50ccir601_dc10
		},
#endif
		.tvn = {
			&f50sqpixel_dc10,
			&f60sqpixel_dc10,
			&f50sqpixel_dc10
		},
		.jpeg_int = 0,
		.vsync_int = ZR36057_ISR_GIRQ1,
		.gpio = { 2, 1, -1, 3, 7, 0, 4, 5 },
		.gpio_pol = { 0, 0, 0, 1, 0, 0, 0, 0 },
		.gpcs = { -1, 0 },
		.vfe_pol = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.gws_not_connected = 0,
		.init = &dc10_init,
	}, {
		.type = DC10_new,
		.name = "DC10(new)",
		.vendor_id = -1,	/* zoran revision too old? */
		.device_id = -1,
		.i2c_decoder = I2C_DRIVERID_SAA7110,
		.i2c_dec_name = "saa7110",
		.i2c_encoder = I2C_DRIVERID_ADV717X,
		.i2c_enc_name = "adv7175",
		.video_codec = CODEC_TYPE_ZR36060,
		.video_codec_name = "zr36060",
		.video_vfe = -1,
		.audio_chip = -1,

		.inputs = 3,
		.input = {
				{ 0, "Composite" },
				{ 7, "S-Video" },
				{ 5, "Internal/comp" }
			},
		.norms = 3,
		.tvn = {
				&f50sqpixel,
				&f60sqpixel,
				&f50sqpixel},
		.jpeg_int = ZR36057_ISR_GIRQ0,
		.vsync_int = ZR36057_ISR_GIRQ1,
		.gpio = { 3, 0, 6, 1, 2, -1, 4, 5 },
		.gpio_pol = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.gpcs = { -1, 1},
		.vfe_pol = { 1, 1, 1, 1, 0, 0, 0, 0 },
		.gws_not_connected = 0,
		.init = &dc10plus_init,
	}, {
		.type = DC10plus,
		.name = "DC10plus",
		.vendor_id = PCI_VENDOR_ID_MIRO,
		.device_id = PCI_DEVICE_ID_MIRO_DC10PLUS,
		.i2c_decoder = I2C_DRIVERID_SAA7110,
		.i2c_dec_name = "saa7110",
		.i2c_encoder = I2C_DRIVERID_ADV717X,
		.i2c_enc_name = "adv7175",
		.video_codec = CODEC_TYPE_ZR36060,
		.video_codec_name = "zr36060",
		.video_vfe = -1,
		.audio_chip = -1,

		.inputs = 3,
		.input = {
			{ 0, "Composite" },
			{ 7, "S-Video" },
			{ 5, "Internal/comp" }
		},
		.norms = 3,
		.tvn = {
			&f50sqpixel,
			&f60sqpixel,
			&f50sqpixel
		},
		.jpeg_int = ZR36057_ISR_GIRQ0,
		.vsync_int = ZR36057_ISR_GIRQ1,
		.gpio = { 3, 0, 6, 1, 2, -1, 4, 5 },
		.gpio_pol = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.gpcs = { -1, 1 },
		.vfe_pol = { 1, 1, 1, 1, 0, 0, 0, 0 },
		.gws_not_connected = 0,
		.init = &dc10plus_init,
	}, {
		.type = DC30,
		.name = "DC30",
		.vendor_id = -1,	/* zoran revision too old? */
		.device_id = -1,
		.i2c_decoder = I2C_DRIVERID_VPX32XX,
		.i2c_dec_name = "vpx3220",
		.i2c_encoder = I2C_DRIVERID_ADV717X,
		.i2c_enc_name = "adv7175",
		.video_codec = CODEC_TYPE_ZR36050,
		.video_codec_name = "zr36050",
		.video_vfe = CODEC_TYPE_ZR36016,
		.video_vfe_name = "zr36016",
		.audio_chip = -1,

		.inputs = 3,
		.input = {
			{ 1, "Composite" },
			{ 2, "S-Video" },
			{ 0, "Internal/comp" }
		},
		.norms = 3,
#if 	0
		.tvn = {
			&f50ccir601_dc10,
			&f60ccir601_dc10,
			&f50ccir601_dc10
		},
#endif
		.tvn = {
			&f50sqpixel_dc10,
			&f60sqpixel_dc10,
			&f50sqpixel_dc10
		},
		.jpeg_int = 0,
		.vsync_int = ZR36057_ISR_GIRQ1,
		.gpio = { 2, 1, -1, 3, 7, 0, 4, 5 },
		.gpio_pol = { 0, 0, 0, 1, 0, 0, 0, 0 },
		.gpcs = { -1, 0 },
		.vfe_pol = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.gws_not_connected = 0,
		.init = &dc10_init,
	}, {
		.type = DC30plus,
		.name = "DC30plus",
		.vendor_id = PCI_VENDOR_ID_MIRO,
		.device_id = PCI_DEVICE_ID_MIRO_DC30PLUS,
		.i2c_decoder = I2C_DRIVERID_VPX32XX,
		.i2c_dec_name = "vpx3220",
		.i2c_encoder = I2C_DRIVERID_ADV717X,
		.i2c_enc_name = "adv7175",
		.video_codec = CODEC_TYPE_ZR36050,
		.video_codec_name = "zr36050",
		.video_vfe = CODEC_TYPE_ZR36016,
		.video_vfe_name = "zr36016",
		.audio_chip = -1,

		.inputs = 3,
		.input = {
			{ 1, "Composite" },
			{ 2, "S-Video" },
			{ 0, "Internal/comp" }
		},
		.norms = 3,
#if 0
		vn = {
			&f50ccir601_dc10,
			&f60ccir601_dc10,
			&f50ccir601_dc10
		},
#endif
		.tvn = {
			&f50sqpixel_dc10,
			&f60sqpixel_dc10,
			&f50sqpixel_dc10
		},
		.jpeg_int = 0,
		.vsync_int = ZR36057_ISR_GIRQ1,
		.gpio = { 2, 1, -1, 3, 7, 0, 4, 5 },
		.gpio_pol = { 0, 0, 0, 1, 0, 0, 0, 0 },
		.gpcs = { -1, 0 },
		.vfe_pol = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.gws_not_connected = 0,
		.init = &dc10_init,
	}, {
		.type = LML33,
		.name = "LML33",
		.vendor_id = -1,
		.device_id = -1,
		.i2c_decoder = I2C_DRIVERID_BT819,
		.i2c_dec_name = "bt819",
		.i2c_encoder = I2C_DRIVERID_BT856,
		.i2c_enc_name = "bt856",
		.video_codec = CODEC_TYPE_ZR36060,
		.video_codec_name = "zr36060",
		.video_vfe = -1,
		.audio_chip = -1,

		.inputs = 2,
		.input = {
			{ 0, "Composite" },
			{ 7, "S-Video" }
		},
		.norms = 2,
		.tvn = {
			&f50ccir601,
			&f60ccir601,
			NULL
		},
		.jpeg_int = ZR36057_ISR_GIRQ1,
		.vsync_int = ZR36057_ISR_GIRQ0,
		.gpio = { 1, -1, 3, 5, 7, -1, -1, -1 },
		.gpio_pol = { 0, 0, 0, 0, 1, 0, 0, 0 },
		.gpcs = { 3, 1 },
		.vfe_pol = { 1, 0, 0, 0, 0, 1, 0, 0 },
		.gws_not_connected = 1,
		.init = &lml33_init,
	}, {
		.type = LML33R10,
		.name = "LML33R10",
		.vendor_id = -1,
		.device_id = -1,
		.i2c_decoder = I2C_DRIVERID_SAA7114,
		.i2c_dec_name = "saa7114",
		.i2c_encoder = I2C_DRIVERID_ADV7170,
		.i2c_enc_name = "adv7170",
		.video_codec = CODEC_TYPE_ZR36060,
		.video_codec_name = "zr36060",
		.video_vfe = -1,
		.audio_chip = -1,

		.inputs = 2,
		.input = {
			{ 0, "Composite" },
			{ 7, "S-Video" }
		},
		.norms = 2,
		.tvn = {
			&f50ccir601_lm33r10,
			&f60ccir601_lm33r10,
			NULL
		},
		.jpeg_int = ZR36057_ISR_GIRQ1,
		.vsync_int = ZR36057_ISR_GIRQ0,
		.gpio = { 1, -1, 3, 5, 7, -1, -1, -1 },
		.gpio_pol = {0, 0, 0, 0, 1, 0, 0, 0 },
		.gpcs = { 3, 1 },
		.vfe_pol = { 1, 0, 0, 0, 0, 1, 0, 0 },
		.gws_not_connected = 1,
		.init = &lml33_init,
	}, {
		.type = BUZ,
		.name = "Buz",
		.vendor_id = PCI_VENDOR_ID_IOMEGA,
		.device_id = PCI_DEVICE_ID_IOMEGA_BUZ,
		.i2c_decoder = I2C_DRIVERID_SAA7111A,
		.i2c_dec_name = "saa7111",
		.i2c_encoder = I2C_DRIVERID_SAA7185B,
		.i2c_enc_name = "saa7185",
		.video_codec = CODEC_TYPE_ZR36060,
		.video_codec_name = "zr36060",
		.video_vfe = -1,
		.audio_chip = -1,

		.inputs = 2,
		.input = {
			{ 3, "Composite" },
			{ 7, "S-Video" }
		},
		.norms = 3,
		.tvn = {
			&f50ccir601,
			&f60ccir601,
			&f50ccir601
		},
		.jpeg_int = ZR36057_ISR_GIRQ1,
		.vsync_int = ZR36057_ISR_GIRQ0,
		.gpio = { 1, -1, 3, -1, -1, -1, -1, -1 },
		.gpio_pol = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.gpcs = { 3, 1 },
		.vfe_pol = { 1, 1, 0, 0, 0, 1, 0, 0 },
		.gws_not_connected = 1,
		.init = &buz_init,
	}
};

/*
 * I2C functions
 */
/* software I2C functions */
static int
zoran_i2c_getsda (void *data)
{
	struct zoran *zr = (struct zoran *) data;
	return (btread(ZR36057_I2CBR) >> 1) & 1;
}

static int
zoran_i2c_getscl (void *data)
{
	struct zoran *zr = (struct zoran *) data;
	return btread(ZR36057_I2CBR) & 1;
}

static void
zoran_i2c_setsda (void *data,
		  int   state)
{
	struct zoran *zr = (struct zoran *) data;
	if (state)
		zr->i2cbr |= 2;
	else
		zr->i2cbr &= ~2;
	btwrite(zr->i2cbr, ZR36057_I2CBR);
}

static void
zoran_i2c_setscl (void *data,
		  int   state)
{
	struct zoran *zr = (struct zoran *) data;
	if (state)
		zr->i2cbr |= 1;
	else
		zr->i2cbr &= ~1;
	btwrite(zr->i2cbr, ZR36057_I2CBR);
}

static void
zoran_i2c_inc_use (struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void
zoran_i2c_dec_use (struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static int
zoran_i2c_client_register (struct i2c_client *client)
{
	struct zoran *zr = (struct zoran *) (client->adapter->data);
	int res = 0;

	dprintk(2,
		KERN_DEBUG "%s: i2c_client_register() - driver id = %d\n",
		zr->name, client->driver->id);

	down(&zr->resource_lock);

	if (zr->user > 0) {
		res = -EBUSY;
		goto clientreg_unlock_and_return;
	}

	if (client->driver->id == zr->card->i2c_decoder)
		zr->decoder = client;
	else if (client->driver->id == zr->card->i2c_encoder)
		zr->encoder = client;
	else
		return -ENODEV;

clientreg_unlock_and_return:
	up(&zr->resource_lock);

	return res;
}

static int
zoran_i2c_client_unregister (struct i2c_client *client)
{
	struct zoran *zr = (struct zoran *) client->adapter->data;
	int res = 0;

	dprintk(2, KERN_DEBUG "%s: i2c_client_unregister()\n", zr->name);

	down(&zr->resource_lock);

	if (zr->user > 0) {
		res = -EBUSY;
		goto clientunreg_unlock_and_return;
	}

	/* try to locate it */
	if (client == zr->encoder) {
		zr->encoder = NULL;
	} else if (client == zr->decoder) {
		zr->decoder = NULL;
		snprintf(zr->name, sizeof(zr->name), "MJPEG[%d]", zr->id);
	}
clientunreg_unlock_and_return:
	up(&zr->resource_lock);
	return res;
}

static struct i2c_algo_bit_data zoran_i2c_bit_data_template = {
	.setsda = zoran_i2c_setsda,
	.setscl = zoran_i2c_setscl,
	.getsda = zoran_i2c_getsda,
	.getscl = zoran_i2c_getscl,
	.udelay = 10,
	.mdelay = 0,
	.timeout = 100,
};

static struct i2c_adapter zoran_i2c_adapter_template = {
	.name = "zr36057",
	.id = I2C_HW_B_ZR36067,
	.algo = NULL,
	.inc_use = zoran_i2c_inc_use,
	.dec_use = zoran_i2c_dec_use,
	.client_register = zoran_i2c_client_register,
	.client_unregister = zoran_i2c_client_unregister,
};

static int
zoran_register_i2c (struct zoran *zr)
{
	memcpy(&zr->i2c_algo, &zoran_i2c_bit_data_template,
	       sizeof(struct i2c_algo_bit_data));
	zr->i2c_algo.data = zr;
	memcpy(&zr->i2c_adapter, &zoran_i2c_adapter_template,
	       sizeof(struct i2c_adapter));
	strcpy(zr->i2c_adapter.name, zr->name);
	zr->i2c_adapter.data = zr;
	zr->i2c_adapter.algo_data = &zr->i2c_algo;
	return i2c_bit_add_bus(&zr->i2c_adapter);
}

static void
zoran_unregister_i2c (struct zoran *zr)
{
	i2c_bit_del_bus((&zr->i2c_adapter));
}

/* Check a zoran_params struct for correctness, insert default params */

int
zoran_check_jpg_settings (struct zoran              *zr,
			  struct zoran_jpg_settings *settings)
{
	int err = 0, err0 = 0;
	dprintk(4,
		KERN_DEBUG
		"%s: check_jpg_settings() - dec: %d, Hdcm: %d, Vdcm: %d, Tdcm: %d\n",
		zr->name, settings->decimation, settings->HorDcm,
		settings->VerDcm, settings->TmpDcm);
	dprintk(4,
		KERN_DEBUG
		"%s: check_jpg_settings() - x: %d, y: %d, w: %d, y: %d\n",
		zr->name, settings->img_x, settings->img_y,
		settings->img_width, settings->img_height);
	/* Check decimation, set default values for decimation = 1, 2, 4 */
	switch (settings->decimation) {
	case 1:

		settings->HorDcm = 1;
		settings->VerDcm = 1;
		settings->TmpDcm = 1;
		settings->field_per_buff = 2;
		settings->img_x = 0;
		settings->img_y = 0;
		settings->img_width = BUZ_MAX_WIDTH;
		settings->img_height = BUZ_MAX_HEIGHT / 2;
		break;
	case 2:

		settings->HorDcm = 2;
		settings->VerDcm = 1;
		settings->TmpDcm = 2;
		settings->field_per_buff = 1;
		settings->img_x = (BUZ_MAX_WIDTH == 720) ? 8 : 0;
		settings->img_y = 0;
		settings->img_width =
		    (BUZ_MAX_WIDTH == 720) ? 704 : BUZ_MAX_WIDTH;
		settings->img_height = BUZ_MAX_HEIGHT / 2;
		break;
	case 4:

		if (zr->card->type == DC10_new) {
			dprintk(1,
				KERN_DEBUG
				"%s: check_jpg_settings() - HDec by 4 is not supported on the DC10\n",
				zr->name);
			err0++;
			break;
		}

		settings->HorDcm = 4;
		settings->VerDcm = 2;
		settings->TmpDcm = 2;
		settings->field_per_buff = 1;
		settings->img_x = (BUZ_MAX_WIDTH == 720) ? 8 : 0;
		settings->img_y = 0;
		settings->img_width =
		    (BUZ_MAX_WIDTH == 720) ? 704 : BUZ_MAX_WIDTH;
		settings->img_height = BUZ_MAX_HEIGHT / 2;
		break;
	case 0:

		/* We have to check the data the user has set */

		if (settings->HorDcm != 1 && settings->HorDcm != 2 &&
		    (zr->card->type == DC10_new || settings->HorDcm != 4))
			err0++;
		if (settings->VerDcm != 1 && settings->VerDcm != 2)
			err0++;
		if (settings->TmpDcm != 1 && settings->TmpDcm != 2)
			err0++;
		if (settings->field_per_buff != 1 &&
		    settings->field_per_buff != 2)
			err0++;
		if (settings->img_x < 0)
			err0++;
		if (settings->img_y < 0)
			err0++;
		if (settings->img_width < 0)
			err0++;
		if (settings->img_height < 0)
			err0++;
		if (settings->img_x + settings->img_width > BUZ_MAX_WIDTH)
			err0++;
		if (settings->img_y + settings->img_height >
		    BUZ_MAX_HEIGHT / 2)
			err0++;
		if (settings->HorDcm && settings->VerDcm) {
			if (settings->img_width %
			    (16 * settings->HorDcm) != 0)
				err0++;
			if (settings->img_height %
			    (8 * settings->VerDcm) != 0)
				err0++;
		}

		if (err0) {
			dprintk(1,
				KERN_ERR
				"%s: check_jpg_settings() - error in params for decimation = 0\n",
				zr->name);
			err++;
		}
		break;
	default:
		dprintk(1,
			KERN_ERR
			"%s: check_jpg_settings() - decimation = %d, must be 0, 1, 2 or 4\n",
			zr->name, settings->decimation);
		err++;
		break;
	}

	if (settings->jpg_comp.quality > 100)
		settings->jpg_comp.quality = 100;
	if (settings->jpg_comp.quality < 5)
		settings->jpg_comp.quality = 5;
	if (settings->jpg_comp.APPn < 0)
		settings->jpg_comp.APPn = 0;
	if (settings->jpg_comp.APPn > 15)
		settings->jpg_comp.APPn = 15;
	if (settings->jpg_comp.APP_len < 0)
		settings->jpg_comp.APP_len = 0;
	if (settings->jpg_comp.APP_len > 60)
		settings->jpg_comp.APP_len = 60;
	if (settings->jpg_comp.COM_len < 0)
		settings->jpg_comp.COM_len = 0;
	if (settings->jpg_comp.COM_len > 60)
		settings->jpg_comp.COM_len = 60;
	if (err)
		return -EINVAL;
	return 0;
}

void
zoran_open_init_params (struct zoran *zr)
{
	int i;

	/* User must explicitly set a window */
	memset(&zr->overlay_settings, 0,
	       sizeof(struct zoran_overlay_settings));
	zr->overlay_settings.is_set = 0;
	zr->overlay_mask = NULL;
	zr->overlay_active = ZORAN_FREE;

	zr->v4l_memgrab_active = 0;
	zr->v4l_overlay_active = 0;
	zr->v4l_grab_frame = NO_GRAB_ACTIVE;
	zr->v4l_grab_seq = 0;
	zr->v4l_settings.width = 192;
	zr->v4l_settings.height = 144;
	zr->v4l_settings.format = &zoran_formats[4];	/* YUY2 - YUV-4:2:2 packed */
	zr->v4l_settings.bytesperline =
	    zr->v4l_settings.width *
	    ((zr->v4l_settings.format->depth + 7) / 8);

	/* DMA ring stuff for V4L */
	zr->v4l_pend_tail = 0;
	zr->v4l_pend_head = 0;
	zr->v4l_buffers.active = ZORAN_FREE;
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		zr->v4l_buffers.buffer[i].state = BUZ_STATE_USER;	/* nothing going on */
	}
	zr->v4l_buffers.allocated = 0;

	for (i = 0; i < BUZ_MAX_FRAME; i++) {
		zr->jpg_buffers.buffer[i].state = BUZ_STATE_USER;	/* nothing going on */
	}
	zr->jpg_buffers.active = ZORAN_FREE;
	zr->jpg_buffers.allocated = 0;
	/* Set necessary params and call zoran_check_jpg_settings to set the defaults */
	zr->jpg_settings.decimation = 1;
	zr->jpg_settings.jpg_comp.quality = 50;	/* default compression factor 8 */
	if (zr->card->type != BUZ)
		zr->jpg_settings.odd_even = 1;
	else
		zr->jpg_settings.odd_even = 0;
	zr->jpg_settings.jpg_comp.APPn = 0;
	zr->jpg_settings.jpg_comp.APP_len = 0;	/* No APPn marker */
	memset(zr->jpg_settings.jpg_comp.APP_data, 0,
	       sizeof(zr->jpg_settings.jpg_comp.APP_data));
	zr->jpg_settings.jpg_comp.COM_len = 0;	/* No COM marker */
	memset(zr->jpg_settings.jpg_comp.COM_data, 0,
	       sizeof(zr->jpg_settings.jpg_comp.COM_data));
	zr->jpg_settings.jpg_comp.jpeg_markers =
	    JPEG_MARKER_DHT | JPEG_MARKER_DQT;
	i = zoran_check_jpg_settings(zr, &zr->jpg_settings);
	if (i)
		dprintk(1,
			KERN_ERR
			"%s: zoran_open_init_params() internal error\n",
			zr->name);

	clear_interrupt_counters(zr);
	zr->testing = 0;
}

static void __devinit
test_interrupts (struct zoran *zr)
{
	int timeout, icr;

	clear_interrupt_counters(zr);

	zr->testing = 1;
	icr = btread(ZR36057_ICR);
	btwrite(0x78000000 | ZR36057_ICR_IntPinEn, ZR36057_ICR);
	timeout = interruptible_sleep_on_timeout(&zr->test_q, 1 * HZ);
	btwrite(0, ZR36057_ICR);
	btwrite(0x78000000, ZR36057_ISR);
	zr->testing = 0;
	dprintk(5, KERN_INFO "%s: Testing interrupts...\n", zr->name);
	if (timeout) {
		dprintk(1, ": time spent: %d\n", 1 * HZ - timeout);
	}
	if (debug > 1)
		print_interrupts(zr);
	btwrite(icr, ZR36057_ICR);
}

static int __devinit
zr36057_init (struct zoran *zr)
{
	unsigned long mem;
	unsigned mem_needed;
	int j;
	int two = 2;
	int zero = 0;

	dprintk(1,
		KERN_INFO
		"%s: zr36057_init() - initializing card[%d], zr=%p\n",
		zr->name, zr->id, zr);

	/* default setup of all parameters which will persist between opens */
	zr->user = 0;

	init_waitqueue_head(&zr->v4l_capq);
	init_waitqueue_head(&zr->jpg_capq);
	init_waitqueue_head(&zr->test_q);
	zr->jpg_buffers.allocated = 0;
	zr->v4l_buffers.allocated = 0;

	zr->buffer.base = (void *) vidmem;
	zr->buffer.width = 0;
	zr->buffer.height = 0;
	zr->buffer.depth = 0;
	zr->buffer.bytesperline = 0;

	/* Avoid nonsense settings from user for default input/norm */
	if (default_norm < VIDEO_MODE_PAL &&
	    default_norm > VIDEO_MODE_SECAM)
		default_norm = VIDEO_MODE_PAL;
	zr->norm = default_norm;
	if (!(zr->timing = zr->card->tvn[zr->norm])) {
		dprintk(1,
			KERN_WARNING
			"%s: zr36057_init() - default TV standard not supported by hardware. PAL will be used.\n",
			zr->name);
		zr->norm = VIDEO_MODE_PAL;
		zr->timing = zr->card->tvn[zr->norm];
	}

	zr->input = default_input = (default_input ? 1 : 0);

	/* Should the following be reset at every open ? */
	zr->hue = 32768;
	zr->contrast = 32768;
	zr->saturation = 32768;
	zr->brightness = 32768;

	/* default setup (will be repeated at every open) */
	zoran_open_init_params(zr);

	/* allocate memory *before* doing anything to the hardware
	 * in case allocation fails */
	mem_needed = BUZ_NUM_STAT_COM * 4;
	mem = (unsigned long) kmalloc(mem_needed, GFP_KERNEL);
	if (!mem) {
		dprintk(1,
			KERN_ERR
			"%s: zr36057_init() - kmalloc (STAT_COM) failed\n",
			zr->name);
		return -ENOMEM;
	}
	memset((void *) mem, 0, mem_needed);
	zr->stat_com = (u32 *) mem;
	for (j = 0; j < BUZ_NUM_STAT_COM; j++) {
		zr->stat_com[j] = 1;	/* mark as unavailable to zr36057 */
	}

	/*
	 *   Now add the template and register the device unit.
	 */
	memcpy(&zr->video_dev, &zoran_template, sizeof(zoran_template));
	strcpy(zr->video_dev.name, zr->name);
	if (video_register_device
	    (&zr->video_dev, VFL_TYPE_GRABBER, video_nr) < 0) {
		zoran_unregister_i2c(zr);
		kfree((void *) zr->stat_com);
		return -1;
	}

	zoran_init_hardware(zr);
	if (debug > 2)
		detect_guest_activity(zr);
	test_interrupts(zr);
	if (!pass_through) {
		decoder_command(zr, DECODER_ENABLE_OUTPUT, &zero);
		encoder_command(zr, ENCODER_SET_INPUT, &two);
	}

	zr->zoran_proc = NULL;
	zr->initialized = 1;
	return 0;
}

static void
zoran_release (struct zoran *zr)
{
	if (!zr->initialized)
		return;
	/* unregister videocodec bus */
	if (zr->codec) {
		struct videocodec_master *master = zr->codec->master_data;
		videocodec_detach(zr->codec);
		if (master)
			kfree(master);
	}
	if (zr->vfe) {
		struct videocodec_master *master = zr->vfe->master_data;
		videocodec_detach(zr->vfe);
		if (master)
			kfree(master);
	}

	/* unregister i2c bus */
	zoran_unregister_i2c(zr);
	/* disable PCI bus-mastering */
	zoran_set_pci_master(zr, 0);
	/* put chip into reset */
	btwrite(0, ZR36057_SPGPPCR);
	free_irq(zr->pci_dev->irq, zr);
	/* unmap and free memory */
	kfree((void *) zr->stat_com);
	zoran_proc_cleanup(zr);
	iounmap(zr->zr36057_mem);
	video_unregister_device(&zr->video_dev);
}

static struct videocodec_master * __devinit
zoran_setup_videocodec (struct zoran *zr,
			int           type)
{
	struct videocodec_master *m = NULL;

	m = kmalloc(sizeof(struct videocodec_master), GFP_KERNEL);
	if (!m) {
		dprintk(1,
			KERN_ERR
			"%s: zoran_setup_videocodec() - no memory\n",
			zr->name);
		return m;
	}

	m->magic = 0L; /* magic not used */
	m->type = VID_HARDWARE_ZR36067;
	m->flags = CODEC_FLAG_ENCODER | CODEC_FLAG_DECODER;
	strncpy(m->name, zr->name, sizeof(m->name));
	m->data = zr;

	switch (type)
	{
	case CODEC_TYPE_ZR36060:
		m->readreg = zr36060_read;
		m->writereg = zr36060_write;
		m->flags |= CODEC_FLAG_JPEG | CODEC_FLAG_VFE;
		break;
	case CODEC_TYPE_ZR36050:
		m->readreg = zr36050_read;
		m->writereg = zr36050_write;
		m->flags |= CODEC_FLAG_JPEG;
		break;
	case CODEC_TYPE_ZR36016:
		m->readreg = zr36016_read;
		m->writereg = zr36016_write;
		m->flags |= CODEC_FLAG_VFE;
		break;
	}

	return m;
}

/*
 *   Scan for a Buz card (actually for the PCI contoler ZR36057),
 *   request the irq and map the io memory
 */
static int __devinit
find_zr36057 (void)
{
	unsigned char latency, need_latency;
	struct zoran *zr;
	struct pci_dev *dev = NULL;
	int result;
	struct videocodec_master *master_vfe = NULL;
	struct videocodec_master *master_codec = NULL;
	int card_num;

	zoran_num = 0;
	while (zoran_num < BUZ_MAX &&
	       (dev =
		pci_find_device(PCI_VENDOR_ID_ZORAN,
				PCI_DEVICE_ID_ZORAN_36057, dev)) != NULL) {
		card_num = card[zoran_num];
		zr = &zoran[zoran_num];
		memset(zr, 0, sizeof(struct zoran));	// Just in case if previous cycle failed
		zr->pci_dev = dev;
		//zr->zr36057_mem = NULL;
		zr->id = zoran_num;
		snprintf(zr->name, sizeof(zr->name), "MJPEG[%u]", zr->id);
		spin_lock_init(&zr->spinlock);
		init_MUTEX(&zr->resource_lock);
		if (pci_enable_device(dev))
			continue;
		zr->zr36057_adr = pci_resource_start(zr->pci_dev, 0);
		pci_read_config_byte(zr->pci_dev, PCI_CLASS_REVISION,
				     &zr->revision);
		if (zr->revision < 2) {
			dprintk(1,
				KERN_INFO
				"%s: Zoran ZR36057 (rev %d) irq: %d, memory: 0x%08x.\n",
				zr->name, zr->revision, zr->pci_dev->irq,
				zr->zr36057_adr);

			if (card_num == -1) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - no card specified, please use the card=X insmod option\n",
					zr->name);
				continue;
			}
		} else {
			int i;
			unsigned short ss_vendor, ss_device;
			ss_vendor = zr->pci_dev->subsystem_vendor;
			ss_device = zr->pci_dev->subsystem_device;
			dprintk(1,
				KERN_INFO
				"%s: Zoran ZR36067 (rev %d) irq: %d, memory: 0x%08x\n",
				zr->name, zr->revision, zr->pci_dev->irq,
				zr->zr36057_adr);
			dprintk(1,
				KERN_INFO
				"%s: subsystem vendor=0x%04x id=0x%04x\n",
				zr->name, ss_vendor, ss_device);
			if (card_num == -1) {
				dprintk(3,
					KERN_DEBUG
					"%s - find_zr36057() - trying to autodetect card type\n",
					zr->name);
				for (i=0;i<NUM_CARDS;i++) {
					if (ss_vendor == zoran_cards[i].vendor_id &&
					    ss_device == zoran_cards[i].device_id) {
						dprintk(3,
							KERN_DEBUG
							"%s: find_zr36057() - card %s detected\n",
							zr->name,
							zoran_cards[i].name);
						card_num = i;
						break;
					}
				}
				if (i == NUM_CARDS) {
					dprintk(1,
						KERN_ERR
						"%s: find_zr36057() - unknown card\n",
						zr->name);
					continue;
				}
			}
		}

		if (card_num == -1)
			BUG();

		zr->card = &zoran_cards[card_num];
		snprintf(zr->name, sizeof(zr->name),
			 "%s[%u]", zr->card->name, zr->id);

		zr->zr36057_mem = ioremap_nocache(zr->zr36057_adr, 0x1000);
		if (!zr->zr36057_mem) {
			dprintk(1,
				KERN_ERR
				"%s: find_zr36057() - ioremap failed\n",
				zr->name);
			continue;
		}

		result =
		    request_irq(zr->pci_dev->irq, zoran_irq,
				SA_SHIRQ | SA_INTERRUPT, zr->name,
				(void *) zr);
		if (result < 0) {
			if (result == -EINVAL) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - bad irq number or handler\n",
					zr->name);
			} else if (result == -EBUSY) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - IRQ %d busy, change your PnP config in BIOS\n",
					zr->name, zr->pci_dev->irq);
			} else {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - can't assign irq, error code %d\n",
					zr->name, result);
			}
			goto zr_unmap;
		}

		/* set PCI latency timer */
		pci_read_config_byte(zr->pci_dev, PCI_LATENCY_TIMER,
				     &latency);
		need_latency = zr->revision > 1 ? 32 : 48;
		if (latency != need_latency) {
			dprintk(2,
				KERN_INFO
				"%s: Changing PCI latency from %d to %d.\n",
				zr->name, latency, need_latency);
			pci_write_config_byte(zr->pci_dev,
					      PCI_LATENCY_TIMER,
					      need_latency);
		}

		zr36057_restart(zr);
		/* i2c */
		dprintk(2, KERN_INFO "%s: Initializing i2c bus...\n",
			zr->name);
		if (zr->card->i2c_encoder != -1)
			request_module(zr->card->i2c_enc_name);
		if (zr->card->i2c_decoder != -1)
			request_module(zr->card->i2c_dec_name);
		if (zoran_register_i2c(zr) < 0) {
			dprintk(1,
				KERN_ERR
				"%s: find_zr36057() - can't initialize i2c bus\n",
				zr->name);
			goto zr_free_irq;
		}

		dprintk(2,
			KERN_INFO "%s: Initializing videocodec bus...\n",
			zr->name);
		if (zr->card->video_codec)
			request_module(zr->card->video_codec_name);
		if (zr->card->video_vfe)
			request_module(zr->card->video_vfe_name);
		/* reset JPEG codec */
		jpeg_codec_sleep(zr, 1);
		jpeg_codec_reset(zr);
		/* video bus enabled */
		/* display codec revision */
		if (zr->card->video_codec != -1) {
			master_codec = zoran_setup_videocodec(zr,
							      zr->card->video_codec);
			if (!master_codec)
				goto zr_unreg_i2c;
			zr->codec = videocodec_attach(master_codec);
			if (!zr->codec) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - no codec found\n",
					zr->name);
				goto zr_free_codec;
			}
			if (zr->codec->type != zr->card->video_codec) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - wrong codec\n",
					zr->name);
				goto zr_detach_codec;
			}
		}
		if (zr->card->video_vfe != -1) {
			master_vfe = zoran_setup_videocodec(zr,
							    zr->card->video_vfe);
			if (!master_vfe)
				goto zr_detach_codec;
			zr->vfe = videocodec_attach(master_vfe);
			if (!zr->vfe) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() - no VFE found\n",
					zr->name);
				goto zr_free_vfe;
			}
			if (zr->vfe->type != zr->card->video_vfe) {
				dprintk(1,
					KERN_ERR
					"%s: find_zr36057() = wrong VFE\n",
					zr->name);
				goto zr_detach_vfe;
			}
		}

		zoran_num++;
		continue;

		// Init errors
	      zr_detach_vfe:
		videocodec_detach(zr->vfe);
	      zr_free_vfe:
		kfree(master_vfe);
	      zr_detach_codec:
		videocodec_detach(zr->codec);
	      zr_free_codec:
		kfree(master_codec);
	      zr_unreg_i2c:
		zoran_unregister_i2c(zr);
	      zr_free_irq:
		btwrite(0, ZR36057_SPGPPCR);
		free_irq(zr->pci_dev->irq, zr);
	      zr_unmap:
		iounmap(zr->zr36057_mem);
		continue;
	}
	if (zoran_num == 0) {
		dprintk(1, KERN_INFO "No known MJPEG cards found.\n");
	}
	return zoran_num;
}

static int __init
init_dc10_cards (void)
{
	int i;
	memset(zoran, 0, sizeof(zoran));
	printk(KERN_INFO "Zoran MJPEG board driver version %d.%d.%d\n",
	       MAJOR_VERSION, MINOR_VERSION, RELEASE_VERSION);
	/* Look for cards */
	if (find_zr36057() < 0) {
		return -EIO;
	}
	if (zoran_num == 0)
		return 0;	//-ENXIO;
	dprintk(1, KERN_INFO "%s: %d card(s) found\n", ZORAN_NAME,
		zoran_num);
	/* check the parameters we have been given, adjust if necessary */
	if (v4l_nbufs < 2)
		v4l_nbufs = 2;
	if (v4l_nbufs > VIDEO_MAX_FRAME)
		v4l_nbufs = VIDEO_MAX_FRAME;
	/* The user specfies the in KB, we want them in byte
	 * (and page aligned) */
	v4l_bufsize = PAGE_ALIGN(v4l_bufsize * 1024);
	if (v4l_bufsize < 32768)
		v4l_bufsize = 32768;
	/* 2 MB is arbitrary but sufficient for the maximum possible images */
	if (v4l_bufsize > 2048 * 1024)
		v4l_bufsize = 2048 * 1024;
	if (jpg_nbufs < 4)
		jpg_nbufs = 4;
	if (jpg_nbufs > BUZ_MAX_FRAME)
		jpg_nbufs = BUZ_MAX_FRAME;
	jpg_bufsize = PAGE_ALIGN(jpg_bufsize * 1024);
	if (jpg_bufsize < 8192)
		jpg_bufsize = 8192;
	if (jpg_bufsize > (512 * 1024))
		jpg_bufsize = 512 * 1024;
	/* Use parameter for vidmem or try to find a video card */
	if (vidmem) {
		dprintk(1,
			KERN_INFO
			"%s: Using supplied video memory base address @ 0x%lx\n",
			ZORAN_NAME, vidmem);
	}

	/* some mainboards might not do PCI-PCI data transfer well */
	if (pci_pci_problems & PCIPCI_FAIL) {
		dprintk(1,
			KERN_WARNING
			"%s: chipset may not support reliable PCI-PCI DMA\n",
			ZORAN_NAME);
	}

	/* take care of Natoma chipset and a revision 1 zr36057 */
	for (i = 0; i < zoran_num; i++) {
		struct zoran *zr = &zoran[i];
		if (pci_pci_problems & PCIPCI_NATOMA && zr->revision <= 1) {
			zr->jpg_buffers.need_contiguous = 1;
			dprintk(1,
				KERN_INFO
				"%s: ZR36057/Natoma bug, max. buffer size is 128K\n",
				zr->name);
		}

		if (zr36057_init(zr) < 0) {
			for (i = 0; i < zoran_num; i++)
				zoran_release(&zoran[i]);
			return -EIO;
		}
		zoran_proc_init(zr);
	}

	return 0;
}

static void __exit
unload_dc10_cards (void)
{
	int i;
	for (i = 0; i < zoran_num; i++)
		zoran_release(&zoran[i]);
}

module_init(init_dc10_cards);
module_exit(unload_dc10_cards);

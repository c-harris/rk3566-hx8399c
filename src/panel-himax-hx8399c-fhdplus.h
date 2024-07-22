#ifndef __PANEL_HX8399C_H__
#define __PANEL_HX8399C_H__			/**< include guard */

/* Manufacturer specific commands sent via DSI, listed in HX8399-C datasheet */
#define HX8399_CMD_SETSEQUENCE    0xb0
#define HX8399_CMD_SETPOWER       0xb1
#define HX8399_CMD_SETDISP        0xb2
#define HX8399_CMD_SETCYC         0xb4
#define HX8399_CMD_SETVCOM        0xb6
#define HX8399_CMD_SETTE          0xb7
#define HX8399_CMD_SETSENSOR      0xb8
#define HX8399_CMD_SETEXTC        0xb9
#define HX8399_CMD_SETMIPI        0xba
#define HX8399_CMD_SETOTP         0xbb
#define HX8399_CMD_SETREGBANK     0xbd
#define HX8399_CMD_UNKNOWN5       0xbf
#define HX8399_CMD_UNKNOWN1       0xc0
#define HX8399_CMD_SETDGCLUT      0xc1
#define HX8399_CMD_SETID          0xc3
#define HX8399_CMD_SETDDB         0xc4
#define HX8399_CMD_UNKNOWN2       0xc6
#define HX8399_CMD_SETCABC        0xc9
#define HX8399_CMD_SETCABCGAIN    0xca
#define HX8399_CMD_SETPANEL       0xcc
#define HX8399_CMD_SETOFFSET      0xd2
#define HX8399_CMD_SETGIP0        0xd3
#define HX8399_CMD_UNKNOWN3       0xd4
#define HX8399_CMD_SETGIP1        0xd5
#define HX8399_CMD_SETGIP2        0xd6
#define HX8399_CMD_SETGIP3        0xd8
#define HX8399_CMD_SETSCALING     0xdd
#define HX8399_CMD_SETIDLE        0xdf
#define HX8399_CMD_SETGAMMA       0xe0
#define HX8399_CMD_SETCHEMODE_DYN 0xe4
#define HX8399_CMD_SETCHE         0xe5
#define HX8399_CMD_SETCESEL       0xe6
#define HX8399_CMD_SET_SP_CMD     0xe9
#define HX8399_CMD_SETREADINDEX   0xfe
#define HX8399_CMD_GETSPIREAD     0xff

#define DRV_NAME "panel-himax-HX8399"

/* enable Readback support on panel" */
/* #define PANEL_SUPPORT_READBACK */

#define HFP (16) // horizontal frontporch
#define HSA (40) // horizontal sync active
#define HBP (40) // horizontal backporch
#define VFP (9) // vertical frontporch
#define VSA (4) // vertical sync active
#define VBP (3) // vertical backporch
#define VAC (2160) // vertical resolution
#define HAC (1080) // hoeizontal resolution

static struct drm_display_mode default_mode = {
        .clock = ((HAC + HFP + HSA + HBP) * (VAC + VFP + VSA + VBP) * 60)/1000, // 153538, //156672,//htotal*vtotal*vrefresh/1000   163943   182495
        .hdisplay = HAC,
        .hsync_start = HAC + HFP,
        .hsync_end = HAC + HFP + HSA,
        .htotal = HAC + HFP + HSA + HBP,
        .vdisplay = VAC,
        .vsync_start = VAC + VFP,
        .vsync_end = VAC + VFP + VSA,
        .vtotal = VAC + VFP + VSA + VBP,
};


#endif	/* __PANEL_HX8399C_H__ */


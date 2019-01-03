/****************************************************************************
 * Hu-Go! Nintendo Gamecube
 *
 * GFX functions
 ****************************************************************************/

#include "defs.h"
#include "sys_gfx.h"
#include "hard_pce.h"
#include "gfx.h"
#include "osd_ngc_machine.h"

//! PC Engine rendered screen
unsigned char *screen = NULL;

//! Host machine rendered screen
unsigned char *physical_screen = NULL;

// COLOR palette
unsigned short RGB565PAL[256];
extern int gcpalette[256];
extern unsigned short RGB565PAL[256];

int osd_gfx_init();
int osd_gfx_init_normal_mode();
void osd_gfx_put_image_normal();
void osd_gfx_shut_normal_mode();

// VI 
GXRModeObj *rmode = NULL;     //Graphics Mode Object
GXRModeObj *vmode = NULL;     //Menu video mode
u32 *xfb[2] = { NULL, NULL }; //Framebuffers
int whichfb = 0;              //Frame buffer toggle

// GX
#define TEX_WIDTH         512
#define TEX_HEIGHT        512
#define DEFAULT_FIFO_SIZE 256 * 1024
#define HASPECT           320
#define VASPECT           112

static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static u8 texturemem[TEX_WIDTH * (TEX_HEIGHT + 8) * 2] ATTRIBUTE_ALIGN (32);
static GXTexObj texobj;
static Mtx view;
static int texwidth, texheight, oldvwidth, oldvheight;
u8 region;
u8 render;
u8 aspect;
int frameticker;

//Console Version Type Helpers
#define GC_CPU_VERSION01 0x00083214
#define GC_CPU_VERSION02 0x00083410
#ifndef mfpvr
#define mfpvr()  ({unsigned int rval; asm volatile("mfpvr %0" : "=r" (rval)); rval;})
#endif
#define is_gamecube() (((mfpvr() == GC_CPU_VERSION01)||((mfpvr() == GC_CPU_VERSION02))))


typedef struct {
  u8   mid;         //Manufacturer ID
  char oid[2];      //OEM/Application ID
  char pnm[5];      //Product Name
  u8   prv;         //product version 0001 0001 = 1.1
  u32  psn;         //product serial number
  u16  mdt;         //bottom 4 bits are month, 8 bits above is year since 2000
  u8   unk;
} CIDdata __attribute__((aligned(32)));

void __SYS_ReadROM(void *buf,u32 len,u32 offset);
char IPLInfo[256] __attribute__((aligned(32)));


//////////////////////////   PCE 240   //////////////////////////
// 240 lines progressive (NTSC or PAL 60Hz PC-Engine Original Aspect)
GXRModeObj TVNtsc_Rgb60_240p = 
{
    VI_TVMODE_EURGB60_DS,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC/2 - 480/2)/2,   // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

     // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

     // vertical filter[7], 1/64 units, 6 bits each
  {
      0,          // line n-1
      0,          // line n-1
      21,         // line n
      22,         // line n
      21,         // line n
      0,          // line n+1
      0           // line n+1
  }
};

// 240 lines progressive (NTSC or PAL 60Hz PC-Engine Original Aspect)
GXRModeObj TVNtsc_Rgb60_240i = 
{
    VI_TVMODE_NTSC_INT,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC/2 - 480/2)/2,   // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

     // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

     // vertical filter[7], 1/64 units, 6 bits each
  {
      0,          // line n-1
      0,          // line n-1
      21,         // line n
      22,         // line n
      21,         // line n
      0,          // line n+1
      0           // line n+1
  }
};

////////////////////////////   NTSC   ////////////////////////////
// 480 lines interlaced (NTSC Stretch)
static GXRModeObj TV60hz_480i = 
{
  VI_TVMODE_NTSC_INT,     // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  480,             // xfbHeight
  (VI_MAX_WIDTH_NTSC - 669)/2,        // viXOrigin
  (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
  672,             // viWidth
  480,             // viHeight
  VI_XFBMODE_DF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     8,         // line n-1
     8,         // line n-1
    10,         // line n
    12,         // line n
    10,         // line n
     8,         // line n+1
     8          // line n+1
  }
};

// 480 lines progressive (NTSC Stretch)
static GXRModeObj TV60hz_480p = 
{
  VI_TVMODE_NTSC_PROG,     // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  480,             // xfbHeight
  (VI_MAX_WIDTH_NTSC - 669)/2,        // viXOrigin
  (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
  672,             // viWidth
  480,             // viHeight
  VI_XFBMODE_SF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     0,         // line n-1
     0,         // line n-1
    21,         // line n
    22,         // line n
    21,         // line n
     0,         // line n+1
     0          // line n+1
  }
};


///////////////////////////   PAL60   ///////////////////////////
// 480 lines interlaced (PAL 60Hz, Forced Stretch)
static GXRModeObj TV60hz_576i = 
{
  VI_TVMODE_EURGB60_INT,      // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  480,             // xfbHeight
  (VI_MAX_WIDTH_EURGB60 - 678)/2,         // viXOrigin
  (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
  678,             // viWidth
  480,             // viHeight
  VI_XFBMODE_DF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     8,          // line n-1
     8,          // line n-1
    10,          // line n
    12,          // line n
    10,          // line n
     8,          // line n+1
     8           // line n+1
  }
};

// 480 lines progressive (PAL 60Hz, Force Stretch)
static GXRModeObj TV60hz_576p = 
{
  VI_TVMODE_EURGB60_PROG,      // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  480,             // xfbHeight
  (VI_MAX_WIDTH_EURGB60 - 678)/2,         // viXOrigin
  (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
  678,             // viWidth
  480,             // viHeight
  VI_XFBMODE_SF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     0,         // line n-1
     0,         // line n-1
    21,         // line n
    22,         // line n
    21,         // line n
     0,         // line n+1
     0          // line n+1
  }
};

///////////////////////////   PAL50   ///////////////////////////
// 576 lines interlaced (PAL 50Hz, Forced Stretch)
static GXRModeObj TV50hz_576i = 
{
  VI_TVMODE_PAL_INT,      // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  576,             // xfbHeight
  (VI_MAX_WIDTH_PAL - 678)/2,        // viXOrigin
  (VI_MAX_HEIGHT_PAL - 576)/2,        // viYOrigin
  678,             // viWidth
  576,             // viHeight
  VI_XFBMODE_DF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     8,          // line n-1
     8,          // line n-1
    10,          // line n
    12,          // line n
    10,          // line n
     8,          // line n+1
     8           // line n+1
  }
};

// 576 lines progressive (PAL 50Hz, Force Stretch)
static GXRModeObj TV50hz_576p = 
{
  VI_TVMODE_PAL_PROG,      // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  576,             // xfbHeight
  (VI_MAX_WIDTH_PAL - 678)/2,        // viXOrigin
  (VI_MAX_HEIGHT_PAL - 574)/2,        // viYOrigin
  678,             // viWidth
  576,             // viHeight
  VI_XFBMODE_SF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     0,         // line n-1
     0,         // line n-1
    21,         // line n
    22,         // line n
    21,         // line n
     0,         // line n+1
     0          // line n+1
  }
};


typedef struct tagcamera
{
  guVector pos;
  guVector up;
  guVector view;
} camera;

// Square Matrix
//     This structure controls the size of the image on the screen.
//     Think of the output as a -80 x 80 by -60 x 60 graph.

static s16 square[] ATTRIBUTE_ALIGN (32) =
{
// X,   Y,  Z
// Values set are for roughly 4:3 aspect
   -HASPECT,  VASPECT, 0,       // 0
    HASPECT,  VASPECT, 0,       // 1
    HASPECT, -VASPECT, 0,       // 2
   -HASPECT, -VASPECT, 0,       // 3
};

static camera cam = {
  {0.0F, 0.0F, -100.0F},
  {0.0F, -1.0F, 0.0F},
  {0.0F, 0.0F, 0.0F}
};

// rendering initialization
// should be called each time you change quad aspect ratio
static void draw_init(void)
{
  // Clear all Vertex params
  GX_ClearVtxDesc ();

  // Set Position Params (set quad aspect ratio)
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

  // Set Tex Coord Params
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
  GX_SetNumTexGens (1);
  GX_SetNumChans(0);

  /** Set Modelview **/
  memset (&view, 0, sizeof (Mtx));
  guLookAt(view, &cam.pos, &cam.up, &cam.view);
  GX_LoadPosMtxImm (view, GX_PNMTX0);
}

static void draw_vert(u8 pos, f32 s, f32 t)               // vertex rendering
{
  GX_Position1x8 (pos);
  GX_TexCoord2f32 (s, t);
}

static void draw_square (void)                            // textured quad rendering
{
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (3, 0.0, 0.0);
  draw_vert (2, 1.0, 0.0);
  draw_vert (1, 1.0, 1.0);
  draw_vert (0, 0.0, 1.0);
  GX_End ();
}

extern void DDASampleRate ();                             // retrace handler

static void framestart(u32 retraceCnt)
{
  frameticker++;                                          // simply increment the tick counter
  DDASampleRate ();
}

static void gxStart(void)
{
  Mtx p;

  // Clear out FIFO area and init GX
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear ((GXColor){0,0,0,255}, 0x00000000);
  GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  f32 yScale = GX_GetYScaleFactor(vmode->efbHeight, vmode->xfbHeight);
  u16 xfbHeight = GX_SetDispCopyYScale (yScale);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
//  GX_SetClipMode(GX_CLIP_DISABLE);
  GX_SetDispCopyGamma (GX_GM_1_0);
  GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
  GX_SetColorUpdate (GX_TRUE);
  guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 100, 1000);
  GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);                // reset XFB
  memset (texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2);     // Initialize texture data
  texwidth = 100;                                         // Force texture update
  texheight = 100;
}

/* rmode configuration
  region 
    0 = NTSC
    1 = PAL60
    2 = PAL50
  render
    0 = INTERLACED
    1 = PROGRESSIVE
  aspect 
    0 = NORMAL
    1 = STRETCH
    2 = PCE_240
*/

// Reinitialize GX
void ogc_video__reset()
{
  Mtx p;
  
   //// GX VIDEO MODE CONFIG FROM OPTIONS MENU
   if (region == 0) {                                    // NTSC
      if (render == 0) {                                    // Interlaced
         if (aspect == 0) rmode = &TVNtsc480IntDf;             // libogc default config
         if (aspect == 1) rmode = &TV60hz_480i;                // custom stretch
         if (aspect == 2) rmode = &TVNtsc_Rgb60_240i;          // default PCE_240
      }
      if (render == 1) {                                    // Progressive
         if (aspect == 0) rmode = &TVNtsc480Prog;              // libogc default config
         if (aspect == 1) rmode = &TV60hz_480p;                // custom stretch
         if (aspect == 2) rmode = &TVNtsc_Rgb60_240p;
      }
   }
   if (region == 1) {                                    // PAL60
      if (render == 0) {                                    // Interlaced
         if (aspect == 0) rmode = &TVEurgb60Hz480IntDf;        // libogc default config
         if (aspect == 1) rmode = &TV60hz_576i;                // custom stretch
         if (aspect == 2) rmode = &TVNtsc_Rgb60_240p;          // default PCE_240
      }
      if (render == 1) {                                    // Progressive
         if (aspect == 0) rmode = &TVEurgb60Hz480Prog;          // libogc default config
         if (aspect == 1) rmode = &TV60hz_576p;                // custom stretch
         if (aspect == 2) rmode = &TVNtsc_Rgb60_240p;          // default PCE_240
      }
   }
   if (region == 2) {                                    // PAL50
      if (render == 0) {                                    // Interlaced
         if (aspect == 0) rmode = &TVPal576IntDfScale;        // libogc default config
         if (aspect == 1) rmode = &TV50hz_576i;                // custom stretch
 //      if (aspect == 2)       NA                             // default PCE_240
      }
      if (render == 1) {                                    // Progressive
         if (aspect == 0) rmode = &TVPal576ProgScale;          // libogc default config
         if (aspect == 1) rmode = &TV50hz_576p;                // custom stretch
 //      if (aspect == 2)       NA                             // default PCE_240
      }
   }
   // RESET SCALER with NEW GX VIDEO MODE ABOVE
   osd_gfx_init_normal_mode();

   VIDEO_Configure (rmode);
   VIDEO_ClearFrameBuffer(rmode, xfb[whichfb], COLOR_BLACK);
   VIDEO_Flush();
   VIDEO_WaitVSync();

   // reset rendering mode
   GX_SetViewport (0.0F, 0.0F, rmode->fbWidth, rmode->efbHeight, 0.0F, 1.0F);
   GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
   f32 yScale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
   u16 xfbHeight = GX_SetDispCopyYScale (yScale);
   GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
   GX_SetDispCopyDst (rmode->fbWidth, xfbHeight);
   GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (rmode->xfbMode == VI_XFBMODE_SF) ? GX_TRUE : GX_FALSE, rmode->vfilter);  
   GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
   GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
   guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 100, 1000);
   GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
// GX_Flush();
}

// Initialize VIDEO subsystem
void ogc_video__init(void)
{
   // Before doing anything else under libogc, Call VIDEO_Init
   region = 0;
   render = 0;
   aspect = 0;
   int retPAD = 0;

   VIDEO_Init ();
   PAD_Init ();
   while(retPAD <= 0) { retPAD = PAD_ScanPads(); usleep(100); }
   
   __SYS_ReadROM(IPLInfo,256,0);                           // Read IPL tag
    
   // Get the current video mode then :
   //    - set menu video mode (fullscreen, 480i, 480p or 576i, 576p)
   //    - set emulator rendering TV modes (PAL/NTSC/EURGB60)  
   //   - Wii has no IPL tags for "PAL" so let libOGC figure out the video mode
   if(!is_gamecube()) {
     vmode = VIDEO_GetPreferredMode(NULL);                //Last mode used
   }
   else {
      // Gamecube, determine based on IPL
      // If Trigger L detected during bootup, force 480i safemode
      // for Digital Component cable for SDTV compatibility.
      if(VIDEO_HaveComponentCable() && !(PAD_ButtonsDown(0) & PAD_BUTTON_START)) {
        if((strstr(IPLInfo,"PAL")!=NULL)) {
          vmode = &TV60hz_576p;                            //Progressive 576p60hz
          region = 1;
          render = 1;
          aspect = 1;
        }
        else {
          vmode = &TV60hz_480p;                           //Progressive 480p
          region = 0;
          render = 1;
          aspect = 1;
//        vmode = &TVNtsc480Prog;                         //Progressive 480p
//        vmode = &TVPal576ProgScale;                     //Progressive 576p
//        vmode = &TV50hz_576p;                           //Progressive 576p
        }
     }
     else {
        //try to use the IPL region
        if(strstr(IPLInfo,"PAL")!=NULL) {
          vmode = &TV60hz_576i;                           //Interlaced 576i60hz
          region = 1;
          render = 0;
          aspect = 1;
        }
        else if(strstr(IPLInfo,"NTSC")!=NULL) {
          vmode = &TV60hz_480i;                           //Interlaced 480i
          region = 0;
          render = 0;
          aspect = 1;
        }
        else {
          vmode = VIDEO_GetPreferredMode(NULL);           //Last mode used
        }
     }  
/* region 
     0 = NTSC
     1 = PAL60
     2 = PAL50
   render
     0 = INTERLACED
     1 = PROGRESSIVE
   aspect 
     0 = NORMAL
     1 = STRETCH
     2 = PCE_240
*/   }

   VIDEO_Configure (vmode);                                // configure video mode
   xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));   // Configure the framebuffers
   xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
   console_init(xfb[0], 20, 64, 640, 480, 480 * 2);        // Define a console
   VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);    // Clear framebuffers to black
   VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
   VIDEO_SetNextFramebuffer (xfb[0]);                      // Set the framebuffer to be displayed at next VBlank
   VIDEO_SetPreRetraceCallback(framestart);                // Register Video Retrace handlers
   VIDEO_SetBlack (FALSE);                                 // Enable Video Interface
   VIDEO_Flush ();                                         // Update video settings for next VBlank
   VIDEO_WaitVSync();                                      // Wait for VBlank
   if (vmode->viTVMode & VI_NON_INTERLACE) {               //  VIDEO_WaitVSync();
     VIDEO_WaitVSync ();
   }

   gxStart();                                              // Initialize GX
}

// Hu-Go! default
osd_gfx_driver osd_gfx_driver_list[3] =
{
  { osd_gfx_init, osd_gfx_init_normal_mode, osd_gfx_put_image_normal, osd_gfx_shut_normal_mode },
  { osd_gfx_init, osd_gfx_init_normal_mode, osd_gfx_put_image_normal, osd_gfx_shut_normal_mode },
  { osd_gfx_init, osd_gfx_init_normal_mode, osd_gfx_put_image_normal, osd_gfx_shut_normal_mode }
};

/*****************************************************************************

    Function: osd_gfx_put_image_normal

    Description: draw the raw computed picture to screen, without any effect
       trying to center it (I bet there is still some work on this, maybe not
                            in this function)
    Parameters: none
    Return: nothing

*****************************************************************************/
void osd_gfx_put_image_normal(void)
{
  unsigned short *texture;
  int tofs;
  int hpos;
  int y, x;

  u8 *buf = (u8 *)osd_gfx_buffer;

  texheight = io.screen_h;
  texwidth  = io.screen_w;
  
  // check if viewport has changed
  if ((oldvheight != texheight) || (oldvwidth != texwidth))
  {
    /** Update scaling **/
    oldvwidth = texwidth;
    oldvheight = texheight;

    // set aspect
    osd_gfx_init_normal_mode();

    // reinitialize texture
    GX_InvalidateTexAll ();
    GX_InitTexObj (&texobj, texturemem, texwidth, texheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
  }

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
 
  texture = (unsigned short *) texturemem;
  tofs = 0;

  for (y = 0; y < texheight; y += 4)
  {
    for (x = 0; x < texwidth; x += 4)
    {
      hpos = (y * XBUF_WIDTH) + x;
      
      // Row One
      texture[tofs++] = RGB565PAL[buf[hpos]];
      texture[tofs++] = RGB565PAL[buf[hpos + 1]];
      texture[tofs++] = RGB565PAL[buf[hpos + 2]];
      texture[tofs++] = RGB565PAL[buf[hpos + 3]];
      
      // Row Two
      hpos += XBUF_WIDTH;
      texture[tofs++] = RGB565PAL[buf[hpos]];
      texture[tofs++] = RGB565PAL[buf[hpos + 1]];
      texture[tofs++] = RGB565PAL[buf[hpos + 2]];
      texture[tofs++] = RGB565PAL[buf[hpos + 3]];
      
      // Row Three
      hpos += XBUF_WIDTH;
      texture[tofs++] = RGB565PAL[buf[hpos]];
      texture[tofs++] = RGB565PAL[buf[hpos + 1]];
      texture[tofs++] = RGB565PAL[buf[hpos + 2]];
      texture[tofs++] = RGB565PAL[buf[hpos + 3]];
      
      // Row Four
      hpos += XBUF_WIDTH;
      texture[tofs++] = RGB565PAL[buf[hpos]];
      texture[tofs++] = RGB565PAL[buf[hpos + 1]];
      texture[tofs++] = RGB565PAL[buf[hpos + 2]];
      texture[tofs++] = RGB565PAL[buf[hpos + 3]];
    }
  }

  // load texture into GX
  DCFlushRange (texturemem, texwidth * texheight * 2);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);
  
  // render textured quad
  draw_square ();
  GX_DrawDone ();

  // switch external framebuffers then copy EFB to XFB
  whichfb ^= 1;
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_Flush ();

  // set next XFB
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
}

/*****************************************************************************

    Function: osd_gfx_set_message

    Description: compute the message that will be displayed to create a sprite
       to blit on screen
    Parameters: char* mess, the message to display
    Return: nothing but set OSD_MESSAGE_SPR

*****************************************************************************/
void osd_gfx_set_message(char* mess)
{
 // TODO: Update the screen info
}

// osd_gfx_init:
// One time initialization of the main output screen
int osd_gfx_init(void)
{
  SetPalette();
  return 1;
}


/*****************************************************************************

    Function:  osd_gfx_init_normal_mode

    Description: initialize the classic 256*224 video mode for normal video_driver
    Parameters: none
    Return: 0 on error
            1 on success

*****************************************************************************/
int osd_gfx_init_normal_mode()
{
  int yscale;
  if (!p_io) return 0;

  // aspect: PCE_240 : PROGRESSIVE or INTERLACED
  if ( aspect == 2 ) yscale = ((rmode->xfbMode == VI_XFBMODE_DF) ? GX_TRUE : GX_FALSE) ? io.screen_h : (io.screen_h /2);
  // aspect: ALL OTHER VIDEO TYPES : NORMAL or STRETCH
  else yscale = rmode ? 224 : 112;

  square[4] = square[1]  =  yscale;
  square[7] = square[10] = -yscale;
  draw_init();
  return 1;  
}

//! Delete the window
void osd_gfx_shut_normal_mode(void)
{
}

/*****************************************************************************

    Function: osd_gfx_set_color

    Description: Change the component of the choosen color
    Parameters: UChar index : index of the color to change
    			UChar r	: new red component of the color
                UChar g : new green component of the color
                UChar b : new blue component of the color
    Return:

*****************************************************************************/
void osd_gfx_set_color(UChar index, UChar r, UChar g, UChar b)
{
  r <<= 2;
  g <<= 2;
  b <<= 2;

  RGB565PAL[index] = ( ( r << 8 ) & 0xf800 ) | ( ( g << 3 ) & 0x7e0 ) | ( b >> 3 ) ;
}

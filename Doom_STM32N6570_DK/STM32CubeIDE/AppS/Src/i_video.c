/*
              | |
     _   _  __| | ___   ___  _ __ ___
    | | | |/ _` |/ _ \ / _ \| '_ ` _ \
    | |_| | (_| | (_) | (_) | | | | | |
     \__,_|\__,_|\___/ \___/|_| |_| |_|

   Doom for the STM32N6 microcontroller
*/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
// #include "deh_str.h"
#include "doomtype.h"
#include "doomkeys.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_swap.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "main.h" /* for STM32 related functions and variables */

static float fb_scaling = 1.0f; /**< Scaling factor for the framebuffer. Calculated based on the display resolution and the original Doom resolution (320x200). */
int usemouse = 0; /**< unused? */

static uint32_t dma2d_clut[256]; /**< DMA2D CLUT (Color Look-Up Table) for exactly 256 colors, in ARGB8888 format. */
static byte* VideoBuffer2X = NULL; /**< Upscaled framebuffer for the display (320x200 is too tiny for modern screens). Palette 1 Byte per pixel. */

#define MAX_SCALING 3
static uint8_t videobuffer[SCREENWIDTH * SCREENHEIGHT]; /**< Original Doom 320x200 buffer. Palette 1 Byte per pixel */
static uint8_t videobuffer_local[SCREENWIDTH * MAX_SCALING * SCREENHEIGHT * MAX_SCALING]; /**< Upscaled framebuffer, Palette 1 Byte per pixel. */

byte *I_VideoBuffer = NULL; /**< This is where Doom draws the game. It's a fixed 320x200 buffer with 1 byte per pixel (palette index). This buffer is then scaled up and sent to the display. */
boolean screensaver_mode = false;
boolean screenvisible;
int vanilla_keyboard_mapping = 1;
float mouse_acceleration = 2.0;
int mouse_threshold = 10;
int usegamma = 4; /* make it a bit brighter by default on the display */

typedef void (*scale_func_t)(const uint8_t* src, uint8_t* dst, int w, int h, float scale);
static scale_func_t selected_scaler = NULL;

extern DMA2D_HandleTypeDef hlcd_dma2d; /* from stm32n6570_discovery_lcd.c */

/* Helper functions (LCD_GetXSize and LCD_GetYSize) to get the actual display
 * resolution. These functions query the LCD driver for the current display
 * size, which is needed for scaling the Doom framebuffer correctly.
 *
 * Note: These functions assume that the LCD driver is already initialized and
 * that the display size can be queried at this point in the code.  *
 * */
static int LCD_GetXSize(void)
{
    uint32_t xsize;
    BSP_LCD_GetXSize(0, &xsize);
    return (int)xsize;
}

static int LCD_GetYSize(void)
{
    uint32_t ysize;
    BSP_LCD_GetYSize(0, &ysize);
    return (int)ysize;
}

/** @brief Generic scaling function that scales a 320x200 8-bit palette buffer
 * to the desired size using nearest-neighbor scaling. The scaling factor can
 * be non-integer.
 * @param src Pointer to the source buffer (320x200, 1 byte per pixel).
 * @param dst Pointer to the destination buffer (scaled size, 1 byte per pixel).
 * @param w Width of the source buffer (should always be 320 (SCREENWIDTH)).
 * @param h Height of the source buffer (should always be 200 (SCREENHEIGHT)).
 * @param scale Scaling factor (e.g. 2.0 for 640x400, 1.5 for 480x300, etc.).
 *
 * */
__attribute__((optimize("O2", "unroll-loops")))
static void scale_generic(const uint8_t* src, uint8_t* dst, int w, int h, float scale)
{
    /*
       x * (1 / scale)
            with: inv_scale_fp ≈ (1 / scale) * 65536
       ≈  x * (inv_scale_fp / 65536)
       ≈ (x * inv_scale_fp) >> 16
    */
    uint32_t inv_scale_fp = (uint32_t)(65536.0f / scale);
    int dst_w = (int)(w * scale);
    int dst_h = (int)(h * scale);

    for (int y = 0; y < dst_h; ++y)
    {
        int src_y = (y * inv_scale_fp) >> 16;
        const uint8_t* src_row = src + src_y * w;
        uint8_t* dst_row = dst + y * dst_w;

        for (int x = 0; x < dst_w; ++x)
        {
            int src_x = (x * inv_scale_fp) >> 16;
            dst_row[x] = src_row[src_x];
        }
    }
}

/** @brief Optimized scaling function for exactly 2x scaling. This is a common
 * case and can be implemented more efficiently than the generic scaler.
 * It simply duplicates each pixel into a 2x2 block in the destination buffer.
 * @param src Pointer to the source buffer (320x200, 1 byte per pixel).
 * @param dst Pointer to the destination buffer (640x400, 1 byte per pixel).
 * @param w Width of the source buffer (should always be 320 (SCREENWIDTH)).
 * @param h Height of the source buffer (should always be 200 (SCREENHEIGHT)).
 * @param scale Scaling factor (ignored and assumed to be 2.0 for this function,
 * but added to have the same interface as the generic scaling function).
 *
 * */
static void scale_2x(const uint8_t* src, uint8_t* dst, int w, int h, float scale)
{
    (void)scale;
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* src_row = src + y * w;
        uint8_t* dst_row0 = dst + (y * 2) * (w * 2);
        uint8_t* dst_row1 = dst_row0 + (w * 2);
        for (int x = 0; x < w; ++x)
        {
            uint8_t p = src_row[x];
            dst_row0[x * 2 + 0] = p;
            dst_row0[x * 2 + 1] = p;
            dst_row1[x * 2 + 0] = p;
            dst_row1[x * 2 + 1] = p;
        }
    }
}

void DMA2D_Init(void)
{
    printf("Initializing DMA2D\n");
    hlcd_dma2d.Instance = DMA2D;
    hlcd_dma2d.Init.Mode = DMA2D_M2M_PFC;
    hlcd_dma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565; /* if you change this, change BSP_LCD_InitEx in main.c too, and change the FB_SIZE macro in app_main.c */
    hlcd_dma2d.Init.OutputOffset = LCD_GetXSize() - (int)(fb_scaling * SCREENWIDTH);

    hlcd_dma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_L8;
    hlcd_dma2d.LayerCfg[1].InputOffset = 0;
    hlcd_dma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hlcd_dma2d.LayerCfg[1].InputAlpha = 0xFF;

    HAL_DMA2D_Init(&hlcd_dma2d);
    HAL_DMA2D_ConfigLayer(&hlcd_dma2d, 1);

    printf("DMA2D initialized\n");
}

void I_InitGraphics(void)
{
    int i;
    const int s_Fb_xres = LCD_GetXSize(); // query the actual display resolution
    const int s_Fb_yres = LCD_GetYSize();

    i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0)
    {
        fb_scaling = atof(myargv[i + 1]);
    }
    else
    {
        float sx = (float)s_Fb_xres / SCREENWIDTH;
        float sy = (float)s_Fb_yres / SCREENHEIGHT;
        fb_scaling = (sx < sy) ? sx : sy;
        // fb_scaling = 1.0f;
    }
    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);
    printf("I_InitGraphics: screen size: w x h: %d x %d\n", s_Fb_xres, s_Fb_yres);
    printf("I_InitGraphics: virtual screen size: %d x %d\n",
           (int)(SCREENWIDTH*fb_scaling), (int)(SCREENHEIGHT*fb_scaling));

    if (fb_scaling == 2.0f)
    {
        selected_scaler = scale_2x;
    }
    else
    {
        selected_scaler = scale_generic;
    }

    if (!I_VideoBuffer)
    {
        I_VideoBuffer = videobuffer;
    }
    screenvisible = true;
    if (fb_scaling != 1.0f && !VideoBuffer2X)
    {
        int buf_size = (int)(SCREENWIDTH * fb_scaling) * (int)(SCREENHEIGHT * fb_scaling);
        if (buf_size <= sizeof(videobuffer_local))
        {
            VideoBuffer2X = videobuffer_local;
        }
        else
        {
            printf("Warning: Using PSRAM for VideoBuffer2X (slow)\n");
            VideoBuffer2X = Z_Malloc(buf_size, PU_STATIC, NULL);
        }
        printf("I_InitGraphics: Scale-up video buffer size %i bytes\n", buf_size);
    }

    DMA2D_Init(); // is using fb_scaling
}

void I_ShutdownGraphics(void)
{
    /* Check if VideoBuffer2X is allocated on the heap (i.e. not pointing to
     * the local static buffer) and free it if necessary */
    if (VideoBuffer2X && (VideoBuffer2X != videobuffer_local))
    {
        Z_Free(VideoBuffer2X);
        VideoBuffer2X = NULL;
    }
}

/** @brief Helper function to scale the original Doom framebuffer (320x200,
 * 8-bit palette) to the display size and blit it using DMA2D.  This function
 * handles both scaling and blitting. It first scales the source buffer if
 * necessary, then uses DMA2D to transfer the data to the display buffer and convert
 * from 8-bit palette to the display's color format (e.g. RGB565) on the fly with
 * hardware acceleration.
 *
 * @param src Pointer to the source buffer (320x200, palette 1 byte per pixel).
 * @param dst Pointer to the destination buffer (display framebuffer, in e.g. RGB565 format).
 * @param width Width of the source buffer (should always be 320 (SCREENWIDTH)).
 * @param height Height of the source buffer (should always be 200 (SCREENHEIGHT)).
 *
 * */
static void BlitDoomFrame(const uint8_t *src, uint32_t *dst, int width, int height)
{
    int out_w = (int)(fb_scaling * width);
    int out_h = (int)(fb_scaling * height);
    uint8_t* scaled = (fb_scaling == 1.0f) ? (uint8_t*)src : VideoBuffer2X;

    if (fb_scaling != 1.0f)
    {
        selected_scaler(src, scaled, width, height, fb_scaling);
    }

    uint32_t* dst_center = dst + ((LCD_GetYSize() - out_h) / 2) * LCD_GetXSize()
                                + ((LCD_GetXSize() - out_w) / 2);

    SCB_CleanDCache_by_Addr((uint32_t*)scaled, out_w * out_h);
    if (HAL_DMA2D_Start_IT(&hlcd_dma2d, (uint32_t)scaled, (uint32_t)dst_center, out_w, out_h) != HAL_OK)
    {
        I_Error("HAL_DMA2D_Start_IT failed\n");
        return;
    }
    const uint32_t timeout = HAL_GetTick() + 200; // 200ms timeout
    while (HAL_DMA2D_GetState(&hlcd_dma2d) != HAL_DMA2D_STATE_READY)
    {
        if (HAL_GetTick() > timeout)
        {
            I_Error("DMA2D timeout!\n");
            break;
        }
        __NOP(); // __WFI();
    }
}

extern void STM32_SignalFrameReady();
extern uint8_t* STM32_ScreenBuffer;
void I_FinishUpdate(void)
{
    /* Scale, convert (to e.g. RGB565) and blit the Doom framebuffer to the display */
    BlitDoomFrame(I_VideoBuffer, (uint32_t *)STM32_ScreenBuffer, SCREENWIDTH, SCREENHEIGHT);
    STM32_SignalFrameReady();
}

void I_StartFrame(void) {}
void I_GetEvent(void) {}
void I_StartTic(void) { I_GetEvent(); }
void I_UpdateNoBlit(void) {}
void I_ReadScreen(byte* scr) { memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT); }

/** @brief Set the color palette for the display. This function takes a pointer to a
 * 256-color palette (768 bytes, with 3 bytes per color for R, G, B), applies
 * gamma correction, and loads it into the DMA2D CLUT (Color Look-Up Table) for
 * hardware-accelerated palette conversion during BlitDoomFrame().
 *
 * @param palette Pointer to the input palette data (768 bytes: 256 colors * 3 bytes per color).
 *
 * The input palette is expected to be in the format of 256 entries, where each
 * entry consists of 3 bytes representing the red, green, and blue components
 * of the color (in that order). The function applies gamma correction to each
 * color component using a precomputed gamma table and then constructs a 32-bit
 * ARGB8888 value for each color, which is stored in the dma2d_clut array.
 * Finally, it configures the DMA2D CLUT with the new palette.
 *
 * Note: The alpha component is set to 0xFF (fully opaque) for all colors in this implementation.
 *
 * */
void I_SetPalette(byte* palette)
{
    for (int i = 0; i < 256; ++i)
    {
        uint8_t r = gammatable[usegamma][*palette++];
        uint8_t g = gammatable[usegamma][*palette++];
        uint8_t b = gammatable[usegamma][*palette++];
        dma2d_clut[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
    DMA2D_CLUTCfgTypeDef clut_cfg =
    {
        .pCLUT = dma2d_clut,
        .CLUTColorMode = DMA2D_CCM_ARGB8888, /* always supply as ARGB8888, even
                                                if the display is RGB565 - the
                                                DMA2D will convert on the fly
                                                */
        .Size = 255
    };
    /* Make sure the CLUT data is written from cache to memory before starting
     * the DMA2D transfer */
    SCB_CleanDCache_by_Addr((uint32_t*)dma2d_clut, sizeof(dma2d_clut));
    HAL_DMA2D_CLUTLoad(&hlcd_dma2d, clut_cfg, 1);
    HAL_DMA2D_PollForTransfer(&hlcd_dma2d, HAL_MAX_DELAY);
}

int I_GetPaletteIndex(int r, int g, int b) { return 0; }
void I_BeginRead(void) {}
void I_EndRead(void) {}
void I_SetWindowTitle(char *title) {}
void I_GraphicsCheckCommandLine(void) {}
void I_SetGrabMouseCallback(grabmouse_callback_t func) {}
void I_EnableLoadingDisk(void) {}
void I_BindVideoVariables(void) {}
void I_DisplayFPSDots(boolean dots_on) {}
void I_CheckIsScreensaver(void) {}


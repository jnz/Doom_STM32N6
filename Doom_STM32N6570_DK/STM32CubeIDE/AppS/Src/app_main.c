#include "main.h"
#include <stdbool.h>
#include <string.h>
#include "stm32_lcd.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define FB_SIZE (LCD_DEFAULT_WIDTH * LCD_DEFAULT_HEIGHT * sizeof(uint16_t))

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

extern int doom_main(int argc, char **argv); /**< main entry point to initialize doom */
extern void doom_tick(void); /**< called every main loop iteration to run doom */

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

extern LTDC_HandleTypeDef hlcd_ltdc; /**< HAL LTDC handle from stm32n6570_discovery_lcd.c */
static volatile uint32_t back_buffer_addr; /**< back buffer address, where doom draws to */
static volatile uint32_t front_buffer_addr; /**< front buffer address, currently shown on LCD, swapped to back buffer on vsync */
static volatile bool frame_ready = false; /**< flag set by doom to indicate that the frame is ready and can be swapped to LCD layer, cleared in line event irq handler */
static volatile uint32_t fps_counter = 0; /**< counter to calculate fps, incremented in line event irq handler */
static volatile uint32_t irq_counter = 0; /**< counter to calculate irq_fps */
static volatile uint32_t fps = 0; /**< for debugging, how many frames per second are generated, updated once per second */
static volatile uint32_t irq_fps = 0; /**< for debugging, how many line events we get per second, should be equal to fps */
static volatile bool double_buffer_enabled = false; /**< there is a teardown
                                                      effect in doom that
                                                      requires a temporary
                                                      disable of double
                                                      buffering, see d_main.c
                                                      */

// Framebuffer Pointer
uint8_t* STM32_ScreenBuffer; /**< buffer for doom to draw into, will be swapped
                                to LCD layer on vsync */

/******************************************************************************
 * FUNCTION BODIES
 ******************************************************************************/

void I_DoubleBufferEnable(int enable)
{
    /* temporarily disable double buffering for the doom teardown effect, see d_main.c */
    double_buffer_enabled = enable ? true : false;
}

/* Simple error handler that prints the error message.
 */
void I_Error(char *error, ...)
{
    /* FIXME: evaluate the variable arguments and print them as well */
    printf("ERROR: %s\r\n", error);
}

void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    if (frame_ready) /* skip if no new frame is ready */
    {
        fps_counter++;
        /* Point display to prepared buffer */
        // HAL_LTDC_SetAddress is the right function but quite large
        // HAL_LTDC_SetAddress(hltdc, back_buffer_addr, LTDC_LAYER_1);
        // Basically this is needed:
        LTDC_LAYER(hltdc, 0)->CFBAR = ((uint32_t)back_buffer_addr);
        __DSB();

        /* Swap pointers */
        if (double_buffer_enabled)
        {
            uint32_t temp = back_buffer_addr;
            back_buffer_addr = front_buffer_addr;
            front_buffer_addr = temp;
        }

        frame_ready = false;
    }
    irq_counter++;
    HAL_LTDC_ProgramLineEvent(hltdc, 0); /* re-enable line event interrupt for the next frame */
    BSP_LED_Toggle(LED_RED); /* just for debugging, toggle the red LED on every vsync */
}

// called by i_video's I_FinishUpdate() to signal that the framebuffer is ready
void STM32_SignalFrameReady()
{
    frame_ready = true; // indicate that we can swap the frame
}

void app_main(void)
{
    front_buffer_addr  = LCD_LAYER_0_ADDRESS;
    back_buffer_addr   = LCD_LAYER_1_ADDRESS;
    STM32_ScreenBuffer = (uint8_t*)back_buffer_addr;
    memset((void *)front_buffer_addr, 0x00, FB_SIZE); /* clear the front buffer to black */
    memset((void *)back_buffer_addr,  0x00, FB_SIZE); /* clear the back buffer to black */
    frame_ready = false;
    I_DoubleBufferEnable(1);
    HAL_LTDC_ProgramLineEvent(&hlcd_ltdc, 0);

    unsigned epoch = 0;
    uint32_t time_ms = HAL_GetTick();
    uint32_t fpstimer_start_ms = time_ms;

    char* argv[] = {
            "doom.exe",   // argv[0]
            "-iwad",      // argv[1]
            "doom1.wad",  // argv[2]
            "-skill",     // argv[3]
            "1",          // argv[4]
            "-warp",      // argv[5]
            "1",          // argv[6]
            "5",          // argv[7]
			"-nnbot"      // argv[8]
        };

    int argc = BSP_PB_GetState(BUTTON_USER1) ? 9 : 1;
    doom_main(argc, argv);

    while (true)
    {
        BSP_LED_Toggle(LED_GREEN);
        epoch++;

        __disable_irq();
        STM32_ScreenBuffer = (uint8_t*)back_buffer_addr;
        __enable_irq();
        doom_tick();

        time_ms = HAL_GetTick();
        if (time_ms - fpstimer_start_ms >= 1000)
        {
            fpstimer_start_ms = time_ms;
            fps = fps_counter;
            irq_fps = irq_counter;
            fps_counter = 0;
            irq_counter = 0;
            printf("\rFPS: %2u IRQ: %2u\r\n", (unsigned)fps, (unsigned)irq_fps);
        }

        while (frame_ready)
        {
            __NOP(); //__WFI();  /* */
        }
    }
}

#include "stdio.h"

/* PMSIS includes. */
#include "pmsis.h"

/* PMSIS BSP includes. */
#include "bsp/bsp.h"
#include "bsp/camera.h"
#include "bsp/display.h"
#include "bsp/display/ili9341.h"

/* Demo includes. */
#include "setup.h"

static pi_task_t task;
static uint8_t *imgBuff0;
static pi_buffer_t buffer;
static struct pi_device cam;
static struct pi_device lcd;
#if defined(USE_BRIDGE)
static uint64_t fb;
#endif  /* USE_BRIDGE */

static void cam_handler(void *arg);
static void lcd_handler(void *arg);

static void cam_handler(void *arg)
{
    pi_camera_control(&cam, PI_CAMERA_CMD_STOP, 0);
    #if defined(HAVE_DISPLAY) && defined(USE_BRIDGE)
    PRINTF("Cam: image captured\n");
    pi_display_write(&lcd, &buffer, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    BRIDGE_FBUpdate(fb, (unsigned int) imgBuff0, 0, 0, CAMERA_WIDTH, CAMERA_HEIGHT, NULL);
    lcd_handler(NULL);
    #else
    #if defined(HAVE_DISPLAY)
    PRINTF("Cam: image captured\n");
    pi_task_callback(&task, lcd_handler, NULL);
    pi_display_write_async(&lcd, &buffer, 0, 0, LCD_WIDTH, LCD_HEIGHT, &task);
    #endif  /* HAVE_DISPLAY */
    #if defined(USE_BRIDGE)
    BRIDGE_FBUpdate(fb, (unsigned int) imgBuff0, 0, 0, CAMERA_WIDTH, CAMERA_HEIGHT, NULL);
    lcd_handler(NULL);
    #endif  /* USE_BRIDGE */
    #endif  /* HAVE_DISPLAY && USE_BRIDGE */
}

static void lcd_handler(void *arg)
{
    pi_task_callback(&task, cam_handler, NULL);
    pi_camera_capture_async(&cam, imgBuff0, CAMERA_WIDTH * CAMERA_HEIGHT, &task);
    pi_camera_control(&cam, PI_CAMERA_CMD_START, 0);
}

#if defined(USE_BRIDGE)
static int32_t open_bridge()
{
    BRIDGE_Init();
    BRIDGE_Connect(1, NULL);

    fb = BRIDGE_FBOpen("Camera", CAMERA_WIDTH, CAMERA_HEIGHT, HAL_BRIDGE_REQ_FB_FORMAT_GRAY, NULL);
    if (fb == 0)
    {
        return -1;
    }
    return 0;
}
#endif  /* USE_BRIDGE */

#if defined(HAVE_DISPLAY)
static int32_t open_display(struct pi_device *device)
{
    struct pi_ili9341_conf ili_conf;
    pi_ili9341_conf_init(&ili_conf);
    pi_open_from_conf(device, &ili_conf);
    if (pi_display_open(device))
    {
        printf("Failed to open display\n");
        return -1;
    }
    //display_ioctl(device, ILI_IOCTL_ORIENTATION, (void *) ILI_ORIENTATION_270);
    return 0;
}
#endif  /* HAVE_DISPLAY */

#if defined(HIMAX)
static int32_t open_camera_himax(struct pi_device *device)
{
    struct pi_himax_conf cam_conf;
    pi_himax_conf_init(&cam_conf);
    #if defined(QVGA)
    cam_conf.format = PI_CAMERA_QVGA;
    #endif  /* QVGA */
    pi_open_from_conf(device, &cam_conf);
    if (pi_camera_open(device))
    {
        return -1;
    }
    return 0;
}
#else
#define MT9V034_BLACK_LEVEL_CTRL (0x47)
#define MT9V034_BLACK_LEVEL_AUTO (0 << 0)
#define MT9V034_AEC_AGC_ENABLE   (0xAF)
#define MT9V034_AEC_ENABLE_A     (1 << 0)
#define MT9V034_AGC_ENABLE_A     (1 << 1)
#define MT9V034_AEC_ENABLE_B     (1 << 8)
#define MT9V034_AGC_ENABLE_B     (1 << 9)

static int32_t open_camera_mt9v034(struct pi_device *device)
{
    struct mt9v034_conf cam_conf;
    pi_mt9v034_conf_init(&cam_conf);
    #if defined(QVGA)
    cam_conf.format = CAMERA_QVGA;
    #else
    cam_conf.format = CAMERA_QQVGA;
    #endif  /* QVGA */
    pi_open_from_conf(device, &cam_conf);
    if (pi_camera_open(device))
    {
        return -1;
    }
    uint16_t val = MT9V034_BLACK_LEVEL_AUTO;
    pi_camera_reg_set(device, MT9V034_BLACK_LEVEL_CTRL, (uint8_t *) &val);
    val = MT9V034_AEC_ENABLE_A|MT9V034_AGC_ENABLE_A;
    pi_camera_reg_set(device, MT9V034_AEC_AGC_ENABLE, (uint8_t *) &val);
    return 0;
}
#endif  /* HIMAX */

static int32_t open_camera(struct pi_device *device)
{
    #if defined(HIMAX)
    return open_camera_himax(device);
    #else
    return open_camera_mt9v034(device);
    #endif  /* HIMAX */
    return -1;
}

void test_camera_with_lcd(void)
{
    printf("Entering main controller...\n");

    #if defined(__PULP_OS__)
    rt_freq_set(__RT_FREQ_DOMAIN_FC, 250000000);
    #endif  /* __PULP_OS__ */

    imgBuff0 = (uint8_t *) pmsis_l2_malloc((CAMERA_WIDTH * CAMERA_HEIGHT) * sizeof(uint8_t));
    if (imgBuff0 == NULL)
    {
        printf("Failed to allocate Memory for Image \n");
        pmsis_exit(-1);
    }

    if (open_camera(&cam))
    {
        printf("Failed to open camera\n");
        pmsis_exit(-2);
    }

    #if defined(HAVE_DISPLAY)
    if (open_display(&lcd))
    {
        printf("Failed to open display\n");
        pmsis_exit(-3);
    }
    pi_display_ioctl(&lcd, PI_ILI_IOCTL_ORIENTATION, (void *) PI_ILI_ORIENTATION_270);
    #endif  /* HAVE_DISPLAY */

    #if defined(USE_BRIDGE)
    if (open_bridge())
    {
        printf("Failed to open bridge\n");
        pmsis_exit(-4);
    }
    #endif  /* USE_BRIDGE */

    #if defined(HIMAX)
    buffer.data = imgBuff0 + CAMERA_WIDTH * 2 + 2;
    buffer.stride = 4;

    // WIth Himax, propertly configure the buffer to skip boarder pixels
    pi_buffer_init(&buffer, PI_BUFFER_TYPE_L2, imgBuff0 + CAMERA_WIDTH * 2 + 2);
    pi_buffer_set_stride(&buffer, 4);
    #else
    buffer.data = imgBuff0;
    pi_buffer_init(&buffer, PI_BUFFER_TYPE_L2, imgBuff0);
    #endif  /* HIMAX */
    pi_buffer_set_format(&buffer, CAMERA_WIDTH, CAMERA_HEIGHT, 1, PI_BUFFER_FORMAT_GRAY);

    printf("Main loop start\n");
    while (1)
    {
        #if defined(ASYNC)
        pi_camera_control(&cam, CAMERA_CMD_STOP, 0);
        PRINTF("Camera stop.\n");
        pi_task_callback(&task, cam_handler, NULL);
        pi_camera_capture_async(&cam, imgBuff0, CAMERA_WIDTH * CAMERA_HEIGHT, &task);
        PRINTF("Image capture.\n");
        pi_camera_control(&cam, CAMERA_CMD_START, 0);
        PRINTF("Camera start.\n");
        pi_task_wait_on(&task);
        #else
        PRINTF("Camera start.\n");
        pi_camera_control(&cam, PI_CAMERA_CMD_START, 0);
        pi_camera_capture(&cam, imgBuff0, CAMERA_WIDTH * CAMERA_HEIGHT);
        PRINTF("Image captured.\n");
        pi_camera_control(&cam, PI_CAMERA_CMD_STOP, 0);
        PRINTF("Camera stop.\n");
        #if defined(HAVE_DISPLAY) && defined(USE_BRIDGE)
        pi_display_write(&lcd, &buffer, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        BRIDGE_FBUpdate(fb, (unsigned int) imgBuff0, 0, 0, CAMERA_WIDTH, CAMERA_HEIGHT, NULL);
        #else
        #if defined(HAVE_DISPLAY)
        pi_display_write(&lcd, &buffer, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        #endif  /* HAVE_DISPLAY */
        #if defined(USE_BRIDGE)
        BRIDGE_FBUpdate(fb, (unsigned int) imgBuff0, 0, 0, CAMERA_WIDTH, CAMERA_HEIGHT, NULL);
        #endif  /* USE_BRIDGE */
        #endif  /* HAVE_DISPLAY && USE_BRIDGE */
        #endif  /* ASYNC */
    }
    pmsis_exit(0);
}

int main(void)
{
    printf("\n\t*** PMSIS Camera with LCD Test ***\n\n");
    return pmsis_kickoff((void *) test_camera_with_lcd);
}

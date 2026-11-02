// The RT1011 side of hal.h: clocks, pins, 1 ms tick, LPI2C1, LPUART1 log, USB PHY.
#include "board.h"
#include <stdarg.h>
#include <stdio.h>
#include "fsl_lpi2c.h"
#include "fsl_lpuart.h"
#include "hal.h"
#include "pins.h"
#include "tusb.h"

#define I2C_HZ 400000u
#define LOG_BAUD 115200u

// The boot ROM wants these symbols; no DCD (flash runs at the ROM's settings until XIP is up).
const uint8_t dcd_data[] = {0x00};

static volatile uint32_t ticks;

void SysTick_Handler(void) { ticks++; }
uint32_t hal_ms(void) { return ticks; }
uint32_t tusb_time_millis_api(void) { return ticks; }

static const struct {
    GPIO_Type *port;
    uint32_t pin;
} outs[OUT_COUNT] = {
    [OUT_BOOST_EN] = {PIN_BOOST_EN_PORT, PIN_BOOST_EN_PIN},
    [OUT_AN_EN] = {PIN_AN_EN_PORT, PIN_AN_EN_PIN},
    [OUT_DAC_PDN] = {PIN_DAC_PDN_PORT, PIN_DAC_PDN_PIN},
    [OUT_OSC44_EN] = {PIN_OSC44_EN_PORT, PIN_OSC44_EN_PIN},
    [OUT_OSC48_EN] = {PIN_OSC48_EN_PORT, PIN_OSC48_EN_PIN},
    [OUT_AMP_EN] = {PIN_AMP_EN_PORT, PIN_AMP_EN_PIN},
    [OUT_AMP_EN_D] = {PIN_AMP_EN_D_PORT, PIN_AMP_EN_D_PIN},
    [OUT_GAIN_HI] = {PIN_GAIN_HI_PORT, PIN_GAIN_HI_PIN},
    [OUT_LED] = {PIN_LED_STAT_PORT, PIN_LED_STAT_PIN},
}, ins[IN_COUNT] = {
    [IN_AMP_PG] = {PIN_AMP_PG_PORT, PIN_AMP_PG_PIN},
    [IN_PD_INT] = {PIN_PD_INT_PORT, PIN_PD_INT_PIN},
    [IN_GG_ALRT] = {PIN_GG_ALRT_PORT, PIN_GG_ALRT_PIN},
    [IN_BTN_UP] = {PIN_BTN_UP_PORT, PIN_BTN_UP_PIN},
    [IN_BTN_SEL] = {PIN_BTN_SEL_PORT, PIN_BTN_SEL_PIN},
    [IN_BTN_DN] = {PIN_BTN_DN_PORT, PIN_BTN_DN_PIN},
    [IN_BTN_PWR] = {PIN_PWR_BTN_S_PORT, PIN_PWR_BTN_S_PIN},
    [IN_JACK44] = {PIN_JACK44_DET_PORT, PIN_JACK44_DET_PIN},
};

void hal_set(hal_out_t pin, bool level) { GPIO_PinWrite(outs[pin].port, outs[pin].pin, level ? 1u : 0u); }
bool hal_get(hal_in_t pin) { return GPIO_PinRead(ins[pin].port, ins[pin].pin) != 0u; }

static bool i2c(uint8_t addr, lpi2c_direction_t dir, uint8_t reg, uint8_t *data, size_t len)
{
    lpi2c_master_transfer_t x = {
        .flags = kLPI2C_TransferDefaultFlag,
        .slaveAddress = addr,
        .direction = dir,
        .subaddress = reg,
        .subaddressSize = 1,
        .data = data,
        .dataSize = len,
    };
    return LPI2C_MasterTransferBlocking(LPI2C1, &x) == kStatus_Success;
}

bool hal_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len)
{
    return i2c(addr, kLPI2C_Write, reg, (uint8_t *)data, len);
}

bool hal_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c(addr, kLPI2C_Read, reg, data, len);
}

void board_log(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) LPUART_WriteBlocking(LPUART1, (const uint8_t *)buf, (size_t)(n < (int)sizeof buf ? n : (int)sizeof buf - 1));
}

static void usb_phy_init(void)
{
    CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
    USBPHY->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;
    USBPHY->PWD = 0;
    // TX trim as NXP's EVK: D_CAL 0x0C, 45-ohm termination trims 0x06
    uint32_t tx = USBPHY->TX & ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK);
    USBPHY->TX = tx | USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) | USBPHY_TX_TXCAL45DM(0x06);
}

void board_init(void)
{
    if (!(SCB->CCR & SCB_CCR_DC_Msk)) SCB_EnableDCache();
    pins_init();  // first: every enable to its safe level, the boost on
    BOARD_BootClockRUN();
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000u);

    lpuart_config_t u;
    LPUART_GetDefaultConfig(&u);
    u.baudRate_Bps = LOG_BAUD;
    u.enableTx = true;
    u.enableRx = true;
    LPUART_Init(LPUART1, &u, BOARD_BOOTCLOCKRUN_UART_CLK_ROOT);

    lpi2c_master_config_t m;
    LPI2C_MasterGetDefaultConfig(&m);
    m.baudRate_Hz = I2C_HZ;
    LPI2C_MasterInit(LPI2C1, &m, BOARD_BOOTCLOCKRUN_LPI2C_CLK_ROOT);

    CLOCK_EnableClock(kCLOCK_Ocotp);  // the USB serial number reads the unique ID fuses
    usb_phy_init();
    NVIC_SetPriority(USB_OTG1_IRQn, 1);
}

void USB_OTG1_IRQHandler(void) { tusb_int_handler(0, true); }

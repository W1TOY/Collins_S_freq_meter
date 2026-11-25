/*
 * HD44780 20 x 4 LCD via a PCF8574 I2C backpack, 4-bit mode.
 *
 * PCF8574 bit mapping (common backpack):
 *   P7..P4 = LCD D7..D4,  P3 = backlight,  P2 = EN,  P1 = R/W (always 0 = write),  P0 = RS
 * Each byte is sent as two nibbles; each nibble is written with EN=1 and then EN=0 to latch it.
 */
#include <unistd.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c-lcd.h"

#define I2C_NUM             I2C_NUM_0
#define SLAVE_ADDRESS_LCD   (0x4E >> 1)     /* 0x27: PCF8574 with A2..A0 open. Use 0x3F for a PCF8574A. */
#define I2C_TIMEOUT_MS      1000

#define LCD_BACKLIGHT   0x08
#define LCD_EN          0x04
#define LCD_RS          0x01

static const char *TAG = "LCD";

/* write one byte as two nibbles; rs = 0 for a command, LCD_RS for character data */
static void lcd_write(char value, uint8_t rs)
{
    uint8_t hi = value & 0xF0;
    uint8_t lo = (value << 4) & 0xF0;
    uint8_t ctrl = LCD_BACKLIGHT | rs;
    uint8_t data_t[4] = {
        hi | ctrl | LCD_EN,     /* high nibble, EN = 1 */
        hi | ctrl,              /* EN = 0: latch */
        lo | ctrl | LCD_EN,     /* low nibble, EN = 1 */
        lo | ctrl,              /* EN = 0: latch */
    };
    esp_err_t err = i2c_master_write_to_device(I2C_NUM, SLAVE_ADDRESS_LCD, data_t, sizeof(data_t),
                                               pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C write failed (%s)", esp_err_to_name(err));
    }
}

void lcd_send_cmd(char cmd)
{
    lcd_write(cmd, 0);
}

void lcd_send_data(char data)
{
    lcd_write(data, LCD_RS);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    usleep(5000);           /* clear takes up to 1.52 ms */
}

/* DDRAM start address of each row on a 20 x 4 display: 0x00, 0x40, 0x14, 0x54 (| 0x80 = set address) */
void lcd_put_cur(int row, int col)
{
    static const uint8_t row_base[4] = {0x80, 0xC0, 0x94, 0xD4};
    if (row < 0 || row > 3) {
        row = 0;
    }
    lcd_send_cmd(row_base[row] | col);
}

void lcd_init(void)
{
    /* 4-bit initialisation (HD44780 datasheet, "initialising by instruction") */
    usleep(50000);          /* > 40 ms after power-up */
    lcd_send_cmd(0x30);
    usleep(5000);           /* > 4.1 ms */
    lcd_send_cmd(0x30);
    usleep(200);            /* > 100 us */
    lcd_send_cmd(0x30);
    usleep(10000);
    lcd_send_cmd(0x20);     /* switch to 4-bit mode */
    usleep(10000);

    /* display set-up */
    lcd_send_cmd(0x28);     /* function set: 4-bit, 2 "lines" (= 4 rows on a 20x4), 5x8 font */
    usleep(1000);
    lcd_send_cmd(0x08);     /* display off */
    usleep(1000);
    lcd_send_cmd(0x01);     /* clear */
    usleep(2000);
    lcd_send_cmd(0x06);     /* entry mode: increment, no shift */
    usleep(1000);
    lcd_send_cmd(0x0C);     /* display on, cursor off, blink off */
    usleep(1000);
}

void lcd_send_string(char *str)
{
    while (*str) {
        lcd_send_data(*str++);
    }
}

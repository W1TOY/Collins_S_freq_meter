/*
 * Minimal driver for an HD44780 character LCD (20 x 4) behind a PCF8574 I2C backpack.
 * Uses the I2C port already configured by the application (I2C_NUM_0).
 */
#ifndef I2C_LCD_H
#define I2C_LCD_H

void lcd_init(void);                    /* 4-bit initialisation sequence, display on, cursor off */
void lcd_send_cmd(char cmd);            /* send an HD44780 command byte */
void lcd_send_data(char data);          /* send one character */
void lcd_send_string(char *str);        /* send a zero-terminated string from the cursor position */
void lcd_put_cur(int row, int col);     /* move the cursor: row 0-3, col 0-19 */
void lcd_clear(void);                   /* clear the display and home the cursor */

#endif

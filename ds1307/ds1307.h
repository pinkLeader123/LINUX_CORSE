#ifndef DS1307_H
#define DS1307_H 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/slab.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)

#define ACK 0
#define NACK 1

int DS1307_tx(struct i2c_client *client, u8 reg, u8 *data, int data_len);
int DS1307_rx(struct i2c_client *client, u8 reg, u8 *str, int data_len);
u8 DS1307_converter(u8 date);
int DS1307_reverter(u8 time);
int DS1307_update_sec(struct i2c_client *client, u8 sec);
int DS1307_update_min(struct i2c_client *client, u8 min);
int DS1307_update_hrs(struct i2c_client *client, u8 hrs);
int DS1307_update_time(struct i2c_client *client, u8 hrs, u8 min, u8 sec);
int DS1307_get_time(struct i2c_client *client, u8 *str);
void int2str(int val, char str[]); 
void example_usage(struct i2c_client *client); 
int time2sec(int h, int m, int s); 
MODULE_LICENSE("GPL");

#endif
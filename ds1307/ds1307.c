// ds1307.c (fixed)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>           // kmalloc/kfree
#include <linux/uaccess.h>        // copy_to_user
#include <linux/miscdevice.h>     // misc_register / struct miscdevice
#include <linux/fs.h>             // file_operations
#include <linux/atomic.h>         // atomic_t
#include "ds1307.h"

#define DRIVER_NAME "ds1307"
#define MISC_DEVICE_NAME "rtc_time"
#define LED_IOC_MAGIC 'k'
#define GET_TIME_CMD _IOR(LED_IOC_MAGIC, 1, int)

static struct task_struct *ds1307_thread;
static atomic_t time_to_user = ATOMIC_INIT(0); /* use atomic to avoid race */

int DS1307_tx(struct i2c_client *client, u8 reg, u8 *data, int data_len)
{
    int ret;
    u8 buf[8]; /* 1 reg + up to 7 bytes */
    if (data_len <= 0 || data_len > 7)
        return -EINVAL;
    buf[0] = reg;
    memcpy(&buf[1], data, data_len);
    ret = i2c_master_send(client, buf, 1 + data_len);
    if (ret != 1 + data_len) {
        dev_err(&client->dev, "Failed to write reg 0x%02x (len %d): %d\n", reg, data_len, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

int DS1307_rx(struct i2c_client *client, u8 reg, u8 *str, int data_len)
{
    int ret;
    if (data_len <= 0 || data_len > 7)
        return -EINVAL;
    /* write register pointer */
    ret = i2c_master_send(client, &reg, 1);
    if (ret != 1) {
        dev_err(&client->dev, "Failed to write reg 0x%02x: %d\n", reg, ret);
        return (ret < 0) ? ret : -EIO;
    }
    /* read block */
    ret = i2c_master_recv(client, str, data_len);
    if (ret != data_len) {
        dev_err(&client->dev, "Failed to read %d bytes from reg 0x%02x: %d\n", data_len, reg, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

u8 DS1307_converter(u8 date)
{
    u8 tens = date / 10;
    u8 units = date % 10;
    return (tens << 4) | units;
}

int DS1307_reverter(u8 time)
{
    u8 tens = time >> 4;
    u8 units = time & 0x0F;
    return (tens * 10) + units;
}

int DS1307_update_sec(struct i2c_client *client, u8 sec)
{
    u8 bcd_sec = DS1307_converter(sec) & 0x7F; /* clear CH bit */
    return DS1307_tx(client, 0x00, &bcd_sec, 1);
}

int DS1307_update_min(struct i2c_client *client, u8 min)
{
    u8 bcd_min = DS1307_converter(min);
    return DS1307_tx(client, 0x01, &bcd_min, 1);
}

int DS1307_update_hrs(struct i2c_client *client, u8 hrs)
{
    u8 bcd_hrs = DS1307_converter(hrs);
    return DS1307_tx(client, 0x02, &bcd_hrs, 1);
}

/* better: write seconds, minutes, hours in one transaction */
int DS1307_update_time(struct i2c_client *client, u8 hrs, u8 min, u8 sec)
{
    u8 buf[3];
    buf[0] = DS1307_converter(sec) & 0x7F; /* ensure CH=0 */
    buf[1] = DS1307_converter(min);
    buf[2] = DS1307_converter(hrs);
    return DS1307_tx(client, 0x00, buf, 3);
}

int DS1307_get_time(struct i2c_client *client, u8 *str)
{
    return DS1307_rx(client, 0x00, str, 7); /* read 7 bytes */
}

void int2str(int val, char str[])
{
    if (val < 0) val = 0;
    if (val > 99) val = 99;
    str[0] = '0' + (val / 10);
    str[1] = '0' + (val % 10);
    str[2] = '\0';
}

void example_usage(struct i2c_client *client)
{
    u8 raw_time[7];
    char sec_str[3], min_str[3], hrs_str[3];
    int ret;
    int sec, min, hrs;
    ret = DS1307_get_time(client, raw_time);
    if (ret < 0)
        return;
    sec = DS1307_reverter(raw_time[0] & 0x7F);
    min = DS1307_reverter(raw_time[1]);
    hrs = DS1307_reverter(raw_time[2]);
    int2str(hrs, hrs_str);
    int2str(min, min_str);
    int2str(sec, sec_str);
    dev_info(&client->dev, "Time: %s:%s:%s\n", hrs_str, min_str, sec_str);
}

int time2sec(int h, int m, int s){
    return h*3600 + m * 60 + s;
}

static int ds1307_thread_fn(void *data)
{
    struct i2c_client *client = data;
    u8 raw_time[7];
    int hrs, min, sec;
    char time_str[9]; /* "HH:MM:SS" + NUL */

    pr_info("ds1307 thread started\n");

    while (!kthread_should_stop()) {
        if (DS1307_get_time(client, raw_time) == 0) {
            sec = DS1307_reverter(raw_time[0] & 0x7F);
            min = DS1307_reverter(raw_time[1]);
            hrs = DS1307_reverter(raw_time[2]);
            atomic_set(&time_to_user, time2sec(hrs, min, sec));
            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hrs, min, sec);
            dev_info(&client->dev, "[DS1307] Time: %s\n", time_str);
        } else {
            dev_err(&client->dev, "[DS1307] Failed to read time\n");
        }
        ssleep(1);
    }

    pr_info("ds1307 thread stopped\n");
    return 0;
}

static long time_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int tmp;

    switch (cmd) {
    case GET_TIME_CMD:
        tmp = atomic_read(&time_to_user);
        if (copy_to_user((int __user *)arg, &tmp, sizeof(tmp)))
            return -EFAULT;
        pr_info("IOCTL: Returning seconds %d\n", tmp);
        break;

    default:
        pr_warn("IOCTL: Unknown command 0x%x\n", cmd);
        ret = -ENOTTY;
    }
    return ret;
}

/* misc device */
static int misc_open(struct inode *node, struct file *filep) { return 0; }
static int misc_release(struct inode *node, struct file *filep) { return 0; }

static const struct file_operations misc_fops = {
    .owner = THIS_MODULE,
    .open = misc_open,
    .release = misc_release,
    .unlocked_ioctl = time_ioctl,
};

static struct miscdevice misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = MISC_DEVICE_NAME,
    .fops = &misc_fops,
};

static int ds1307_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;

    pr_info("ds1307: probe success, addr=0x%02x\n", client->addr);
    
    ret = DS1307_update_time(client,0,0,0); 

    ds1307_thread = kthread_run(ds1307_thread_fn, client, "ds1307_thread");
    if (IS_ERR(ds1307_thread)) {
        dev_err(&client->dev, "Failed to create ds1307 thread\n");
        ds1307_thread = NULL;
        return PTR_ERR(ds1307_thread);
    }

    ret = misc_register(&misc_device);
    if (ret) {
        dev_err(&client->dev, "%s: Failed to register misc device: %d\n", DRIVER_NAME, ret);
        /* cleanup thread before returning */
        if (ds1307_thread) {
            kthread_stop(ds1307_thread);
            ds1307_thread = NULL;
        }
        return ret;
    }
    dev_info(&client->dev, "%s: Misc device registered at /dev/%s\n", DRIVER_NAME, MISC_DEVICE_NAME);

    return 0;
}

static int ds1307_remove(struct i2c_client *client)
{
    pr_info("ds1307: remove\n");

    if (ds1307_thread) {
        pr_info("stopping ds1307 thread\n");
        kthread_stop(ds1307_thread);
        ds1307_thread = NULL;
    }

    misc_deregister(&misc_device);
    dev_info(&client->dev, "%s: Misc device deregistered.\n", DRIVER_NAME);
    return 0;
}

/* of/i2c ids */
static const struct of_device_id ds1307_of_match[] = {
    { .compatible = "dallas,ds1307" },
    { }
};
MODULE_DEVICE_TABLE(of, ds1307_of_match);

static const struct i2c_device_id ds1307_id[] = {
    { "ds1307", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ds1307_id);

static struct i2c_driver ds1307_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = ds1307_of_match,
    },
    .probe = ds1307_probe,
    .remove = ds1307_remove,
    .id_table = ds1307_id,
};

module_i2c_driver(ds1307_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dang Van Phuc");
MODULE_DESCRIPTION("DS1307 RTC driver (template)");

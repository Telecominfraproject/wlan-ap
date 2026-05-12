#include <linux/bitrev.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include <linux/param.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/irqreturn.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <asm/uaccess.h>

int wdfeed = 0;

static const struct of_device_id my_of_ids[] = {
                {.compatible = "hw-watchdog"},
                {},
};

void watchdog_feed(void)
{
    static unsigned int toggle = 0;
    gpio_set_value(wdfeed, toggle);
    toggle = !toggle;
    return;
}

static struct timer_list hw_wd_timer;

static void hw_wd_timer_expire(struct timer_list *t)
{
    watchdog_feed();
    mod_timer(&hw_wd_timer, jiffies + HZ * 5);
}

static void init_hw_wdt(void)
{
    timer_setup(&hw_wd_timer, hw_wd_timer_expire, 0);
    hw_wd_timer_expire(0);
}

static void stop_watchdog_feed(void)
{
    del_timer_sync(&hw_wd_timer);
}

static ssize_t stop_feed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "Use echo 1 > stop_feed to stop watchdog feeding\n");
}

static ssize_t stop_feed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == '1')
        stop_watchdog_feed();

    return count;
}

static DEVICE_ATTR(stop_feed, S_IWUSR | S_IRUGO, stop_feed_show, stop_feed_store);

static int hw_watch_probe(struct platform_device *dev)
{
    int wdset = 0;
    int ret;

    wdfeed = of_get_named_gpio(dev->dev.of_node, "wdfeed", 0);
    if (wdfeed > 0)
    {
        if (gpio_request(wdfeed, "watchdog-feed-gpio"))
        {
            pr_err("request GPIO%d fail\n", wdfeed);
            return -5;
        }
        gpio_direction_output(wdfeed, 0x0);
    }
    else
    {
        pr_err("of get wdfeed gpio fail\n");
        return -5;
    }

    wdset = of_get_named_gpio(dev->dev.of_node, "wdset", 0);
    if (wdset > 0)
    {
        if (gpio_request(wdset, "watchdog-set-gpio"))
        {
            pr_err("request GPIO%d fail\n", wdset);
            return -5;
        }
        gpio_direction_output(wdset, 0x0);
        gpio_set_value(wdset, 1);
    }
    else
    {
        pr_err("of get wdset gpio fail\n");
        return -5;
    }

    init_hw_wdt();

    ret = device_create_file(&dev->dev, &dev_attr_stop_feed);
    if (ret)
    {
        pr_err("Failed to create stop_feed attribute\n");
        return ret;
    }

    return 0;
}

static int hw_watch_remove(struct platform_device *dev)
{
    device_remove_file(&dev->dev, &dev_attr_stop_feed);
    printk("%s enter.\n", __func__);
    return 0;
}

static struct platform_driver hw_watchdog_driver =
    {
        .driver = {
            .name = "hw_watchdog",
            .of_match_table = my_of_ids,
            .owner = THIS_MODULE,
        },
        .probe = hw_watch_probe,
        .remove = hw_watch_remove,
};

module_platform_driver(hw_watchdog_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("hw watchdog driver");
MODULE_AUTHOR("huangyunxiang<huangyunxiang@cigtech.com>");

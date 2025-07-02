#include <linux/module.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irqreturn.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <asm/uaccess.h>

#define GPIO_MAX 		8 
#define INT_MODE_MASK 		0xf0
#define DETECT_MODE1	 	0x3
#define DETECT_MODE2	 	0x4

enum POE_TYPE {
	POE_TYPE_AT = 1,
	POE_TYPE_AF,
	POE_TYPE_BT,
	POE_TYPE_DC,
	POE_TYPE_MAX
};

struct poe_irq_data
{
	struct device *dev;
	int irq;
	struct tasklet_struct tasklet;
	int poe_type;
};

static void  *poe_gpio_base;
static void  *pse_gpio_base;
struct poe_irq_data *data = NULL;
u32 mode;

static const struct of_device_id my_of_ids[] = {
	{.compatible = "poe-judge"},
	{},
};

MODULE_DEVICE_TABLE(of,my_of_ids);

static int poe_info_proc_show(struct seq_file *m, void *v)
{
	const char *type_str = "invalid";

	switch (data->poe_type) {
		case POE_TYPE_AT: 
			type_str = "AT"; 
			break;
		case POE_TYPE_AF: 
			type_str = "AF"; 
			break;
		case POE_TYPE_BT: 
			type_str = "BT"; 
			break;
		case POE_TYPE_DC: 
			type_str = "DC"; 
			break;
	}

	seq_printf(m, "%s\n", type_str);

	return 0;
}

static void poe_tasklet_func(unsigned long data)
{
	struct poe_irq_data *irq_data = (struct poe_irq_data *)data;

	writel(0x3, pse_gpio_base+4);
	irq_data->poe_type = POE_TYPE_BT;

	return;
}

static irqreturn_t gpio_key1_irq_handler(int irq, void *dev_id)
{
	struct poe_irq_data *data = (struct poe_irq_data *)dev_id;

	disable_irq_nosync(data->irq);
	tasklet_schedule(&data->tasklet);

	return IRQ_HANDLED;
}

static int poe_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, poe_info_proc_show, NULL);
}

static const struct proc_ops poe_info_proc_fops = {
	.proc_open = poe_info_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int gpio_key_probe(struct platform_device *dev)
{
	int ret = 0;
	int irq = (-1);
	int val = 0;
	int gpio_id = 0;
	int gpio[GPIO_MAX] = {0}, gpio_val[GPIO_MAX] = {0};
	int num;
	char name[16];
	struct device_node *np = dev->dev.of_node;

	if (of_property_read_u32(np, "detect-mode", &mode)) {
		pr_err("Failed to read detect mode\n");
		return -EINVAL;
	}

	data = devm_kzalloc(&dev->dev, sizeof(struct poe_irq_data), GFP_KERNEL);
	if (!data)
	{
		return -ENOMEM;
	}

	if (mode & INT_MODE_MASK) {
		poe_gpio_base = ioremap(0x1017004, 8);
		pse_gpio_base = ioremap(0x1026000, 8);

		writel(0x2c1, pse_gpio_base);
		writel(0x0, pse_gpio_base+4);


		data->dev = &dev->dev;
		dev_set_drvdata(&dev->dev, data);

		gpio_id = of_get_named_gpio(dev->dev.of_node, "detect_gpio", 0);
		if (gpio_id < 0)
		{
			return -EINVAL;
		}

		tasklet_init(&data->tasklet, poe_tasklet_func, (unsigned long)data);

		ret = devm_gpio_request_one(&dev->dev, gpio_id, GPIOF_IN, "poe_judge");
		if(ret)
		{
			pr_err("GPIO23 request failed, ret:%d\n", ret);
			return ret;
		}

		val = readl(poe_gpio_base);
		if(val == 1)
		{
			pr_info("poe type is af or bt,close pse first\n");
			data->poe_type = POE_TYPE_AF;
		}
		else
		{
			data->poe_type = POE_TYPE_AT;
		}

		irq = gpio_to_irq(gpio_id);
		data->irq = irq;

		ret = devm_request_any_context_irq(&dev->dev, data->irq, 
				gpio_key1_irq_handler, 
				IRQF_TRIGGER_FALLING, 
				"poe_judge", data);
		if (ret)
		{
			pr_err("Request irq fail %d\n", ret);
			return ret;
		}
	} else {
		if (mode == DETECT_MODE1 || mode == DETECT_MODE2) {
			num = 3;
		} else {
			pr_err("Wrong poe detect mode\n");
			return -ENOMEM;
		}

		for (int i = 0; i < num; i++) {
			sprintf(name, "detect-gpio%d", i+1);
			gpio[i] = of_get_named_gpio(np, name, 0); 	
			ret = gpio_request(gpio[i], name);
			if (ret) {
				pr_err("Can not request gpio %d\n", i+1);
				return ret;
			}
			gpio_direction_input(gpio[i]);
			gpio_val[i] = gpio_get_value_cansleep(gpio[i]);
		}
		
		if (mode == DETECT_MODE1){
			if (gpio_val[0] > 0) {
				data->poe_type = POE_TYPE_DC;
			} else if (gpio_val[1] > 0){
				if (gpio_val[2] == 0) {
					data->poe_type = POE_TYPE_AT;
				} else {
					data->poe_type = POE_TYPE_AF;
				}
			}
		}else if (mode == DETECT_MODE2){
			switch (gpio_val[0] | (gpio_val[1] << 1) | (gpio_val[2] << 2)) {
				case 3:
					data->poe_type = POE_TYPE_AT;
					break;
				case 4:
					data->poe_type = POE_TYPE_BT;
					break;
				case 7:
					data->poe_type = POE_TYPE_AF;
					break;
			}
		}

	}

	if (!proc_create("poe_info", 0444, NULL, &poe_info_proc_fops)) {
		pr_err("Failed to create proc entry for poe_info\n");
		return -ENOMEM;
	}

	return ret;
}

static int gpio_key_remove(struct platform_device *dev)
{
	remove_proc_entry("poe_info", NULL);

	return 0;
}

static struct platform_driver poe_judge_driver =
{
	.driver = {
		.name = "poe_judge",
		.of_match_table = my_of_ids,
		.owner = THIS_MODULE,
	},
	.probe = gpio_key_probe,
	.remove = gpio_key_remove,
};

module_platform_driver(poe_judge_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Poe type judgment driver");
MODULE_AUTHOR("huangyunxiang<huangyunxiang@cigtech.com>");

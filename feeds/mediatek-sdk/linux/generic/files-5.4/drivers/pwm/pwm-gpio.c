// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2020 Axis Communications AB */

#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/err.h>
#include <linux/of.h>

struct pwm_gpio {
	struct pwm_chip chip;
	struct hrtimer hrtimer;
	struct gpio_desc *gpio;
	struct pwm_state state;
	struct pwm_state nextstate;
	spinlock_t lock;
	bool changing;
	bool running;
	bool level;
};

static unsigned long pwm_gpio_toggle(struct pwm_gpio *gpwm, bool level)
{
	const struct pwm_state *state = &gpwm->state;
	bool invert = state->polarity == PWM_POLARITY_INVERSED;

	gpwm->level = level;
	gpiod_set_value(gpwm->gpio, gpwm->level ^ invert);

	if (!state->duty_cycle || state->duty_cycle == state->period) {
		gpwm->running = false;
		return 0;
	}

	gpwm->running = true;
	return level ? state->duty_cycle : state->period - state->duty_cycle;
}

static enum hrtimer_restart pwm_gpio_timer(struct hrtimer *hrtimer)
{
	struct pwm_gpio *gpwm = container_of(hrtimer, struct pwm_gpio, hrtimer);
	unsigned long nexttoggle;
	unsigned long flags;
	bool newlevel;

	spin_lock_irqsave(&gpwm->lock, flags);

	/* Apply new state at end of current period */
	if (!gpwm->level && gpwm->changing) {
		gpwm->changing = false;
		gpwm->state = gpwm->nextstate;
		newlevel = !!gpwm->state.duty_cycle;
	} else {
		newlevel = !gpwm->level;
	}

	nexttoggle = pwm_gpio_toggle(gpwm, newlevel);
	if (nexttoggle)
		hrtimer_forward(hrtimer, hrtimer_get_expires(hrtimer),
				ns_to_ktime(nexttoggle));

	spin_unlock_irqrestore(&gpwm->lock, flags);

	return nexttoggle ? HRTIMER_RESTART : HRTIMER_NORESTART;
}

static int pwm_gpio_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			  const struct pwm_state *state)
{
	struct pwm_gpio *gpwm = container_of(chip, struct pwm_gpio, chip);
	unsigned long flags;

	if (!state->enabled)
		hrtimer_cancel(&gpwm->hrtimer);

	spin_lock_irqsave(&gpwm->lock, flags);

	if (!state->enabled) {
		gpwm->state = *state;
		gpwm->running = false;
		gpwm->changing = false;

		gpiod_set_value(gpwm->gpio, 0);
	} else if (gpwm->running) {
		gpwm->nextstate = *state;
		gpwm->changing = true;
	} else {
		unsigned long nexttoggle;

		gpwm->state = *state;
		gpwm->changing = false;

		nexttoggle = pwm_gpio_toggle(gpwm, !!state->duty_cycle);
		if (nexttoggle)
			hrtimer_start(&gpwm->hrtimer, nexttoggle,
				      HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&gpwm->lock, flags);

	return 0;
}

static void pwm_gpio_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			       struct pwm_state *state)
{
	struct pwm_gpio *gpwm = container_of(chip, struct pwm_gpio, chip);
	unsigned long flags;

	spin_lock_irqsave(&gpwm->lock, flags);

	if (gpwm->changing)
		*state = gpwm->nextstate;
	else
		*state = gpwm->state;

	spin_unlock_irqrestore(&gpwm->lock, flags);
}

static const struct pwm_ops pwm_gpio_ops = {
	.owner = THIS_MODULE,
	.apply = pwm_gpio_apply,
	.get_state = pwm_gpio_get_state,
};

static int pwm_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwm_gpio *gpwm;
	int ret;

	gpwm = devm_kzalloc(dev, sizeof(*gpwm), GFP_KERNEL);
	if (!gpwm)
		return -ENOMEM;

	spin_lock_init(&gpwm->lock);

	gpwm->gpio = devm_gpiod_get(dev, NULL, GPIOD_OUT_LOW);
	if (IS_ERR(gpwm->gpio)) {
		dev_err(dev, "could not get gpio\n");
		return PTR_ERR(gpwm->gpio);
	}

	if (gpiod_cansleep(gpwm->gpio)) {
		dev_err(dev, "sleeping GPIOs not supported\n");
		return -EINVAL;
	}

	gpwm->chip.dev = dev;
	gpwm->chip.ops = &pwm_gpio_ops;
	gpwm->chip.base = pdev->id;
	gpwm->chip.npwm = 1;

	hrtimer_init(&gpwm->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gpwm->hrtimer.function = pwm_gpio_timer;

	ret = pwmchip_add(&gpwm->chip);
	if (ret < 0) {
		dev_err(dev, "could not add pwmchip\n");
		return ret;
	}

	platform_set_drvdata(pdev, gpwm);

	return 0;
}

static int pwm_gpio_remove(struct platform_device *pdev)
{
	struct pwm_gpio *gpwm = platform_get_drvdata(pdev);

	pwm_disable(&gpwm->chip.pwms[0]);

	return pwmchip_remove(&gpwm->chip);
}

static const struct of_device_id pwm_gpio_dt_ids[] = {
	{ .compatible = "pwm-gpio" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pwm_gpio_dt_ids);

static struct platform_driver pwm_gpio_driver = {
	.driver = {
		.name = "pwm-gpio",
		.of_match_table = pwm_gpio_dt_ids,
	},
	.probe = pwm_gpio_probe,
	.remove = pwm_gpio_remove,
};

module_platform_driver(pwm_gpio_driver);

MODULE_LICENSE("GPL v2");

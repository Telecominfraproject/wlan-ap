/* SPDX-License-Identifier: ISC */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_USB_DWC3_MSM_H
#define __LINUX_USB_DWC3_MSM_H

#include <linux/pm_runtime.h>
#include <linux/usb/gadget.h>

/* EBC TRB parameters */
#define EBC_TRB_SIZE			16384

enum usb_hw_ep_mode {
	USB_EP_NONE,
	USB_EP_BAM,
	USB_EP_GSI,
	USB_EP_EBC,
};

struct dwc3;

/**
 * usb_gadget_autopm_get_async - increment PM-usage counter of usb gadget's
 * parent device.
 * @gadget: usb gadget whose parent device counter is incremented
 *
 * This routine increments @gadget parent device PM usage counter and queue an
 * autoresume request if the device is suspended. It does not autoresume device
 * directly (it only queues a request). After a successful call, the device may
 * not yet be resumed.
 *
 * This routine can run in atomic context.
 */
static inline int usb_gadget_autopm_get_async(struct usb_gadget *gadget)
{
	int status = -ENODEV;

	if (!gadget || !gadget->dev.parent)
		return status;

	status = pm_runtime_get(gadget->dev.parent);
	if (status < 0 && status != -EINPROGRESS)
		pm_runtime_put_noidle(gadget->dev.parent);

	if (status > 0 || status == -EINPROGRESS)
		status = 0;
	return status;
}

/**
 * usb_gadget_autopm_put_async - decrement PM-usage counter of usb gadget's
 * parent device.
 * @gadget: usb gadget whose parent device counter is decremented.
 *
 * This routine decrements PM-usage counter of @gadget parent device and
 * schedules a delayed autosuspend request if the counter is <= 0.
 *
 * This routine can run in atomic context.
 */
static inline void usb_gadget_autopm_put_async(struct usb_gadget *gadget)
{
	if (gadget && gadget->dev.parent)
		pm_runtime_put(gadget->dev.parent);
}

#if IS_ENABLED(CONFIG_USB_DWC3)
int qcom_ep_config(struct usb_ep *ep, struct usb_request *request, u32 bam_opts);
int qcom_ep_unconfig(struct usb_ep *ep);
int qcom_ep_update_ops(struct usb_ep *ep);
int qcom_ep_clear_ops(struct usb_ep *ep);
int qcom_ep_set_mode(struct usb_ep *ep, enum usb_hw_ep_mode mode);
#else
static inline int qcom_ep_config(struct usb_ep *ep, struct usb_request *request,
		u32 bam_opts)
{ return -ENODEV; }
static inline int qcom_ep_unconfig(struct usb_ep *ep)
{ return -ENODEV; }
int qcom_ep_update_ops(struct usb_ep *ep)
{ return -ENODEV; }
int qcom_ep_clear_ops(struct usb_ep *ep)
{ return -ENODEV; }
int qcom_ep_set_mode(struct usb_ep *ep, enum usb_hw_ep_mode mode)
{ return -ENODEV; }
#endif

#endif /* __LINUX_USB_DWC3_MSM_H */

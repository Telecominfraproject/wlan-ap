// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/usb.h>

#if LINUX_VERSION_IS_LESS(6,3,5) && \
	!LINUX_VERSION_IN_RANGE(6,1,31, 6,2,0) &&     \
	!LINUX_VERSION_IN_RANGE(5,15,114, 5,16,0) &&     \
	!LINUX_VERSION_IN_RANGE(5,10,181, 5,11,0) &&     \
	!LINUX_VERSION_IN_RANGE(5,4,244, 5,5,0)

/**
 * usb_find_endpoint() - Given an endpoint address, search for the endpoint's
 * usb_host_endpoint structure in an interface's current altsetting.
 * @intf: the interface whose current altsetting should be searched
 * @ep_addr: the endpoint address (number and direction) to find
 *
 * Search the altsetting's list of endpoints for one with the specified address.
 *
 * Return: Pointer to the usb_host_endpoint if found, %NULL otherwise.
 */
static const struct usb_host_endpoint *usb_find_endpoint(
		const struct usb_interface *intf, unsigned int ep_addr)
{
	int n;
	const struct usb_host_endpoint *ep;

	n = intf->cur_altsetting->desc.bNumEndpoints;
	ep = intf->cur_altsetting->endpoint;
	for (; n > 0; (--n, ++ep)) {
		if (ep->desc.bEndpointAddress == ep_addr)
			return ep;
	}
	return NULL;
}

/**
 * usb_check_bulk_endpoints - Check whether an interface's current altsetting
 * contains a set of bulk endpoints with the given addresses.
 * @intf: the interface whose current altsetting should be searched
 * @ep_addrs: 0-terminated array of the endpoint addresses (number and
 * direction) to look for
 *
 * Search for endpoints with the specified addresses and check their types.
 *
 * Return: %true if all the endpoints are found and are bulk, %false otherwise.
 */
bool usb_check_bulk_endpoints(
		const struct usb_interface *intf, const u8 *ep_addrs)
{
	const struct usb_host_endpoint *ep;

	for (; *ep_addrs; ++ep_addrs) {
		ep = usb_find_endpoint(intf, *ep_addrs);
		if (!ep || !usb_endpoint_xfer_bulk(&ep->desc))
			return false;
	}
	return true;
}
EXPORT_SYMBOL_GPL(usb_check_bulk_endpoints);

/**
 * usb_check_int_endpoints - Check whether an interface's current altsetting
 * contains a set of interrupt endpoints with the given addresses.
 * @intf: the interface whose current altsetting should be searched
 * @ep_addrs: 0-terminated array of the endpoint addresses (number and
 * direction) to look for
 *
 * Search for endpoints with the specified addresses and check their types.
 *
 * Return: %true if all the endpoints are found and are interrupt,
 * %false otherwise.
 */
bool usb_check_int_endpoints(
		const struct usb_interface *intf, const u8 *ep_addrs)
{
	const struct usb_host_endpoint *ep;

	for (; *ep_addrs; ++ep_addrs) {
		ep = usb_find_endpoint(intf, *ep_addrs);
		if (!ep || !usb_endpoint_xfer_int(&ep->desc))
			return false;
	}
	return true;
}
EXPORT_SYMBOL_GPL(usb_check_int_endpoints);
#endif

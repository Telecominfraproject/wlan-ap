// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/bitmap.h>
#include <linux/slab.h>

unsigned long *bitmap_alloc(unsigned int nbits, gfp_t flags)
{
	return kmalloc_array(BITS_TO_LONGS(nbits), sizeof(unsigned long),
			     flags);
}
EXPORT_SYMBOL_GPL(bitmap_alloc);

unsigned long *bitmap_zalloc(unsigned int nbits, gfp_t flags)
{
	return bitmap_alloc(nbits, flags | __GFP_ZERO);
}
EXPORT_SYMBOL_GPL(bitmap_zalloc);

void bitmap_free(const unsigned long *bitmap)
{
	kfree(bitmap);
}
EXPORT_SYMBOL_GPL(bitmap_free);

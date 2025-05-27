#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/major.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/pagemap.h>

#define PROC_NAME "rf_switch"
#define PARTITION_NAME "RF_SWITCH"
#define MAX_DATA_SIZE 2 
#define MAX_MMC_DEVICE 2

struct block_device *target_bdev = NULL;
static char current_value[MAX_DATA_SIZE] = "3"; 

static unsigned int __blkdev_sectors_to_bio_pages(sector_t nr_sects)
{
	sector_t pages = DIV_ROUND_UP_SECTOR_T(nr_sects, PAGE_SIZE / 512);

	return min(pages, (sector_t)BIO_MAX_VECS);
}

struct block_device *find_mmc_partition(void)
{
	struct gendisk *disk = NULL;
	unsigned long idx;
	struct block_device *bdev = NULL;
	unsigned int i;
	
	for (i = 0; i < MAX_MMC_DEVICE; i++) {
		bdev = blkdev_get_by_dev(MKDEV(MMC_BLOCK_MAJOR, i * CONFIG_MMC_BLOCK_MINORS), FMODE_READ | FMODE_WRITE, NULL);
		if (IS_ERR(bdev)) {
			pr_err("Failed to open MMC device %u: %ld\n", i, PTR_ERR(bdev));
			continue;
		}
	
		disk = bdev->bd_disk;
		if (!disk) {
			blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
			continue;
		}
	
		xa_for_each_start(&disk->part_tbl, idx, bdev, 1) {
			if (bdev->bd_meta_info && strcmp(bdev->bd_meta_info->volname, PARTITION_NAME) == 0) {
				pr_info("Found RF_SWITCH partition at device %u\n", i);
				return bdev;
			}
		}
	
		blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
	}
	
	return NULL;
}

int read_string_from_emmc(struct block_device *bdev, size_t max_length, char *buffer)
{
	struct bio *bio;
	struct page *page;
	int err = 0;
	void *data;

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		return -ENOMEM;
	}

	bio = bio_alloc(bdev, __blkdev_sectors_to_bio_pages(1), REQ_OP_READ, GFP_KERNEL);
	bio_set_dev(bio, bdev);
	bio->bi_iter.bi_sector = 0;
	bio_add_page(bio, page, PAGE_SIZE, 0);

	submit_bio_wait(bio);

	if (bio->bi_status) {
		err = -EIO;
		goto out_bio;
	}

	data = kmap(page);
	kunmap(page);
	memcpy(buffer, data, max_length - 1);
	buffer[max_length - 1] = '\0';

out_bio:
	bio_put(bio);
	__free_page(page);

	return err;
}

static int rf_switch_proc_show(struct seq_file *m, void *v)
{
	char buffer[MAX_DATA_SIZE] = {0};
	int ret;
	
	ret = read_string_from_emmc(target_bdev, MAX_DATA_SIZE, buffer);
	if (ret) {
		seq_printf(m, "%s\n", current_value);
		return 0;
	}
	
	if (strcmp(buffer, "2") == 0 || strcmp(buffer, "3") == 0) {
		strncpy(current_value, buffer, MAX_DATA_SIZE);
		seq_printf(m, "%s\n", current_value);
	} else {
		seq_printf(m, "%s\n", current_value);
	}
	
	return 0;
}

static int blkdev_issue_write(struct block_device *bdev, sector_t sector, sector_t nr_sects, gfp_t gfp_mask, struct page *page)
{
	int ret = 0;
	sector_t bs_mask;
	struct bio *bio;
	
	int bi_size = 0;
	unsigned int sz;
	
	if (bdev_read_only(bdev))
		return -EPERM;
	
	bs_mask = (bdev_logical_block_size(bdev) >> 9) - 1;
	if ((sector | nr_sects) & bs_mask)
		return -EINVAL;
	
	bio = bio_alloc(bdev, __blkdev_sectors_to_bio_pages(nr_sects),
			REQ_OP_WRITE, gfp_mask);
	if (!bio) {
		pr_err("Couldn't alloc bio");
		return -1;
	}
	
	bio->bi_iter.bi_sector = sector;
	bio_set_dev(bio, bdev);
	bio_set_op_attrs(bio, REQ_OP_WRITE, 0);
	
	sz = bdev_logical_block_size(bdev);
	bi_size = bio_add_page(bio, page, sz, 0);
	
	if(bi_size != sz) {
		pr_err("Couldn't add page to the log block");
		goto error;
	}
	if (bio)
	{
		ret = submit_bio_wait(bio);
		bio_put(bio);
	}
	
	return ret;
error:
	bio_put(bio);
	return -1;
}

static int write_data_to_emmc(struct block_device *bdev, const unsigned char *data, unsigned char fill_byte)
{
	struct page *page;
	void *ptr;
	sector_t sector_offset = 0;
	int ret = 0;
	
	if (!bdev || !data)
		return -EINVAL;
	
	page = alloc_page(GFP_KERNEL);
	if (!page) {
		pr_err("Failed to allocate page\n");
		return -ENOMEM;
	}
	
	ptr = kmap_atomic(page);
	
	memcpy(ptr, data, MAX_DATA_SIZE);
			
	memset(ptr + MAX_DATA_SIZE, fill_byte, 512-MAX_DATA_SIZE);
	kunmap_atomic(ptr);
		
	ret = blkdev_issue_write(bdev, sector_offset , 1 ,GFP_ATOMIC, page);
	if (ret) {
		pr_err("Failed to write to eMMC at offset 0: %d\n", ret);
		__free_page(page);
		return ret;
	}
	
	sync_blockdev(bdev); 
	__free_page(page);
	return ret;
}

static ssize_t rf_switch_proc_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	unsigned char buffer[MAX_DATA_SIZE] = {0};
	int ret;
	
	if (count != MAX_DATA_SIZE)
		return -EINVAL;
	
	if (copy_from_user(buffer, user_buffer, count))
		return -EFAULT;
	
	buffer[count -1] = '\0';
	
	if (strcmp(buffer, "2") != 0 && strcmp(buffer, "3") != 0) {
		pr_err("Invalid value: %s. Only '2' or '3' are allowed.\n", buffer);
		return -EINVAL;
	}
	
	ret = write_data_to_emmc(target_bdev,  buffer, 0xFF);
	if (ret) {
		pr_err("Failed to write to RF_SWITCH\n");
		return ret;
	}
	
	strncpy(current_value, buffer, MAX_DATA_SIZE);
	
	return count;
}

static int rf_switch_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rf_switch_proc_show, NULL);
}

static const struct proc_ops rf_switch_proc_fops = {
	.proc_open = rf_switch_proc_open,
	.proc_read = seq_read,
	.proc_write = rf_switch_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init rf_switch_init(void)
{
	target_bdev = find_mmc_partition();
	if (!target_bdev) {
		pr_err("Failed to find eMMC card or RF_SWITCH partition\n");
		return -ENOMEM;
	}
	
	if (!proc_create(PROC_NAME, 0666, NULL, &rf_switch_proc_fops)) {
		pr_err("Failed to create proc entry\n");
		return -ENOMEM;
	}
	
	pr_info("RF_SWITCH partition proc interface created\n");
	return 0;
}

static void __exit rf_switch_exit(void)
{
	if (target_bdev) {
		blkdev_put(target_bdev, FMODE_READ | FMODE_WRITE);
		target_bdev = NULL;
	}
	remove_proc_entry(PROC_NAME, NULL);
	pr_info("RF_SWITCH partition proc interface removed\n");
}

module_init(rf_switch_init);
module_exit(rf_switch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yunxiang Huang");
MODULE_DESCRIPTION("RF_SWITCH partition read/write driver");

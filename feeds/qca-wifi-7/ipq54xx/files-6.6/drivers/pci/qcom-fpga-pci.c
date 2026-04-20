// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * FPGA PCIe driver to access the FPGA via PCI
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>

#include <linux/qcom-fpga-pci.h>

struct qcom_fpga_pci_priv {
	void __iomem *mmio_addr_base;
	struct pci_dev *pci_dev;
	u64 last_prog_reg;
};

static struct qcom_fpga_pci_priv *qcom_fpga_pci;
static DEFINE_SPINLOCK(pci_lock);

void qcom_program_window(u64 reg)
{
	int retry = 100000;

	if (qcom_fpga_pci->last_prog_reg == reg)
		return;

	writel(reg, qcom_fpga_pci->mmio_addr_base +
	       PCIE_MEM_ACCESS_BASE_ADDR_REG);

	while (retry--) {
		udelay(1);
		if (readl(qcom_fpga_pci->mmio_addr_base +
			  PCIE_MEM_ACCESS_BASE_ADDR_REG) == reg)
			break;
	}

	qcom_fpga_pci->last_prog_reg = reg;
}

/**
 * qcom_fpga_mem_read() - Read the value from the FPGA register through PCI
 *
 * @reg : FPGA register value
 *
 * The read value will be returned on success, a negative errno will be
 * returned in error cases.
 */
u64 qcom_fpga_mem_read(u64 reg)
{
	u64 base_addr, new_addr, val;
	unsigned long flags;

	spin_lock_irqsave(&pci_lock, flags);

	new_addr = QCOM_GET_NEW_ADDR(reg);
	base_addr = QCOM_GET_BASE_ADDR(reg);

	qcom_program_window(base_addr);

	val = readl(qcom_fpga_pci->mmio_addr_base + new_addr);

	spin_unlock_irqrestore(&pci_lock, flags);

	return val;
}
EXPORT_SYMBOL(qcom_fpga_mem_read);

/**
 * qcom_fpga_mem_write() - Write the value to the FPGA register through PCI
 *
 * @reg : FPGA register value
 * @val : Value to be written in FPGA register
 *
 */
void qcom_fpga_mem_write(u64 reg, u64 val)
{
	u64 base_addr, new_addr;
	unsigned long flags;

	spin_lock_irqsave(&pci_lock, flags);

	new_addr = QCOM_GET_NEW_ADDR(reg);
	base_addr = QCOM_GET_BASE_ADDR(reg);

	qcom_program_window(base_addr);

	writel(val, qcom_fpga_pci->mmio_addr_base + new_addr);

	spin_unlock_irqrestore(&pci_lock, flags);
}
EXPORT_SYMBOL(qcom_fpga_mem_write);

/**
 * qcom_fpga_bulk_reg_write() - Sequentially write the bulk values to the
 *                              FPGA registers through PCI
 *
 * @reg : Start of the FPGA register value for the sequential write
 * @val : Array of values to be written in FPGA register
 * @val_count : Number of values to be written
 *
 */
u64 qcom_fpga_bulk_reg_write(u64 reg, const u64 *val,
			     size_t val_count)
{
	u64 i;

	for (i = 0; i < val_count; i++) {
		qcom_fpga_mem_write(reg, val[i]);
		reg += 4;
	}

	return val_count;
}
EXPORT_SYMBOL(qcom_fpga_bulk_reg_write);

/**
 * qcom_fpga_bulk_reg_read() - Sequentially read the bulk values from the
 *                             FPGA registers through PCI
 *
 * @reg : Start of the FPGA register value for the sequential read
 * @val : Array of values to be read from the FPGA register
 * @val_count : Number of values to be read
 *
 */
u64 qcom_fpga_bulk_reg_read(u64 reg, u64 *val, size_t val_count)
{
	u64 i;

	for (i = 0; i < val_count; i++) {
		val[i] = qcom_fpga_mem_read(reg);
		reg += 4;
	}

	return val_count;
}
EXPORT_SYMBOL(qcom_fpga_bulk_reg_read);

/**
 * qcom_fpga_multi_reg_write() - Write the multiple FPGA registers through PCI
 *
 * @regs : Array of structures containing register,value to be written
 * @num_regs : Number of registers to write
 *
 */
u64 qcom_fpga_multi_reg_write(const struct reg_sequence *regs,
			      u64 num_regs)
{
	u64 i;

	for (i = 0; i < num_regs; i++) {
		qcom_fpga_mem_write(regs[i].reg, regs[i].def);
		if (regs[i].delay_us)
			udelay(regs[i].delay_us);
	}

	return num_regs;
}
EXPORT_SYMBOL(qcom_fpga_multi_reg_write);

/**
 * qcom_fpga_multi_reg_read() - Read the multiple FPGA registers through PCI
 *
 * @regs : Array of structures containing register,value to be read
 * @num_regs : Number of registers to read
 *
 */
u64 qcom_fpga_multi_reg_read(struct reg_sequence *regs, u64 num_regs)
{
	u64 i;

	for (i = 0; i < num_regs; i++) {
		regs[i].def = qcom_fpga_mem_read(regs[i].reg);
		if (regs[i].delay_us)
			udelay(regs[i].delay_us);
	}

	return num_regs;
}
EXPORT_SYMBOL(qcom_fpga_multi_reg_read);

static ssize_t fpga_reg_write_store(struct device *device,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	u64 addr, val;

	if (sscanf(buf, "%llX %llX", &addr, &val) != 2)
		return -EINVAL;

	qcom_fpga_mem_write(addr, val);

	return count;
}

static ssize_t fpga_reg_read_store(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	u64 addr, val;

	if (kstrtoull(buf, 16, &addr))
		return -EINVAL;

	val = qcom_fpga_mem_read(addr);

	pr_info("\n0x%llX: 0x%llX\n", addr, val);
	return count;
}

static DEVICE_ATTR(fpga_reg_write, 0644, NULL, fpga_reg_write_store);
static DEVICE_ATTR(fpga_reg_read, 0644, NULL, fpga_reg_read_store);

static struct attribute *qcom_fpga_reg_wr_attrs[] = {
	&dev_attr_fpga_reg_write.attr,
	&dev_attr_fpga_reg_read.attr,
	NULL,
};

ATTRIBUTE_GROUPS(qcom_fpga_reg_wr);

static int fpga_pci_init(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int  region, ret = 0;
	void __iomem * const *iomap_table;

	qcom_fpga_pci = kmalloc(sizeof(*qcom_fpga_pci), GFP_KERNEL);
	if (!qcom_fpga_pci)
		return -ENOMEM;

	memset(qcom_fpga_pci, 0, sizeof(struct qcom_fpga_pci_priv));

	qcom_fpga_pci->pci_dev = pdev;

	/* Enable device (incl. PCI PM wakeup and hotplug setup) */
	ret = pcim_enable_device(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to Enable PCI device Error:%d\n",
			ret);
		return ret;
	}

	ret = pcim_set_mwi(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Mem-Wr-Inval unavailable Error:%d\n",
			ret);
		return ret;
	}

	/* Use the first MMIO region */
	region = ffs(pci_select_bars(pdev, IORESOURCE_MEM)) - 1;
	if (region < 0) {
		dev_err(&pdev->dev, "No MMIO resource found\n");
		return -ENODEV;
	}

	/* Check for weird/broken PCI region reporting */
	if (pci_resource_len(pdev, region) < 256) {
		dev_err(&pdev->dev, "Invalid PCI region size(s), aborting\n");
		return -ENODEV;
	}

	ret = pcim_iomap_regions(pdev, BIT(region), MODULENAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot remap MMIO, aborting\n");
		return ret;
	}

	iomap_table = pcim_iomap_table(pdev);
	if (!iomap_table) {
		dev_err(&pdev->dev, "pci iomap allocation table failed\n");
		return -ENOMEM;
	}

	qcom_fpga_pci->mmio_addr_base = iomap_table[region];
	if (!qcom_fpga_pci->mmio_addr_base) {
		dev_err(&pdev->dev, "unable to map PCI I/O memory regions\n");
		return -ENOMEM;
	}

	pci_set_master(pdev);

	return 0;
}

static void fpga_pci_remove(struct pci_dev *pdev)
{
	kfree(qcom_fpga_pci);
}

static const struct pci_device_id fpga_pci_tbl[] = {
	{QTI_FPGA_PON_VENDOR_ID, QTI_FPGA_PON_DEVICE_ID_1, PCI_ANY_ID,
		PCI_ANY_ID},
	{QTI_FPGA_PON_VENDOR_ID, QTI_FPGA_PON_DEVICE_ID_2, PCI_ANY_ID,
		PCI_ANY_ID},
	{QTI_FPGA_PON_VENDOR_ID, QTI_FPGA_PON_DEVICE_ID_3, PCI_ANY_ID,
		PCI_ANY_ID},
	{QTI_FPGA_PON_VENDOR_ID, QTI_FPGA_PON_DEVICE_ID_4, PCI_ANY_ID,
		PCI_ANY_ID},
	{}
};

static struct pci_driver fpga_pci_driver = {
	.name		= MODULENAME,
	.id_table	= fpga_pci_tbl,
	.probe		= fpga_pci_init,
	.remove		= fpga_pci_remove,
	.dev_groups	= qcom_fpga_reg_wr_groups,
};

MODULE_DEVICE_TABLE(pci, fpga_pci_tbl);
module_pci_driver(fpga_pci_driver);
MODULE_LICENSE("GPL");

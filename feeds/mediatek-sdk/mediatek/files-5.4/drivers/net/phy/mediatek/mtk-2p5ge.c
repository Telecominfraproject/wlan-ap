// SPDX-License-Identifier: GPL-2.0+
#include <linux/bitfield.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/phy.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>

#include "mtk.h"

#define MTK_2P5GPHY_ID_MT7987	(0x00339c91)
#define MTK_2P5GPHY_ID_MT7988	(0x00339c11)

#define MT7987_2P5GE_PMB_FW		"mediatek/mt7987/i2p5ge-phy-pmb.bin"
#define MT7987_2P5GE_PMB_FW_SIZE	(0x18000)
#define MT7987_2P5GE_DSPBITTB \
	"mediatek/mt7987/i2p5ge-phy-DSPBitTb.bin"
#define MT7987_2P5GE_DSPBITTB_SIZE	(0x7000)

#define MT7988_2P5GE_PMB_FW		"mediatek/mt7988/i2p5ge-phy-pmb.bin"
#define MT7988_2P5GE_PMB_FW_SIZE	(0x20000)
#define MT7988_2P5GE_PMB_FW_BASE	(0x0f100000)
#define MT7988_2P5GE_PMB_FW_LEN		(0x20000)

#define MTK_2P5GPHY_PMD_REG_BASE	(0x0f010000)
#define MTK_2P5GPHY_PMD_REG_LEN		(0x210)
#define DO_NOT_RESET			(0x28)
#define   DO_NOT_RESET_XBZ		BIT(0)
#define   DO_NOT_RESET_PMA		BIT(3)
#define   DO_NOT_RESET_RX		BIT(5)
#define FNPLL_PWR_CTRL1			(0x208)
#define   RG_SPEED_MASK			GENMASK(3, 0)
#define   RG_SPEED_2500			BIT(3)
#define   RG_SPEED_100			BIT(0)
#define FNPLL_PWR_CTRL_STATUS		(0x20c)
#define   RG_STABLE_MASK		GENMASK(3, 0)
#define   RG_SPEED_2500_STABLE		BIT(3)
#define   RG_SPEED_100_STABLE		BIT(0)

#define MTK_2P5GPHY_XBZ_PCS_REG_BASE	(0x0f030000)
#define MTK_2P5GPHY_XBZ_PCS_REG_LEN	(0x844)
#define PHY_CTRL_CONFIG			(0x200)
#define PMU_WP				(0x800)
#define   WRITE_PROTECT_KEY		(0xCAFEF00D)
#define PMU_PMA_AUTO_CFG		(0x820)
#define   POWER_ON_AUTO_MODE		BIT(16)
#define   PMU_AUTO_MODE_EN		BIT(0)
#define PMU_PMA_STATUS			(0x840)
#define   CLK_IS_DISABLED		BIT(3)

#define MTK_2P5GPHY_XBZ_PMA_RX_BASE	(0x0f080000)
#define MTK_2P5GPHY_XBZ_PMA_RX_LEN	(0x5228)
#define SMEM_WDAT0			(0x5000)
#define SMEM_WDAT1			(0x5004)
#define SMEM_WDAT2			(0x5008)
#define SMEM_WDAT3			(0x500c)
#define SMEM_CTRL			(0x5024)
#define   SMEM_HW_RDATA_ZERO		BIT(24)
#define SMEM_ADDR_REF_ADDR		(0x502c)
#define CM_CTRL_P01			(0x5100)
#define CM_CTRL_P23			(0x5124)
#define DM_CTRL_P01			(0x5200)
#define DM_CTRL_P23			(0x5224)

#define MTK_2P5GPHY_CHIP_SCU_BASE	(0x0f0cf800)
#define MTK_2P5GPHY_CHIP_SCU_LEN	(0x12c)
#define SYS_SW_RESET			(0x128)
#define   RESET_RST_CNT			BIT(0)

#define MTK_2P5GPHY_MCU_CSR_BASE	(0x0f0f0000)
#define MTK_2P5GPHY_MCU_CSR_LEN		(0x20)
#define MD32_EN_CFG			(0x18)
#define   MD32_EN			BIT(0)

#define MTK_2P5GPHY_PMB_FW_BASE		(0x0f100000)
//#define MTK_2P5GPHY_PMB_FW_LEN		MT7988_2P5GE_PMB_FW_SIZE

#define MTK_2P5GPHY_APB_BASE		(0x11c30000)
#define MTK_2P5GPHY_APB_LEN		(0x9c)
#define SW_RESET			(0x94)
#define   MD32_RESTART_EN_CLEAR		BIT(9)

#define BASE100T_STATUS_EXTEND		(0x10)
#define BASE1000T_STATUS_EXTEND		(0x11)
#define EXTEND_CTRL_AND_STATUS		(0x16)

#define PHY_AUX_CTRL_STATUS		(0x1d)
#define   PHY_AUX_DPX_MASK		GENMASK(5, 5)
#define   PHY_AUX_SPEED_MASK		GENMASK(4, 2)

/* Registers on MDIO_MMD_VEND1 */
#define MTK_PHY_LINK_STATUS_RELATED		(0x147)
#define   MTK_PHY_BYPASS_LINK_STATUS_OK		BIT(4)
#define   MTK_PHY_FORCE_LINK_STATUS_HCD		BIT(3)

#define MTK_PHY_PMA_PMD_SPPED_ABILITY		0x300

#define MTK_PHY_AN_FORCE_SPEED_REG		(0x313)
#define   MTK_PHY_MASTER_FORCE_SPEED_SEL_EN	BIT(7)
#define   MTK_PHY_MASTER_FORCE_SPEED_SEL_MASK	GENMASK(6, 0)

#define MTK_PHY_LPI_PCS_DSP_CTRL		(0x121)
#define   MTK_PHY_LPI_SIG_EN_LO_THRESH100_MASK	GENMASK(12, 8)

/* Registers on Token Ring debug nodes */
/* ch_addr = 0x0, node_addr = 0xf, data_addr = 0x3c */
#define AUTO_NP_10XEN				BIT(6)

struct mtk_i2p5ge_phy_priv {
	bool fw_loaded;
	int pd_en;
};

enum {
	PHY_AUX_SPD_10 = 0,
	PHY_AUX_SPD_100,
	PHY_AUX_SPD_1000,
	PHY_AUX_SPD_2500,
};

static int mt7987_2p5ge_phy_load_fw(struct phy_device *phydev)
{
	struct mtk_i2p5ge_phy_priv *priv = phydev->priv;
	struct device *dev = &phydev->mdio.dev;
	void __iomem *xbz_pcs_reg_base;
	void __iomem *xbz_pma_rx_base;
	void __iomem *chip_scu_base;
	void __iomem *pmd_reg_base;
	void __iomem *mcu_csr_base;
	const struct firmware *fw;
	void __iomem *apb_base;
	void __iomem *pmb_addr;
	int ret, i;
	u32 reg;

	if (priv->fw_loaded)
		return 0;

	apb_base = ioremap(MTK_2P5GPHY_APB_BASE,
			   MTK_2P5GPHY_APB_LEN);
	if (!apb_base)
		return -ENOMEM;

	pmd_reg_base = ioremap(MTK_2P5GPHY_PMD_REG_BASE,
			       MTK_2P5GPHY_PMD_REG_LEN);
	if (!pmd_reg_base) {
		ret = -ENOMEM;
		goto free_apb;
	}

	xbz_pcs_reg_base = ioremap(MTK_2P5GPHY_XBZ_PCS_REG_BASE,
				   MTK_2P5GPHY_XBZ_PCS_REG_LEN);
	if (!xbz_pcs_reg_base) {
		ret = -ENOMEM;
		goto free_pmd;
	}

	xbz_pma_rx_base = ioremap(MTK_2P5GPHY_XBZ_PMA_RX_BASE,
				  MTK_2P5GPHY_XBZ_PMA_RX_LEN);
	if (!xbz_pma_rx_base) {
		ret = -ENOMEM;
		goto free_pcs;
	}

	chip_scu_base = ioremap(MTK_2P5GPHY_CHIP_SCU_BASE,
				MTK_2P5GPHY_CHIP_SCU_LEN);
	if (!chip_scu_base) {
		ret = -ENOMEM;
		goto free_pma;
	}

	mcu_csr_base = ioremap(MTK_2P5GPHY_MCU_CSR_BASE,
			       MTK_2P5GPHY_MCU_CSR_LEN);
	if (!mcu_csr_base) {
		ret = -ENOMEM;
		goto free_chip_scu;
	}

	pmb_addr = ioremap(MTK_2P5GPHY_PMB_FW_BASE, MT7987_2P5GE_PMB_FW_SIZE);
	if (!pmb_addr) {
		return -ENOMEM;
		goto free_mcu_csr;
	}

	ret = request_firmware(&fw, MT7987_2P5GE_PMB_FW, dev);
	if (ret) {
		dev_err(dev, "failed to load firmware: %s, ret: %d\n",
			MT7987_2P5GE_PMB_FW, ret);
		goto free_pmb_addr;
	}

	if (fw->size != MT7987_2P5GE_PMB_FW_SIZE) {
		dev_err(dev, "PMb firmware size 0x%zx != 0x%x\n",
			fw->size, MT7987_2P5GE_PMB_FW_SIZE);
		ret = -EINVAL;
		goto release_fw;
	}

	/* Force 2.5Gphy back to AN state */
	phy_set_bits(phydev, MII_BMCR, BMCR_RESET);
	usleep_range(5000, 6000);
	phy_set_bits(phydev, MII_BMCR, BMCR_PDOWN);

	reg = readw(apb_base + SW_RESET);
	writew(reg & ~MD32_RESTART_EN_CLEAR, apb_base + SW_RESET);
	writew(reg | MD32_RESTART_EN_CLEAR, apb_base + SW_RESET);
	writew(reg & ~MD32_RESTART_EN_CLEAR, apb_base + SW_RESET);

	reg = readw(mcu_csr_base + MD32_EN_CFG);
	writew(reg & ~MD32_EN, mcu_csr_base + MD32_EN_CFG);

	for (i = 0; i < MT7987_2P5GE_PMB_FW_SIZE - 1; i += 4)
		writel(*((uint32_t *)(fw->data + i)), pmb_addr + i);
	dev_info(dev, "Firmware date code: %x/%x/%x, version: %x.%x\n",
		 be16_to_cpu(*((__be16 *)(fw->data +
					  MT7987_2P5GE_PMB_FW_SIZE - 8))),
		 *(fw->data + MT7987_2P5GE_PMB_FW_SIZE - 6),
		 *(fw->data + MT7987_2P5GE_PMB_FW_SIZE - 5),
		 *(fw->data + MT7987_2P5GE_PMB_FW_SIZE - 2),
		 *(fw->data + MT7987_2P5GE_PMB_FW_SIZE - 1));
	release_firmware(fw);

	/* Enable 100Mbps module clock. */
	writel(FIELD_PREP(RG_SPEED_MASK, RG_SPEED_100),
	       pmd_reg_base + FNPLL_PWR_CTRL1);

	/* Check if 100Mbps module clock is ready. */
	ret = readl_poll_timeout(pmd_reg_base + FNPLL_PWR_CTRL_STATUS, reg,
				 reg & RG_SPEED_100_STABLE, 1, 10000);
	if (ret)
		dev_err(dev, "Fail to enable 100Mbps module clock: %d\n", ret);

	/* Enable 2.5Gbps module clock. */
	writel(FIELD_PREP(RG_SPEED_MASK, RG_SPEED_2500),
	       pmd_reg_base + FNPLL_PWR_CTRL1);

	/* Check if 2.5Gbps module clock is ready. */
	ret = readl_poll_timeout(pmd_reg_base + FNPLL_PWR_CTRL_STATUS, reg,
				 reg & RG_SPEED_2500_STABLE, 1, 10000);

	if (ret)
		dev_err(dev, "Fail to enable 2.5Gbps module clock: %d\n", ret);

	/* Disable AN */
	phy_clear_bits(phydev, MII_BMCR, BMCR_ANENABLE);

	/* Force to run at 2.5G speed */
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, MTK_PHY_AN_FORCE_SPEED_REG,
		       MTK_PHY_MASTER_FORCE_SPEED_SEL_MASK,
		       MTK_PHY_MASTER_FORCE_SPEED_SEL_EN |
		       FIELD_PREP(MTK_PHY_MASTER_FORCE_SPEED_SEL_MASK, 0x1b));

	phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, MTK_PHY_LINK_STATUS_RELATED,
			 MTK_PHY_BYPASS_LINK_STATUS_OK |
			 MTK_PHY_FORCE_LINK_STATUS_HCD);

	/* Set xbz, pma and rx as "do not reset" in order to input DSP code. */
	reg = readl(pmd_reg_base + DO_NOT_RESET);
	reg |= DO_NOT_RESET_XBZ | DO_NOT_RESET_PMA | DO_NOT_RESET_RX;
	writel(reg, pmd_reg_base + DO_NOT_RESET);

	reg = readl(chip_scu_base + SYS_SW_RESET);
	writel(reg & ~RESET_RST_CNT, chip_scu_base + SYS_SW_RESET);

	writel(WRITE_PROTECT_KEY, xbz_pcs_reg_base + PMU_WP);

	reg = readl(xbz_pcs_reg_base + PMU_PMA_AUTO_CFG);
	reg |= PMU_AUTO_MODE_EN | POWER_ON_AUTO_MODE;
	writel(reg, xbz_pcs_reg_base + PMU_PMA_AUTO_CFG);

	/* Check if clock in auto mode is disabled. */
	ret = readl_poll_timeout(xbz_pcs_reg_base + PMU_PMA_STATUS, reg,
				 (reg & CLK_IS_DISABLED) == 0x0, 1, 100000);
	if (ret)
		dev_err(dev, "Clock isn't disabled in auto mode: %d\n", ret);

	reg = readl(xbz_pma_rx_base + SMEM_CTRL);
	writel(reg | SMEM_HW_RDATA_ZERO, xbz_pma_rx_base + SMEM_CTRL);

	reg = readl(xbz_pcs_reg_base + PHY_CTRL_CONFIG);
	writel(reg | BIT(16), xbz_pcs_reg_base + PHY_CTRL_CONFIG);

	/* Initialize data memory */
	reg = readl(xbz_pma_rx_base + DM_CTRL_P01);
	writel(reg | BIT(28), xbz_pma_rx_base + DM_CTRL_P01);
	reg = readl(xbz_pma_rx_base + DM_CTRL_P23);
	writel(reg | BIT(28), xbz_pma_rx_base + DM_CTRL_P23);

	/* Initialize coefficient memory */
	reg = readl(xbz_pma_rx_base + CM_CTRL_P01);
	writel(reg | BIT(28), xbz_pma_rx_base + CM_CTRL_P01);
	reg = readl(xbz_pma_rx_base + CM_CTRL_P23);
	writel(reg | BIT(28), xbz_pma_rx_base + CM_CTRL_P23);

	/* Initilize PM offset */
	writel(0, xbz_pma_rx_base + SMEM_ADDR_REF_ADDR);

	ret = request_firmware(&fw, MT7987_2P5GE_DSPBITTB, dev);
	if (ret) {
		dev_err(dev, "failed to load firmware: %s, ret: %d\n",
			MT7987_2P5GE_DSPBITTB, ret);
		goto free_pmb_addr;
	}
	if (fw->size != MT7987_2P5GE_DSPBITTB_SIZE) {
		dev_err(dev, "DSPBITTB size 0x%zx != 0x%x\n",
			fw->size, MT7987_2P5GE_DSPBITTB_SIZE);
		ret = -EINVAL;
		goto release_fw;
	}

	for (i = 0; i < fw->size - 1; i += 16) {
		writel(*((uint32_t *)(fw->data + i)),
		       xbz_pma_rx_base + SMEM_WDAT0);
		writel(*((uint32_t *)(fw->data + i + 0x4)),
		       xbz_pma_rx_base + SMEM_WDAT1);
		writel(*((uint32_t *)(fw->data + i + 0x8)),
		       xbz_pma_rx_base + SMEM_WDAT2);
		writel(*((uint32_t *)(fw->data + i + 0xc)),
		       xbz_pma_rx_base + SMEM_WDAT3);
	}

	reg = readl(xbz_pma_rx_base + DM_CTRL_P01);
	writel(reg & ~BIT(28), xbz_pma_rx_base + DM_CTRL_P01);

	reg = readl(xbz_pma_rx_base + DM_CTRL_P23);
	writel(reg & ~BIT(28), xbz_pma_rx_base + DM_CTRL_P23);

	reg = readl(xbz_pma_rx_base + CM_CTRL_P01);
	writel(reg & ~BIT(28), xbz_pma_rx_base + CM_CTRL_P01);

	reg = readl(xbz_pma_rx_base + CM_CTRL_P23);
	writel(reg & ~BIT(28), xbz_pma_rx_base + CM_CTRL_P23);

	reg = readw(mcu_csr_base + MD32_EN_CFG);
	writew(reg | MD32_EN, mcu_csr_base + MD32_EN_CFG);
	phy_set_bits(phydev, MII_BMCR, BMCR_RESET);
	/* We need a delay here to stabilize initialization of MCU */
	usleep_range(7000, 8000);
	dev_info(dev, "Firmware loading/trigger ok.\n");

	priv->fw_loaded = true;

release_fw:
	release_firmware(fw);
free_pmb_addr:
	iounmap(pmb_addr);
free_mcu_csr:
	iounmap(mcu_csr_base);
free_chip_scu:
	iounmap(chip_scu_base);
free_pma:
	iounmap(xbz_pma_rx_base);
free_pcs:
	iounmap(xbz_pcs_reg_base);
free_pmd:
	iounmap(pmd_reg_base);
free_apb:
	iounmap(apb_base);

	return ret;
}

static int mt7988_2p5ge_phy_load_fw(struct phy_device *phydev)
{
	struct mtk_i2p5ge_phy_priv *priv = phydev->priv;
	void __iomem *mcu_csr_base, *pmb_addr;
	struct device *dev = &phydev->mdio.dev;
	const struct firmware *fw;
	int ret, i;
	u32 reg;

	if (priv->fw_loaded)
		return 0;

	pmb_addr = ioremap(MT7988_2P5GE_PMB_FW_BASE, MT7988_2P5GE_PMB_FW_LEN);
	if (!pmb_addr)
		return -ENOMEM;
	mcu_csr_base = ioremap(MTK_2P5GPHY_MCU_CSR_BASE,
			       MTK_2P5GPHY_MCU_CSR_LEN);
	if (!mcu_csr_base) {
		ret = -ENOMEM;
		goto free_pmb;
	}

	ret = request_firmware(&fw, MT7988_2P5GE_PMB_FW, dev);
	if (ret) {
		dev_err(dev, "failed to load firmware: %s, ret: %d\n",
			MT7988_2P5GE_PMB_FW, ret);
		goto free;
	}

	if (fw->size != MT7988_2P5GE_PMB_FW_SIZE) {
		dev_err(dev, "Firmware size 0x%zx != 0x%x\n",
			fw->size, MT7988_2P5GE_PMB_FW_SIZE);
		ret = -EINVAL;
		goto release_fw;
	}

	reg = readw(mcu_csr_base + MD32_EN_CFG);
	if (reg & MD32_EN) {
		phy_set_bits(phydev, MII_BMCR, BMCR_RESET);
		usleep_range(10000, 11000);
	}
	phy_set_bits(phydev, MII_BMCR, BMCR_PDOWN);

	/* Write magic number to safely stall MCU */
	phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x800e, 0x1100);
	phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x800f, 0x00df);

	for (i = 0; i < MT7988_2P5GE_PMB_FW_SIZE - 1; i += 4)
		writel(*((uint32_t *)(fw->data + i)), pmb_addr + i);
	dev_info(dev, "Firmware date code: %x/%x/%x, version: %x.%x\n",
		 be16_to_cpu(*((__be16 *)(fw->data +
					  MT7988_2P5GE_PMB_FW_SIZE - 8))),
		 *(fw->data + MT7988_2P5GE_PMB_FW_SIZE - 6),
		 *(fw->data + MT7988_2P5GE_PMB_FW_SIZE - 5),
		 *(fw->data + MT7988_2P5GE_PMB_FW_SIZE - 2),
		 *(fw->data + MT7988_2P5GE_PMB_FW_SIZE - 1));

	writew(reg & ~MD32_EN, mcu_csr_base + MD32_EN_CFG);
	writew(reg | MD32_EN, mcu_csr_base + MD32_EN_CFG);
	phy_set_bits(phydev, MII_BMCR, BMCR_RESET);
	/* We need a delay here to stabilize initialization of MCU */
	usleep_range(7000, 8000);
	dev_info(dev, "Firmware loading/trigger ok.\n");

	priv->fw_loaded = true;

release_fw:
	release_firmware(fw);
free:
	iounmap(mcu_csr_base);
free_pmb:
	iounmap(pmb_addr);

	return ret;
}

static int mt798x_2p5ge_phy_config_init(struct phy_device *phydev)
{
	struct pinctrl *pinctrl;
	int ret;

	/* Check if PHY interface type is compatible */
	if (phydev->interface != PHY_INTERFACE_MODE_INTERNAL)
		return -ENODEV;

	switch (phydev->drv->phy_id) {
	case MTK_2P5GPHY_ID_MT7987:
		ret = mt7987_2p5ge_phy_load_fw(phydev);
		phy_clear_bits_mmd(phydev, MDIO_MMD_VEND2, MTK_PHY_LED0_ON_CTRL,
				   MTK_PHY_LED_ON_POLARITY);
		break;
	case MTK_2P5GPHY_ID_MT7988:
		ret = mt7988_2p5ge_phy_load_fw(phydev);
		phy_set_bits_mmd(phydev, MDIO_MMD_VEND2, MTK_PHY_LED0_ON_CTRL,
				 MTK_PHY_LED_ON_POLARITY);
		break;
	default:
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	/* Setup LED */
	phy_set_bits_mmd(phydev, MDIO_MMD_VEND2, MTK_PHY_LED0_ON_CTRL,
			 MTK_PHY_LED_ON_LINK10 | MTK_PHY_LED_ON_LINK100 |
			 MTK_PHY_LED_ON_LINK1000 | MTK_PHY_LED_ON_LINK2500);
	phy_set_bits_mmd(phydev, MDIO_MMD_VEND2, MTK_PHY_LED1_ON_CTRL,
			 MTK_PHY_LED_ON_FDX | MTK_PHY_LED_ON_HDX);

	/* Switch pinctrl after setting polarity to avoid bogus blinking */
	pinctrl = devm_pinctrl_get_select(&phydev->mdio.dev, "i2p5gbe-led");
	if (IS_ERR(pinctrl))
		dev_err(&phydev->mdio.dev, "Fail to set LED pins!\n");

	phy_modify_mmd(phydev, MDIO_MMD_VEND1, MTK_PHY_LPI_PCS_DSP_CTRL,
		       MTK_PHY_LPI_SIG_EN_LO_THRESH100_MASK, 0);

	/* Enable 16-bit next page exchange bit if 1000-BT isn't advertising */
	mtk_tr_modify(phydev, 0x0, 0xf, 0x3c, AUTO_NP_10XEN,
		      FIELD_PREP(AUTO_NP_10XEN, 0x1));

	/* Enable HW auto downshift */
	phy_modify_paged(phydev, MTK_PHY_PAGE_EXTENDED_1,
			 MTK_PHY_AUX_CTRL_AND_STATUS,
			 0, MTK_PHY_ENABLE_DOWNSHIFT);

	return 0;
}

static int mt798x_2p5ge_phy_config_aneg(struct phy_device *phydev)
{
	struct mtk_i2p5ge_phy_priv *priv = phydev->priv;
	bool changed = false;
	u32 adv;
	int ret;

	if (phydev->autoneg == AUTONEG_DISABLE)
		goto out;

	ret = genphy_c45_an_config_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/* Clause 45 doesn't define 1000BaseT support. Use Clause 22 instead in
	 * our design.
	 */
	adv = linkmode_adv_to_mii_ctrl1000_t(phydev->advertising);
	ret = phy_modify_changed(phydev, MII_CTRL1000, ADVERTISE_1000FULL, adv);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

out:
	if (priv->pd_en) {
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      MTK_PHY_PMA_PMD_SPPED_ABILITY, 0xf7cf);
		phy_modify_paged(phydev, MTK_PHY_PAGE_STANDARD,
				 MII_ADVERTISE, 0,
				 ADVERTISE_100HALF | ADVERTISE_10HALF);
	}

	if (phydev->autoneg == AUTONEG_DISABLE)
		return genphy_c45_pma_setup_forced(phydev);
	else
		return genphy_c45_check_and_restart_aneg(phydev, changed);
}

static int mt798x_2p5ge_phy_get_features(struct phy_device *phydev)
{
	int ret;

	ret = genphy_c45_pma_read_abilities(phydev);
	if (ret)
		return ret;

	/* This phy can't handle collision, and neither can (XFI)MAC it's
	 * connected to. Although it can do HDX handshake, it doesn't support
	 * CSMA/CD that HDX requires.
	 */
	linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
			   phydev->supported);

	return 0;
}

static int mt798x_2p5ge_phy_read_status(struct phy_device *phydev)
{
	int ret;

	/* When MDIO_STAT1_LSTATUS is raised genphy_c45_read_link(), this phy
	 * actually hasn't finished AN. So use CL22's link update function
	 * instead.
	 */
	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	/* We'll read link speed through vendor specific registers down below.
	 * So remove phy_resolve_aneg_linkmode (AN on) & genphy_c45_read_pma
	 * (AN off).
	 */
	if (phydev->autoneg == AUTONEG_ENABLE && phydev->autoneg_complete) {
		ret = genphy_c45_read_lpa(phydev);
		if (ret < 0)
			return ret;

		/* Clause 45 doesn't define 1000BaseT support. Read the link
		 * partner's 1G advertisement via Clause 22.
		 */
		ret = phy_read(phydev, MII_STAT1000);
		if (ret < 0)
			return ret;
		mii_stat1000_mod_linkmode_lpa_t(phydev->lp_advertising, ret);
	} else if (phydev->autoneg == AUTONEG_DISABLE) {
		linkmode_zero(phydev->lp_advertising);
	}

	if (phydev->link) {
		ret = phy_read(phydev, PHY_AUX_CTRL_STATUS);
		if (ret < 0)
			return ret;

		switch (FIELD_GET(PHY_AUX_SPEED_MASK, ret)) {
		case PHY_AUX_SPD_10:
			phydev->speed = SPEED_10;
			break;
		case PHY_AUX_SPD_100:
			phydev->speed = SPEED_100;
			break;
		case PHY_AUX_SPD_1000:
			phydev->speed = SPEED_1000;
			break;
		case PHY_AUX_SPD_2500:
			phydev->speed = SPEED_2500;
			break;
		}

		phydev->duplex = DUPLEX_FULL;
		/* FIXME:
		 * The current firmware always enables rate adaptation mode.
		 */
		phydev->rate_matching = RATE_MATCH_PAUSE;
	}

	return 0;
}

static int mt798x_2p5ge_phy_get_rate_matching(struct phy_device *phydev,
					      phy_interface_t iface)
{
	return RATE_MATCH_PAUSE;
}

static ssize_t pd_en_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct phy_device *phydev = container_of(dev, struct phy_device,
						 mdio.dev);
	struct mtk_i2p5ge_phy_priv *priv = phydev->priv;

	return sprintf(buf, "%d\n", priv->pd_en);
}

static ssize_t pd_en_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct phy_device *phydev = container_of(dev, struct phy_device,
						 mdio.dev);
	struct mtk_i2p5ge_phy_priv *priv = phydev->priv;
	int val;

	if (kstrtoint(buf, 0, &val) == 0)
		priv->pd_en = !!val;

	return count;
}

static DEVICE_ATTR_RW(pd_en);

static int mt798x_2p5ge_phy_probe(struct phy_device *phydev)
{
	struct mtk_i2p5ge_phy_priv *priv;

	priv = devm_kzalloc(&phydev->mdio.dev,
			    sizeof(struct mtk_i2p5ge_phy_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	switch (phydev->drv->phy_id) {
	case MTK_2P5GPHY_ID_MT7987:
	case MTK_2P5GPHY_ID_MT7988:
		/* The original hardware only sets MDIO_DEVS_PMAPMD */
		phydev->c45_ids.devices_in_package |= MDIO_DEVS_PCS |
						      MDIO_DEVS_AN |
						      MDIO_DEVS_VEND1 |
						      MDIO_DEVS_VEND2;
		break;
	default:
		return -EINVAL;
	}

	priv->fw_loaded = false;
	phydev->priv = priv;

	priv->pd_en = 1;
	device_create_file(&phydev->mdio.dev, &dev_attr_pd_en);

	return 0;
}

static struct phy_driver mtk_2p5gephy_driver[] = {
	{
		PHY_ID_MATCH_MODEL(MTK_2P5GPHY_ID_MT7987),
		.name = "MediaTek MT7987 2.5GbE PHY",
		.probe = mt798x_2p5ge_phy_probe,
		.config_init = mt798x_2p5ge_phy_config_init,
		.config_aneg = mt798x_2p5ge_phy_config_aneg,
		.get_features = mt798x_2p5ge_phy_get_features,
		.read_status = mt798x_2p5ge_phy_read_status,
		.get_rate_matching = mt798x_2p5ge_phy_get_rate_matching,
		.suspend = genphy_suspend,
		.resume = genphy_resume,
		.read_page = mtk_phy_read_page,
		.write_page = mtk_phy_write_page,
	},
	{
		PHY_ID_MATCH_MODEL(MTK_2P5GPHY_ID_MT7988),
		.name = "MediaTek MT7988 2.5GbE PHY",
		.probe = mt798x_2p5ge_phy_probe,
		.config_init = mt798x_2p5ge_phy_config_init,
		.config_aneg = mt798x_2p5ge_phy_config_aneg,
		.get_features = mt798x_2p5ge_phy_get_features,
		.read_status = mt798x_2p5ge_phy_read_status,
		.get_rate_matching = mt798x_2p5ge_phy_get_rate_matching,
		.suspend = genphy_suspend,
		.resume = genphy_resume,
		.read_page = mtk_phy_read_page,
		.write_page = mtk_phy_write_page,
	},
};

module_phy_driver(mtk_2p5gephy_driver);

static struct mdio_device_id __maybe_unused mtk_2p5ge_phy_tbl[] = {
	{ PHY_ID_MATCH_VENDOR(0x00339c00) },
	{ }
};

MODULE_DESCRIPTION("MediaTek 2.5Gb Ethernet PHY driver");
MODULE_AUTHOR("SkyLake Huang <SkyLake.Huang@mediatek.com>");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(mdio, mtk_2p5ge_phy_tbl);
MODULE_FIRMWARE(MT7988_2P5GE_PMB_FW);

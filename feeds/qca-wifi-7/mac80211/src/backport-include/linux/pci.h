#ifndef _BACKPORT_LINUX_PCI_H
#define _BACKPORT_LINUX_PCI_H
#include_next <linux/pci.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,4,0)
#include <linux/pci-aspm.h>
#endif

#if defined(CONFIG_PCI)
#if LINUX_VERSION_IS_LESS(5,3,0)
static inline int
backport_pci_disable_link_state(struct pci_dev *pdev, int state)
{
	u16 aspmc;

	pci_disable_link_state(pdev, state);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &aspmc);
	if ((state & PCIE_LINK_STATE_L0S) &&
	    (aspmc & PCI_EXP_LNKCTL_ASPM_L0S))
		return -EPERM;

	if ((state & PCIE_LINK_STATE_L1) &&
	    (aspmc & PCI_EXP_LNKCTL_ASPM_L1))
		return -EPERM;

	return 0;
}
#define pci_disable_link_state LINUX_BACKPORT(pci_disable_link_state)

#endif /* < 5.3 */
#endif /* defined(CONFIG_PCI) */

#endif /* _BACKPORT_LINUX_PCI_H */

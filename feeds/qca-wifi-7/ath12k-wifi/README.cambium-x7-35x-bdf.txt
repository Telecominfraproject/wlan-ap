Cambium X7-35X board data
=========================

This image ships with the closest open-source OpenWiFi board data rather than
any vendor BDF. The X7-35X is an IPQ5332 plus a QCN9274 (QCN92XX) on PCIe for
Wi-Fi 7, matching the Edgecore EAP105, so it installs the in-tree
ath12k-wifi-edgecore-eap105 package, which places board-2.bin at the fixed
ath12k firmware path(s):

  /lib/firmware/ath12k/IPQ5332/hw1.0/board-2.bin
  /lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin

Per-unit RF calibration is read from the device's own 0:ART partition at boot;
only the reference/regulatory template comes from the Edgecore EAP105 board
file.

Using this image
----------------
The Edgecore EAP105 board file is the nearest-hardware match, NOT a
X7-35X-specific BDF. RF output power and regulatory limits are therefore not
calibrated or validated for X7-35X hardware. Use the image as-is, at your own
discretion.

If you want the device's exact board data, you can extract it from a Cambium
X7-35X and drop it in at the ath12k firmware path above, replacing the
reference file. Sourcing vendor board data that way, and ensuring the
regulatory compliance of whatever BDF you run, is your responsibility.

The device tree sets the IPQ5332 radio's qcom,board_id to the EAP105's 0x16 so
ath12k selects the EAP105 entry from board-2.bin (the QCN radio's 0x1019
already matches EAP105). If you install a device-specific BDF, set the
board_id to match the id inside it.

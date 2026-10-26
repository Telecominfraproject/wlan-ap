Cambium XE3-4 board data
========================

This image ships with the closest open-source OpenWiFi board data rather than
any vendor BDF. The XE3-4 is a tri-band IPQ6018 (2.4/5 GHz) plus a QCN9074 on
PCIe for 6 GHz, matching the Wallys DR6018-v4, so it installs the in-tree
ath11k-wifi-wallys-dr6018-v4 package, which places board.bin at the fixed
ath11k firmware path(s):

  /lib/firmware/ath11k/IPQ6018/hw1.0/board.bin
  /lib/firmware/ath11k/qcn9074/hw1.0/board.bin

Per-unit RF calibration is read from the device's own 0:ART partition at boot;
only the reference/regulatory template comes from the Wallys DR6018-v4 board
file.

Using this image
----------------
The Wallys DR6018-v4 board file is the nearest-hardware match, NOT a
XE3-4-specific BDF. RF output power and regulatory limits are therefore not
calibrated or validated for XE3-4 hardware. Use the image as-is, at your own
discretion.

If you want the device's exact board data, you can extract it from a Cambium
XE3-4 and drop it in at the ath11k firmware path above, replacing the
reference file. Sourcing vendor board data that way, and ensuring the
regulatory compliance of whatever BDF you run, is your responsibility.

Cambium XV2-22H board data
==========================

This image ships with the closest open-source OpenWiFi board data rather than
any vendor BDF. The XV2-22H is an IPQ5018 (2.4 GHz) plus a QCN6122 (5 GHz),
matching the CIG WF186h, so it installs the in-tree ath11k-wifi-cig-wf186h
package, which places board.bin at the fixed ath11k firmware path(s):

  /lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
  /lib/firmware/ath11k/qcn6122/hw1.0/board.bin

Per-unit RF calibration is read from the device's own 0:ART partition at boot;
only the reference/regulatory template comes from the CIG WF186h board file.

Using this image
----------------
The CIG WF186h board file is the nearest-hardware match, NOT a
XV2-22H-specific BDF. RF output power and regulatory limits are therefore not
calibrated or validated for XV2-22H hardware. Use the image as-is, at your own
discretion.

If you want the device's exact board data, you can extract it from a Cambium
XV2-22H and drop it in at the ath11k firmware path above, replacing the
reference file. Sourcing vendor board data that way, and ensuring the
regulatory compliance of whatever BDF you run, is your responsibility.

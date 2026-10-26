Cambium XV2-2 board data
========================

This image ships with the closest open-source OpenWiFi board data rather than
any vendor BDF. The XV2-2 is a single dual-band IPQ6018 radio, matching the
CIG WF188n, so it installs the in-tree ath11k-wifi-cig-wf188n package, which
places board.bin at the fixed ath11k firmware path(s):

  /lib/firmware/ath11k/IPQ6018/hw1.0/board.bin

Per-unit RF calibration is read from the device's own 0:ART partition at boot;
only the reference/regulatory template comes from the CIG WF188n board file.

Using this image
----------------
The CIG WF188n board file is the nearest-hardware match, NOT a XV2-2-specific
BDF. RF output power and regulatory limits are therefore not calibrated or
validated for XV2-2 hardware. Use the image as-is, at your own discretion.

If you want the device's exact board data, you can extract it from a Cambium
XV2-2 and drop it in at the ath11k firmware path above, replacing the
reference file. Sourcing vendor board data that way, and ensuring the
regulatory compliance of whatever BDF you run, is your responsibility.

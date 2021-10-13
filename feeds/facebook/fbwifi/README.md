# Facebook Wi-Fi v2.0 Reference Implementation for OpenWrt 

## Getting started

Case studies for OEM customers are available at the official page of [Facebook Wi-Fi](https://www.facebook.com/facebook-wifi).

For OEM engineers, start by reading the init script in [files/etc/init.d/fbwifi](https://github.com/facebookincubator/fbc_owrt_feed/blob/master/fbwifi/files/etc/init.d/fbwifi)

To enable Facebook Wi-Fi, configure the gateway_token in `/etc/config/fbwifi`, and run `fbwifi enable`.
To disable Facebook Wi-Fi, run `fbwifi disable`.

## Contents

The 'files' subdirectory contains two subdirectories, one for the fbwifi
package that implements the Facebook Wi-Fi v2.0 standard for OpenWrt, and
another one containing a LuCI application to configure Facebook Wi-Fi.

The folder structures follow *nix conventions:
- 'etc' is the boot time scripts and configuration
- 'usr' contains procedural scripts, lua common code module and GUI prototype for luci
- 'www' contains the HTTP endpoints as CGI handlers 


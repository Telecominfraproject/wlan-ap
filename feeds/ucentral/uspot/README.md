# uspot

A captive portal

##

TBC

## Basic firewall setup

In /etc/config/firewall:

```
config zone
	option name 'captive'
	list network 'captive'
	option input 'REJECT'
	option output 'ACCEPT'
	option forward 'REJECT'

config redirect
	option name 'Redirect-unauth-captive-CPD'
	option src 'captive'
	option src_dport '80'
	option proto 'tcp'
	option target 'DNAT'
	option reflection '0'
	option mark '!1/127'

config rule
	option name 'Allow-captive-CPD-UAM'
	option src 'captive'
	option dest_port '80 3990'
	option proto 'tcp'
	option target 'ACCEPT'

config rule
	option name 'Forward-auth-captive'
	option src 'captive'
	option dest 'wan'
	option proto 'any'
	option target 'ACCEPT'
	option mark '1/127'

config rule
	option name 'Allow-DHCP-captive'
	option src 'captive'
	option proto 'udp'
	option dest_port '67'
	option target 'ACCEPT'

config rule
	option name 'Allow-DNS-captive'
	option src 'captive'
	list proto 'udp'
	list proto 'tcp'
	option dest_port '53'
	option target 'ACCEPT'
```

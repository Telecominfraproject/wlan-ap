bundle.wifi();

let paths = [
        [ 'network.wireless', 'status' ],
        [ 'network.device', 'status' ],
        [ 'network.interface', 'dump' ],
        [ 'log', 'read', { stream: false } ],
];
for (let path in paths)
        bundle.ubus(path[0], path[1], path[2]);

for (let config in [ 'network', 'wireless', 'dhcp', 'firewall', 'system' ])
        bundle.uci(config);

for (let cmd in [ "route", "ifconfig", "logread" ])
        bundle.shell(cmd);

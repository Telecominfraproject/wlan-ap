#!/usr/bin/ucode

let fs = require("fs");

let cmd = ARGV[0];
let ifname = getenv("interface");
let vendor = getenv("vendor");
let value = getenv("opt43");

if (!ifname)
        exit(0);

if (!vendor)
    vendor = "unknown";

if (cmd == "deconfig" || !vendor || !value) {
        // No VSI available
        if (fs.stat("/tmp/udhcpc-vsi.json")) {
                let f = fs.open("/tmp/udhcpc-vsi.json", "r");
                let r_list = json(f.read('all'));
                f.close();
                if (type(r_list) == "object")
                        vsi_list = r_list;

                if (ifname in vsi_list) {
                        delete vsi_list[ifname];
                        if (length(vsi_list))
                                fs.writefile("/tmp/udhcpc-vsi.json", vsi_list);
                        else
                                fs.unlink("/tmp/udhcpc-vsi.json");
                }
        }
}
else if (cmd == "bound") {
    let vsi = { vendor, value };

    let vsi_list = {};
    if (fs.stat("/tmp/udhcpc-vsi.json")) {
            let f = fs.open("/tmp/udhcpc-vsi.json", "r");
            let r_list = json(f.read('all'));
            f.close();
            if (type(r_list) == "object")
                    vsi_list = r_list;
    }
    vsi_list[ifname] = vsi;
    fs.writefile("/tmp/udhcpc-vsi.json", vsi_list);
}

exit(0);

-- Copyright 
-- Licensed to the public under the GNU General Public License v2.

module("luci.controller.fbwifi", package.seeall)

sys = require "luci.sys"
ut = require "luci.util"

function index()
    entry({"admin", "network", "fbwifi"}, template("fbwifi"), "Facebook Wi-Fi", 90).dependent=false
end


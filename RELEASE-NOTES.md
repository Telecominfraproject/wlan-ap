OpenSync™ Release Notes
-----------------------


### Release 1.4.0.1

* Notable Enhancements
    - OVSDB schema was updated with fields added in newer releases of OVS
    - Handling of the Kconfig-driven `target_managers_config` table was changed
      so that it can be overridden from the vendor, platform, or 3rdparty layer
* Notable Fixes
    - Fix for IPv6 address preferred lifetime not set correctly
    - Fixed processing of IPv6 RDNSS in case of multiple entries
    - Fix for DHCP options not applied immediately after being received
    - Fixed code logic for reporting the onboarding status
    - Fixed a potential 'use after free' problem when an FSM policy is removed


### Release 1.4.0.0

* New Features
    - IPv6 support:
        - schema changes for IPv6 (old tables are still used for IPv4)
        - added a new layer (`src/lib/osn`) which replaces the `inet_target` API
        - NM2 refactored
    - Infrastructure for inspection of data flows (deep packet inspection):
        - added support for _flow tags_ (FSM and plugins)
        - schema updated to add support for IP threat detection
    - Infrastructure for collecting and reporting health statistics:
        - IP and DNS monitoring counters: `interfaces/ip_dns_telemetry.proto`
        - library: `src/lib/network_telemetry`
    - Infrastructure for collecting and reporting data flow statistics:
        - added a new manager (FCM, or *Flow Collection Manager*)
        - protobuf interface: `interfaces/network_metadata.proto`
        - library: `src/lib/network_metadata`
    - Sample code for the new features:
        - a sample DPI plugin for FSM: `src/lib/fsm_demo_dpi_plugin`
        - sample code for generating and sending protobuf reports through MQTT
          added to `src/lib/fsm_demo_plugin`
* Notable Enhancements
    - Build system uses `Kconfiglib` to generate the necessary files for
      easier use of Kconfig
    - FSM no longer uses `pcap` as the plugin API
    - Additional FSM plugin libraries (for advanced device typing):
        - DHCP (`src/lib/dhcp_parse`)
        - DNS (`src/lib/dns_parse`)
        - HTTP (`src/lib/http_parse`)
        - UPNP (`src/lib/upnp_parse`)
    - Improved 802.11v and 802.11k steering mechanisms
    - DFS enhancements to improve its use in Multi-AP networks


### Release 1.2.4.0

* Notable Enhancements
    - Extended `hello_world` manager/service with additional examples
    - Native build (using Kconfig) for faster development of new features
* Notable Fixes
    - Fixed incorrectly reported device capabilities (GW mode)
    - Fixed incorrect parsing of `pref_5g` field when not populated
    - Added reporting of a `CONNECT` event when a client is '11v-band-steered'
      from 2.4 GHz to 5 GHz on the same AP


### Release 1.2.3.0

* New Features
    - Introduces a new manager (XM, or *Exchange Manager*), which facilitates
      data transfer between OVSDB and other management systems.
      Stub code is provided, where communication with an external database can
      be implemented.  
      (see: https://github.com/plume-design/opensync/blob/osync_1.2.3/src/lib/connector/src/connector_stub.c)
* Notable Enhancements
    - Added random delay debounce (to avoid congestion)
* Notable Fixes
    - Fixed memory leaks in `ovs_mac_learn.c`


### Release 1.2.2.0

* New Features
    - Introduces a new manager (FSM, or *Flow Service Manager*), which provides
      processing of events, based on `libpcap` filtering through an extendable
      architecture using plugins.  
      (additional information: https://www.opensync.io/s/FSM-Plugin-API.pdf)
* Notable Enhancements
    - Added a demo service `hello_world` as a template for 3rd-party developers
    - Added the ability to have multiple GRE implementations
    - Enhanced low-level statistics (CPU, memory, filesystem)
    - Asynchronous DNS resolution using `c-ares`
* Notable Fixes
    - Fixed uninitialized memory access in `ovsdb_sync_api.c`


### Release 1.2.0.0

* First public release

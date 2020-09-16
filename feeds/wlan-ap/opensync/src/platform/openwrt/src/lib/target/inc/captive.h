#ifndef _CAPTIVE_H__
#define _CAPTIVE_H__

extern struct blob_buf c;

extern struct schema_Wifi_VIF_State vstate;
extern const struct schema_Wifi_VIF_Config *vconf;
extern void vif_captive_portal_set (const struct schema_Wifi_VIF_Config *vconf, char *ifname);
extern void vif_state_captive_portal_options_get(struct schema_Wifi_VIF_State *vstate, struct uci_section *s);
extern void captive_portal_init();
extern void splash_page_logo(char* dest_file,char* src_url);
extern void vif_dhcp_opennds_allowlist_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname);
extern void vif_state_dhcp_allowlist_get(struct schema_Wifi_VIF_State *vstate);

#endif

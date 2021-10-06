#include "log.h"
#include <json-c/json.h>
#include "schema_consts.h"
#include "eth_vlan.h"

static struct eth_port_state *ethport[MAX_ETH_PORTS+1];
static int eth_index;

bool is_dup_vlan(struct eth_port_state *eps, int vlan_id)
{
	int i = 0;
	for (i=0; i < eps->vlans.vindex; i++)
		if (eps->vlans.allowed_vlans[i] == vlan_id)
			return true;
	return false;
}

struct eth_port_state *get_eth_port(const char *ifname)
{
	int i = 0;
	if (strcmp(wanport.ifname, ifname) == 0) {
		return &wanport;
	}
	for (i = 0; i < MAX_ETH_PORTS && lanport[i].ifname != NULL; i++) {
		if (strcmp(lanport[i].ifname, ifname) == 0) {
			return &lanport[i];
		}
	}
	return NULL;
}

int parse_each_array_element(json_object *arr_json,
			      void (*parse_func)(json_object *)) {
	int len = 0, i = 0;

	len = json_object_array_length(arr_json);
	if (len == 0)
		return -1;

	for (i = 0; i < len; i++) {
		json_object *tmp;
		tmp = json_object_array_get_idx(arr_json, i);
		if (tmp)
			parse_func(tmp);
		else {
			LOGI("%s:Error: Failed to parse info",
			     __func__);
			continue;
		}
	}
	return 0;
}

/* ["PVID","Egress Untagged"] */
void flags_parse_element(json_object *flag_json)
{
	const char *flags;

	flags = json_object_get_string(flag_json);
	if (!strncmp(flags, "PVID", 4)) {

	ethport[eth_index-1]->vlans.pvid = ethport[eth_index-1]->vlans.allowed_vlans[ethport[eth_index-1]->vlans.vindex];
	}
}

/*[{"vlan":1,"flags":["PVID","Egress Untagged"]}] */
void vlans_parse_element(json_object *vlans_json) 
{
	int i = 0;
	int exists = 0;
	const char *vlan_id = 0;
	json_object *vlan_json;
	json_object *flags_json;

	exists = json_object_object_get_ex(vlans_json, "vlan", &vlan_json);
	if (!exists) {
		LOGI("%s: ifname doesnt exist", __func__);
		json_object_put(vlan_json);
		return;
	}

	vlan_id = json_object_get_string(vlan_json);
	for (i=0; i < eth_index; i++) {
		if(!is_dup_vlan(ethport[i], atoi(vlan_id)))
			ethport[i]->vlans.allowed_vlans[ethport[i]->vlans.vindex] = atoi(vlan_id);
	}


	exists = json_object_object_get_ex(vlans_json, "flags", &flags_json);
	if (!exists) {
		LOGD("%s:flags doesnt exist", __func__);
		for (i=0; i < eth_index; i++)
			ethport[i]->vlans.vindex++;
		json_object_put(flags_json);
		return;
	}

	parse_each_array_element(flags_json, flags_parse_element);

	for (i=0; i < eth_index; i++)
		ethport[i]->vlans.vindex++;

	json_object_put(vlan_json);
	json_object_put(flags_json);
}

void bridge_vlan_parse_element(json_object *if_obj) 
{
	const char *ifname;
	struct json_object *ifname_json;
	struct json_object *vlans_json;
	bool exists;

	exists = json_object_object_get_ex(if_obj, "ifname", &ifname_json);
	if (!exists) {
		LOGI("%s:ifname doesnt exist", __func__);
		json_object_put(ifname_json);
		return;
	}

	ifname = json_object_get_string(ifname_json);

	ethport[eth_index] = get_eth_port(ifname);
	if (!ethport[eth_index]) {
		LOGI("%s:ifname=%s not there", __func__, ifname);
		return;
	}

	eth_index++;
	exists = json_object_object_get_ex(if_obj, "vlans", &vlans_json);
	if (!exists) {
		LOGD("%s: vlans doesnt exist", __func__);
		json_object_put(vlans_json);
		return;
	}

	if (parse_each_array_element(vlans_json, vlans_parse_element) < 0) {
		LOGD("%s: vlans len doesnt exist", __func__);
		json_object_put(vlans_json);
		return;
	}

	eth_index = 0;

	json_object_put(vlans_json);
	json_object_put(ifname_json);
}

int vlan_state_json_parse(void)
{
	int i = 0;
	struct json_object *vlan_json;

	system("bridge -j vlan > /tmp/vlans.json");
	vlan_json = json_object_from_file(TMP_VLAN_JSON);

	memset(&wanport.vlans, 0, sizeof(struct eth_port_vlans));

	for (i = 0; i < MAX_ETH_PORTS; i++) {
		memset(&lanport[i].vlans, 0x0, sizeof(struct eth_port_vlans));
	}

	parse_each_array_element(vlan_json, bridge_vlan_parse_element);

	json_object_put(vlan_json);
	return 0;
}

// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "core.h"
#include "debug.h"
#include "ini.h"
#include <linux/vmalloc.h>

/* Variable to track the ath12k cfg global initialization */
static bool ath12k_cfg_global_init_done;
/* Pointer to ath12k_cfg_value_store */
static struct ath12k_cfg_value_store *ath12k_cfg_global_store;
/* list head to track the ath12k_cfg_store users*/
struct list_head  ath12k_cfg_stores_list;
/* spinlock to be used while changing the list */
static spinlock_t ath12k_cfg_stores_lock;

/**
 * struct ath12k_cfg_ctx - configuration context
 * @store: cfg value store reference
 * @initialized: To mark if the ctx has been initialized
 */
struct ath12k_cfg_ctx {
	struct ath12k_cfg_value_store *store;
	bool initialized;
};

/**
 * struct ath12k_cfg_value_store - backing store for an ini file
 * @path: file path of the ini file
 * @node: internal list node for keeping track of all the allocated stores
 * @users: number of references on the store
 * @values: a values struct containing the parsed values from the ini file
 */
struct ath12k_cfg_value_store {
	char *path;
	struct list_head node;
	atomic_t  users;
	struct ath12k_cfg_values values;
};

/**
 * enum ath12k_cfg_data_type - Enum for CFG/INI types
 * @ATH12K_CFG_INT_ITEM: Integer CFG/INI
 * @ATH12K_CFG_UINT_ITEM: Unsigned integer CFG/INI
 * @ATH12K_CFG_BOOL_ITEM: Boolean CFG/INI
 * @ATH12K_CFG_STRING_ITEM: String CFG/INI
 * @ATH12K_CFG_MAC_ADDR_ITEM: Mac address CFG/INI
 * @ATH12K_CFG_MAX_ITEM: Max CFG type
 */
enum ath12k_cfg_data_type {
	ATH12K_CFG_INT_ITEM,
	ATH12K_CFG_UINT_ITEM,
	ATH12K_CFG_BOOL_ITEM,
	ATH12K_CFG_STRING_ITEM,
	ATH12K_CFG_MAC_ADDR_ITEM,
	ATH12K_CFG_MAX_ITEM,
};

/**
 * struct ath12k_cfg_meta - item metadata for dynamic lookup during parse
 * @name: name of the config item used in the ini file (i.e. "gScanDwellTime")
 * @field_offset: offset to find the desired value in the structure
 * @cfg_data_type: configuration type that is being used
 * @ath12k_cfg_item_handler: parsing callback based on type of the config item
 * @min: minimum value for use in bounds checking (min_len for strings)
 * @max: maximum value for use in bounds checking (max_len for strings)
 * @fallback: the fallback behavior to use when configured values are invalid
 */
struct ath12k_cfg_meta {
	const char *name;
	const u32 field_offset;
	const enum ath12k_cfg_data_type cfg_data_type;

	void (*const ath12k_cfg_item_handler)(struct ath12k_cfg_value_store *store,
					  const struct ath12k_cfg_meta *meta,
					  const char *value);
	const s32 min;
	const s32 max;
	const enum ath12k_cfg_fallback_behavior fallback;
};

#define ath12k_cfg_value_ptr(store, meta) \
	((void *)&(store)->values + (meta)->field_offset)

/**
 * ath12k_cfg_int_item_handler - Integer handler to parse the values
 * @store: cfg store pointer
 * @meta: configuration meta information that is stored in the device
 * @str_value: The value to be searched in the database for sanity
 * Return: void
 */
static __maybe_unused void
ath12k_cfg_int_item_handler(struct ath12k_cfg_value_store *store,
			    const struct ath12k_cfg_meta *meta,
			    const char *str_value)
{
	int ret;
	s32 *store_value = ath12k_cfg_value_ptr(store, meta);
	s32 value;

	ret = kstrtoint(str_value, 0, &value);
	if (ret) {
		ath12k_err(NULL,
			   "%s=%s -Invalid format (status %d) Using default %d",
			   meta->name, str_value, ret, *store_value);
		return;
	}

	WARN_ON(!(meta->min <= meta->max));
	if (meta->min > meta->max) {
		ath12k_err(NULL,
			   "Invalid config item meta for %s", meta->name);
		return;
	}

	if (value >= meta->min && value <= meta->max) {
		*store_value = value;
		return;
	}

	switch (meta->fallback) {
	default:
		ath12k_err(NULL,
			   "Unknown fallback method %d for cfg item '%s'",
			   meta->fallback, meta->name);
		fallthrough;
	case ATH12K_CFG_VALUE_OR_CLAMP:
		*store_value = ath12k_cfg_clamp(value, meta->min, meta->max);
		break;
	case ATH12K_CFG_VALUE_OR_DEFAULT:
		/* store already contains default */
		break;
	}

	ath12k_err(NULL,
		   "%s=%d - Out of range [%d, %d]; Using %d", meta->name,
		   value, meta->min, meta->max, *store_value);
}

/**
 * ath12k_cfg_uint_item_handler - Unsigned integer handler to parse the values
 * @store: cfg store pointer
 * @meta: configuration meta information that is stored in the device
 * @str_value: The value to be searched in the database for sanity
 * Return: void
 */
static void
ath12k_cfg_uint_item_handler(struct ath12k_cfg_value_store *store,
			     const struct ath12k_cfg_meta *meta,
			     const char *str_value)
{
	int ret;
	u32 *store_value = ath12k_cfg_value_ptr(store, meta);
	u32 value;
	u32 min;
	u32 max;

	/**
	 * Since meta min and max are of type int32_t
	 * We need explicit type casting to avoid
	 * implicit wrap around for uint32_t type cfg data.
	 */
	min = (uint32_t)meta->min;
	max = (uint32_t)meta->max;

	ret = kstrtouint(str_value, 0, &value);
	if (ret) {
		ath12k_err(NULL,
			   "%s = %s - Invalid format (status %d) using default %d",
			   meta->name, str_value, ret, *store_value);
		return;
	}

	WARN_ON(!(min <= max));
	if (min > max) {
		ath12k_err(NULL,
			   "Invalid config item meta for %s", meta->name);
		return;
	}

	if (value >= min && value <= max) {
		*store_value = value;
		return;
	}

	switch (meta->fallback) {
	default:
		ath12k_err(NULL,
			   "Unknown fallback method %d for cfg item '%s'",
			   meta->fallback, meta->name);
		fallthrough;
	case ATH12K_CFG_VALUE_OR_CLAMP:
		*store_value = ath12k_cfg_clamp(value, min, max);
		break;
	case ATH12K_CFG_VALUE_OR_DEFAULT:
		/* store already contains default */
		break;
	}

	ath12k_err(NULL,
		   "%s=%u - Out of range [%d, %d]; Using %u",
		   meta->name, value, min, max, *store_value);
}

/**
 * ath12k_cfg_bool_item_handler - Bool handler to parse the values
 * @store: cfg store pointer
 * @meta: configuration meta information that is stored in the device
 * @str_value: The value to be searched in the database for sanity
 * Return: void
 */
static void
ath12k_cfg_bool_item_handler(struct ath12k_cfg_value_store *store,
			     const struct ath12k_cfg_meta *meta,
			     const char *str_value)
{
	int ret;
	bool *store_value = ath12k_cfg_value_ptr(store, meta);

	if (!str_value || strlen(str_value) == 0) {
		ath12k_err(NULL, "Invalid boolean value for %s", meta->name);
		return;
	}
	ret = kstrtobool(str_value, store_value);
	if (ret) {
		ath12k_err(NULL,
			   "%s=%s - Invalid format (status %d); Using default '%s'",
			   meta->name, str_value, ret,
			   *store_value ? "true" : "false");
		return;
	}
}

/**
 * ath12k_cfg_string_item_handler - String handler to parse the values
 * @store: cfg store pointer
 * @meta: configuration meta information that is stored in the device
 * @str_value: The value to be searched in the database for sanity
 * Return: void
 */
static void
ath12k_cfg_string_item_handler(struct ath12k_cfg_value_store *store,
			       const struct ath12k_cfg_meta *meta,
			       const char *str_value)
{
	char *store_value = ath12k_cfg_value_ptr(store, meta);
	size_t len;

	WARN_ON(!(meta->min >= 0));
	WARN_ON(!(meta->min <= meta->max));
	if (meta->min < 0 || meta->min > meta->max) {
		ath12k_err(NULL,
			   "Invalid config item meta for %s", meta->name);
		return;
	}

	/* ensure min length */
	len = strnlen(str_value, meta->min);
	if (len < meta->min) {
		ath12k_err(NULL,
			   "%s=%s - Too short; Using default '%s'",
			   meta->name, str_value, store_value);
		return;
	}

	/* check max length */
	len += strnlen(str_value + meta->min, meta->max - meta->min + 1);
	if (len > meta->max) {
		ath12k_err(NULL,
			   "%s=%s - Too long; Using default '%s'",
			   meta->name, str_value, store_value);
		return;
	}

	strscpy(store_value, str_value, meta->max + 1);
}

/**
 * ath12k_cfg_mac_addr_item_handler - MAC address handler to parse the values
 * @store: cfg store pointer
 * @meta: configuration meta information that is stored in the device
 * @str_value: The value to be searched in the database for sanity
 * Return: void
 */
static __maybe_unused void
ath12k_cfg_mac_addr_item_handler(struct ath12k_cfg_value_store *store,
				 const struct ath12k_cfg_meta *meta,
				 const char *str_value)
{
	int ret;
	struct ath12k_ini_mac_addr *store_value = ath12k_cfg_value_ptr(store, meta);

	ret = mac_pton(str_value, store_value->bytes);
	if (ret) {
		ath12k_err(NULL,
			   "%s=%s - Invalid format (status %d); Using default ",
			   meta->name, str_value, ret);
	}

	return;
}

/* Refer to header file on how nested macros are expanded to generate table,
 * set the default values and generate the cfg_value structure.
 */
#undef __ATH12K_CFG_INI
#define __ATH12K_CFG_INI(_id, _mtype, _ctype, _name, _min, _max, _fallback, ...) \
{ \
	.name = _name, \
	.field_offset = offsetof(struct ath12k_cfg_values, _id##_internal), \
	.cfg_data_type = ATH12K_CFG_ ##_mtype ## _ITEM, \
	.ath12k_cfg_item_handler = ath12k_cfg_ ## _mtype ## _item_handler, \
	.min = _min, \
	.max = _max, \
	.fallback = _fallback, \
},

#define ath12k_cfg_INT_item_handler ath12k_cfg_int_item_handler
#define ath12k_cfg_UINT_item_handler ath12k_cfg_uint_item_handler
#define ath12k_cfg_BOOL_item_handler ath12k_cfg_bool_item_handler
#define ath12k_cfg_STRING_item_handler ath12k_cfg_string_item_handler
#define ath12k_cfg_MAC_ADDR_item_handler ath12k_cfg_mac_addr_item_handler

static const struct ath12k_cfg_meta ath12k_cfg_meta_lookup_table[] = {
	ATH12K_CFG_ALL
};

/**
 * ath12k_cfg_store_set_defaults - Function to initialize the store
 * @store: cfg store reference
 * Return: void
 */
static void ath12k_cfg_store_set_defaults(struct ath12k_cfg_value_store *store)
{
#undef __ATH12K_CFG_INI
#define __ATH12K_CFG_INI(id, mtype, ctype, name, min, max, fallback, desc, def...) \
	ctype id = def;

	ATH12K_CFG_ALL

#undef __ATH12K_CFG_INI_STRING
#define __ATH12K_CFG_INI_STRING(id, mtype, ctype, name, min_len, max_len, ...) \
	strscpy((char *)&store->values.id##_internal, id, (max_len) + 1);

#undef __ATH12K_CFG_INI
#define __ATH12K_CFG_INI(id, mtype, ctype, name, min, max, fallback, desc, def...) \
	*(ctype *)&store->values.id##_internal = id;

	ATH12K_CFG_ALL
}

/**
 * ath12k_cfg_lookup_meta - Function to lookup named value in the cfg meta data
 * @name: The key to be searched in the cfg meta data
 * Return: ath12k_cfg_meta reference if the data is found, else NULL
 */
static const struct ath12k_cfg_meta *ath12k_cfg_lookup_meta(const char *name)
{
	int i;
	char *param1;
	char param[ATH12K_CFG_META_NAME_LENGTH_MAX];
	u8 ini_name[ATH12K_CFG_INI_LENGTH_MAX];

	WARN_ON(!name);
	if (!name)
		return NULL;

	ath12k_dbg(NULL, ATH12K_DBG_INI, "INI meta lookpup name %s ", name);
	/* linear search for now; optimize in the future if needed */
	for (i = 0; i < ARRAY_SIZE(ath12k_cfg_meta_lookup_table); i++) {
		const struct ath12k_cfg_meta *meta = &ath12k_cfg_meta_lookup_table[i];
		size_t name_len = strnlen(meta->name, ATH12K_CFG_META_NAME_LENGTH_MAX);

		memset(ini_name, 0, ATH12K_CFG_INI_LENGTH_MAX);
		memset(param, 0, ATH12K_CFG_META_NAME_LENGTH_MAX);
		if (name_len >= ATH12K_CFG_META_NAME_LENGTH_MAX) {
			ath12k_err(NULL, "Invalid meta name %s",
				   meta->name);
			continue;
		}

		memcpy(param, meta->name, name_len);
		param[name_len] = '\0';
		param1 = param;
		if (sscanf(param1, "%s", ini_name) != 1) {
			ath12k_err(NULL,
				   "Cannot get ini name %s", param1);
			return NULL;
		}
		if (!strcmp(name, ini_name))
			return meta;

		param1 = strpbrk(param, " ");
		while (param1) {
			param1++;
			if (sscanf(param1, "%s ", ini_name) != 1) {
				ath12k_err(NULL,
					   "Invalid ini name %s", meta->name);
				return NULL;
			}
			if (!strcmp(name, ini_name))
				return meta;

			param1 = strpbrk(param1, " ");
		}
	}
	return NULL;
}

/**
 * ath12k_cfg_ini_item_handler - Function to parse various ini values
 * @context: The cfg store context
 * @key: The key to be searched in the cfg store
 * @value: The value to check against the cfg store
 * Return: 0 on success
 */
static int
ath12k_cfg_ini_item_handler(void *context, const char *key, const char *value)
{
	struct ath12k_cfg_value_store *store = context;
	const struct ath12k_cfg_meta *meta;

	meta = ath12k_cfg_lookup_meta(key);
	if (!meta) {
		ath12k_err(NULL, "Unknown cfg item '%s'", key);
		return 0;
	}

	if (!value) {
		ath12k_err(NULL, "Invalid value pointer for %s", meta->name);
		return 0;
	}

	WARN_ON(!(meta->ath12k_cfg_item_handler));
	if (!meta->ath12k_cfg_item_handler)
		return 0;

	meta->ath12k_cfg_item_handler(store, meta, value);
	return 0;
}

/**
 * ath12k_cfg_store_alloc - Allocate cfg store
 * @path: Path from where store is created
 * @out_store: Reference to cfg value store
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_store_alloc(const char *path,
		       struct ath12k_cfg_value_store **out_store)
{
	struct ath12k_cfg_value_store *store;
	store = vzalloc(sizeof(*store));

	if (!store)
		return -ENOMEM;

	store->path = kstrdup(path, GFP_KERNEL);
	atomic_set(&store->users, 1);
	spin_lock_bh(&ath12k_cfg_stores_lock);
	list_add_tail(&store->node, &ath12k_cfg_stores_list);
	spin_unlock_bh(&ath12k_cfg_stores_lock);
	*out_store = store;

	return 0;
}

/**
 * ath12k_cfg_store_free - Free the cfg store
 * @store: Reference to cfg value store
 * Return: void
 */
static void ath12k_cfg_store_free(struct ath12k_cfg_value_store *store)
{
	if (store) {
		spin_lock_bh(&ath12k_cfg_stores_lock);
		list_del_init(&store->node);
		spin_unlock_bh(&ath12k_cfg_stores_lock);
		kfree(store->path);
		vfree(store);
	}
}

/**
 * list_peek_front - Get the front of the list
 * @list: list head reference
 * @node2: The node that is in the front of the linked list
 * Return: 0 on success, Non-zero on error
 */
static
int list_peek_front(struct list_head *list,  struct list_head **node2)
{
	struct list_head *listptr;

	if (list_empty(list))
		return -EINVAL;

	listptr = list->next;
	*node2 = listptr;

	return 0;
}

/**
 * list_peek_next - Get the next element of the list
 * @list: list head reference
 * @node: node element from which the next element should be fetched
 * @node2: The node element that is the next element from the node
 * Return: 0 on success, Non-zero on error
 */
static
int list_peek_next(struct list_head *list, struct list_head *node,
		   struct list_head **node2)
{
	if (!list || !node || !node2)
		return -EINVAL;

	if (list_empty(list))
		return -EINVAL;

	if (node->next == list)
		return -EINVAL;

	*node2 = node->next;

	return 0;
}

/**
 * ath12k_cfg_store_get - Get the reference of the store
 * @path: Path from where store is created
 * @out_store: Reference to cfg value store
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_store_get(const char *path, struct ath12k_cfg_value_store **out_store)
{
	int ret;
	struct list_head *node;

	*out_store = NULL;

	spin_lock_bh(&ath12k_cfg_stores_lock);
	ret = list_peek_front(&ath12k_cfg_stores_list, &node);
	while ((ret == 0)) {
		struct ath12k_cfg_value_store *store =
		    container_of(node, struct ath12k_cfg_value_store, node);

		if (!strcmp(path, store->path)) {
			atomic_inc(&store->users);
			*out_store = store;
			break;
		}
		ret = list_peek_next(&ath12k_cfg_stores_list, node, &node);
	}
	spin_unlock_bh(&ath12k_cfg_stores_lock);

	return ret;
}

/**
 * ath12k_cfg_store_put - release reference of the store
 * @store: Reference to cfg value store
 * Return: void
 */
static void ath12k_cfg_store_put(struct ath12k_cfg_value_store *store)
{
	if (store && atomic_dec_and_test(&store->users))
		ath12k_cfg_store_free(store);
}

/**
 * ath12k_cfg_store_set - set reference of the store to the ath12k_base
 * @cfg_ctx: Pointer to ath12k_cfg_ctx
 * Return: void
 */
static void ath12k_cfg_store_set(struct ath12k_cfg_ctx *cfg_ctx)
{
	atomic_inc(&ath12k_cfg_global_store->users);
	cfg_ctx->store = ath12k_cfg_global_store;
}

/**
 * ath12k_cfg_get_ctx - get the ath12k_cfg context from the base
 * @ab: ath12k_base reference
 * Return: reference to the cfg context on success
 */
static struct ath12k_cfg_ctx *ath12k_cfg_get_ctx(struct ath12k_base *ab)
{
	struct ath12k_cfg_ctx *cfg_ctx;

	cfg_ctx = ab->cfg_ctx;
	WARN_ON(!cfg_ctx);
	return cfg_ctx;
}

struct ath12k_cfg_values *ath12k_cfg_get_values(struct ath12k_base *ab)
{
	return &ath12k_cfg_get_ctx(ab)->store->values;
}
EXPORT_SYMBOL(ath12k_cfg_get_values);

/**
 * ath12k_cfg_ini_parse_to_store - Parse the ini file to populate the cfg store
 * @path: Path from where store is created
 * @store: Reference to cfg value store
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_ini_parse_to_store(const char *path,
			      struct ath12k_cfg_value_store *store)
{
	int ret;

	ret = ath12k_ini_parse(path, store, ath12k_cfg_ini_item_handler);

	if (ret)
		ath12k_err(NULL,
			   "Failed to parse *.ini file @ %s; status:%d",
			   path, ret);

	return ret;
}

/**
 * ath12k_cfg_ini_section_parse_to_store - Parse ini section to fill cfg store
 * @path: Path from where store is created
 * @section_name: Section name to be parsed
 * @store: Reference to cfg value store
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_ini_section_parse_to_store(const char *path,
				      const char *section_name,
				      struct ath12k_cfg_value_store *store)
{
	int ret;

	ret = ath12k_ini_section_parse(path, store,
					  ath12k_cfg_ini_item_handler,
					  section_name);
	if (ret)
		ath12k_err(NULL,
			   "Failed to parse *.ini file @ %s; status:%d",
			   path, ret);

	return ret;
}

/**
 * ath12k_cfg_parse_to_store - Parse the ini to populate the cfg store
 * @ab: reference to ath12_base
 * @path: Path from where store is created
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_parse_to_store(struct ath12k_base *ab, const char *path)
{
	return ath12k_cfg_ini_parse_to_store(path,
					     ath12k_cfg_get_ctx(ab)->store);
}

/**
 * ath12k_cfg_section_parse_to_store - Parse the section and populate the store
 * @ab: reference to ath12_base
 * @path: Path from where store is created
 * @section_name: Section name to be parsed
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_section_parse_to_store(struct ath12k_base *ab,
				  const char *path,
				  const char *section_name)
{
	return ath12k_cfg_ini_section_parse_to_store(path, section_name,
						     ath12k_cfg_get_ctx(ab)->store);
}

/**
 * ath12k_cfg_on_create - Create the cfg store context
 * @ab: reference to ath12_base
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_on_create(struct ath12k_base *ab)
{
	struct ath12k_cfg_ctx *cfg_ctx;

	WARN_ON(!ath12k_cfg_global_store);
	if (!ath12k_cfg_global_store)
		return -EINVAL;

	cfg_ctx = kzalloc(sizeof(*cfg_ctx), GFP_KERNEL);

	if (!cfg_ctx)
		return -ENOMEM;

	ath12k_cfg_store_set(cfg_ctx);
	ab->cfg_ctx = cfg_ctx;
	return 0;
}

/**
 * ath12k_cfg_on_destroy - Destroy the cfg store context
 * @ab: reference to ath12_base
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_on_destroy(struct ath12k_base *ab)
{
	int ret = 0;

	if (!ab || !ab->cfg_ctx)
		return -EINVAL;

	if (!ab->cfg_ctx->store) {
		kfree(ab->cfg_ctx);
		ab->cfg_ctx = NULL;
		return -EINVAL;
	}

	ath12k_cfg_store_put(ab->cfg_ctx->store);
	kfree(ab->cfg_ctx);
	ab->cfg_ctx = NULL;

	return ret;
}

/**
 * ath12k_cfg_parse - main function to start the parsing of file
 * @path: path to the file
 * Return: 0 on success, Non-zero on error
 */
int ath12k_cfg_parse(const char *path)
{
	int ret;
	struct ath12k_cfg_value_store *store;

	if (!ath12k_cfg_global_store) {
		ret = ath12k_cfg_store_alloc(path, &store);
		if (ret)
			return ret;

		ath12k_cfg_store_set_defaults(store);
		ret = ath12k_cfg_ini_parse_to_store(path, store);
		if (ret)
			goto free_store;

		ath12k_cfg_global_store = store;
		return 0;
	}
	store = ath12k_cfg_global_store;
	ret = ath12k_cfg_ini_parse_to_store(path, store);
	return ret;

free_store:
	ath12k_cfg_store_free(store);

	return ret;
}

/**
 * ath12k_cfg_ab_parse - Parse the file to populate the ab cfg context
 * @ab: reference to ath12_base
 * @path: Path from where store is created
 * Return: 0 on success, Non-zero on error
 */
static int
ath12k_cfg_ab_parse(struct ath12k_base *ab, const char *path)
{
	int ret;
	struct ath12k_cfg_value_store *store;
	struct ath12k_cfg_ctx *cfg_ctx;

	if (!ath12k_cfg_global_store || !ath12k_cfg_global_init_done || !ab || !path) {
		ath12k_err(ab, "Failed to parse the file investigate \n");
		return -EINVAL;
	}

	cfg_ctx = ath12k_cfg_get_ctx(ab);

	if (!cfg_ctx || cfg_ctx->store != ath12k_cfg_global_store)
		return 0;

	/* check if @path has been parsed before */
	ret = ath12k_cfg_store_get(path, &store);
	if (ret) {
		ret = ath12k_cfg_store_alloc(path, &store);
		if (ret)
			return ret;

		/* inherit global configuration */
		memcpy(&store->values, &ath12k_cfg_global_store->values,
		       sizeof(store->values));

		ret = ath12k_cfg_ini_parse_to_store(path, store);
		if (ret)
			goto put_store;
	}

	cfg_ctx->store = store;
	ath12k_cfg_store_put(ath12k_cfg_global_store);

	return 0;

put_store:
	ath12k_cfg_store_put(store);

	return ret;
}

/**
 * ath12k_cfg_parse_section_store - Parse the section to populate cfg store
 * @ab: reference to ath12_base
 * @ini_buf: ini buffer reference
 * Return:  void
 */
static void ath12k_cfg_parse_section_store(struct ath12k_base *ab,
					   char *ini_buf)
{
	ath12k_cfg_section_parse_to_store(ab, ini_buf, ATH12K_CFG_512M_SECTION);
}

int
ath12k_cfg_store_print(struct ath12k_base *ab)
{
	struct ath12k_cfg_value_store *store;
	struct ath12k_cfg_ctx *cfg_ctx;
	void *offset;
	u32 i;

	cfg_ctx = ath12k_cfg_get_ctx(ab);
	if (!cfg_ctx)
		return -EINVAL;

	store = cfg_ctx->store;
	if (!store)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(ath12k_cfg_meta_lookup_table); i++) {
		const struct ath12k_cfg_meta *meta = &ath12k_cfg_meta_lookup_table[i];

		offset = ath12k_cfg_value_ptr(store, meta);
		switch (meta->cfg_data_type) {
		case ATH12K_CFG_INT_ITEM:
			ath12k_dbg(NULL, ATH12K_DBG_INI, "%p %s %d", offset,
				   meta->name, *((int32_t *)offset));
			break;
		case ATH12K_CFG_UINT_ITEM:
			ath12k_dbg(NULL, ATH12K_DBG_INI, "%p %s %d", offset,
				   meta->name, *((uint32_t *)offset));
			break;
		case ATH12K_CFG_BOOL_ITEM:
			ath12k_dbg(NULL, ATH12K_DBG_INI, "%p %s %d", offset,
				   meta->name, *((bool *)offset));
			break;
		case ATH12K_CFG_STRING_ITEM:
			ath12k_dbg(NULL, ATH12K_DBG_INI, "%p %s %s", offset,
				   meta->name, (char *)offset);
			break;
		case ATH12K_CFG_MAC_ADDR_ITEM:
			ath12k_dbg(NULL, ATH12K_DBG_INI, "%p %s " MAC_ADDR_FMT,
				   offset, meta->name,
				   MAC_ADDR_REF((uint8_t *)offset));
			break;
		default:
			continue;
		}
	}

	return 0;
}

int
ath12k_cfg_ini_config_print(struct ath12k_base *ab, uint8_t *buf,
			    ssize_t *plen, ssize_t buflen)
{
	struct ath12k_cfg_value_store *store;
	struct ath12k_cfg_ctx *cfg_ctx;
	ssize_t len;
	ssize_t total_len = buflen;
	u32 i;
	void *offset;

	cfg_ctx = ath12k_cfg_get_ctx(ab);
	if (!cfg_ctx)
		return -1;

	store = cfg_ctx->store;
	if (!store)
		return -1;

	for (i = 0; i < ARRAY_SIZE(ath12k_cfg_meta_lookup_table); i++) {
		const struct ath12k_cfg_meta *meta = &ath12k_cfg_meta_lookup_table[i];

		offset = ath12k_cfg_value_ptr(store, meta);

		switch (meta->cfg_data_type) {
		case ATH12K_CFG_INT_ITEM:
			len = scnprintf(buf, buflen, "%s %d\n", meta->name,
					*((int32_t *)offset));
			buf += len;
			buflen -= len;
			break;
		case ATH12K_CFG_UINT_ITEM:
			len = scnprintf(buf, buflen, "%s %d\n", meta->name,
					*((uint32_t *)offset));
			buf += len;
			buflen -= len;
			break;
		case ATH12K_CFG_BOOL_ITEM:
			len = scnprintf(buf, buflen, "%s %d\n", meta->name,
					*((bool *)offset));
			buf += len;
			buflen -= len;
			break;
		case ATH12K_CFG_STRING_ITEM:
			len = scnprintf(buf, buflen, "%s %s\n", meta->name,
					(char *)offset);
			buf += len;
			buflen -= len;
			break;
		case ATH12K_CFG_MAC_ADDR_ITEM:
			len = scnprintf(buf, buflen,
					"%s " MAC_ADDR_FMT "\n",
					meta->name,
					MAC_ADDR_REF((uint8_t *)offset));
			buf += len;
			buflen -= len;
			break;
		default:
			continue;
		}
	}

	*plen = total_len - buflen;

	return 0;
}

int ath12k_cfg_get_ini_file_name(u32 target_type, struct ath12k_ini_file *ini)
{
	int ret = 0;

	switch (target_type) {
	case ATH12K_HW_QCN9274_HW10:
	case ATH12K_HW_QCN9274_HW20:
		ini->external = "QCN9274.ini";
		ini->internal = "QCN9274_i.ini";
		break;
	case ATH12K_HW_IPQ5332_HW10:
		ini->external = "IPQ5332.ini";
		ini->internal = "IPQ5332_i.ini";
		break;
	case ATH12K_HW_IPQ5424_HW10:
		ini->external = "IPQ5424.ini";
		ini->internal = "IPQ5424_i.ini";
		break;
	case ATH12K_HW_QCN6432_HW10:
		ini->external = "QCN6432.ini";
		ini->internal = "QCN6432_i.ini";
		break;
	default:
		ini->external = NULL;
		ini->internal = NULL;
		ret = -EINVAL;
		ath12k_err(NULL,
			   "Target specific INI not found tgt_type %d",
			   target_type);
		break;
	}
	return ret;
}

void ath12k_cfg_global_init(void)
{
	char global_file[ATH12K_CFG_FILE_NAME_MAX] = {0};
	char global_file_i[ATH12K_CFG_FILE_NAME_MAX] = {0};

	ath12k_info(NULL, "Initializing global INI configuration");

	/* Check if already initialized */
	if (ath12k_cfg_global_init_done) {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "Global INI configuration already initialized");
		return;
	}

	INIT_LIST_HEAD(&ath12k_cfg_stores_list);
	spin_lock_init(&ath12k_cfg_stores_lock);

	/* Parse global configuration files */
	scnprintf(global_file, sizeof(global_file), "global.ini");
	if (ath12k_cfg_parse(global_file)) {
		ath12k_err(NULL, "Failed to parse the global ini %s\n", global_file);
		ath12k_cfg_global_deinit();
		return;
	}

	/* Parse internal global configuration files */
	scnprintf(global_file_i, sizeof(global_file_i), "internal/global_i.ini");
	if (ath12k_cfg_parse(global_file_i)) {
		ath12k_err(NULL, "Failed to parse global_i ini %s\n", global_file_i);
		ath12k_cfg_global_deinit();
		return;
	}

	/* Mark global initialization as complete */
	ath12k_cfg_global_init_done = true;
	ath12k_info(NULL, "Global INI configuration initialized successfully");
}
EXPORT_SYMBOL(ath12k_cfg_global_init);

void ath12k_cfg_global_deinit(void)
{
	ath12k_info(NULL, "Deinitializing INI global configuration");

	if (!ath12k_cfg_global_init_done) {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "Global INI configuration not initialized");
		return;
	}
	/* Release the global store */
	ath12k_cfg_store_put(ath12k_cfg_global_store);
	ath12k_cfg_global_init_done = false;
	ath12k_cfg_global_store = NULL;
	ath12k_info(NULL, "Global INI configuration deinitialized successfully");
}
EXPORT_SYMBOL(ath12k_cfg_global_deinit);

int ath12k_cfg_init(struct ath12k_base *ab)
{
	struct ath12k_ini_file ini;
	char ini_buf[ATH12K_CFG_FILE_NAME_MAX] = {0};
	int ret;

	ath12k_info(ab, "Initializing per-radio INI configuration");

	if (!ab) {
		ath12k_err(NULL, "Invalid base pointer");
		return -EINVAL;
	}

	/* Ensure global initialization is complete */
	if (!ath12k_cfg_global_init_done) {
		ath12k_err(ab, "Global INI configuration not initialized\n");
		return -EINVAL;
	}
	 /* Check if this radio is already initialized */
	if (ab->cfg_ctx && ab->cfg_ctx->initialized) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "INI Configuration already initialized for this radio\n");
		return 0;
	}

	 /* Create the per-radio context if it doesn't exist */
	if (!ab->cfg_ctx) {
		ret = ath12k_cfg_on_create(ab);
		if (ret) {
			ath12k_err(ab, "Failed to create the cfg store context\n");
			return ret;
		}
	}

	/* Parse the target specific INI */
	scnprintf(ini_buf, sizeof(ini_buf), "internal/");
	ret = ath12k_cfg_get_ini_file_name(ab->hw_rev, &ini);

	if (!ret) {
		strlcat(ini_buf, ini.internal, sizeof(ini_buf));
		if (ath12k_cfg_ab_parse(ab, ini.external) == 0)
			ath12k_cfg_parse_to_store(ab, ini_buf);
		else
			ath12k_cfg_ab_parse(ab, ini_buf);

		ath12k_cfg_parse_section_store(ab, ini_buf);
	}
	/* Mark this radio's configuration as initialized */
	ab->cfg_ctx->initialized = true;
	ath12k_info(ab, "Successfully initialized INI info \n");
	return ret;
}

void ath12k_cfg_deinit(struct ath12k_base *ab)
{
	if (!ab) {
		ath12k_err(NULL, "Invalid base pointer");
		return;
	}

	ath12k_info(ab, "Deinitializing per-radio INI configuration");

	if (ab->cfg_ctx) {
		ab->cfg_ctx->initialized = false;
		ath12k_cfg_on_destroy(ab);
	}

	ath12k_info(ab, "Successfully deinitialized per-radio INI configuration\n");
}

void ath12k_cfg_parse_pdev_section(struct ath12k_base *ab)
{
	struct ath12k_pdev *pdev;
	struct ath12k_ini_file ini;
	char ini_buf[100] = {0};
	int ret;
	u32 i;

	scnprintf(ini_buf, sizeof(ini_buf), "internal/");
	if (!ab->cfg_ctx)
		return;
	/* Parse the target specific INI */
	ret = ath12k_cfg_get_ini_file_name(ab->hw_rev, &ini);
	if (ret)
		return;

	strlcat(ini_buf, ini.internal, sizeof(ini_buf));

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (!strcmp(pdev->phy_name, ATH12K_PHY_6GHZ)) {
			ath12k_cfg_section_parse_to_store(ab, ini_buf,
							  ATH12K_CFG_6GHZ_SECTION);
		}
		if (!strcmp(pdev->phy_name, ATH12K_PHY_5GHZ_LOW)) {
			ath12k_cfg_section_parse_to_store(ab, ini_buf,
							  ATH12K_CFG_5GHZ_LOW_SECTION);
		}
	}
}

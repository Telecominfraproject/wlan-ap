/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#include <linux/export.h>
#include <linux/rbtree.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/netlink.h>

#define FUNC_OBJ_ARR_SIZE 500
#define BUF_SIZE 10192

char athmem_stats_buf[BUF_SIZE];
unsigned int athmem_stats_num_nodes;
unsigned long long athmem_stats_dup_ptr;

bool athmem_flag_stop_tracking;
bool athmem_flag_track_only_txskb;
bool athmem_flag_print_only_fn_adds;
bool athmem_flag_limit_op;
bool athmem_flag_init_done;

static struct rb_root athmem_obj_tree_root = RB_ROOT;
spinlock_t athmem_spinlock;

struct athmem_func_obj {
	void *func;
	int line;
	int count;
	unsigned long long size;
};

struct athmem_debug_object {
	struct rb_node rb_node;
	unsigned long long pointer;
	size_t size;
	int line;
	void *func;
};

struct athmem_func_obj athmem_func_obj_arr[FUNC_OBJ_ARR_SIZE];
int athmem_func_obj_arr_len;

static struct athmem_debug_object *lookup_object(unsigned long ptr)
{
	struct rb_node *rb = athmem_obj_tree_root.rb_node;
	struct athmem_debug_object *object;

	while (rb) {
		object = rb_entry(rb, struct athmem_debug_object, rb_node);
		if (ptr < object->pointer)
			rb = object->rb_node.rb_left;
		else if (ptr > object->pointer)
			rb = object->rb_node.rb_right;
		else if (object->pointer == ptr)
			return object;
	}
	return NULL;
}

static void athmem_remove_object(struct athmem_debug_object *object)
{
	rb_erase(&object->rb_node, &athmem_obj_tree_root);
}

static struct athmem_debug_object *athmem_find_and_remove_obj(unsigned long ptr)
{
	unsigned long flags;
	struct athmem_debug_object *object;

	spin_lock_irqsave(&athmem_spinlock, flags);
	object = lookup_object(ptr);
	if (object) {
		athmem_stats_num_nodes--;
		athmem_remove_object(object);
	}
	spin_unlock_irqrestore(&athmem_spinlock, flags);

	return object;
}

static void athmem_delete_obj_full(unsigned long ptr)
{
	struct athmem_debug_object *object;

	object = athmem_find_and_remove_obj(ptr);
	if (!object)
		return;
	kfree(object);
}

static void athmem_debug_print_rbtree(void)
{
	struct rb_node *node;
	unsigned long flags;
	struct athmem_debug_object *obj;

	pr_info("rb tree :\n");
	spin_lock_irqsave(&athmem_spinlock, flags);
	for (node = rb_first(&athmem_obj_tree_root); node; node = rb_next(node)) {
		obj = rb_entry(node, struct athmem_debug_object, rb_node);
		pr_info("func : %s line : %d addr : %llx\n", (char *)obj->func, obj->line,
			obj->pointer);
	}
	spin_unlock_irqrestore(&athmem_spinlock, flags);
}

static inline void athmem_remove_dup(struct rb_node *node)
{
	int i;
	struct athmem_debug_object *obj = rb_entry(node,
						   struct athmem_debug_object,
						   rb_node);

	for (i = 0; i < athmem_func_obj_arr_len; i++) {
		if (obj->func == athmem_func_obj_arr[i].func &&
		    obj->line == athmem_func_obj_arr[i].line) {
			athmem_func_obj_arr[i].size +=  obj->size;
			athmem_func_obj_arr[i].count++;
			return;
		}
	}

	if (athmem_func_obj_arr_len < FUNC_OBJ_ARR_SIZE) {
		athmem_func_obj_arr[i].func =  obj->func;
		athmem_func_obj_arr[i].line =  obj->line;
		athmem_func_obj_arr[i].size =  obj->size;
		athmem_func_obj_arr[i].count = 1;
		athmem_func_obj_arr_len++;
	} else {
		pr_err("functions stats buffer is full\n");
	}
}

static void athmem_sort_func_obj_arr(void)
{
	struct athmem_func_obj temp;
	int i;
	int j;

	for (i = 0 ; i < athmem_func_obj_arr_len; i++) {
		for (j = i + 1; j < athmem_func_obj_arr_len; j++) {
			if (athmem_func_obj_arr[i].size < athmem_func_obj_arr[j].size) {
				temp = athmem_func_obj_arr[i];
				athmem_func_obj_arr[i] = athmem_func_obj_arr[j];
				athmem_func_obj_arr[j] = temp;
			}
		}
	}
}

static void athmem_reset_func_obj(void)
{
	memset(athmem_func_obj_arr, 0, sizeof(athmem_func_obj_arr));
	athmem_func_obj_arr_len = 0;
}

static int athmem_copy_memory_stats(int is_panic)
{
	struct rb_node *node;
	int i;
	unsigned long flags;
	unsigned long long total_size = 0;
	int len = 0;
	unsigned long long rb_nodes_count;
	char *buf = athmem_stats_buf;
	int buf_len = BUF_SIZE;

	if (is_panic)
		athmem_flag_stop_tracking = 1;

	spin_lock_irqsave(&athmem_spinlock, flags);
	len += scnprintf(buf + len, buf_len - len, "node count :  %d\n", athmem_stats_num_nodes);

	for (node = rb_first(&athmem_obj_tree_root); node; node = rb_next(node))
		athmem_remove_dup(node);

	rb_nodes_count = athmem_stats_num_nodes;
	spin_unlock_irqrestore(&athmem_spinlock, flags);

	athmem_sort_func_obj_arr();

	for (i = 0; i < athmem_func_obj_arr_len; i++) {
		if (!athmem_flag_limit_op || i < 10) {
			if (!athmem_flag_print_only_fn_adds) {
				len += scnprintf(buf + len, buf_len - len,
						 "func : %s (%p) \t line : %d count : %d size %llu\n",
						 (char *)athmem_func_obj_arr[i].func,
						 (char *)athmem_func_obj_arr[i].func,
						 athmem_func_obj_arr[i].line,
						 athmem_func_obj_arr[i].count,
						 athmem_func_obj_arr[i].size);
			} else {
				len += scnprintf(buf + len, buf_len - len,
						 "func : %p \t line : %d count : %d size %llu\n",
						 (char *)athmem_func_obj_arr[i].func,
						 athmem_func_obj_arr[i].line,
						 athmem_func_obj_arr[i].count,
						 athmem_func_obj_arr[i].size);
			}
		}
		total_size += athmem_func_obj_arr[i].size;
	}

	len += scnprintf(buf + len, buf_len - len, "total allocated mem(bytes) : %llu\n",
			 total_size);
	len += scnprintf(buf + len, buf_len - len, "memory size of rb tree : %llu\n",
			 rb_nodes_count * sizeof(struct athmem_debug_object));
	len += scnprintf(buf + len, buf_len - len, "dup ptr count %llu\n",
			 athmem_stats_dup_ptr);

	athmem_reset_func_obj();

	return len;
}

static void athmem_delete_rb_tree(void)
{
	struct rb_node *node;
	unsigned long flags;

	pr_info("deleting rb tree\n");
	spin_lock_irqsave(&athmem_spinlock, flags);

	for (node = rb_first(&athmem_obj_tree_root); node; node = rb_next(node))
		rb_erase(node, &athmem_obj_tree_root);

	athmem_stats_num_nodes = 0;
	spin_unlock_irqrestore(&athmem_spinlock, flags);
}

static ssize_t athmem_debug_read(struct file *file,
				 char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	size_t len;
	int ret;
	char *buf = athmem_stats_buf;

	len = athmem_copy_memory_stats(0);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	return ret;
}

static void athmem_debug_help(void)
{
	pr_info("ATHMEMDEBUG: ------ HELP ------\n"
		"0:\t print help\n"
		"1:\t stop memory tracking\n"
		"2:\t resume memory tracking\n"
		"3:\t print node_count in rb_tree\n"
		"4:\t enable flag to print only function name address\n"
		"5:\t disable flag to print only function name address\n"
		"6:\t limit stats output\n"
		"7:\t enable track only TX SKB\n"
		"8:\t disable track only TX SKB\n"
		"9:\t delete rb tree\n"
		"10:\t print rb tree\n");
}

static ssize_t athmem_debug_write(struct file *file,
				  const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	u8 type = 0;
	int ret;

	ret = kstrtou8_from_user(user_buf, count, 0, &type);
	if (ret) {
		pr_err("no input received\n");
		return ret;
	}

	switch (type) {
	case 0:
		athmem_debug_help();
		break;
	case 1:
		athmem_flag_stop_tracking = 1;
		pr_info("athmem: stopped tracking\n");
		break;
	case 2:
		athmem_flag_stop_tracking = 0;
		pr_info("athmem: resume tracking\n");
		break;
	case 3:
		pr_info("athmem: node count: %d\n", athmem_stats_num_nodes);
		break;
	case 4:
		athmem_flag_print_only_fn_adds = 1;
		pr_info("athmem: athmem_flag_print_only_fn_adds enabled\n");
		break;
	case 5:
		athmem_flag_print_only_fn_adds = 0;
		pr_info("athmem: athmem_flag_print_only_fn_adds disabled\n");
		break;
	case 6:
		athmem_flag_limit_op = 1;
		pr_info("athmem: mem stats output is limited\n");
		break;
	case 7:
		athmem_flag_track_only_txskb = 1;
		pr_info("athmem: track only TX SKB enabled\n");
		break;
	case 8:
		athmem_flag_track_only_txskb = 0;
		pr_info("athmem: track only TX SKB disabled\n");
		break;
	case 9:
		athmem_delete_rb_tree();
		break;
	case 10:
		athmem_debug_print_rbtree();
		break;
	default:
		pr_info("athmem: unknown option\n");
		athmem_debug_help();
		break;
	}

	ret = count;

	return ret;
}

static const struct file_operations athmem_debug_fops = {
	.read = athmem_debug_read,
	.write = athmem_debug_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int athmem_debug_debugfs_add(void)
{
	struct dentry *subdir;

	subdir = debugfs_create_dir("ath_memdebug", NULL);
	debugfs_create_file("meminfo", 0644, subdir,
			    &athmem_obj_tree_root, &athmem_debug_fops);
	return 0;
}

static void athmem_create_object(unsigned long long ptr, size_t size,
			  gfp_t gfp, int line, void *func)
{
	unsigned long flags;
	struct athmem_debug_object *object, *parent;
	struct rb_node **link, *rb_parent;

	if (!athmem_flag_init_done) {
		athmem_debug_debugfs_add();
		athmem_flag_init_done = 1;
		spin_lock_init(&athmem_spinlock);
	}

	object = kmalloc(sizeof(*object), (gfp));
	if (!object) {
		pr_warn("Allocation of athmem_debug_object failed\n");
		return;
	}

	object->pointer = ptr;
	object->size = size;
	object->line = line;
	object->func = func;

	spin_lock_irqsave(&athmem_spinlock, flags);

	link = &athmem_obj_tree_root.rb_node;
	rb_parent = NULL;
	while (*link) {
		rb_parent = *link;
		parent = rb_entry(rb_parent, struct athmem_debug_object, rb_node);
                if (ptr < parent->pointer) {
			link = &parent->rb_node.rb_left;
		} else if (ptr > parent->pointer){
			link = &parent->rb_node.rb_right;
		} else {
			athmem_stats_dup_ptr++;
			spin_unlock_irqrestore(&athmem_spinlock, flags);
			pr_debug("dup ptr incrementing %s %d ptr = %p", (char *)func,
				 line, (void *)ptr);
			kfree(object);
			return;
		}
	}
	athmem_stats_num_nodes++;
	rb_link_node(&object->rb_node, rb_parent, link);
	rb_insert_color(&object->rb_node, &athmem_obj_tree_root);
	spin_unlock_irqrestore(&athmem_spinlock, flags);
}

void ath_upate_oom_panic(int is_panic)
{
	if (is_panic)
	athmem_flag_stop_tracking = 1;
}
EXPORT_SYMBOL(ath_upate_oom_panic);

void ath_update_free(void *ptr)
{
	if (athmem_flag_stop_tracking || !athmem_flag_init_done)
		return;

	athmem_delete_obj_full((unsigned long)ptr);
}
EXPORT_SYMBOL(ath_update_free);

void ath_update_free_skb_list(struct sk_buff_head *skb_list)
{
	struct sk_buff *skb = NULL, *next = NULL;
	skb_queue_walk_safe(skb_list, skb, next) {
		if (skb)
			ath_update_free(skb);
	}
}
EXPORT_SYMBOL(ath_update_free_skb_list);

void ath_update_alloc(void *ptr, int len, int line, void *func, int is_txskb)
{
	if (!ptr || athmem_flag_stop_tracking || (athmem_flag_track_only_txskb && !is_txskb))
		return;

	athmem_create_object((unsigned long)ptr, len, GFP_ATOMIC, line, func);
}
EXPORT_SYMBOL(ath_update_alloc);

void *ath_kmalloc(size_t len, gfp_t flags, int line, void *func)
{
	void *addr = kmalloc(len, flags);
	ath_update_alloc(addr, len, line, func, 0);
        return addr;
}
EXPORT_SYMBOL(ath_kmalloc);

void *ath_kmemdup(const void *src, size_t len, gfp_t flags, int line,
		  void *func)
{
	void *addr = (void *)kmemdup(src, len, flags);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_kmemdup);

void *ath_kzalloc(size_t len, gfp_t flags, int line, void *func)
{
	void *addr = kzalloc(len, flags);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_kzalloc);

void *ath_netdev_alloc_skb(struct net_device *dev, unsigned int len,
			   int line, void *func)
{
	void *addr = netdev_alloc_skb(dev, len);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_netdev_alloc_skb);

void *ath_netdev_alloc_skb_fast(struct net_device *dev, unsigned int len,
				int line, void *func)
{
	void *addr = netdev_alloc_skb_fast(dev, len);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_netdev_alloc_skb_fast);

void *ath_netdev_alloc_skb_no_skb_reset(struct net_device *dev, unsigned int len,
					gfp_t flags, int line, void *func)
{
	void *addr = __netdev_alloc_skb_no_skb_reset(dev, len, flags);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_netdev_alloc_skb_no_skb_reset);

void *ath_dev_alloc_skb(unsigned int len, int line, void *func)
{
	void *addr = dev_alloc_skb(len);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_dev_alloc_skb);

void *ath_skb_copy(const struct sk_buff *skb, gfp_t flags, int line,
		   void *func)
{
	void *addr = skb_copy(skb, flags);
	ath_update_alloc(addr, skb->truesize, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_skb_copy);

void *ath_skb_clone(struct sk_buff *skb, gfp_t flags, int line,
                    void *func)
{
        void *addr = skb_clone(skb, flags);
        ath_update_alloc(addr, skb->truesize, line, func, 0);
        return addr;
}
EXPORT_SYMBOL(ath_skb_clone);

void *ath_skb_clone_sk(struct sk_buff *skb, int line, void *func)
{
	void *addr = skb_clone_sk(skb);
	ath_update_alloc(addr, skb->truesize, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_skb_clone_sk);

void *ath_skb_share_check(struct sk_buff *skb, gfp_t flags, int line,
			  void *func)
{
	void *addr = skb_share_check(skb, flags);
	ath_update_alloc(addr, skb->truesize, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_skb_share_check);

void *ath_nlmsg_new(size_t len, gfp_t flags, int line, void *func)
{
	void *addr = nlmsg_new(len, flags);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_nlmsg_new);

void *ath_vmalloc(unsigned long len, int line, void *func)
{
	void *addr = vmalloc(len);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_vmalloc);

void *ath_vzalloc(unsigned long len, int line, void *func)
{
	void *addr = vzalloc(len);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_vzalloc);

void *ath_dma_alloc_coherent(struct device *dev, size_t len,
			     dma_addr_t *handle, gfp_t flags,
			     int line, void *func)
{
	void *addr = dma_alloc_coherent(dev, len, handle, flags);
	ath_update_alloc(addr, len, line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_dma_alloc_coherent);

void *ath_kcalloc(size_t n, size_t len, gfp_t flags, int line,
		  void *func)
{
	void *addr = kcalloc(n, len, flags);
	ath_update_alloc(addr, (n * len), line, func, 0);
	return addr;
}
EXPORT_SYMBOL(ath_kcalloc);

void ath_kfree(void *ptr)
{
	ath_update_free(ptr);
	kfree(ptr);
}
EXPORT_SYMBOL(ath_kfree);

void ath_vfree(void *ptr)
{
	ath_update_free(ptr);
	vfree(ptr);
}
EXPORT_SYMBOL(ath_vfree);

void ath_dma_free_coherent(struct device *dev, size_t size,
			   void *cpu_addr, dma_addr_t dma_handle)
{
	ath_update_free(cpu_addr);
	dma_free_coherent(dev, size, cpu_addr, dma_handle);
	return;
}
EXPORT_SYMBOL(ath_dma_free_coherent);

void ath_kfree_sensitive(const void *ptr)
{
	ath_update_free((void *)ptr);
	kfree_sensitive(ptr);
}
EXPORT_SYMBOL(ath_kfree_sensitive);

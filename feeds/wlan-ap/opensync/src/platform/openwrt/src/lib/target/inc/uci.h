#ifndef _UCI_UTIL_H__
#define _UCI_UTIL_H__

#include <uci.h>
#include <uci_blob.h>

int blob_to_uci(struct blob_attr *a, const struct uci_blob_param_list *param, struct uci_section *s);

#endif

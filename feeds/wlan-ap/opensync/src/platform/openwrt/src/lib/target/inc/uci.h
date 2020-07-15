/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _UCI_UTIL_H__
#define _UCI_UTIL_H__

#include <uci.h>
#include <uci_blob.h>

extern int blob_to_uci_section(struct uci_context *uci,
			       const char *package, const char *section,
			       const char *type, struct blob_attr *a,
			       const struct uci_blob_param_list *param,
			       struct blob_attr *del);
extern int uci_section_del(struct uci_context *uci, char *prefix, char *package, char *section, char *type);
extern void uci_commit_all(struct uci_context *uci);
extern int uci_section_to_blob(struct uci_context *uci, char *package, char *section,
			       struct blob_buf *buf, const struct uci_blob_param_list *param);
#endif

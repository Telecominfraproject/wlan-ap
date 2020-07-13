#ifndef _UCI_UTIL_H__
#define _UCI_UTIL_H__

#include <uci.h>
#include <uci_blob.h>

extern int blob_to_uci_section(struct uci_context *uci,
			       const char *package, const char *section,
			       const char *type, struct blob_attr *a,
			       const struct uci_blob_param_list *param);
extern int uci_section_del(struct uci_context *uci, char *prefix, char *section);
extern void uci_commit_all(struct uci_context *uci);

#endif

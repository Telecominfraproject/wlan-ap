#include <stdio.h>
#include <string.h>
#include <radcli/radcli.h>

int
main(int argc, char **argv)
{
	int result;
	char username[128];
	char passwd[AUTH_PASS_LEN + 1];
	VALUE_PAIR *send, *received;
	uint32_t service;
	rc_handle *rh;

	/* Not needed if you already used openlog() */
	rc_openlog("radiusprobe");

	if ((rh = rc_read_config("/tmp/radius.conf")) == NULL)
		return ERROR_RC;

	strcpy(username, "healthcheck");
	strcpy(passwd, "uCentral");

	send = NULL;

	if (rc_avpair_add(rh, &send, PW_USER_NAME, username, -1, 0) == NULL)
		return ERROR_RC;

	if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, passwd, -1, 0) == NULL)
		return ERROR_RC;

	service = PW_AUTHENTICATE_ONLY;
	if (rc_avpair_add(rh, &send, PW_SERVICE_TYPE, &service, -1, 0) == NULL)
		return ERROR_RC;

	result = rc_auth(rh, 0, send, &received, NULL);

	if (result == OK_RC || result == REJECT_RC) {
		fprintf(stderr, "RADIUS server OK\n");
		result = 0;
	} else {
		fprintf(stderr, "RADIUS server failure\n");
		result = -1;
	}

	return result;
}

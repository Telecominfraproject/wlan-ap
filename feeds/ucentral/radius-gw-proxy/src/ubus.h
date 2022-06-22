enum socket_type {
	RADIUS_AUTH = 0,
	RADIUS_ACCT,
	RADIUS_DAS
};

extern struct ubus_auto_conn conn;
extern uint32_t ucentral;

void ubus_init(void);
void ubus_deinit(void);
void gateway_recv(char *data, enum socket_type type);


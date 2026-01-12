#include <arpa/inet.h>
#include <net/if.h>

#include <libubox/list.h>
#include <libubox/ulog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct route {
	struct list_head list;
	char devname[64];
	uint32_t domain;
	uint32_t mask;
};

static struct list_head routes = LIST_HEAD_INIT(routes);

static int parse_routes(void)
{
	FILE *fp = fopen("/proc/net/route", "r");
	int flgs, ref, use, metric, mtu, win, ir;
	struct route *route;
	unsigned long g;
	int r;

	r = fscanf(fp, "%*[^\n]\n");
	if (r < 0) {
		fprintf(stderr, "failed to parse routes\n");
		return -1;
	}
	while (1) {
		route = malloc(sizeof(*route));
		if (!route)
			break;
		memset(route, 0, sizeof(*route));
		r = fscanf(fp, "%63s%x%lx%X%d%d%d%x%d%d%d\n",
			route->devname, &route->domain, &g, &flgs, &ref, &use, &metric, &route->mask,
			&mtu, &win, &ir);
		if (r != 11 && (r < 0) && feof(fp))
			break;
		list_add(&route->list, &routes);
		printf("1 %s %x %x\n", route->devname, ntohl(route->domain), ntohl(route->mask));
	}

	fclose(fp);

	return 0;
}

static int find_collisions(void)
{
	struct route *route;

	list_for_each_entry(route, &routes, list) {
		struct route *compare;

		if (!route->domain || !route->mask)
			continue;
		list_for_each_entry(compare, &routes, list) {
			if (!compare->domain || !compare->mask)
				continue;
			if (compare == route)
				continue;
			if (((route->domain & route->mask) == (compare->domain & route->mask)) ||
			    ((route->domain & compare->mask) == (compare->domain & compare->mask))) {
				ULOG_ERR("collision detected\n");
				return 1;
			}
		}
	}
	ULOG_INFO("no collision detected\n");
	return 0;
}

int main(int argc, char **argv)
{
	ulog_open(ULOG_SYSLOG | ULOG_STDIO, LOG_DAEMON, "ip-collide");

	parse_routes();
	if (!list_empty(&routes))
		return find_collisions();

	return 0;
}

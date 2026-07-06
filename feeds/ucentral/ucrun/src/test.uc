#!./ucrun

let n_runs = 0;

function timeout(priv) {
	printf("timeout[%d]: %d - %s\n", n_runs, time(), priv);

	if (++n_runs >= 3) {
		printf("not scheduling new timeout\n");
		return false;
	}

	return 5000;
}

function process(retcode, priv) {
	printf("process completed %d %s\n", retcode, priv);
}

global.ulog = {
	identity: "ucrun",
	channels: [ "stdio", "syslog" ],
};

global.ubus = {
	object: "ucrun",

	connect: function() {
		printf("connected to ubus\n");
	},

	methods: {
		foo: {
			cb: function(msg) {
				printf("%s\n", msg);
				printf("fooo\n");
				return { foo: true };
			}
		}
	}
};

global.start = function() {
	printf("%s\n", ARGV);

	ulog_info("info: %08d\n", 123);
	ulog_note("note: [%8s]\n", "abc");
	ulog_warn("warn: %c\n", 64);
	ulog_err("err: %.3f\n", 1.0/3.0);

	uloop_timeout(timeout, 1000, { private: "data" });
	uloop_process(process, [ "sleep", "10" ], { sleep: 10 });
	uloop_process(process, [ "echo", "abc" ], { echo: "abc" });
};

global.stop = function() {
	ulog_info("stopping\n");
};

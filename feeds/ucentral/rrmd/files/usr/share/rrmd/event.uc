return {

send: function(type, payload) {
	global.ubus.conn.call('ucentral', 'event', {
		type,
		payload
	});
},

};

'use strict';
'require view';
'require form';
'require rpc';
'require uci';
'require ui';
'require fs';

var callUciCommit = rpc.declare({
	object: 'uci',
	method: 'commit',
	params: [ 'config' ]
});

var callLuciSetPassword = rpc.declare({
	object: 'luci',
	method: 'setPassword',
	params: [ 'username', 'password' ],
	reject: true
});

var callSystemValidateFirmwareImage = rpc.declare({
	object: 'system',
	method: 'validate_firmware_image',
	params: [ 'path' ],
	reject: true
});

var callSystemValidateCertificates = rpc.declare({
	object: 'system',
	method: 'validate_tip_certificates',
	params: [ 'path' ],
	reject: true
});

function parseAddressAndNetmask(ipaddr, netmask) {
        var m = (ipaddr || '').match(/^(.+)\/(\d+)$/);
        if (m) {
                var a = validation.parseIPv4(m[1]),
                    s = network.prefixToMask(m[2]);

                if (a && s)
                        return [ m[1], s ];
        }
        else {
                m = (ipaddr || '').match(/^(.+)\/(.+)$/);

                if (m) {
                        var a = validation.parseIPv4(m[1]),
                            s = network.maskToPrefix(m[2]);

                        if (a && s)
                                return [ m[1], network.prefixToMask(s) ];
                }
                else {
                        return [ ipaddr, netmask ];
                }
        }

        return null;
}

var cbiRichListValue = form.ListValue.extend({
	renderWidget: function(section_id, option_index, cfgvalue) {
		var choices = this.transformChoices();
		var widget = new ui.Dropdown((cfgvalue != null) ? cfgvalue : this.default, choices, {
			id: this.cbid(section_id),
			sort: this.keylist,
			optional: this.optional,
			select_placeholder: this.select_placeholder || this.placeholder,
			custom_placeholder: this.custom_placeholder || this.placeholder,
			validate: L.bind(this.validate, this, section_id),
			disabled: (this.readonly != null) ? this.readonly : this.map.readonly
		});

		return widget.render();
	}
});

var cbiPasswordStrengthIndicator = form.DummyValue.extend({
	setStrength: function(section_id, password) {
		var node = this.map.findElement('id', this.cbid(section_id)),
		    segments = node ? node.firstElementChild.childNodes : [],
		    colors = [ '#d44', '#d84', '#ee4', '#4e4' ],
		    labels = [ _('too short', 'Password strength'), _('weak', 'Password strength'), _('medium', 'Password strength'), _('strong', 'Password strength') ],
		    strongRegex = new RegExp('^(?=.{8,})(?=.*[A-Z])(?=.*[a-z])(?=.*[0-9])(?=.*\\W).*$', 'g'),
		    mediumRegex = new RegExp('^(?=.{7,})(((?=.*[A-Z])(?=.*[a-z]))|((?=.*[A-Z])(?=.*[0-9]))|((?=.*[a-z])(?=.*[0-9]))).*$', 'g'),
		    enoughRegex = new RegExp('(?=.{6,}).*', 'g'),
		    strength;

		if (strongRegex.test(password))
			strength = 3;
		else if (mediumRegex.test(password))
			strength = 2;
		else if (enoughRegex.test(password))
			strength = 1;
		else
			strength = 0;

		for (var i = 0; i < segments.length; i++)
			segments[i].style.background = (i <= strength) ? colors[strength] : '';

		if (node)
			node.lastElementChild.firstChild.data = labels[strength];
	},

	renderWidget: function(section_id, option_index, cfgvalue) {
		return E('div', { 'id': this.cbid(section_id), 'style': 'display:flex' }, [
			E('div', { 'style': 'align-self:center; display:flex; border:1px solid #aaa; height:.4rem; width:200px; margin:.2rem' }, [
				E('div', { 'style': 'flex:1 1 25%; border-right:1px solid #aaa' }),
				E('div', { 'style': 'flex:1 1 25%; border-right:1px solid #aaa' }),
				E('div', { 'style': 'flex:1 1 25%; border-right:1px solid #aaa' }),
				E('div', { 'style': 'flex:1 1 25%' })
			]),
			E('span', { 'style': 'margin-left:.5rem' }, [ '' ])
		]);
	}
});

function showProgress(text, ongoing) {
	var dlg = ui.showModal(null, [
		E('p', ongoing ? { 'class': 'spinning' } : {}, [ text ])
	]);

	dlg.removeChild(dlg.firstElementChild);

	if (!ongoing) {
		window.setTimeout(function() {
			ui.hideIndicator('uci-changes');
			ui.hideModal();
		}, 750);
	}
}


return view.extend({
	load: function() {
		return Promise.all([
			uci.load('network')
		]);
	},

	handleChangePassword: function() {
		var formdata = { password: {} };
		var m, s, o;

		m = new form.JSONMap(formdata);
		s = m.section(form.NamedSection, 'password', 'password');

		o = s.option(form.Value, 'pw1', _('Enter new password'));
		o.password = true;
		o.validate = function(section_id, value) {
			this.section.children.filter(function(oo) { return oo.option == 'strength' })[0].setStrength(section_id, value);
			return true;
		};

		o = s.option(cbiPasswordStrengthIndicator, 'strength', ' ');

		o = s.option(form.Value, 'pw2', _('Confirm new password'));
		o.password = true;
		o.validate = function(section_id, value) {
			var other = this.section.children.filter(function(oo) { return oo.option == 'pw1' })[0].formvalue(section_id);

			if (other != value)
				return _('The given passwords do not match!');

			return true;
		};

		return m.render().then(L.bind(function(nodes) {
			ui.showModal(_('Change Login Password'), [
				nodes,
				E('div', { 'class': 'right' }, [
					E('button', {
						'click': ui.hideModal
					}, [ _('Cancel') ]),
					E('button', {
						'class': 'important',
						'click': ui.createHandlerFn(this, function(m) {
							return m.save(null, true).then(function() {
								showProgress(_('Setting login password…'), true);
								return callLuciSetPassword('root', formdata.password.pw1).then(function() {
									showProgress(_('Password has been changed.'), false);
								}).catch(function(err) {
									ui.hideModal();
									ui.addNotification(null, _('Unable to change the login password: %s').format(err))
								});
							}).catch(function() {
								var inval = nodes.querySelector('input.cbi-input-invalid');
								if (inval)
									inval.focus();
							});
						}, m)
					}, [ 'Change password' ])
				])
			]);
		}, this));
	},


	handleFirmwareFlash: function(ev) {
		return ui.uploadFile('/tmp/firmware.bin').then(function(res) {
			showProgress(_('Validating image…'), true);

			return callSystemValidateFirmwareImage('/tmp/firmware.bin');
		}).then(function(res) {
			if (!res.valid) {
				showProgress(_('The uploaded firmware image is invalid!'), false);
				return L.resolveDefault(fs.remove('/tmp/firmware.bin'));
			}

			var m, s, o;
			var formdata = { settings: { keep: res.allow_backup ? '1' : null } };

			m = new form.JSONMap(formdata);
			s = m.section(form.NamedSection, 'settings', 'settings');

			if (res.allow_backup) {
				o = s.option(form.Flag, 'keep', _('Keep current system settings over reflash'));
			}
			else {
				o = s.option(form.DummyValue, 'keep');
				o.default = '<em>%h</em>'.format(_('System settings will be reset to factory defaults.'));
				o.rawhtml = true;
			}

			return m.render().then(function(nodes) {
				ui.showModal('Confirm Firmware Flash', [
					E('p', [ _('The uploaded file contains a valid firmware image. Press "Continue" below to start the flash process.') ]),
					nodes,
					E('div', { 'class': 'right' }, [
						E('button', {
							'click': ui.createHandlerFn({}, function() {
								return L.resolveDefault(fs.remove('/tmp/firmware.bin')).then(function() {
									showProgress(_('Upgrade process aborted.'), false);
								});
							})
						}, [ _('Cancel') ]),
						E('button', {
							'class': 'cbi-button-negative',
							'click': ui.createHandlerFn({}, function() {
								return m.save(null, true).then(function() {
									var keep = (formdata.settings.keep == '1'),
									    args = (keep ? [] : [ '-n' ]).concat([ '/tmp/firmware.bin' ]);

									fs.exec('/sbin/sysupgrade', args); /* does not return */

									showProgress(E([], [
										_('The firmware image is flashing now.'),
										E('br'),
										E('em', [ _('Do NOT power off the device until the process is complete!') ])
									]), true);

									window.setTimeout(function() {
										/* FIXME: clarify default IP / domainname */
										ui.awaitReconnect.apply(ui, keep ? [ window.location.host ] : [ '192.168.1.1', 'openwrt.lan', 'openap.lan' ]);
									}, 3000);
								});
							})
						}, [ _('Continue') ])
					])
				]);
			});
		}).catch(function(err) {
			showProgress(_('Firmware upload failed.'), false);
			ui.addNotification(null, _('Unable to upload firmware image: %s').format(err));
		});
	},

	handleSettingsReset: function(ev) {
		ui.showModal(_('Confirm Reset'), [
			E('p', [ _('Do you really want to reset all system settings?') ]),
			E('p', [
				E('em', [  _('Any changes made, including wireless passwords, DHCP reservations, block rules etc. will be erased!') ])
			]),
			E('div', { 'class': 'right' }, [
				E('button', { 'click': ui.hideModal }, [ _('Cancel') ]),
				E('button', {
					'class': 'cbi-button-negative',
					'click': function() {
						showProgress(_('Resetting system configuration…'), true);

						fs.exec('/sbin/firstboot', [ '-r', '-y' ]).then(function() {
							ui.awaitReconnect();
						}).catch(function(err) {
							showProgress(_('Reset command failed.'), false);
							ui.addNotification(null, _('Unable to execute reset command: %s').format(err));
						});
					}
				}, [ _('Reset') ])
			])
		]);
	},

	handleReboot: function(ev) {
		ui.showModal(_('Confirm Reboot'), [
			E('p', [ _('Do you really want to reboot the device?') ]),
			E('div', { 'class': 'right' }, [
				E('button', { 'click': ui.hideModal }, [ _('Cancel') ]),
				E('button', {
					'class': 'important',
					'click': function() {
						showProgress(_('Rebooting device…'), true);

						fs.exec('/sbin/reboot').then(function() {
							ui.awaitReconnect();
						}).catch(function(err) {
							showProgress(_('Reboot command failed.'), false);
							ui.addNotification(null, _('Unable to execute reboot command: %s').format(err));
						});
					}
				}, [ _('Reboot') ])
			])
		]);
	},

	handleCertificateUpload: function(ev) {
		return ui.uploadFile('/tmp/certs.tar').then(function(res) {
			showProgress(_('Uploading certificate…'), true);

			return callSystemValidateCertificates('/tmp/certs.tar');
		}).then(function(res) {
			if (!res.valid) {
				showProgress(_('The uploaded certificates are invalid!'), false);
				return L.resolveDefault(fs.remove('/tmp/certs.tar'));
			}
		}).catch(function(err) {
			showProgress(_('Certificate upload failed.'), false);
			ui.addNotification(null, _('Unable to upload certificates: %s').format(err));
		});
	},

	handleSettingsSave: function(formdata, ev) {
		var m = L.dom.findClassInstance(document.querySelector('.cbi-map'));

		return m.save().then(L.bind(function() {

			var wan = formdata.data.data.wan

			uci.set('network', 'wan', 'proto', wan.proto);
			uci.set('network', 'wan', 'ipaddr', wan.addr);
			uci.set('network', 'wan', 'netmask', wan.mask);
			uci.set('network', 'wan', 'gateway', wan.gateway);
			uci.set('network', 'wan', 'dns', wan.dns);

			return this.handleApply();
		}, this));
	},

	handleApply: function() {
		var dlg = ui.showModal(null, [ E('em', { 'class': 'spinning' }, [ _('Saving configuration…') ]) ]);
		dlg.removeChild(dlg.firstElementChild);

		return uci.save().then(function() {
			return Promise.all([
				callUciCommit('network')
			]);
		}).catch(function(err) {
			ui.addNotification(null, [ E('p', [ _('Failed to save configuration: %s').format(err) ]) ])
		}).finally(function() {
			ui.hideIndicator('uci-changes');
			ui.hideModal();
		});
	},

	render: function(cert_key) {
		var m, s, o;

		var addr_wan = parseAddressAndNetmask(
			uci.get('network', 'wan', 'ipaddr'),
			uci.get('network', 'wan', 'netmask'),
			uci.get('network', 'wan', 'gateway'),
			uci.get('network', 'wan', 'dns'));

		var formdata = {
			wan: {
				proto: uci.get('network', 'wan', 'proto'),
				addr: addr_wan ? addr_wan[0] : null,
				mask: addr_wan ? addr_wan[1] : null,
				gateway: addr_wan ? addr_wan[1] : null,
				dns: addr_wan ? addr_wan[1] : null
			},
			maintenance: {},
			certificates: {}
		};

		m = new form.JSONMap(formdata, _('Setup'));
		m.tabbed = true;

		s = m.section(form.NamedSection, 'wan', 'wan', _('Connectivity'));

		o = s.option(cbiRichListValue, 'proto');
		o.value('dhcp', E('div', { 'style': 'white-space:normal' }, [
			E('strong', [ _('Automatic address configuration (DHCP)') ]), E('br'),
			E('span', { 'class': 'hide-open' })
		]));

		o.value('static', E('div', { 'style': 'white-space:normal' }, [
			E('strong', [ _('Static address configuration') ]), E('br'),
			E('span', { 'class': 'hide-open' })
		]));

		o = s.option(form.Value, 'addr', _('IP Address'));
		o.rmempty = false;
		o.datatype = 'ip4addr("nomask")';
		o.depends('proto', 'static');

		o = s.option(form.Value, 'mask', _('Netmask'));
		o.rmempty = false;
		o.datatype = 'ip4addr("nomask")';
		o.depends('proto', 'static');

		o = s.option(form.Value, 'gateway', _('Gateway'));
		o.rmempty = false;
		o.datatype = 'ip4addr("nomask")';
		o.depends('proto', 'static');

		o = s.option(form.Value, 'dns', _('Nameserver'));
		o.rmempty = false;
		o.datatype = 'ip4addr("nomask")';
		o.depends('proto', 'static');

		o = s.option(form.Button, 'save', _(''));
		o.inputtitle = _('Save Settings');
		o.onclick = ui.createHandlerFn(this, 'handleSettingsSave', m);

		s = m.section(form.NamedSection, 'maintenance', 'maintenance', _('System Maintenance'));

		o = s.option(form.Button, 'upgrade', _('Flash device firmware'));
		o.inputtitle = _('Upload firmware image…');
		o.onclick = ui.createHandlerFn(this, 'handleFirmwareFlash');

		o = s.option(form.Button, 'reset', _('Reset system settings'));
		o.inputtitle = _('Restore system defaults…');
		o.onclick = ui.createHandlerFn(this, 'handleSettingsReset');

		o = s.option(form.Button, 'reboot', _('Restart device'));
		o.inputtitle = _('Reboot…');
		o.onclick = ui.createHandlerFn(this, 'handleReboot');

		s = m.section(form.NamedSection, 'certificates', 'certificates', _('Upgrade Certificates'));

		o = s.option(form.Button, 'upgrade', _('Certificate upload'));
		o.inputtitle = _('Upload certificate…');
		o.onclick = ui.createHandlerFn(this, 'handleCertificateUpload');

		return m.render();
	},

	handleSave: null,
	handleSaveApply: null,
	handleReset: null
});

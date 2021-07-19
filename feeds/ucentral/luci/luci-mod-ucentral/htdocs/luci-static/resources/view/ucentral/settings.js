'use strict';
'require view';
'require form';
'require dom';
'require fs';
'require ui';

var profile = null;

function serialize(data) {
	if (!L.isObject(profile.unit))
		profile.unit = {};

	profile.redirector = data.local.redirector;
	profile.unit.location = data.local.location;

	return JSON.stringify(profile, null, '\t');
}

function showNotification(text, style, timeout) {
	var node = ui.addNotification(null, E('p', text), style);

	if (timeout) {
		var btn = node.querySelector('input, button');
		btn.parentNode.removeChild(btn);
		window.setTimeout(function() { node.parentNode.removeChild(node) }, timeout);
	}

	return node;
}

function showProgress(text, ongoing) {
	var dlg = ui.showModal(null, [
		E('p', { 'class': ongoing ? 'spinning' : null }, [ text ])
	]);

	dlg.removeChild(dlg.firstElementChild);

	if (!ongoing)
		window.setTimeout(ui.hideModal, 750);
}

return view.extend({
	load: function() {
		return L.resolveDefault(fs.read('/etc/ucentral/profile.json'), '').then(function(data) {
			try { profile = JSON.parse(data); }
			catch(e) { profile = {}; };
		});
	},

	render: function() {
		var m, s, o, data = { local: {
			redirector: profile.redirector,
			location: L.isObject(profile.unit) ? profile.unit.location : ''
		} };

		m = new form.JSONMap(data);
		m.readonly = !L.hasViewPermission();

		s = m.section(form.NamedSection, 'local', 'local', _('Local settings'),
			_('The settings on this page specify how the local uCentral client connects to the controller server.'));

		s.option(form.Value, 'redirector', _('Redirector URL'));
		s.option(form.Value, 'location', _('Unit location'));

		o = s.option(form.Button, '_certs', _('Certificates'));
		o.inputtitle = _('Upload certificate bundle…');
		o.onclick = function(ev) {
			return ui.uploadFile('/tmp/certs.tar').then(function(res) {
				showProgress(_('Verifying certificates…'), true);

				return fs.exec('/sbin/certupdate').then(function(res) {
					if (res.code)
						showNotification(_('Certificate validation failed: %s').format(res.stderr || res.stdout), 'error');
					else
						showNotification(_('Certificates updated.'), null, 3000);
				}).catch(function(err) {
					showNotification(_('Unable to update certificates: %s').format(err), 'error');
				});
			}).finally(ui.hideModal);
		};

		return m.render();
	},

	handleSave: function(ev) {
		var m = dom.findClassInstance(document.querySelector('.cbi-map'));

		return m.save().then(function() {
			return fs.write('/etc/ucentral/profile.json', serialize(m.data.data));
		}).then(function() {
			return fs.exec('/sbin/profileupdate');
		}).then(function() {
			showNotification(_('Connection settings have been saved'), null, 3000);
		}).catch(function(err) {
			showNotification(_('Unable to save connection settings: %s').format(err), 'error');
		});
	},

	handleSaveApply: null,
	handleReset: null
});

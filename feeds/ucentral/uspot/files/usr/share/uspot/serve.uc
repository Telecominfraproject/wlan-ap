Status: 200 OK
Content-Type: text/html

{% let fs = require('fs'); %}
{{ fs.readfile('/tmp/ucentral/www-uspot/' + location) }}

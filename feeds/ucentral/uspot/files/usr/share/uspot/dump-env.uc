Status: 200 OK
Content-Type: text/html

<h1>Headers</h1>

{% for (let k, v in env.headers): %}
<strong>{{ replace(k, /(^|-)(.)/g, (m0, d, c) => d + uc(c)) }}</strong>: {{ v }}<br>
{% endfor %}

<h1>Environment</h1>

{% for (let k, v in env): if (type(v) == 'string'): %}
<code>{{ k }}={{ v }}</code><br>
{% endif; endfor %}

{% if (env.CONTENT_LENGTH > 0): %}
<h1>Body Contents</h1>

{% for (let chunk = uhttpd.recv(64); chunk != null; chunk = uhttpd.recv(64)): %}
<code>{{ replace(chunk, /[^[:graph:]]/g, '.') }}</code><br>
{% endfor %}
{% endif %}

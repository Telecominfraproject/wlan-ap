Status: 200 OK
Content-Type: text/html

{{ header }}
{% if (error): %}
<h1> {{ PO('accept_terms_error', 'Please accept the terms of use.') }} </h1>
{% endif %}
<h5 class="card-title">{{ PO('welcome', 'Welcome!') }}</h5>
<p class="card-text">{{ PO('accept_terms_header', 'To access the Internet you must Accept the Terms of Service.') }}</p>
<hr>
<form action="/hotspot" method="post">
<input type="hidden" name="action" value="click">
<input type="checkbox" name="accept_terms" value="clicked">{{ PO('accept_terms_checkbox', 'I accept the terms of use') }}
<input type="submit" value="{{ PO('accept_terms_button', 'Accept Terms of Service') }}" class="btn btn-primary btn-block">
{% if (query_string?.redir): %}
<input type="hidden" name="redir" value="{{ query_string.redir }}">
{% endif %}
</form>
{{ footer }}

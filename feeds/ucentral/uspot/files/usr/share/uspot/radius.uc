Status: 200 OK
Content-Type: text/html

{{ header }}
{% if (error): %}
<h1> Invalid credentials </h1>
{% endif %}

<form action="/hotspot" method="POST">
<label for="fname">Username:</label>
<input type="text" name="username"><br>
<label for="fname">Password:</label>
<input type="password" name="password">
<input type="hidden" name="action" value="radius">
<input type="submit" value="Login" class="btn btn-primary btn-block">
</form>

{{ footer }}

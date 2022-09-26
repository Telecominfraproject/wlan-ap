Status: 200 OK
Content-Type: text/html

{{ header }}
{% if (error): %}
<h2> Invalid credentials </h2>
{% endif %}

<form action="/hotspot" method="POST">
	<table>
		<tr><td><label for="fname">Username:</label></td>
			<td><input type="text" name="username"></td>
		</tr>
		<tr><td><label for="fname">Password:</label></td>
			<td><input type="password" name="password"></td>
		</tr>
	</table>
<input type="hidden" name="action" value="radius">
<input type="submit" value="Login" class="btn btn-primary btn-block">
</form>

{{ footer }}

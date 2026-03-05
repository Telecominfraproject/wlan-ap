# Jira Bug Report Template

Use this template when creating bug reports in Jira with proper wiki markup syntax.

## Template

```
h2. Bug Description

[Provide a clear, concise description of the bug]

h3. Environment
* *Browser:* Chrome 120.0
* *OS:* Windows 11
* *Version:* 2.1.0
* *Environment:* Production

h3. Steps to Reproduce
# Navigate to [specific page/feature]
# Perform [specific action]
# Observe [unexpected behavior]

h3. Expected Behavior
[Describe what should happen]

h3. Actual Behavior
[Describe what actually happens]

{panel:title=Error Message|bgColor=#FFEBE9}
{code:java}
[Paste error message or stack trace here]
{code}
{panel}

h3. Additional Context
* Frequency: [Always/Sometimes/Rare]
* User Impact: [Critical/High/Medium/Low]
* Workaround Available: [Yes/No]

h3. Screenshots
[^screenshot1.png]
[^screenshot2.png]

h3. Related Issues
* Blocks [PROJ-XXX]
* Related to [PROJ-YYY]

h3. Technical Notes
{code:javascript}
// Code snippet showing the issue
function problematicCode() {
    // Details here
}
{code}

---
*Reported by:* [~username]
*Date:* YYYY-MM-DD
```

## Example - Filled Template

```
h2. Bug Description

Login button becomes unresponsive after failed authentication attempt, requiring page refresh to retry.

h3. Environment
* *Browser:* Chrome 120.0.6099.109
* *OS:* Windows 11
* *Version:* 2.3.1
* *Environment:* Production

h3. Steps to Reproduce
# Navigate to {{/login}} page
# Enter invalid credentials
# Click *Login* button
# Observe error message
# Try to enter correct credentials
# Click *Login* button again
# Button remains disabled

h3. Expected Behavior
After a failed login attempt, the login button should become active again, allowing users to retry with different credentials.

h3. Actual Behavior
The login button remains in a disabled state after the first failed attempt. Users must refresh the page to attempt login again.

{panel:title=Error in Browser Console|bgColor=#FFEBE9}
{code:javascript}
TypeError: Cannot read property 'reset' of null
    at LoginForm.handleSubmit (login.js:45)
    at onClick (login.js:23)
{code}
{panel}

h3. Additional Context
* Frequency: Always (100% reproduction rate)
* User Impact: High (blocks login functionality)
* Workaround Available: Yes (page refresh)
* Affects ~1000 daily users based on error logs

h3. Screenshots
[^login-disabled-state.png] - Login button stuck in disabled state
[^console-error.png] - Browser console showing error

h3. Related Issues
* Blocks [PROJ-234] - User authentication improvements
* Related to [PROJ-189] - Form validation refactoring

h3. Technical Notes
{code:javascript}
// Problem in LoginForm component
handleSubmit(event) {
    event.preventDefault();
    this.setState({ isSubmitting: true });

    // Error occurs here if form ref is null
    this.formRef.reset();  // BUG: formRef can be null

    // Rest of submission logic
}
{code}

Suggested fix: Add null check before calling {{reset()}} method.

---
*Reported by:* [~john.smith]
*Date:* 2025-11-06
```

## Usage with jira-communication Skill

```bash
# Create the bug report using the script
uv run scripts/workflow/jira-create.py issue PROJ \
  "Login button unresponsive after failed authentication" \
  --type Bug \
  --priority High \
  --labels frontend,authentication,ux \
  --description-file bug-description.txt

# Or with inline description (short version)
uv run scripts/workflow/jira-create.py issue PROJ \
  "Login button unresponsive after failed authentication" \
  --type Bug \
  --priority High
```

## Checklist Before Submitting

- [ ] h2. heading for main Description section
- [ ] h3. headings for subsections
- [ ] Numbered list (#) for Steps to Reproduce
- [ ] Bulleted list (*) for Environment details
- [ ] {panel} for error messages with appropriate bgColor
- [ ] {code:language} for code snippets with correct language
- [ ] [^filename] format for attachment references
- [ ] [PROJ-XXX] format for issue links
- [ ] [~username] format for user mentions
- [ ] *bold* for emphasis on key terms
- [ ] {{monospace}} for UI elements and paths

# Jira Feature Request Template

Use this template when creating feature requests in Jira with proper wiki markup syntax.

## Template

```
h2. Feature Overview

[Provide a brief summary of the feature]

h3. Business Value
* *User Impact:* [Describe who benefits and how]
* *Business Goal:* [Align with strategic objectives]
* *Priority Justification:* [Why this should be prioritized]

h3. User Stories

h4. As a [user type]
* I want to [action]
* So that [benefit]

h4. As a [another user type]
* I want to [action]
* So that [benefit]

h3. Acceptance Criteria
# [Specific, testable criterion]
# [Specific, testable criterion]
# [Specific, testable criterion]

h3. Functional Requirements

h4. Must Have
* Requirement 1
* Requirement 2

h4. Should Have
* Requirement 3
* Requirement 4

h4. Could Have
* Requirement 5
* Requirement 6

h3. Non-Functional Requirements
* *Performance:* [Response time requirements]
* *Security:* [Security considerations]
* *Scalability:* [Scale requirements]
* *Accessibility:* [WCAG compliance level]

h3. Technical Considerations
{code:language}
// Pseudocode or technical notes
{code}

h3. UI/UX Mockups
[^wireframe-01.png] - Main interface mockup
[^user-flow.png] - User journey diagram

h3. Dependencies
* Requires [PROJ-XXX] to be completed first
* Impacts [PROJ-YYY] - needs coordination

h3. Open Questions
? Question 1 - needs clarification
? Question 2 - requires decision

h3. Success Metrics
||Metric||Target||Measurement Method||
|User Adoption|80% of active users|Analytics tracking|
|Performance|< 200ms response|Performance monitoring|
|Satisfaction|4.5/5 rating|User surveys|

---
*Requested by:* [~username]
*Stakeholders:* [~pm], [~designer], [~engineer]
```

## Example - Filled Template

```
h2. Feature Overview

Implement bulk export functionality allowing users to export multiple projects to various formats (CSV, JSON, Excel) with customizable field selection.

h3. Business Value
* *User Impact:* 500+ power users currently export data manually one project at a time (30+ clicks per export)
* *Business Goal:* Reduce data export time by 90%, improving productivity and user satisfaction
* *Priority Justification:* #1 requested feature in Q4 2024 user survey (78% of respondents), competitive gap vs competitors

h3. User Stories

h4. As a Project Manager
* I want to export multiple projects at once with selected fields
* So that I can create consolidated reports without manual data entry

h4. As a Data Analyst
* I want to export historical project data in machine-readable formats
* So that I can perform advanced analytics in external tools

h4. As a Team Lead
* I want to schedule automated exports to run daily
* So that stakeholders receive up-to-date reports without manual intervention

h3. Acceptance Criteria
# User can select 1-100 projects for bulk export from project list
# Export supports CSV, JSON, and Excel formats
# User can customize which fields to include in export
# Export progress is shown with cancel option
# Completed exports are downloadable from exports history page
# Export file size limit is 50MB with pagination for larger datasets
# Exports are available for 30 days before auto-deletion

h3. Functional Requirements

h4. Must Have
* Multi-select checkbox interface for project selection
* Format selector (CSV, JSON, Excel)
* Field customization with drag-and-drop ordering
* Progress indicator during export generation
* Download link with expiration notice
* Export history page showing last 10 exports

h4. Should Have
* Search and filter for project selection
* Save field configurations as templates
* Email notification when export completes
* Preview sample data before full export

h4. Could Have
* Schedule recurring exports
* Share export links with team members
* Export directly to cloud storage (Google Drive, Dropbox)
* Advanced filtering within export data

h3. Non-Functional Requirements
* *Performance:* Export generation completes within 30 seconds for 50 projects
* *Security:* Exports encrypted at rest, only accessible to authorized users with audit trail
* *Scalability:* Support 1000 concurrent export requests
* *Accessibility:* WCAG 2.1 AA compliance for export interface
* *Browser Support:* Chrome 90+, Firefox 88+, Safari 14+, Edge 90+

h3. Technical Considerations
{code:python}
# Proposed export architecture
class BulkExporter:
    def export_projects(self, project_ids, format, fields):
        # Use async task queue for processing
        task = ExportTask.create(
            projects=project_ids,
            format=format,
            fields=fields,
            user=current_user
        )

        # Process in background
        celery.send_task('exports.process_bulk', args=[task.id])

        return task.id

    def get_export_status(self, task_id):
        # Return progress percentage and download link when complete
        return ExportTask.get(task_id).status
{code}

*Database Impact:* New {{exports}} table for tracking, ~1GB storage for 30-day retention
*API Endpoints:*
* {{POST /api/exports}} - Initiate export
* {{GET /api/exports/:id}} - Check status
* {{GET /api/exports/:id/download}} - Download file

h3. UI/UX Mockups
[^bulk-export-interface.png] - Main export dialog with project selection
[^field-customization.png] - Field selector with drag-and-drop ordering
[^export-progress.png] - Progress indicator during generation
[^export-history.png] - Export history page design

h3. Dependencies
* Requires [PROJ-456] - Background task queue infrastructure
* Impacts [PROJ-789] - Storage quota system (needs capacity planning)
* Coordinates with [PROJ-321] - API rate limiting (exports count as API calls)

h3. Open Questions
? Should exports include archived projects or only active ones?
? What permission level is required to export data? (View vs Export permission)
? Should we support incremental exports (only new/changed data)?
? How to handle very large exports exceeding 50MB limit?

h3. Success Metrics
||Metric||Target||Measurement Method||
|User Adoption|60% of power users within 3 months|Analytics event tracking|
|Time Savings|90% reduction in export time|User session timing comparison|
|User Satisfaction|4.5/5 feature rating|In-app feedback survey|
|Export Volume|5000+ exports per month|Export usage dashboard|
|Error Rate|< 1% failed exports|Error monitoring logs|

{panel:title=Launch Plan|bgColor=#DEEBFF}
h4. Phase 1 - Beta (Week 1-2)
* Release to 50 beta users
* Gather feedback and fix critical bugs

h4. Phase 2 - Limited Release (Week 3-4)
* Release to 20% of user base
* Monitor performance and error rates

h4. Phase 3 - Full Release (Week 5+)
* Release to all users
* Announce via email and in-app notifications
{panel}

---
*Requested by:* [~sarah.johnson]
*Stakeholders:* [~product.manager], [~ux.designer], [~backend.lead], [~frontend.lead]
*Estimated Effort:* 3 sprints (6 weeks)
*Target Release:* Q1 2025
```

## Usage with jira-communication Skill

```bash
# Create the feature request using the script
uv run scripts/workflow/jira-create.py issue PROJ \
  "Bulk export functionality for multiple projects" \
  --type Story \
  --priority High \
  --labels feature-request,export,productivity \
  --description-file feature-description.txt

# Or with inline description (short version)
uv run scripts/workflow/jira-create.py issue PROJ \
  "Bulk export functionality for multiple projects" \
  --type Story \
  --priority High
```

## Checklist Before Submitting

- [ ] h2. heading for Feature Overview
- [ ] h3. headings for all major sections
- [ ] h4. headings for User Stories and subsections
- [ ] Bulleted lists (*) for requirements and criteria
- [ ] Numbered lists (#) for Acceptance Criteria
- [ ] Tables (||header|| syntax) for Success Metrics
- [ ] {code:language} blocks for technical details
- [ ] {panel} for important launch/timeline information
- [ ] [^filename] for mockup/wireframe references
- [ ] [PROJ-XXX] format for dependency links
- [ ] [~username] format for stakeholder mentions
- [ ] *bold* for emphasis on metrics and targets
- [ ] {{monospace}} for technical terms and paths
- [ ] ? prefix for Open Questions

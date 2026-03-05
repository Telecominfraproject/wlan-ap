# JQL Quick Reference

Common JQL patterns for `jira-search.py query "<JQL>"`.

## Operators

### Comparison
| Operator | Example | Notes |
|----------|---------|-------|
| `=` | `status = "In Progress"` | Exact match |
| `!=` | `status != Done` | Not equal |
| `>` | `votes > 4` | Greater than (dates, versions, numbers) |
| `>=` | `duedate >= "2024-01-01"` | Greater than or equal |
| `<` | `priority < High` | Less than |
| `<=` | `updated <= -4w` | Less than or equal |

### Text Search
| Operator | Example | Notes |
|----------|---------|-------|
| `~` | `summary ~ "login"` | Contains (fuzzy match) |
| `!~` | `summary !~ "test"` | Does not contain |

### List & Null
| Operator | Example | Notes |
|----------|---------|-------|
| `IN` | `status IN (Open, "In Progress")` | Multiple values |
| `NOT IN` | `priority NOT IN (Low, Lowest)` | Exclude values |
| `IS EMPTY` | `assignee IS EMPTY` | Field has no value |
| `IS NOT EMPTY` | `fixVersion IS NOT EMPTY` | Field has value |

### Historical
| Operator | Example | Notes |
|----------|---------|-------|
| `WAS` | `assignee WAS "john"` | Previous value |
| `WAS IN` | `status WAS IN (Open, "To Do")` | Previous in list |
| `WAS NOT` | `status WAS NOT Done` | Was never value |
| `CHANGED` | `status CHANGED` | Value was modified |

`CHANGED` supports predicates: `FROM`, `TO`, `BY`, `DURING`, `BEFORE`, `AFTER`, `ON`
```jql
status CHANGED FROM "Open" TO "In Progress" BY currentUser() AFTER -7d
```

## Functions

### User Functions
| Function | Description |
|----------|-------------|
| `currentUser()` | Logged-in user |
| `membersOf("group")` | Members of a group |

### Date Functions
| Function | Description |
|----------|-------------|
| `now()` | Current timestamp |
| `startOfDay()` | Midnight today |
| `startOfWeek()` | Start of current week |
| `startOfMonth()` | First of current month |
| `startOfYear()` | January 1st current year |
| `endOfDay()` | End of today (23:59:59) |
| `endOfWeek()` | End of current week |
| `endOfMonth()` | Last day of current month |
| `endOfYear()` | December 31st current year |

Date offsets: `startOfDay(-1)` = yesterday, `startOfWeek(1)` = next week

### Relative Dates
| Format | Example | Description |
|--------|---------|-------------|
| `-Nd` | `-7d` | N days ago |
| `-Nw` | `-2w` | N weeks ago |
| `-Nm` | `-1m` | N months ago |
| `"YYYY-MM-DD"` | `"2024-01-15"` | Specific date |

### Sprint Functions
| Function | Description |
|----------|-------------|
| `openSprints()` | Active sprints |
| `closedSprints()` | Completed sprints |
| `futureSprints()` | Planned sprints |

### Version Functions
| Function | Description |
|----------|-------------|
| `releasedVersions()` | Released versions |
| `unreleasedVersions()` | Unreleased versions |
| `latestReleasedVersion()` | Most recent release |

## Common Queries

### By Assignment
```jql
assignee = currentUser()
assignee = "john.doe"
assignee IS EMPTY
assignee IN membersOf("developers")
```

### By Status
```jql
status = "In Progress"
status IN (Open, "To Do", "In Progress")
status != Done
status WAS "Open"
status CHANGED FROM "Open" TO "In Progress"
```

### By Date
```jql
created >= -7d
updated >= startOfWeek()
due <= endOfMonth()
resolved >= "2024-01-01"
created >= startOfMonth(-1) AND created < startOfMonth()
```

### By Sprint
```jql
sprint IN openSprints()
sprint IN closedSprints()
sprint = "Sprint 42"
sprint IS EMPTY
```

### By Text
```jql
text ~ "error message"
summary ~ "login bug"
description ~ "timeout"
comment ~ "workaround"
```

## Combining Conditions

```jql
project = PROJ AND status = Open

priority = High OR priority = Highest

project = PROJ AND (status = Open OR status = "In Progress") AND assignee = currentUser()

NOT status = Done

project = PROJ ORDER BY priority DESC, created ASC
```

## Keywords

| Keyword | Usage |
|---------|-------|
| `AND` | Both conditions must match |
| `OR` | Either condition matches |
| `NOT` | Negate a condition |
| `EMPTY` | Alias for null/no value |
| `NULL` | Alias for empty/no value |
| `ORDER BY` | Sort results (`ASC` or `DESC`) |

## Quoting Rules

**Must quote values containing:**
- Spaces: `project = "My Project"`
- Special characters: `summary ~ "error@host"`
- Reserved words used as values: `labels = "AND"`

**No quotes needed for:**
- Single words: `status = Open`
- Project keys: `project = PROJ`
- Function calls: `assignee = currentUser()`

## Sources

- [JQL Operators](https://support.atlassian.com/jira-software-cloud/docs/jql-operators/)
- [JQL Functions](https://support.atlassian.com/jira-software-cloud/docs/jql-functions/)
- [JQL Keywords](https://support.atlassian.com/jira-software-cloud/docs/jql-keywords/)

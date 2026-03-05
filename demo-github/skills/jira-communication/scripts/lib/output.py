"""Output formatting utilities for Jira CLI scripts."""

import json
import sys
from typing import Any, Optional

# === INLINE_START: output ===

def format_json(data: Any, indent: int = 2) -> str:
    """Format data as JSON string.

    Args:
        data: Data to format
        indent: Indentation level

    Returns:
        JSON formatted string
    """
    return json.dumps(data, indent=indent, default=str)


def format_table(data: list, columns: Optional[list] = None) -> str:
    """Format list of dicts as ASCII table.

    Args:
        data: List of dictionaries
        columns: Optional list of column names to include

    Returns:
        ASCII table string
    """
    if not data:
        return "(no data)"

    # Determine columns
    if columns is None:
        columns = list(data[0].keys()) if isinstance(data[0], dict) else ['value']

    # Calculate column widths
    widths = {col: len(col) for col in columns}
    for row in data:
        if isinstance(row, dict):
            for col in columns:
                val = str(row.get(col, ''))
                widths[col] = max(widths[col], len(val))

    # Build table
    lines = []

    # Header
    header = " | ".join(col.ljust(widths[col]) for col in columns)
    lines.append(header)
    lines.append("-+-".join("-" * widths[col] for col in columns))

    # Rows
    for row in data:
        if isinstance(row, dict):
            line = " | ".join(str(row.get(col, '')).ljust(widths[col]) for col in columns)
        else:
            line = str(row)
        lines.append(line)

    return "\n".join(lines)


def format_output(data: Any, as_json: bool = False, quiet: bool = False) -> None:
    """Format and print output based on flags.

    Args:
        data: Data to output
        as_json: Output as JSON if True
        quiet: Minimal output if True
    """
    if quiet:
        if isinstance(data, dict) and 'key' in data:
            print(data['key'])
        elif isinstance(data, list) and data and isinstance(data[0], dict) and 'key' in data[0]:
            for item in data:
                print(item.get('key', ''))
        else:
            print(data if isinstance(data, str) else format_json(data))
        return

    if as_json:
        print(format_json(data))
        return

    # Human-readable format
    if isinstance(data, dict):
        _print_dict(data)
    elif isinstance(data, list):
        if data and isinstance(data[0], dict):
            print(format_table(data))
        else:
            for item in data:
                print(item)
    else:
        print(data)


def _print_dict(data: dict, indent: int = 0) -> None:
    """Pretty print a dictionary."""
    prefix = "  " * indent
    for key, value in data.items():
        if isinstance(value, dict):
            print(f"{prefix}{key}:")
            _print_dict(value, indent + 1)
        elif isinstance(value, list):
            print(f"{prefix}{key}: {', '.join(str(v) for v in value[:5])}")
            if len(value) > 5:
                print(f"{prefix}  ... and {len(value) - 5} more")
        else:
            print(f"{prefix}{key}: {value}")


def error(message: str, suggestion: Optional[str] = None) -> None:
    """Print error message with optional suggestion.

    Args:
        message: Error message
        suggestion: Optional suggestion for resolution
    """
    print(f"✗ {message}", file=sys.stderr)
    if suggestion:
        print(f"\n  {suggestion}", file=sys.stderr)


def success(message: str) -> None:
    """Print success message."""
    print(f"✓ {message}")


def warning(message: str) -> None:
    """Print warning message."""
    print(f"⚠ {message}", file=sys.stderr)

# === INLINE_END: output ===

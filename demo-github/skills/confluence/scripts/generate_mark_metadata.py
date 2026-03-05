#!/usr/bin/env python3
"""
Generate mark-compatible metadata headers for Markdown files.

This script adds or updates metadata headers in Markdown files for use
with the mark CLI tool (https://github.com/kovetskiy/mark).

Usage:
    python generate_mark_metadata.py file.md --space DEV --title "Page Title"
    python generate_mark_metadata.py file.md --space DEV --parent "Parent Page" --labels api,docs
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional


class MarkMetadata:
    """Represents mark metadata for a Confluence page."""

    def __init__(
        self,
        space: str,
        title: Optional[str] = None,
        parent: Optional[str] = None,
        parents: Optional[List[str]] = None,
        labels: Optional[List[str]] = None,
        attachments: Optional[List[str]] = None
    ):
        self.space = space
        self.title = title
        self.parent = parent
        self.parents = parents or []
        self.labels = labels or []
        self.attachments = attachments or []

    def to_header(self) -> str:
        """
        Generate mark metadata header.

        Returns:
            Formatted metadata header as string
        """
        lines = []

        # Space (required)
        lines.append(f"<!-- Space: {self.space} -->")

        # Title (optional, can be inferred from first heading)
        if self.title:
            lines.append(f"<!-- Title: {self.title} -->")

        # Parent page(s)
        if self.parent:
            lines.append(f"<!-- Parent: {self.parent} -->")

        for parent in self.parents:
            lines.append(f"<!-- Parent: {parent} -->")

        # Labels
        for label in self.labels:
            lines.append(f"<!-- Label: {label} -->")

        # Attachments
        for attachment in self.attachments:
            lines.append(f"<!-- Attachment: {attachment} -->")

        return '\n'.join(lines)


def extract_title_from_markdown(content: str) -> Optional[str]:
    """
    Extract title from first H1 heading in Markdown.

    Args:
        content: Markdown content

    Returns:
        Title if found, None otherwise
    """
    lines = content.split('\n')

    for line in lines:
        # ATX-style heading
        match = re.match(r'^#\s+(.+)$', line)
        if match:
            title = match.group(1).strip()
            # Remove trailing #
            title = re.sub(r'\s*#+\s*$', '', title)
            # Remove anchor
            title = re.sub(r'\s*\{#[^}]+\}\s*$', '', title)
            return title

        # Setext-style heading
        if line.strip() and not line.startswith('#'):
            next_idx = lines.index(line) + 1
            if next_idx < len(lines):
                next_line = lines[next_idx]
                if re.match(r'^=+\s*$', next_line):
                    return line.strip()

    return None


def extract_existing_metadata(content: str) -> dict:
    """
    Extract existing mark metadata from Markdown content.

    Args:
        content: Markdown content

    Returns:
        Dictionary of existing metadata
    """
    metadata = {
        'space': None,
        'title': None,
        'parents': [],
        'labels': [],
        'attachments': []
    }

    # Match metadata comments
    space_match = re.search(r'<!-- Space:\s*(.+?)\s*-->', content)
    if space_match:
        metadata['space'] = space_match.group(1)

    title_match = re.search(r'<!-- Title:\s*(.+?)\s*-->', content)
    if title_match:
        metadata['title'] = title_match.group(1)

    # Find all parents
    for match in re.finditer(r'<!-- Parent:\s*(.+?)\s*-->', content):
        metadata['parents'].append(match.group(1))

    # Find all labels
    for match in re.finditer(r'<!-- Label:\s*(.+?)\s*-->', content):
        metadata['labels'].append(match.group(1))

    # Find all attachments
    for match in re.finditer(r'<!-- Attachment:\s*(.+?)\s*-->', content):
        metadata['attachments'].append(match.group(1))

    return metadata


def remove_existing_metadata(content: str) -> str:
    """
    Remove existing mark metadata from content.

    Args:
        content: Markdown content with metadata

    Returns:
        Content without metadata
    """
    # Remove all mark metadata comments
    content = re.sub(r'<!-- Space:.*?-->\n?', '', content)
    content = re.sub(r'<!-- Title:.*?-->\n?', '', content)
    content = re.sub(r'<!-- Parent:.*?-->\n?', '', content)
    content = re.sub(r'<!-- Label:.*?-->\n?', '', content)
    content = re.sub(r'<!-- Attachment:.*?-->\n?', '', content)

    # Remove leading blank lines
    content = re.sub(r'^\n+', '', content)

    return content


def add_metadata_to_file(
    file_path: Path,
    metadata: MarkMetadata,
    infer_title: bool = True,
    preserve_existing: bool = False
) -> None:
    """
    Add or update mark metadata in a Markdown file.

    Args:
        file_path: Path to Markdown file
        metadata: Metadata to add
        infer_title: Infer title from first H1 heading if not provided
        preserve_existing: Preserve existing metadata values
    """
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract existing metadata if preserving
    if preserve_existing:
        existing = extract_existing_metadata(content)

        # Merge with new metadata (new values take precedence)
        if not metadata.space and existing['space']:
            metadata.space = existing['space']
        if not metadata.title and existing['title']:
            metadata.title = existing['title']
        if not metadata.parents and existing['parents']:
            metadata.parents = existing['parents']
        if not metadata.labels and existing['labels']:
            metadata.labels = existing['labels']
        if not metadata.attachments and existing['attachments']:
            metadata.attachments = existing['attachments']

    # Infer title if requested and not provided
    if infer_title and not metadata.title:
        inferred_title = extract_title_from_markdown(content)
        if inferred_title:
            metadata.title = inferred_title

    # Remove any existing metadata
    clean_content = remove_existing_metadata(content)

    # Generate new metadata header
    header = metadata.to_header()

    # Combine header and content
    new_content = f"{header}\n\n{clean_content}"

    # Write back to file
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(new_content)


def main():
    """Main entry point for CLI usage."""
    parser = argparse.ArgumentParser(
        description='Generate mark-compatible metadata for Markdown files'
    )

    parser.add_argument(
        'file',
        type=Path,
        help='Markdown file to add metadata to'
    )
    parser.add_argument(
        '--space', '-s',
        required=True,
        help='Confluence space key (e.g., DEV, PROJ)'
    )
    parser.add_argument(
        '--title', '-t',
        help='Page title (inferred from H1 if not provided)'
    )
    parser.add_argument(
        '--parent', '-p',
        help='Parent page title'
    )
    parser.add_argument(
        '--parents',
        help='Multiple parent pages (comma-separated, innermost first)'
    )
    parser.add_argument(
        '--labels', '-l',
        help='Page labels (comma-separated)'
    )
    parser.add_argument(
        '--attachments', '-a',
        help='Attachment file paths (comma-separated)'
    )
    parser.add_argument(
        '--no-infer-title',
        action='store_true',
        help='Do not infer title from first H1 heading'
    )
    parser.add_argument(
        '--preserve-existing',
        action='store_true',
        help='Preserve existing metadata values'
    )

    args = parser.parse_args()

    if not args.file.exists():
        print(f"Error: File '{args.file}' not found", file=sys.stderr)
        sys.exit(1)

    # Parse comma-separated lists
    parents = args.parents.split(',') if args.parents else []
    labels = args.labels.split(',') if args.labels else []
    attachments = args.attachments.split(',') if args.attachments else []

    # Clean up whitespace
    parents = [p.strip() for p in parents]
    labels = [l.strip() for l in labels]
    attachments = [a.strip() for a in attachments]

    # Create metadata
    metadata = MarkMetadata(
        space=args.space,
        title=args.title,
        parent=args.parent,
        parents=parents,
        labels=labels,
        attachments=attachments
    )

    try:
        add_metadata_to_file(
            args.file,
            metadata,
            infer_title=not args.no_infer_title,
            preserve_existing=args.preserve_existing
        )
        print(f"Added metadata to {args.file}")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()

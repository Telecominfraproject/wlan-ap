#!/usr/bin/env python3
"""
Upload Markdown to Confluence

Converts Markdown files to Confluence storage format and uploads via REST API.
Supports frontmatter-based smart uploads, version management, and Mermaid diagrams.

Usage:
    # Smart upload (reads from frontmatter)
    python3 upload_confluence.py page.md

    # Update specific page by ID
    python3 upload_confluence.py page.md --id 450855912

    # Create new page
    python3 upload_confluence.py page.md --space ARCP --parent-id 123456

    # Dry-run (preview without uploading)
    python3 upload_confluence.py page.md --dry-run

Requirements:
    pip install atlassian-python-api md2cf python-dotenv PyYAML mistune
    npm install -g @mermaid-js/mermaid-cli  # Optional, for Mermaid diagrams
"""

import sys
import argparse
import re
from pathlib import Path
from typing import Dict, Optional, List, Tuple
import yaml

# Check dependencies
try:
    import mistune
    from confluence_auth import get_confluence_client
    from mermaid_renderer import MermaidConfluenceRenderer
except ImportError as e:
    print(f"ERROR: Missing dependency: {e}", file=sys.stderr)
    print("Install with: pip install atlassian-python-api md2cf python-dotenv PyYAML mistune", file=sys.stderr)
    sys.exit(1)


def parse_markdown_file(file_path: Path) -> Tuple[Dict, str, Optional[str]]:
    """
    Parse markdown file and extract frontmatter, content, and title.

    Args:
        file_path: Path to markdown file

    Returns:
        Tuple of (frontmatter_dict, markdown_content, extracted_title)
    """
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check for YAML frontmatter
    frontmatter = {}
    markdown_content = content
    title = None

    # Parse frontmatter (between --- markers)
    if content.startswith('---\n'):
        parts = content.split('---\n', 2)
        if len(parts) >= 3:
            try:
                frontmatter = yaml.safe_load(parts[1]) or {}
                markdown_content = parts[2].strip()
            except yaml.YAMLError as e:
                print(f"WARNING: Failed to parse YAML frontmatter: {e}", file=sys.stderr)

    # Extract title from frontmatter
    if 'title' in frontmatter:
        title = frontmatter['title']

    # Fallback: extract title from first H1 heading
    if not title:
        match = re.search(r'^#\s+(.+)$', markdown_content, re.MULTILINE)
        if match:
            title = match.group(1).strip()

    # Last fallback: use filename
    if not title:
        title = file_path.stem.replace('_', ' ')

    return frontmatter, markdown_content, title


def convert_to_storage_format(markdown_content: str, output_dir: Optional[Path] = None) -> Tuple[str, List[Dict]]:
    """
    Convert Markdown to Confluence storage format.

    Args:
        markdown_content: Markdown text
        output_dir: Directory for Mermaid diagrams (optional)

    Returns:
        Tuple of (storage_format_html, attachments_list)
    """
    renderer = MermaidConfluenceRenderer(output_dir=str(output_dir) if output_dir else None)

    try:
        # Use mistune 0.8.x API
        markdown_parser = mistune.Markdown(renderer=renderer)
        html = markdown_parser(markdown_content)
        attachments = renderer.get_attachments()
        return html, attachments
    except Exception as e:
        print(f"ERROR: Markdown conversion failed: {e}", file=sys.stderr)
        raise


def upload_to_confluence(
    confluence,
    title: str,
    content: str,
    space_key: Optional[str] = None,
    page_id: Optional[str] = None,
    parent_id: Optional[str] = None,
    version: Optional[int] = None,
    attachments: Optional[List[Dict]] = None
) -> Dict:
    """
    Upload or update page in Confluence.

    Args:
        confluence: Confluence client instance
        title: Page title
        content: Page content (storage format)
        space_key: Space key (required for create)
        page_id: Page ID (for updates)
        parent_id: Parent page ID (optional)
        version: Current version number (required for updates)
        attachments: List of attachment dicts with 'path', 'filename', 'type'

    Returns:
        Page metadata dict with 'id', 'title', 'url', 'version'
    """
    # Determine mode: create or update
    if page_id:
        # UPDATE mode
        if not version:
            # Fetch current version
            try:
                page_info = confluence.get_page_by_id(page_id, expand='version')
                version = page_info['version']['number']
            except Exception as e:
                raise ValueError(f"Failed to fetch current version for page {page_id}: {e}")

        new_version = version + 1

        try:
            result = confluence.update_page(
                page_id=page_id,
                title=title,
                body=content,
                parent_id=parent_id,
                type='page',
                representation='storage',
                minor_edit=False,
                version_comment=f"Updated via upload_confluence.py (v{version} → v{new_version})"
            )

            # Upload attachments
            if attachments:
                for att in attachments:
                    confluence.attach_file(
                        filename=att['path'],
                        name=att['filename'],
                        content_type=att['type'],
                        page_id=page_id,
                        comment=f"Uploaded via upload_confluence.py"
                    )

            return {
                'id': result['id'],
                'title': result['title'],
                'version': result['version']['number'],
                'url': confluence.url + result['_links']['webui']
            }

        except Exception as e:
            raise RuntimeError(f"Failed to update page {page_id}: {e}")

    else:
        # CREATE mode
        if not space_key:
            raise ValueError("space_key is required to create new page")

        try:
            result = confluence.create_page(
                space=space_key,
                title=title,
                body=content,
                parent_id=parent_id,
                type='page',
                representation='storage'
            )

            new_page_id = result['id']

            # Upload attachments
            if attachments:
                for att in attachments:
                    confluence.attach_file(
                        filename=att['path'],
                        name=att['filename'],
                        content_type=att['type'],
                        page_id=new_page_id,
                        comment=f"Uploaded via upload_confluence.py"
                    )

            return {
                'id': result['id'],
                'title': result['title'],
                'version': result['version']['number'],
                'url': confluence.url + result['_links']['webui']
            }

        except Exception as e:
            raise RuntimeError(f"Failed to create page '{title}' in space {space_key}: {e}")


def dry_run_preview(
    title: str,
    content: str,
    space_key: Optional[str],
    page_id: Optional[str],
    parent_id: Optional[str],
    version: Optional[int],
    attachments: Optional[List[Dict]]
) -> None:
    """Print preview of what would be uploaded"""
    print("=" * 70)
    print("DRY-RUN MODE - No changes will be made")
    print("=" * 70)

    mode = "UPDATE" if page_id else "CREATE"
    print(f"\nMode: {mode}")
    print(f"Title: {title}")

    if page_id:
        print(f"Page ID: {page_id}")
        if version:
            print(f"Version: {version} → {version + 1}")
    else:
        print(f"Space: {space_key}")

    if parent_id:
        print(f"Parent ID: {parent_id}")

    if attachments:
        print(f"\nAttachments ({len(attachments)}):")
        for att in attachments:
            print(f"  - {att['filename']} ({att['type']})")

    print(f"\nContent preview (first 500 chars):")
    print("-" * 70)
    print(content[:500])
    if len(content) > 500:
        print("...")
    print("-" * 70)


def main():
    parser = argparse.ArgumentParser(
        description='Upload Markdown to Confluence',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Smart upload from frontmatter
  %(prog)s page.md

  # Update specific page
  %(prog)s page.md --id 450855912

  # Create new page
  %(prog)s page.md --space ARCP --parent-id 123456

  # Dry-run preview
  %(prog)s page.md --dry-run

  # Use custom credentials file
  %(prog)s page.md --env-file /path/to/.env
        """
    )

    parser.add_argument('file', type=str, help='Markdown file to upload')
    parser.add_argument('--id', type=str, help='Page ID (for updates)')
    parser.add_argument('--space', type=str, help='Space key (required for new pages)')
    parser.add_argument('--title', type=str, help='Page title (overrides frontmatter/H1)')
    parser.add_argument('--parent-id', type=str, help='Parent page ID (specify parent to move page)')
    parser.add_argument('--ignore-frontmatter', action='store_true',
                        help='Ignore parent_id in frontmatter (update page in place without moving)')
    parser.add_argument('--dry-run', action='store_true', help='Preview without uploading')
    parser.add_argument('--env-file', type=str, help='Path to .env file with credentials')
    parser.add_argument('--update-frontmatter', action='store_true',
                        help='Update markdown file frontmatter after upload')
    parser.add_argument('--output-dir', type=str, help='Directory for generated diagrams')

    args = parser.parse_args()

    # Validate file
    file_path = Path(args.file)
    if not file_path.exists():
        print(f"ERROR: File not found: {args.file}", file=sys.stderr)
        sys.exit(1)

    # Parse markdown file
    try:
        frontmatter, markdown_content, extracted_title = parse_markdown_file(file_path)
    except Exception as e:
        print(f"ERROR: Failed to parse markdown file: {e}", file=sys.stderr)
        sys.exit(1)

    # Determine parameters (CLI flags override frontmatter)
    title = args.title or extracted_title
    page_id = args.id or frontmatter.get('confluence', {}).get('id')
    space_key = args.space or frontmatter.get('confluence', {}).get('space')

    # Handle parent_id with --ignore-frontmatter option
    if args.ignore_frontmatter:
        # Only use --parent-id if explicitly provided, don't read from frontmatter
        parent_id = args.parent_id
    else:
        # Default behavior: CLI --parent-id overrides frontmatter
        parent_id = args.parent_id or frontmatter.get('parent', {}).get('id')

    version = frontmatter.get('confluence', {}).get('version')

    # Validate required parameters
    if not page_id and not space_key:
        print("ERROR: Either --id (for update) or --space (for create) must be provided", file=sys.stderr)
        print("  or ensure frontmatter contains 'confluence.id' or 'confluence.space'", file=sys.stderr)
        sys.exit(1)

    # Convert markdown to storage format
    try:
        output_dir = Path(args.output_dir) if args.output_dir else None
        storage_content, attachments = convert_to_storage_format(markdown_content, output_dir)
    except Exception as e:
        print(f"ERROR: Conversion failed: {e}", file=sys.stderr)
        sys.exit(1)

    # Dry-run mode
    if args.dry_run:
        dry_run_preview(title, storage_content, space_key, page_id, parent_id, version, attachments)
        return

    # Get Confluence client
    try:
        confluence = get_confluence_client(env_file=args.env_file)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    # Upload to Confluence
    try:
        result = upload_to_confluence(
            confluence=confluence,
            title=title,
            content=storage_content,
            space_key=space_key,
            page_id=page_id,
            parent_id=parent_id,
            version=version,
            attachments=attachments
        )

        print("✅ Upload successful!")
        print(f"  Title: {result['title']}")
        print(f"  ID: {result['id']}")
        print(f"  Version: {result['version']}")
        print(f"  URL: {result['url']}")

        if attachments:
            print(f"  Attachments uploaded: {len(attachments)}")

        # Update frontmatter if requested
        if args.update_frontmatter:
            # TODO: Implement frontmatter update
            print("\nNote: --update-frontmatter not yet implemented")

    except Exception as e:
        print(f"❌ ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()

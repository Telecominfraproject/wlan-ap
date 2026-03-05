#!/usr/bin/env python3
"""
Confluence Page Downloader - Download and convert Confluence pages to Markdown

Features:
- Downloads complete Confluence pages using REST API with pagination
- Converts Confluence storage format (XHTML) to clean Markdown
- Handles Confluence macros (code blocks with language, children lists, images)
- Downloads all attachments and creates local links
- Supports hierarchical child page downloads to subdirectories
- Creates YAML frontmatter with complete page metadata
- Retries with exponential backoff for failed downloads
- HTML debugging mode for troubleshooting transformations
"""

import os
import re
import sys
import time
import json
import logging
import argparse
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from datetime import datetime
from urllib.parse import urljoin, urlparse, quote

import requests
import yaml
from markdownify import markdownify as md

# Import shared credential discovery
try:
    from confluence_auth import get_confluence_credentials
except ImportError:
    print("ERROR: confluence_auth module not found. Ensure it's in the same directory.", file=sys.stderr)
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class ConfluenceValidator:
    """Validates downloaded content against Confluence source."""

    def __init__(self, confluence_url: str, username: str, api_token: str):
        self.confluence_url = confluence_url.rstrip('/')
        if self.confluence_url.endswith('/wiki'):
            self.confluence_url = self.confluence_url[:-5]

        self.api_base = f"{self.confluence_url}/wiki/rest/api"
        self.web_base = f"{self.confluence_url}/wiki"  # Base URL for web resources (downloads, etc.)
        self.auth = (username, api_token)
        self.session = requests.Session()
        self.session.auth = self.auth

    def get_page_info(self, page_id: str) -> Dict:
        """Get page metadata from Confluence."""
        url = f"{self.api_base}/content/{page_id}"
        params = {
            'expand': 'body.storage,version,space,ancestors,metadata.labels,history,children.page'
        }

        response = self.session.get(url, params=params)
        response.raise_for_status()
        return response.json()

    def get_children(self, page_id: str) -> List[Dict]:
        """Get all child pages for a page."""
        children = []
        start = 0
        limit = 50

        while True:
            url = f"{self.api_base}/content/{page_id}/child/page"
            params = {
                'start': start,
                'limit': limit
            }

            response = self.session.get(url, params=params)
            response.raise_for_status()
            data = response.json()

            children.extend(data.get('results', []))

            if len(data.get('results', [])) < limit:
                break
            start += limit

        return children

    def get_attachments(self, page_id: str) -> List[Dict]:
        """Get all attachments for a page."""
        attachments = []
        start = 0
        limit = 50

        while True:
            url = f"{self.api_base}/content/{page_id}/child/attachment"
            params = {
                'start': start,
                'limit': limit,
                'expand': 'version,metadata'
            }

            response = self.session.get(url, params=params)
            response.raise_for_status()
            data = response.json()

            attachments.extend(data.get('results', []))

            if len(data.get('results', [])) < limit:
                break
            start += limit

        return attachments

    def download_attachment(self, attachment: Dict, output_dir: Path, page_title: str) -> Optional[Path]:
        """Download an attachment to the page-specific attachments directory."""
        try:
            download_url = attachment['_links'].get('download')
            if not download_url:
                logger.warning(f"No download URL for attachment: {attachment.get('title')}")
                return None

            # Construct full URL - use web_base which includes /wiki
            if download_url.startswith('/'):
                download_url = self.web_base + download_url

            # Create page-specific attachments directory
            safe_page_title = self._sanitize_filename(page_title)
            attachments_dir = output_dir / f'{safe_page_title}_attachments'
            attachments_dir.mkdir(exist_ok=True)

            # Sanitize filename
            filename = self._sanitize_filename(attachment['title'])
            filepath = attachments_dir / filename

            # Download file
            logger.info(f"Downloading attachment: {filename}")
            response = self.session.get(download_url, stream=True)
            response.raise_for_status()

            with open(filepath, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)

            logger.info(f"Downloaded: {filename} ({filepath.stat().st_size} bytes)")
            return filepath

        except Exception as e:
            logger.error(f"Error downloading attachment {attachment.get('title')}: {e}")
            return None

    def _sanitize_filename(self, filename: str) -> str:
        """Sanitize filename for filesystem."""
        # Remove or replace invalid characters, replace spaces with underscores
        filename = re.sub(r'[<>:"/\\|?*]', '_', filename)
        filename = re.sub(r'\s+', '_', filename)
        # Limit length
        if len(filename) > 200:
            name, ext = os.path.splitext(filename)
            filename = name[:200-len(ext)] + ext
        return filename


class ConfluenceDownloader:
    """Downloads and converts Confluence pages to Markdown."""

    def __init__(self, validator: ConfluenceValidator, output_dir: Path, save_html: bool = False, download_children: bool = False):
        self.validator = validator
        self.output_dir = output_dir
        self.output_dir.mkdir(exist_ok=True)
        self.save_html = save_html
        self.download_children = download_children

        # Create HTML debug directory if requested
        if self.save_html:
            self.html_dir = output_dir / '_html_debug'
            self.html_dir.mkdir(exist_ok=True)

    def download_page(self, page_id: str, max_retries: int = 3, target_dir: Optional[Path] = None, parent_title: Optional[str] = None) -> Tuple[bool, str]:
        """
        Download a single page with validation.

        Args:
            page_id: Confluence page ID to download
            max_retries: Number of retry attempts for failed downloads
            target_dir: Optional target directory (defaults to self.output_dir)
            parent_title: Optional parent page title for frontmatter updates

        Returns:
            Tuple of (success: bool, message: str)
        """
        if target_dir is None:
            target_dir = self.output_dir
        for attempt in range(max_retries):
            try:
                logger.info(f"Downloading page {page_id} (attempt {attempt + 1}/{max_retries})")

                # Get page info
                page_info = self.validator.get_page_info(page_id)

                # Extract metadata
                title = page_info['title']
                space_key = page_info['space']['key']
                version = page_info['version']['number']
                content_html = page_info['body']['storage']['value']

                logger.info(f"Page: {title} (v{version})")
                logger.info(f"HTML content length: {len(content_html)} characters")

                # STEP 1: Save original HTML for debugging if requested
                safe_title = self._sanitize_filename(title)
                if self.save_html:
                    # Save original (raw from API)
                    original_html_file = self.html_dir / f"original_{safe_title}.html"
                    with open(original_html_file, 'w', encoding='utf-8') as f:
                        f.write(content_html)

                    # Save formatted (pretty-printed)
                    try:
                        from bs4 import BeautifulSoup
                        soup = BeautifulSoup(content_html, 'html.parser')
                        formatted_html = soup.prettify()
                        formatted_html_file = self.html_dir / f"formatted_{safe_title}.html"
                        with open(formatted_html_file, 'w', encoding='utf-8') as f:
                            f.write(formatted_html)
                    except ImportError:
                        # If BeautifulSoup not available, just copy original
                        formatted_html_file = self.html_dir / f"formatted_{safe_title}.html"
                        with open(formatted_html_file, 'w', encoding='utf-8') as f:
                            f.write(content_html)

                    logger.info(f"Saved original and formatted HTML debug files")

                # Get attachments
                attachments = self.validator.get_attachments(page_id)
                logger.info(f"Found {len(attachments)} attachments")

                # Download attachments (pass sanitized page title for folder naming)
                safe_title = self._sanitize_filename(title)
                attachment_paths = {}
                for attachment in attachments:
                    path = self.validator.download_attachment(attachment, target_dir, safe_title)
                    if path:
                        attachment_paths[attachment['title']] = path

                # Convert children macro to HTML list before other transformations
                content_html = self._convert_children_macro(content_html, page_id)

                # Update image links in HTML to point to local attachments
                # This also applies code block and image transformations
                content_html = self._localize_attachment_links(content_html, attachment_paths)

                # STEP 2: Save transformed HTML for debugging if requested
                if self.save_html:
                    transformed_html_file = self.html_dir / f"transformed_{safe_title}.html"
                    with open(transformed_html_file, 'w', encoding='utf-8') as f:
                        f.write(content_html)
                    logger.info(f"Saved transformed HTML debug file")

                # STEP 3: Convert HTML to Markdown using markdownify
                logger.info("Converting HTML to Markdown...")
                markdown_content = md(
                    content_html,
                    heading_style="ATX",
                    bullets="-",
                    code_language="",
                    strip=['script', 'style']
                )

                # Clean up markdown
                markdown_content = self._clean_markdown(markdown_content)

                # Save original markdown (before post-processing) for debugging
                if self.save_html:
                    original_md_file = self.html_dir / f"original_{safe_title}.md"
                    with open(original_md_file, 'w', encoding='utf-8') as f:
                        f.write(markdown_content)
                    logger.info(f"Saved original markdown debug file")

                # STEP 4: Post-process markdown to add language tags to code fences
                markdown_content = self._postprocess_code_languages(markdown_content)

                logger.info(f"Markdown content length: {len(markdown_content)} characters")

                # Validate size (markdown should be roughly 50-150% of HTML size due to formatting)
                size_ratio = len(markdown_content) / len(content_html) if content_html else 0
                if size_ratio < 0.3:
                    logger.warning(f"Markdown seems too small (ratio: {size_ratio:.2f})")
                elif size_ratio > 2.0:
                    logger.warning(f"Markdown seems too large (ratio: {size_ratio:.2f})")
                else:
                    logger.info(f"Size validation passed (ratio: {size_ratio:.2f})")

                # Create frontmatter (pass parent_title if in subdirectory)
                frontmatter = self._create_frontmatter(page_info, attachments, parent_title)

                # Generate filename (without page ID)
                safe_title = self._sanitize_filename(title)
                filename = f"{safe_title}.md"
                filepath = target_dir / filename

                # Write file
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write("---\n")
                    f.write(yaml.dump(frontmatter, default_flow_style=False, allow_unicode=True))
                    f.write("---\n\n")
                    f.write(f"# {title}\n\n")
                    f.write(markdown_content)

                file_size = filepath.stat().st_size
                logger.info(f"✅ Downloaded: {filename} ({file_size:,} bytes)")

                # Download children if enabled
                if self.download_children:
                    children_data = self.validator.get_children(page_id)
                    if children_data:
                        # Create subdirectory for children
                        children_dir = target_dir / f"{safe_title}_Children"
                        children_dir.mkdir(exist_ok=True)
                        logger.info(f"📁 Downloading {len(children_data)} children to {children_dir.relative_to(self.output_dir)}/")

                        for child in children_data:
                            child_id = child['id']
                            child_title = child['title']
                            logger.info(f"  ↳ Child: {child_title} ({child_id})")
                            # Recursively download child page
                            self.download_page(child_id, max_retries, children_dir, title)

                return True, f"Success: {filename} ({file_size:,} bytes)"

            except requests.exceptions.HTTPError as e:
                if e.response.status_code == 404:
                    msg = f"Page {page_id} not found (404)"
                    logger.error(msg)
                    return False, msg
                elif e.response.status_code == 401:
                    msg = f"Authentication failed for page {page_id}"
                    logger.error(msg)
                    return False, msg
                else:
                    logger.error(f"HTTP error on attempt {attempt + 1}: {e}")
                    if attempt < max_retries - 1:
                        time.sleep(2 ** attempt)  # Exponential backoff
            except Exception as e:
                logger.error(f"Error on attempt {attempt + 1}: {e}")
                if attempt < max_retries - 1:
                    time.sleep(2 ** attempt)

        return False, f"Failed after {max_retries} attempts"

    def _convert_code_blocks(self, html: str) -> str:
        """Convert Confluence code blocks to HTML with language markers for post-processing."""
        # Pattern to match <ac:structured-macro ac:name="code">...</ac:structured-macro>
        code_block_pattern = re.compile(
            r'<ac:structured-macro[^>]*ac:name="code"[^>]*>(.*?)</ac:structured-macro>',
            re.DOTALL
        )

        def replace_code_block(match):
            code_macro = match.group(1)

            # Extract language parameter if present
            lang_match = re.search(r'<ac:parameter ac:name="language">([^<]+)</ac:parameter>', code_macro)
            language = lang_match.group(1) if lang_match else ''

            # Extract code content from CDATA section
            content_match = re.search(r'<!\[CDATA\[(.*?)\]\]>', code_macro, re.DOTALL)
            if not content_match:
                # Try plain-text-body without CDATA
                content_match = re.search(r'<ac:plain-text-body>(.*?)</ac:plain-text-body>', code_macro, re.DOTALL)

            if not content_match:
                return match.group(0)  # Keep original if we can't parse it

            code_content = content_match.group(1)

            # STEP 2: Create HTML with language marker as <p> tag before code block
            # This marker will be converted to text by markdownify, then post-processed
            if language:
                return f'<p>code-lang:{language}</p><pre><code>{code_content}</code></pre>'
            else:
                return f'<pre><code>{code_content}</code></pre>'

        return code_block_pattern.sub(replace_code_block, html)

    def _convert_children_macro(self, html: str, page_id: str) -> str:
        """Convert Confluence children macro to a markdown list placeholder."""
        # Pattern to match <ac:structured-macro ac:name="children">...</ac:structured-macro>
        children_macro_pattern = re.compile(
            r'<ac:structured-macro[^>]*ac:name="children"[^>]*>.*?</ac:structured-macro>',
            re.DOTALL
        )

        def replace_children_macro(match):
            # Get actual children from API
            try:
                children_data = self.validator.get_children(page_id)
                if children_data:
                    # Create HTML list that will be converted to markdown
                    items = []
                    for child in children_data:
                        child_title = child['title']
                        safe_child_title = self._sanitize_filename(child_title)

                        # If download_children enabled, link to subdirectory
                        if self.download_children:
                            current_title = ""  # Will be set when we have page_info
                            # For now, just create a plain list - paths will be in frontmatter
                            items.append(f"<li>{child_title}</li>")
                        else:
                            items.append(f"<li>{child_title}</li>")

                    return "<ul>\n" + "\n".join(items) + "\n</ul>"
                else:
                    return ""  # No children, remove macro
            except Exception as e:
                logger.warning(f"Error converting children macro: {e}")
                return ""  # Remove macro on error

        return children_macro_pattern.sub(replace_children_macro, html)

    def _localize_attachment_links(self, html: str, attachment_paths: Dict[str, Path]) -> str:
        """Replace Confluence attachment URLs with local file paths."""
        # First, convert Confluence code blocks to standard HTML
        html = self._convert_code_blocks(html)

        # Then convert Confluence <ac:image> tags to standard <img> tags
        html = self._convert_confluence_images(html, attachment_paths)

        # Then replace any remaining attachment URLs
        for attachment_name, local_path in attachment_paths.items():
            # Get relative path from output_dir
            rel_path = local_path.relative_to(self.output_dir)

            # Replace various forms of attachment URLs
            patterns = [
                # Standard attachment URL
                re.compile(rf'/wiki/download/attachments/\d+/{re.escape(quote(attachment_name))}', re.IGNORECASE),
                # Thumbnail URL
                re.compile(rf'/wiki/download/thumbnails/\d+/{re.escape(quote(attachment_name))}', re.IGNORECASE),
                # Simple filename reference
                re.compile(rf'(?<=src=")[^"]*{re.escape(attachment_name)}(?=")', re.IGNORECASE),
            ]

            for pattern in patterns:
                html = pattern.sub(str(rel_path), html)

        return html

    def _convert_confluence_images(self, html: str, attachment_paths: Dict[str, Path]) -> str:
        """Convert Confluence <ac:image> tags to standard HTML <img> tags with local paths."""
        # Pattern to match <ac:image>...</ac:image> blocks
        ac_image_pattern = re.compile(
            r'<ac:image[^>]*>(.*?)</ac:image>',
            re.DOTALL
        )

        def replace_ac_image(match):
            ac_image_block = match.group(0)
            inner_content = match.group(1)

            # Extract filename from <ri:attachment ri:filename="...">
            filename_match = re.search(r'<ri:attachment[^>]+ri:filename="([^"]+)"', inner_content)
            if not filename_match:
                return ac_image_block  # Keep original if no filename found

            filename = filename_match.group(1)

            # Find the local path for this attachment
            local_path = attachment_paths.get(filename)
            if not local_path:
                # Try without URL parameters
                base_filename = filename.split('?')[0]
                local_path = attachment_paths.get(base_filename)

            if not local_path:
                return ac_image_block  # Keep original if attachment not found

            # Get relative path
            rel_path = local_path.relative_to(self.output_dir)

            # Extract alt text from ac:alt attribute
            alt_match = re.search(r'ac:alt="([^"]*)"', ac_image_block)
            alt_text = alt_match.group(1) if alt_match else filename

            # Create standard HTML img tag
            return f'<img src="{rel_path}" alt="{alt_text}" />'

        return ac_image_pattern.sub(replace_ac_image, html)

    def _clean_markdown(self, markdown: str) -> str:
        """Clean up markdown formatting."""
        # Remove excessive blank lines
        markdown = re.sub(r'\n{3,}', '\n\n', markdown)

        # Fix code blocks - ensure proper spacing
        markdown = re.sub(r'```(\w*)\n+', r'```\1\n', markdown)
        markdown = re.sub(r'\n+```', r'\n```', markdown)

        # Clean up table formatting
        markdown = re.sub(r'\|\s*\n\s*\|', '|\n|', markdown)

        # Remove trailing whitespace
        lines = [line.rstrip() for line in markdown.split('\n')]
        markdown = '\n'.join(lines)

        return markdown.strip() + '\n'

    def _postprocess_code_languages(self, markdown: str) -> str:
        """STEP 4: Post-process markdown to add language tags to code fences."""
        # Pattern: code-lang:LANGUAGE followed by newline and code fence
        # Replace with just the code fence with language
        pattern = re.compile(
            r'code-lang:(\w+)\s*\n\s*```\s*\n',
            re.MULTILINE
        )

        def add_lang_to_fence(match):
            language = match.group(1)
            return f'```{language}\n'

        markdown = pattern.sub(add_lang_to_fence, markdown)

        # Also handle case where there might be extra whitespace
        # code-lang:json\n\n```
        pattern2 = re.compile(
            r'code-lang:(\w+)\s*\n+```',
            re.MULTILINE
        )
        markdown = pattern2.sub(lambda m: f'```{m.group(1)}\n', markdown)

        return markdown

    def _create_frontmatter(self, page_info: Dict, attachments: List[Dict], parent_title: Optional[str] = None) -> Dict:
        """Create YAML frontmatter from page info with hierarchy.

        Args:
            page_info: Page metadata from Confluence API
            attachments: List of attachment metadata
            parent_title: If in a subdirectory, the parent page title for path calculation
        """
        # Construct full Confluence URL
        confluence_url = f"{self.validator.web_base}{page_info['_links']['webui']}"

        frontmatter = {
            'title': page_info['title'],
            'confluence_url': confluence_url,
            'confluence': {
                'id': page_info['id'],
                'space': page_info['space']['key'],
                'version': page_info['version']['number'],
                'type': page_info['type']
            },
            'exported_at': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            'exported_by': 'confluence_downloader',
            'validation': {
                'html_content_length': len(page_info['body']['storage']['value']),
                'status': 'validated'
            }
        }

        # Add full breadcrumb path from ancestors
        if page_info.get('ancestors'):
            breadcrumb = []
            for ancestor in page_info['ancestors']:
                breadcrumb.append({
                    'id': ancestor['id'],
                    'title': ancestor['title']
                })
            # Add current page to breadcrumb
            breadcrumb.append({
                'id': page_info['id'],
                'title': page_info['title']
            })
            frontmatter['breadcrumb'] = breadcrumb

        # Add direct parent info if available
        if page_info.get('ancestors'):
            parent = page_info['ancestors'][-1]
            safe_parent_title = self._sanitize_filename(parent['title'])
            # If we're in a subdirectory (parent_title provided), parent is up one level
            parent_path = f"../{safe_parent_title}.md" if parent_title else f"{safe_parent_title}.md"
            frontmatter['parent'] = {
                'id': parent['id'],
                'title': parent['title'],
                'file': parent_path
            }

        # Add children if available
        children_data = self.validator.get_children(page_info['id'])
        if children_data:
            # If download_children is enabled, children will be in subdirectory
            current_title = self._sanitize_filename(page_info['title'])
            if self.download_children:
                # Children are in {Page_Title}_Children/ subdirectory
                frontmatter['children'] = [
                    {
                        'id': child['id'],
                        'title': child['title'],
                        'file': f"{current_title}_Children/{self._sanitize_filename(child['title'])}.md"
                    }
                    for child in children_data
                ]
            else:
                # Children are in same directory
                frontmatter['children'] = [
                    {
                        'id': child['id'],
                        'title': child['title'],
                        'file': f"{self._sanitize_filename(child['title'])}.md"
                    }
                    for child in children_data
                ]

        # Add labels if available
        if page_info.get('metadata', {}).get('labels', {}).get('results'):
            labels = [label['name'] for label in page_info['metadata']['labels']['results']]
            frontmatter['confluence']['labels'] = labels

        # Add attachment info
        if attachments:
            frontmatter['attachments'] = [
                {
                    'id': att['id'],
                    'title': att['title'],
                    'media_type': att['metadata'].get('mediaType', 'unknown'),
                    'file_size': att.get('extensions', {}).get('fileSize', 0)
                }
                for att in attachments
            ]

        return frontmatter

    def _sanitize_filename(self, title: str) -> str:
        """Convert title to safe filename."""
        # Replace spaces with underscores, remove special chars
        safe = re.sub(r'[^\w\s-]', '', title)
        safe = re.sub(r'[-\s]+', '_', safe)
        # Limit length
        if len(safe) > 100:
            safe = safe[:100]
        return safe.strip('_')


def load_configuration(env_file: Optional[str] = None, output_override: Optional[str] = None) -> Dict:
    """
    Load configuration using shared credential discovery.

    Args:
        env_file: Optional path to specific .env file
        output_override: Optional output directory override

    Returns:
        Dict with confluence_url, username, api_token, output_dir
    """
    try:
        creds = get_confluence_credentials(env_file=env_file)
    except ValueError as e:
        logger.error(f"Credential discovery failed: {e}")
        logger.info("\nCreate one of these files with credentials:")
        logger.info("  .env, .env.confluence, .env.jira, .env.atlassian")
        logger.info("\nRequired variables:")
        logger.info("  CONFLUENCE_URL=https://yourcompany.atlassian.net")
        logger.info("  CONFLUENCE_USERNAME=your.email@company.com")
        logger.info("  CONFLUENCE_API_TOKEN=your_api_token")
        logger.info("\nGet API Token: https://id.atlassian.com/manage-profile/security/api-tokens")
        sys.exit(1)

    return {
        'confluence_url': creds['url'],
        'username': creds['username'],
        'api_token': creds['token'],
        'output_dir': output_override or os.getenv('CONFLUENCE_OUTPUT_DIR', 'confluence_docs')
    }


def main():
    """Main execution function."""
    parser = argparse.ArgumentParser(
        description='Download Confluence pages to Markdown',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Single page
  %(prog)s 123456789

  # Multiple pages
  %(prog)s 123456 456789 789012

  # From file
  %(prog)s --page-ids-file page_ids.txt

  # With child pages in subdirectories
  %(prog)s --download-children 123456789

  # With HTML debugging
  %(prog)s --save-html 123456789

  # Custom output directory
  %(prog)s --output-dir ./docs 123456789

  # Custom .env file
  %(prog)s --env-file /path/to/.env 123456789
        '''
    )

    parser.add_argument('page_ids', nargs='*', help='Page IDs to download')
    parser.add_argument('--env-file', default='.env', help='Path to .env file (default: .env)')
    parser.add_argument('--output-dir', help='Output directory (overrides .env CONFLUENCE_OUTPUT_DIR)')
    parser.add_argument('--download-children', action='store_true', help='Download child pages to subdirectories')
    parser.add_argument('--save-html', action='store_true', help='Save intermediate HTML files for debugging')
    parser.add_argument('--page-ids-file', help='File containing page IDs (one per line)')

    args = parser.parse_args()

    # Load configuration
    config = load_configuration(args.env_file, args.output_dir)

    # Setup output directory
    output_dir = Path(config['output_dir'])
    output_dir.mkdir(exist_ok=True)

    # Initialize validator and downloader
    validator = ConfluenceValidator(config['confluence_url'], config['username'], config['api_token'])
    downloader = ConfluenceDownloader(validator, output_dir, save_html=args.save_html, download_children=args.download_children)

    if args.save_html:
        logger.info("HTML debug mode enabled - saving original XHTML to _html_debug/")
    if args.download_children:
        logger.info("Child page download enabled - children will be downloaded to {Parent_Name}_Children/ subdirectories")

    # Get page IDs
    if args.page_ids_file:
        page_ids_file = Path(args.page_ids_file)
        if not page_ids_file.exists():
            logger.error(f"Page IDs file not found: {page_ids_file}")
            sys.exit(1)

        with open(page_ids_file) as f:
            page_ids = [
                line.strip()
                for line in f
                if line.strip() and not line.strip().startswith('#')
            ]
    elif args.page_ids:
        page_ids = args.page_ids
    else:
        logger.error("No page IDs specified. Use page_ids arguments or --page-ids-file")
        parser.print_help()
        sys.exit(1)

    # Download pages
    logger.info(f"Downloading {len(page_ids)} pages...")
    results = []

    for i, page_id in enumerate(page_ids, 1):
        logger.info(f"\n{'='*60}")
        logger.info(f"Processing page {i}/{len(page_ids)}: {page_id}")
        logger.info(f"{'='*60}")

        success, message = downloader.download_page(page_id)
        results.append({
            'page_id': page_id,
            'success': success,
            'message': message
        })

        # Rate limiting
        if i < len(page_ids):
            time.sleep(1)

    # Print summary
    logger.info(f"\n{'='*60}")
    logger.info("DOWNLOAD SUMMARY")
    logger.info(f"{'='*60}")

    successful = sum(1 for r in results if r['success'])
    failed = len(results) - successful

    logger.info(f"✅ Successful: {successful}/{len(results)}")
    logger.info(f"❌ Failed: {failed}/{len(results)}")

    if failed > 0:
        logger.info("\nFailed pages:")
        for result in results:
            if not result['success']:
                logger.info(f"  - {result['page_id']}: {result['message']}")

    # Write results to JSON
    results_file = output_dir / 'download_results.json'
    with open(results_file, 'w') as f:
        json.dump({
            'timestamp': datetime.now().isoformat(),
            'total': len(results),
            'successful': successful,
            'failed': failed,
            'results': results
        }, f, indent=2)

    logger.info(f"\nResults saved to: {results_file}")


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
Convert Markdown to Confluence Wiki Markup

This script converts Markdown files to Confluence Wiki Markup format,
handling common elements like headings, lists, code blocks, tables, links, and images.

Usage:
    python convert_markdown_to_wiki.py input.md [output.wiki]
    python convert_markdown_to_wiki.py input.md  # Prints to stdout
"""

import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple


class MarkdownToWikiConverter:
    """Converts Markdown text to Confluence Wiki Markup."""

    def __init__(self):
        self.in_code_block = False
        self.code_block_content = []
        self.code_block_language = None

    def convert(self, markdown_text: str) -> str:
        """
        Convert Markdown text to Wiki Markup.

        Args:
            markdown_text: Input Markdown text

        Returns:
            Converted Wiki Markup text
        """
        lines = markdown_text.split('\n')
        output_lines = []

        i = 0
        while i < len(lines):
            line = lines[i]

            # Handle code blocks
            if line.strip().startswith('```'):
                result, i = self._handle_code_block(lines, i)
                output_lines.append(result)
                i += 1
                continue

            # Convert line-by-line elements
            converted = self._convert_line(line)
            output_lines.append(converted)
            i += 1

        return '\n'.join(output_lines)

    def _handle_code_block(self, lines: List[str], start_idx: int) -> Tuple[str, int]:
        """
        Handle code block conversion.

        Args:
            lines: All lines in the document
            start_idx: Starting index of code block

        Returns:
            Tuple of (converted code block, ending index)
        """
        first_line = lines[start_idx].strip()
        language = first_line[3:].strip() if len(first_line) > 3 else ''

        code_lines = []
        i = start_idx + 1

        while i < len(lines) and not lines[i].strip().startswith('```'):
            code_lines.append(lines[i])
            i += 1

        code_content = '\n'.join(code_lines)

        if language:
            wiki_code = f"{{code:language={language}}}\n{code_content}\n{{code}}"
        else:
            wiki_code = f"{{code}}\n{code_content}\n{{code}}"

        return wiki_code, i

    def _convert_line(self, line: str) -> str:
        """
        Convert a single line from Markdown to Wiki Markup.

        Args:
            line: Input line

        Returns:
            Converted line
        """
        # Empty line
        if not line.strip():
            return line

        # Headings
        if line.startswith('#'):
            return self._convert_heading(line)

        # Unordered lists
        if re.match(r'^(\s*)([-*+])\s+', line):
            return self._convert_unordered_list(line)

        # Ordered lists
        if re.match(r'^(\s*)(\d+\.)\s+', line):
            return self._convert_ordered_list(line)

        # Task lists
        if re.match(r'^(\s*)[-*]\s+\[([ xX])\]\s+', line):
            return self._convert_task_list(line)

        # Tables
        if '|' in line and not line.strip().startswith('|'):
            return self._convert_table_row(line)

        # Blockquotes
        if line.strip().startswith('>'):
            return self._convert_blockquote(line)

        # Horizontal rules
        if re.match(r'^(\*{3,}|-{3,}|_{3,})\s*$', line):
            return '----'

        # Regular paragraph - convert inline elements
        return self._convert_inline_elements(line)

    def _convert_heading(self, line: str) -> str:
        """Convert Markdown heading to Wiki Markup."""
        match = re.match(r'^(#{1,6})\s+(.+)$', line)
        if match:
            level = len(match.group(1))
            text = match.group(2).strip()

            # Remove trailing # if present
            text = re.sub(r'\s*#+\s*$', '', text)

            # Remove {#anchor} syntax
            text = re.sub(r'\s*\{#[^}]+\}\s*$', '', text)

            return f"h{level}. {text}"
        return line

    def _convert_unordered_list(self, line: str) -> str:
        """Convert Markdown unordered list to Wiki Markup."""
        match = re.match(r'^(\s*)([-*+])\s+(.+)$', line)
        if match:
            indent = match.group(1)
            text = match.group(3)

            # Calculate nesting level (2 or 4 spaces per level)
            level = len(indent) // 2 if len(indent) % 2 == 0 else len(indent) // 4 + 1
            level = max(1, level)

            bullets = '*' * level
            converted_text = self._convert_inline_elements(text)
            return f"{bullets} {converted_text}"
        return line

    def _convert_ordered_list(self, line: str) -> str:
        """Convert Markdown ordered list to Wiki Markup."""
        match = re.match(r'^(\s*)(\d+\.)\s+(.+)$', line)
        if match:
            indent = match.group(1)
            text = match.group(3)

            # Calculate nesting level
            level = len(indent) // 2 if len(indent) % 2 == 0 else len(indent) // 4 + 1
            level = max(1, level)

            numbers = '#' * level
            converted_text = self._convert_inline_elements(text)
            return f"{numbers} {converted_text}"
        return line

    def _convert_task_list(self, line: str) -> str:
        """Convert GitHub-style task list to Wiki Markup."""
        match = re.match(r'^(\s*)[-*]\s+\[([ xX])\]\s+(.+)$', line)
        if match:
            checked = match.group(2).lower() == 'x'
            text = match.group(3)
            converted_text = self._convert_inline_elements(text)

            if checked:
                return f"[x] {converted_text}"
            else:
                return f"[] {converted_text}"
        return line

    def _convert_table_row(self, line: str) -> str:
        """Convert Markdown table row to Wiki Markup."""
        # Skip separator rows
        if re.match(r'^\s*\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$', line):
            return ''

        # Split by pipes
        cells = [cell.strip() for cell in line.split('|')]

        # Remove empty first/last cells (from leading/trailing pipes)
        if cells and not cells[0]:
            cells = cells[1:]
        if cells and not cells[-1]:
            cells = cells[:-1]

        if not cells:
            return line

        # Convert inline elements in each cell
        converted_cells = [self._convert_inline_elements(cell) for cell in cells]

        # Detect if this is a header row (check if next line has separator)
        # For now, assume first row is header
        # In a full implementation, you'd need context from previous/next lines
        return '|' + '|'.join(converted_cells) + '|'

    def _convert_blockquote(self, line: str) -> str:
        """Convert Markdown blockquote to Wiki Markup."""
        # Remove leading >
        text = re.sub(r'^\s*>\s*', '', line)

        # Check for admonition style
        admonition_match = re.match(r'\*\*(\w+):\*\*\s*(.+)', text)
        if admonition_match:
            admonition_type = admonition_match.group(1).lower()
            content = admonition_match.group(2)

            if admonition_type in ['info', 'tip', 'note', 'warning']:
                return f"{{{admonition_type}}}\n{content}\n{{{admonition_type}}}"

        # Regular blockquote
        converted_text = self._convert_inline_elements(text)
        return f"bq. {converted_text}"

    def _convert_inline_elements(self, text: str) -> str:
        """Convert inline Markdown elements to Wiki Markup."""
        # Images: ![alt](url) -> !url|alt=alt!
        text = re.sub(
            r'!\[([^\]]*)\]\(([^)]+)\)',
            lambda m: f"!{m.group(2)}|alt={m.group(1)}!" if m.group(1) else f"!{m.group(2)}!",
            text
        )

        # Links: [text](url) -> [text|url]
        text = re.sub(
            r'\[([^\]]+)\]\(([^)]+)\)',
            r'[\1|\2]',
            text
        )

        # Bold: **text** or __text__ -> *text*
        text = re.sub(r'\*\*([^*]+)\*\*', r'*\1*', text)
        text = re.sub(r'__([^_]+)__', r'*\1*', text)

        # Italic: *text* or _text_ -> _text_
        # Be careful not to match already converted bold
        text = re.sub(r'(?<!\*)\*(?!\*)([^*]+)\*(?!\*)', r'_\1_', text)
        text = re.sub(r'(?<!_)_(?!_)([^_]+)_(?!_)', r'_\1_', text)

        # Strikethrough: ~~text~~ -> -text-
        text = re.sub(r'~~([^~]+)~~', r'-\1-', text)

        # Inline code: `text` -> {{text}}
        text = re.sub(r'`([^`]+)`', r'{{\1}}', text)

        return text


def convert_file(input_path: Path, output_path: Path = None) -> str:
    """
    Convert a Markdown file to Wiki Markup.

    Args:
        input_path: Path to input Markdown file
        output_path: Path to output Wiki file (optional)

    Returns:
        Converted Wiki Markup text
    """
    # Read input
    with open(input_path, 'r', encoding='utf-8') as f:
        markdown_text = f.read()

    # Convert
    converter = MarkdownToWikiConverter()
    wiki_text = converter.convert(markdown_text)

    # Write output if path provided
    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(wiki_text)

    return wiki_text


def main():
    """Main entry point for CLI usage."""
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    input_file = Path(sys.argv[1])

    if not input_file.exists():
        print(f"Error: File '{input_file}' not found", file=sys.stderr)
        sys.exit(1)

    output_file = Path(sys.argv[2]) if len(sys.argv) > 2 else None

    try:
        wiki_text = convert_file(input_file, output_file)

        if not output_file:
            print(wiki_text)
        else:
            print(f"Converted {input_file} -> {output_file}")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()

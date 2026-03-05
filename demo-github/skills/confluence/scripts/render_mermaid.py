#!/usr/bin/env python3
"""
Render Mermaid diagrams to PNG or SVG images.

This script uses the Mermaid CLI (mmdc) to render Mermaid diagram code
to image files. Requires @mermaid-js/mermaid-cli to be installed.

Installation:
    npm install -g @mermaid-js/mermaid-cli

Usage:
    python render_mermaid.py input.mmd output.png
    python render_mermaid.py input.mmd output.svg
    python render_mermaid.py -c "graph TD; A-->B" output.png
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional


def check_mermaid_cli() -> bool:
    """
    Check if mermaid-cli (mmdc) is installed.

    Returns:
        True if mmdc is available, False otherwise
    """
    try:
        result = subprocess.run(
            ['mmdc', '--version'],
            capture_output=True,
            text=True,
            check=False
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


def render_mermaid(
    input_path: Optional[Path] = None,
    output_path: Path = None,
    mermaid_code: Optional[str] = None,
    format: str = 'png',
    theme: str = 'default',
    background_color: str = 'white',
    width: Optional[int] = None,
    height: Optional[int] = None
) -> bool:
    """
    Render Mermaid diagram to image file.

    Args:
        input_path: Path to .mmd file (optional if mermaid_code provided)
        output_path: Path to output image file
        mermaid_code: Mermaid diagram code (optional if input_path provided)
        format: Output format ('png', 'svg', or 'pdf')
        theme: Mermaid theme ('default', 'forest', 'dark', 'neutral')
        background_color: Background color (e.g., 'white', 'transparent', '#f0f0f0')
        width: Output width in pixels (optional)
        height: Output height in pixels (optional)

    Returns:
        True if rendering succeeded, False otherwise
    """
    if not check_mermaid_cli():
        print("Error: mermaid-cli (mmdc) is not installed", file=sys.stderr)
        print("Install with: npm install -g @mermaid-js/mermaid-cli", file=sys.stderr)
        return False

    # Create temporary file if code provided instead of file
    temp_file = None
    if mermaid_code:
        temp_file = tempfile.NamedTemporaryFile(mode='w', suffix='.mmd', delete=False)
        temp_file.write(mermaid_code)
        temp_file.close()
        input_path = Path(temp_file.name)

    if not input_path or not input_path.exists():
        print(f"Error: Input file '{input_path}' not found", file=sys.stderr)
        return False

    # Build mmdc command
    cmd = [
        'mmdc',
        '-i', str(input_path),
        '-o', str(output_path),
        '-t', theme,
        '-b', background_color
    ]

    # Add width and height if specified
    if width:
        cmd.extend(['-w', str(width)])
    if height:
        cmd.extend(['-H', str(height)])

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False
        )

        if result.returncode != 0:
            print(f"Error rendering Mermaid diagram:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return False

        print(f"Successfully rendered: {output_path}")
        return True

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return False

    finally:
        # Clean up temporary file
        if temp_file:
            Path(temp_file.name).unlink(missing_ok=True)


def extract_mermaid_from_markdown(markdown_text: str) -> list:
    """
    Extract Mermaid diagram code blocks from Markdown text.

    Args:
        markdown_text: Markdown content

    Returns:
        List of Mermaid diagram code strings
    """
    import re

    pattern = r'```mermaid\s*\n(.*?)\n```'
    matches = re.findall(pattern, markdown_text, re.DOTALL)
    return matches


def render_markdown_diagrams(
    markdown_file: Path,
    output_dir: Path,
    format: str = 'png',
    prefix: str = 'diagram'
) -> list:
    """
    Extract and render all Mermaid diagrams from a Markdown file.

    Args:
        markdown_file: Path to Markdown file
        output_dir: Directory to save rendered diagrams
        format: Output format ('png', 'svg', or 'pdf')
        prefix: Prefix for output filenames

    Returns:
        List of tuples (diagram_index, output_path)
    """
    with open(markdown_file, 'r', encoding='utf-8') as f:
        markdown_text = f.read()

    diagrams = extract_mermaid_from_markdown(markdown_text)
    output_dir.mkdir(parents=True, exist_ok=True)

    rendered_files = []

    for i, diagram_code in enumerate(diagrams):
        output_filename = f"{prefix}-{i+1}.{format}"
        output_path = output_dir / output_filename

        success = render_mermaid(
            mermaid_code=diagram_code,
            output_path=output_path,
            format=format
        )

        if success:
            rendered_files.append((i, output_path))

    return rendered_files


def main():
    """Main entry point for CLI usage."""
    parser = argparse.ArgumentParser(
        description='Render Mermaid diagrams to image files'
    )

    parser.add_argument(
        'input',
        nargs='?',
        help='Input .mmd file or Markdown file'
    )
    parser.add_argument(
        'output',
        nargs='?',
        help='Output image file path'
    )
    parser.add_argument(
        '-c', '--code',
        help='Mermaid diagram code (alternative to input file)'
    )
    parser.add_argument(
        '-f', '--format',
        choices=['png', 'svg', 'pdf'],
        default='png',
        help='Output format (default: png)'
    )
    parser.add_argument(
        '-t', '--theme',
        choices=['default', 'forest', 'dark', 'neutral'],
        default='default',
        help='Mermaid theme (default: default)'
    )
    parser.add_argument(
        '-b', '--background',
        default='white',
        help='Background color (default: white)'
    )
    parser.add_argument(
        '-w', '--width',
        type=int,
        help='Output width in pixels'
    )
    parser.add_argument(
        '-H', '--height',
        type=int,
        help='Output height in pixels'
    )
    parser.add_argument(
        '--extract-from-markdown',
        action='store_true',
        help='Extract and render all diagrams from Markdown file'
    )
    parser.add_argument(
        '--output-dir',
        type=Path,
        help='Output directory (for --extract-from-markdown)'
    )

    args = parser.parse_args()

    # Check if mmdc is installed
    if not check_mermaid_cli():
        sys.exit(1)

    # Extract from Markdown mode
    if args.extract_from_markdown:
        if not args.input:
            print("Error: Input Markdown file required", file=sys.stderr)
            sys.exit(1)

        input_path = Path(args.input)
        output_dir = args.output_dir or Path('diagrams')

        rendered = render_markdown_diagrams(
            input_path,
            output_dir,
            format=args.format
        )

        print(f"\nRendered {len(rendered)} diagrams to {output_dir}/")
        for idx, path in rendered:
            print(f"  [{idx+1}] {path.name}")

        sys.exit(0)

    # Regular rendering mode
    if not args.code and not args.input:
        parser.print_help()
        sys.exit(1)

    if not args.output:
        print("Error: Output file path required", file=sys.stderr)
        sys.exit(1)

    input_path = Path(args.input) if args.input else None
    output_path = Path(args.output)

    success = render_mermaid(
        input_path=input_path,
        output_path=output_path,
        mermaid_code=args.code,
        format=args.format,
        theme=args.theme,
        background_color=args.background,
        width=args.width,
        height=args.height
    )

    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()

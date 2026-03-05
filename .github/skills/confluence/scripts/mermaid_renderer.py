"""
Mermaid Diagram Support for Confluence

Extends md2cf's ConfluenceRenderer to detect Mermaid code blocks,
render them to SVG images, and convert to Confluence image macros.

Requirements:
    - mermaid-cli (mmdc): npm install -g @mermaid-js/mermaid-cli
    - md2cf: pip install md2cf

Usage:
    from mermaid_renderer import MermaidConfluenceRenderer
    import mistune

    renderer = MermaidConfluenceRenderer()
    markdown = "# Title\n\n```mermaid\ngraph TD\n  A-->B\n```"
    markdown_parser = mistune.Markdown(renderer=renderer)
    html = markdown_parser(markdown)

    # Access generated attachments
    for attachment in renderer.attachments:
        print(f"Upload: {attachment}")
"""

import os
import sys
import subprocess
import tempfile
import hashlib
from pathlib import Path
from typing import List, Dict, Optional

try:
    from md2cf.confluence_renderer import ConfluenceRenderer
except ImportError:
    print("ERROR: md2cf not installed. Install with: pip install md2cf", file=sys.stderr)
    sys.exit(1)


class MermaidConfluenceRenderer(ConfluenceRenderer):
    """
    Confluence renderer with Mermaid diagram support.

    Extends md2cf's ConfluenceRenderer to:
    - Detect ```mermaid code blocks
    - Render to SVG using mermaid-cli (mmdc)
    - Track attachments for upload
    - Convert to Confluence image macros
    """

    def __init__(self, output_dir: Optional[str] = None):
        """
        Initialize renderer.

        Args:
            output_dir: Directory for generated diagram files (default: temp dir)
        """
        super().__init__()
        self.attachments: List[Dict[str, str]] = []
        self.output_dir = Path(output_dir) if output_dir else Path(tempfile.mkdtemp(prefix='mermaid_'))
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self._mermaid_count = 0

    def block_code(self, code: str, lang: Optional[str] = None) -> str:
        """
        Override block_code to handle Mermaid diagrams.

        Args:
            code: Code block content
            lang: Language identifier (e.g., 'python', 'mermaid')

        Returns:
            Confluence storage format HTML
        """
        if lang and lang.lower() == 'mermaid':
            return self._render_mermaid(code)
        else:
            # Use parent renderer for regular code blocks
            return super().block_code(code, lang)

    def _render_mermaid(self, mermaid_code: str) -> str:
        """
        Render Mermaid diagram to SVG and return image macro.

        Args:
            mermaid_code: Mermaid diagram syntax

        Returns:
            Confluence image macro HTML
        """
        self._mermaid_count += 1

        # Generate unique filename
        code_hash = hashlib.md5(mermaid_code.encode()).hexdigest()[:8]
        filename = f"diagram-{self._mermaid_count:03d}-{code_hash}.svg"
        svg_path = self.output_dir / filename

        try:
            # Render Mermaid to SVG
            self._run_mmdc(mermaid_code, svg_path)

            # Track for attachment upload
            self.attachments.append({
                'path': str(svg_path),
                'filename': filename,
                'type': 'image/svg+xml'
            })

            # Return Confluence image macro
            return f'<p><ac:image><ri:attachment ri:filename="{filename}"/></ac:image></p>'

        except Exception as e:
            # Fallback: render as regular code block with error comment
            print(f"WARNING: Failed to render Mermaid diagram: {e}", file=sys.stderr)
            fallback = super().block_code(mermaid_code, 'text')
            error_msg = f'<p><ac:structured-macro ac:name="warning"><ac:rich-text-body><p>Mermaid diagram rendering failed: {e}</p></ac:rich-text-body></ac:structured-macro></p>'
            return error_msg + fallback

    def _run_mmdc(self, mermaid_code: str, output_path: Path) -> None:
        """
        Run mermaid-cli (mmdc) to render diagram.

        Args:
            mermaid_code: Mermaid syntax
            output_path: Path to output SVG file

        Raises:
            RuntimeError: If mmdc is not installed or rendering fails
            subprocess.CalledProcessError: If mmdc returns non-zero exit code
        """
        # Check if mmdc is available
        if not self._check_mmdc_installed():
            raise RuntimeError(
                "mermaid-cli (mmdc) not found. Install with: npm install -g @mermaid-js/mermaid-cli"
            )

        # Write Mermaid code to temp file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.mmd', delete=False) as f:
            f.write(mermaid_code)
            input_path = f.name

        try:
            # Run mmdc with explicit background and format
            result = subprocess.run(
                [
                    'mmdc',
                    '-i', input_path,
                    '-o', str(output_path),
                    '-b', 'transparent',  # Transparent background
                    '-t', 'default'       # Default theme
                ],
                capture_output=True,
                text=True,
                timeout=30,
                check=True
            )

            # Verify output file was created
            if not output_path.exists():
                raise RuntimeError(f"mmdc did not create output file: {output_path}")

            # Verify output file is not empty
            if output_path.stat().st_size == 0:
                raise RuntimeError(f"mmdc created empty file: {output_path}")

        except subprocess.TimeoutExpired:
            raise RuntimeError("mmdc timed out after 30 seconds")

        except subprocess.CalledProcessError as e:
            error_msg = f"mmdc failed with exit code {e.returncode}"
            if e.stderr:
                error_msg += f": {e.stderr}"
            raise RuntimeError(error_msg)

        finally:
            # Clean up temp input file
            try:
                os.unlink(input_path)
            except OSError:
                pass

    @staticmethod
    def _check_mmdc_installed() -> bool:
        """Check if mermaid-cli (mmdc) is installed and available"""
        try:
            result = subprocess.run(
                ['mmdc', '--version'],
                capture_output=True,
                timeout=5
            )
            return result.returncode == 0
        except (FileNotFoundError, subprocess.TimeoutExpired):
            return False

    def get_attachments(self) -> List[Dict[str, str]]:
        """
        Get list of attachments generated during rendering.

        Returns:
            List of dicts with 'path', 'filename', 'type' keys
        """
        return self.attachments

    def clear_attachments(self) -> None:
        """Clear attachment list and reset counter"""
        self.attachments = []
        self._mermaid_count = 0


def test_mermaid_rendering():
    """Test Mermaid rendering functionality"""
    import mistune

    # Sample Mermaid diagram
    markdown = """
# Test Document

Regular paragraph.

## Architecture Diagram

```mermaid
graph TD
    A[Client] --> B[Load Balancer]
    B --> C[Web Server 1]
    B --> D[Web Server 2]
    C --> E[Database]
    D --> E
```

## Another Section

More content here.

```python
def hello():
    print("Hello, Confluence!")
```
"""

    renderer = MermaidConfluenceRenderer()

    try:
        # Use mistune 0.8.x API
        markdown_parser = mistune.Markdown(renderer=renderer)
        html = markdown_parser(markdown)

        print("✅ Rendering successful")
        print(f"\nGenerated {len(renderer.attachments)} attachment(s):")
        for att in renderer.attachments:
            print(f"  - {att['filename']} ({att['type']}) at {att['path']}")

        print(f"\nHTML output preview (first 500 chars):")
        print(html[:500] + "...")

        # Verify SVG files were created
        for att in renderer.attachments:
            path = Path(att['path'])
            if not path.exists():
                print(f"❌ ERROR: Attachment file not found: {path}")
            elif path.stat().st_size == 0:
                print(f"❌ ERROR: Attachment file is empty: {path}")
            else:
                print(f"✅ Attachment file OK: {path} ({path.stat().st_size} bytes)")

    except Exception as e:
        print(f"❌ ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    test_mermaid_rendering()

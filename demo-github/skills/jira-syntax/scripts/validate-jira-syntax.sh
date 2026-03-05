#!/bin/bash

# Jira Wiki Markup Syntax Validator
# Checks text for common Jira syntax errors and suggests corrections

set -e

# Colors for output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Counters
ERRORS=0
WARNINGS=0

# Function to print error
error() {
    echo -e "${RED}❌ ERROR:${NC} $1"
    ((ERRORS++))
}

# Function to print warning
warning() {
    echo -e "${YELLOW}⚠️  WARNING:${NC} $1"
    ((WARNINGS++))
}

# Function to print success
success() {
    echo -e "${GREEN}✅ $1${NC}"
}

# Function to check file
validate_file() {
    local file="$1"
    echo ""
    echo "=========================================="
    echo "Validating: $file"
    echo "=========================================="

    if [ ! -f "$file" ]; then
        error "File not found: $file"
        return
    fi

    local content=$(cat "$file")
    local line_num=0

    # Check for Markdown-style headings (## instead of h2.)
    if echo "$content" | grep -qE "^##+ "; then
        error "Found Markdown-style headings (##). Use Jira format: h2. Heading"
        echo "   Lines with issue:"
        echo "$content" | grep -nE "^##+ " | head -5
    fi

    # Check for Markdown-style bold (**text** instead of *text*)
    if echo "$content" | grep -qE "\*\*[^*]+\*\*"; then
        warning "Found Markdown-style bold (**text**). Use Jira format: *text*"
        echo "   Examples found:"
        echo "$content" | grep -oE "\*\*[^*]+\*\*" | head -3
    fi

    # Check for Markdown-style italic (_text_ is ok, but *text* for bold might be confused)
    if echo "$content" | grep -qE "\*[^*]+\*\*[^*]+\*"; then
        warning "Found potential Markdown-style italic mixed with bold"
    fi

    # Check for Markdown-style code blocks (``` instead of {code})
    if echo "$content" | grep -qE "^\`\`\`"; then
        error "Found Markdown code blocks (\`\`\`). Use Jira format: {code:language}"
        echo "   Lines with issue:"
        echo "$content" | grep -nE "^\`\`\`" | head -5
    fi

    # Check for Markdown-style inline code (` instead of {{)
    if echo "$content" | grep -qE "\`[^\`]+\`" && ! echo "$content" | grep -qE "{{[^}]+}}"; then
        warning "Found Markdown inline code (\`code\`). Consider Jira format: {{code}}"
    fi

    # Check for Markdown-style links ([text](url) instead of [text|url])
    if echo "$content" | grep -qE "\[([^\]]+)\]\(([^)]+)\)"; then
        error "Found Markdown-style links ([text](url)). Use Jira format: [text|url]"
        echo "   Examples found:"
        echo "$content" | grep -oE "\[([^\]]+)\]\(([^)]+)\)" | head -3
    fi

    # Check for headings without space after period (h2.Title instead of h2. Title)
    if echo "$content" | grep -qE "^h[1-6]\.[^ ]"; then
        error "Found headings without space after period. Use: h2. Title (not h2.Title)"
        echo "   Lines with issue:"
        echo "$content" | grep -nE "^h[1-6]\.[^ ]" | head -5
    fi

    # Check for code blocks without language specification
    if echo "$content" | grep -qE "{code}[^{]"; then
        warning "Found {code} blocks without language. Consider: {code:java} for syntax highlighting"
    fi

    # Check for tables with incorrect header syntax (|Header| instead of ||Header||)
    if echo "$content" | grep -qE "^\|[^|]+\|$" && ! echo "$content" | grep -qE "^\|\|"; then
        warning "Potential table header without double pipes. Headers should use: ||Header||"
    fi

    # Check for unclosed {code} blocks
    local code_open=$(echo "$content" | grep -c "{code")
    local code_close=$(echo "$content" | grep -c "{/code}")
    if [ "$code_open" -ne "$code_close" ]; then
        error "Mismatched {code} tags: $code_open opening, $code_close closing"
    fi

    # Check for unclosed {panel} blocks
    local panel_open=$(echo "$content" | grep -c "{panel")
    local panel_close=$(echo "$content" | grep -c "{/panel}")
    if [ "$panel_open" -ne "$panel_close" ]; then
        error "Mismatched {panel} tags: $panel_open opening, $panel_close closing"
    fi

    # Check for unclosed {color} blocks
    local color_count=$(echo "$content" | grep -o "{color" | wc -l)
    if [ $((color_count % 2)) -ne 0 ]; then
        warning "Potential unclosed {color} tag (odd number of occurrences)"
    fi

    # Check for Markdown-style lists (- item instead of * item)
    if echo "$content" | grep -qE "^- [^-]"; then
        warning "Found Markdown-style bullets (- item). Jira prefers: * item"
    fi

    # Positive checks
    if echo "$content" | grep -qE "^h[1-6]\. "; then
        success "Found correctly formatted Jira headings"
    fi

    if echo "$content" | grep -qE "{code:[a-z]+}"; then
        success "Found code blocks with language specification"
    fi

    if echo "$content" | grep -qE "\[~[a-z.]+\]"; then
        success "Found user mentions ([~username])"
    fi

    if echo "$content" | grep -qE "\[[A-Z]+-[0-9]+\]"; then
        success "Found issue links ([PROJ-123])"
    fi
}

# Main script
echo "Jira Wiki Markup Syntax Validator"
echo "=================================="

if [ $# -eq 0 ]; then
    echo "Usage: $0 <file1> [file2] [file3] ..."
    echo ""
    echo "Validates Jira wiki markup syntax in text files"
    echo ""
    echo "Example:"
    echo "  $0 issue-description.txt"
    echo "  $0 templates/*.md"
    exit 1
fi

# Validate each file
for file in "$@"; do
    validate_file "$file"
done

# Summary
echo ""
echo "=========================================="
echo "Validation Summary"
echo "=========================================="
echo "Files checked: $#"
echo -e "${RED}Errors: $ERRORS${NC}"
echo -e "${YELLOW}Warnings: $WARNINGS${NC}"

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✅ All checks passed!${NC}"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠️  No errors, but $WARNINGS warnings found${NC}"
    exit 0
else
    echo -e "${RED}❌ $ERRORS errors found - please fix before submitting to Jira${NC}"
    exit 1
fi

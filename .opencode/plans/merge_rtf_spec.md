# RTF 1.9.1 Specification Merge Plan

## Analysis Summary

### File Comparison

| File | Lines | Size | Quality | Key Strengths | Key Weaknesses |
|------|-------|------|---------|---------------|----------------|
| `Final.RTF.1.9.1.pdf` | 16,354 | 8.0 MB | Ground truth | Original source | Not markdown |
| `RTF.1.9.1.md` | 8,114 | 464 KB | ⚠️ Missing sections | Good heading structure | Missing ~1,800 lines of content |
| `RTF.1.9.1.md.1` | 9,969 | 647 KB | ✅ Best base | Cleanest, most complete | ~1,800 lines less than PDF |
| `RTF.1.9.1.md.2` | 8,938 | 677 KB | ❌ Broken | - | Appendix B completely missing |
| `RTF.1.9.1.md.3` | 13,577 | 676 KB | ⚠️ Noisy | Most verbose, has some missing sections | 264 page header artifacts, 350 deep headings |

### Quality Metrics

| Metric | md | md.1 | md.3 |
|--------|-----|------|------|
| Page header artifacts | 0 | 0 | 264 |
| Page footer refs | 198 | 56 | 278 |
| Copyright footers | 144 | 2 | 0 |
| `###` headings (deep) | 11 | 62 | 350 |
| Code fences | 162 | 185 | 162 |
| Unique headings | 76 | 186 | 1006 |

### Sections Unique to Each File

**Missing from md.1** (must add from md.3):
- `## Paragraph Group Properties` — PGP table with syntax, ~23 lines
- `## Revision Marks` — with `## RSID` nested, ~6 lines

**Missing from md** (present in md.1):
- `## Ink Information`, `## Drawing Canvases and Diagrams`, `## Connectors`
- `## Table Styles Example`, `## Table RTF`
- `## Word 97 through Word 2007 RTF` (Bullets section)
- `## Revision Marks for Paragraph Numbers and ListNum Fields`
- `## Paragraph Borders`, `## Paragraph Shading`
- `## East Asian Support`, `## Escaped Expressions`
- `## Pictures`, `## Objects` (with extensive sub-sections)
- `## Mathematics` (comprehensive section)

## Merge Strategy

**Base**: `RTF.1.9.1.md.1` (cleanest, most complete, minimal artifacts)

### Phase 1: Clean md.1

1. **Remove `***` page separators** (2 occurrences)
2. **Remove page numbers** (`Page N` at end of lines) — ~56 occurrences
3. **Remove copyright footers** — ~2 occurrences  
4. **Remove duplicate title page** — the second "Rich Text Format (RTF) Specification" after `***`
5. **Remove code fence wrappers** (` ``` `) — ~185 occurrences (PDF extraction artifacts)
6. **Normalize blank lines** (max 2 consecutive)

### Phase 2: Insert Missing Sections from md.3

Insert these sections after `### List Override Table` and before `## Old Properties`:

```markdown
## Paragraph Group Properties

Word 2002 introduced paragraph group properties, similar to style sheets. A document using paragraph group properties places a `\pgptbl` entry in the header...

## Revision Marks

This table allows tracking of multiple authors and reviewers of a document...

## RSID

The RSID control words are used to track revision identification...
```

### Phase 3: Fix Heading Hierarchy

Per PDF TOC, correct heading levels:

| Current in md.1 | Correct Level | Notes |
|-----------------|---------------|-------|
| `# Control Symbol` | `## Control Symbol` | Child of Introduction |
| `# Group` | `## Group` | Child of Introduction |
| `# Destinations` | `## Destinations` | Child of Introduction |
| `# List Tables` | `## List Tables` | Child of Contents of an RTF File |
| `# XML Namespace Table` | `## XML Namespace Table` | Child of Contents |
| `# Document Formatting Properties` | `## Document Formatting Properties` | Child of Contents |
| `# Mail Merge Source Document Types` | `### Mail Merge Source Document Types` | Child of Mail Merge |
| `# Mail Merge Data Types` | `### Mail Merge Data Types` | Child of Mail Merge |
| `# Section Text` | `### Section Text` | Child of Document Formatting Properties |
| `# Section Formatting Properties` | `### Section Formatting Properties` | Child of Document Formatting Properties |
| `# Paragraph Text` | `### Paragraph Text` | Child of Document Formatting Properties |
| `# Paragraph Formatting Properties` | `### Paragraph Formatting Properties` | Child of Document Formatting Properties |
| `# Tabs` | `### Tabs` | Child of Paragraph Formatting |
| `# Bullets and Numbering` | `### Bullets and Numbering` | Child of Paragraph Formatting |
| `# Table Styles Example` | `## Table Styles Example` | Child of Contents |
| `# Table RTF` | `## Table RTF` | Child of Contents |
| `# Mathematics` | `### Mathematics` | Child of Paragraph Formatting |
| `# Document Variables` | `## Document Variables` | Child of Contents |
| `# Bookmarks` | `## Bookmarks` | Child of Contents |
| `# Move Bookmarks` | `## Move Bookmarks` | Child of Bookmarks |
| `# Protection Exceptions` | `## Protection Exceptions` | Child of Contents |
| `# Pictures` | `## Pictures` | Child of Contents |
| `# Custom XML Tags` | `## Custom XML Tags` | Child of Contents |
| `# SmartTag Data` | `## SmartTag Data` | Child of Contents |
| `# Custom XML Data Properties` | `## Custom XML Data Properties` | Child of Contents |
| `# Objects` | `## Objects` | Child of Contents |
| `# Macintosh Edition Manager Publisher Objects` | `## Macintosh Edition Manager Publisher Objects` | Child of Objects |
| `# Drawing Objects` | `## Drawing Objects` | Child of Objects |
| `# Footnotes` | `## Footnotes` | Child of Contents |
| `# Comments (Annotations)` | `## Comments (Annotations)` | Child of Contents |
| `# Fields` | `## Fields` | Child of Contents |
| `# East Asian Control Words Created by Word 6J` | `## East Asian Control Words Created by Word 6J` | Child of Contents |
| `# East Asian Control Words` | `## East Asian Control Words` | Child of Contents |
| `# East Asian Control Words Created by Word 2000` | `## East Asian Control Words Created by Word 2000` | Child of Contents |

### Phase 4: Validation

1. **Section count**: Verify all sections from PDF TOC are present
2. **Control word count**: Verify Appendix B has ~1,800 control word entries
3. **No duplicate headings**: Ensure no duplicate section names at same level
4. **No page artifacts**: Zero "Page N" or copyright footer occurrences
5. **Heading hierarchy**: All headings follow consistent `#`/`##`/`###` structure matching PDF TOC

### Phase 5: Final Cleanup

1. Remove trailing whitespace from all lines
2. Ensure single trailing newline at end of file
3. Verify TOC section entries match actual headings
4. Check that all code blocks use proper ```` ``` ```` syntax

## Expected Output

- **Target**: `Specifications/RTF.1.9.1.md` (overwrite existing)
- **Expected size**: ~9,500-10,000 lines (slightly less than md.3, more complete than md.1)
- **Structure**: Clean heading hierarchy matching PDF TOC
- **Content**: All sections from PDF, with best-of-3 quality per section

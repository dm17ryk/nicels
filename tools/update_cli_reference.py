#!/usr/bin/env python3
"""Generate a Markdown table of nls CLI options by parsing --help output."""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import List


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "binary",
        nargs="?",
        default="build/nls",
        help="Path to the nls executable (default: build/nls)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Path to write the generated Markdown (defaults to stdout)",
    )
    return parser.parse_args()


@dataclass
class CliOption:
    names: List[str]
    argument: str
    default: str
    description_lines: List[str]
    group: str

    def description(self) -> str:
        text = " ".join(line.strip() for line in self.description_lines if line.strip()).strip()
        return re.sub(r"\s+", " ", text)


ARG_TOKEN_RE = re.compile(r"^(\+?[A-Z0-9_-]+|\.{3})$")


def _split_spec(spec: str) -> tuple[list[str], str]:
    tokens = spec.split()
    arg_tokens: List[str] = []
    while tokens:
        token = tokens[-1]
        stripped = token.strip(',')
        if not stripped:
            tokens.pop()
            continue
        if stripped.startswith('-'):
            break
        if ARG_TOKEN_RE.match(stripped) or stripped.endswith('...'):
            arg_tokens.append(stripped)
            tokens.pop()
            continue
        if stripped.upper() == stripped and not stripped.startswith('--'):
            arg_tokens.append(stripped)
            tokens.pop()
            continue
        break
    arg_tokens.reverse()
    names_part = " ".join(tokens).strip()
    names = [name.strip() for name in names_part.split(',') if name.strip()]
    argument = " ".join(arg_tokens)
    return names, argument


def _split_inline_description(spec_text: str, is_positional: bool) -> tuple[str, str]:
    if is_positional:
        return spec_text, ''
    tokens = spec_text.split()
    if not tokens:
        return spec_text, ''
    split_index = len(tokens)
    for idx, token in enumerate(tokens):
        core = token.strip(',')
        if core.startswith('-'):
            continue
        if ARG_TOKEN_RE.match(core) or (core.upper() == core and not core.startswith('--')):
            continue
        split_index = idx
        break
    spec_tokens = tokens[:split_index]
    inline_tokens = tokens[split_index:]
    return " ".join(spec_tokens), " ".join(inline_tokens).strip()


def parse_cli_help(raw_help: str) -> list[CliOption]:
    group = "General"
    section: str | None = None
    current: CliOption | None = None
    options: list[CliOption] = []
    for line in raw_help.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith('Usage:'):
            continue
        if stripped == 'POSITIONALS:':
            section = 'positionals'
            group = 'Positionals'
            if current:
                options.append(current)
            current = None
            continue
        if stripped == 'OPTIONS:':
            section = 'options'
            if current:
                options.append(current)
            if group == 'Positionals':
                group = 'General'
            current = None
            continue
        if line.startswith('[Option Group:') and line.endswith(']'):
            if current:
                options.append(current)
            group = line[len('[Option Group:'): -1].strip()
            section = 'options'
            current = None
            continue
        if stripped.startswith('The SIZE argument'):
            break

        indent_len = len(line) - len(line.lstrip())
        content = line[indent_len:]
        if section == 'options' and not content.startswith('-'):
            if current:
                current.description_lines.append(stripped)
            continue
        if section == 'positionals' and indent_len > 4:
            if current:
                current.description_lines.append(stripped)
            continue

        parts = [segment for segment in re.split(r"\s{2,}", content.strip()) if segment]
        if not parts:
            continue
        last_segment = parts[-1].strip()
        if len(parts) > 1 and not last_segment.startswith('-') and not ARG_TOKEN_RE.match(last_segment) and last_segment.upper() != last_segment:
            spec_text = " ".join(parts[:-1])
            description_seed = last_segment
        else:
            spec_text = " ".join(parts)
            description_seed = ''

        spec_text = spec_text.strip()
        default = ''
        if spec_text.endswith(']') and '[' in spec_text:
            spec_text, default_part = spec_text.rsplit('[', 1)
            default = default_part.rstrip(']')
            spec_text = spec_text.strip()

        spec_text, inline_desc = _split_inline_description(spec_text, section == 'positionals')
        spec = re.sub(r"\s+", " ", spec_text)
        names, argument = _split_spec(spec)
        if not names:
            if current and (inline_desc or description_seed):
                extra = " ".join(token for token in [inline_desc, description_seed] if token)
                if extra:
                    current.description_lines.append(extra)
            continue
        if current:
            options.append(current)
        description_lines: list[str] = []
        if inline_desc:
            description_lines.append(inline_desc)
        if description_seed and description_seed not in description_lines:
            description_lines.append(description_seed)
        current = CliOption(names=names, argument=argument, default=default, description_lines=description_lines, group=group)

    if current:
        options.append(current)
    return options


def format_markdown(options: list[CliOption]) -> str:
    group_order: list[str] = []
    grouped: dict[str, list[CliOption]] = defaultdict(list)
    for opt in options:
        if opt.group not in grouped:
            group_order.append(opt.group)
        grouped[opt.group].append(opt)

    lines: list[str] = []
    for group in group_order:
        lines.append(f"#### {group}")
        lines.append("")
        lines.append("| Option(s) | Argument | Default | Description |")
        lines.append("| --- | --- | --- | --- |")
        for opt in grouped[group]:
            names = ", ".join(opt.names)
            argument = opt.argument or "—"
            default = opt.default or "—"
            desc = opt.description() or "—"
            lines.append(f"| `{names}` | `{argument}` | `{default}` | {desc} |")
        lines.append("")
    return "\n".join(lines).strip() + "\n"


def main() -> int:
    args = parse_arguments()
    binary = Path(args.binary)
    if not binary.exists():
        print(f"error: {binary} does not exist. Build nls first.", file=sys.stderr)
        return 2
    try:
        result = subprocess.run(
            [str(binary), "--help"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env={**os.environ, "TERM": "dumb"},
        )
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr)
        return exc.returncode
    options = parse_cli_help(result.stdout)
    markdown = format_markdown(options)
    if args.output:
        args.output.write_text(markdown, encoding='utf-8')
    else:
        sys.stdout.write(markdown)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

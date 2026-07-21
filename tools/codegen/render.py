"""Layer 3 of the codegen: the template engine + renderer.

Emitters render text through a target template pack (``templates/<target>/``) instead of concatenating
strings by hand. Adding an output language becomes adding a template pack, not new emitter modules.

The renderer returns a list of lines (matching the historical emitter contract) so the surrounding
generator keeps joining fragments the same way; ``render_lines`` splits on newlines, so a template that
ends in a newline contributes a trailing blank line exactly like the old ``[..., ""]`` list tails.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
# Vendored, pure-Python Jinja2 (+ MarkupSafe) so the build needs no pip step.
sys.path.insert(0, os.path.join(_HERE, "_vendor"))

from jinja2 import Environment, FileSystemLoader, StrictUndefined  # noqa: E402

_ENVIRONMENT = Environment(
    loader=FileSystemLoader(os.path.join(_HERE, "templates")),
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
    undefined=StrictUndefined,
    autoescape=False,
)


def render(template_name: str, **context: object) -> str:
    return _ENVIRONMENT.get_template(template_name).render(**context)


def render_lines(template_name: str, **context: object) -> list[str]:
    return render(template_name, **context).split("\n")

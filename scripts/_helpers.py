"""Shared helpers for the scripts/ entrypoints."""

import sys


def fail(msg):
    """Print a bold-red FAILED banner to stderr.

    ANSI escapes are gated on stderr.isatty() so redirected logs stay clean.
    """
    banner = f"FAILED: {msg}"
    if sys.stderr.isatty():
        sys.stderr.write(f"\n\033[1;31m{banner}\033[0m\n")
    else:
        sys.stderr.write(f"\n{banner}\n")

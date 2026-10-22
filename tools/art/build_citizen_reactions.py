"""Compatibility entry point for library-only citizen reaction generation.

The former synthetic arm/recovery pose authoring has been retired.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_citizen_library_actions import build

if __name__ == '__main__':
    build(behavior=False)

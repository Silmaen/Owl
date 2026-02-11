from logging import getLogger
from pathlib import Path

root = Path(__file__).resolve().parent.parent
log = getLogger(__name__)

import sys

# add the source root to the sys.path for easier imports
sys.path.insert(0, str(root))

from ci.utils.logging import setup_logging

setup_logging()

import sys
from _chx import *
import _chx

class Writer:
    encoding: str = "utf-8"

    def write(self, text: str) -> int:
        if text:
            _chx._notify_by_stream(text)
        return len(text)

    def flush(self) -> None:
        pass

    def isatty(self) -> bool:
        return False

    def writable(self) -> bool:
        return True

tty = sys.stdout

sys.stdout = Writer()
sys.stderr = Writer()

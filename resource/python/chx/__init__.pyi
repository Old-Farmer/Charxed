# -----------------------------------------
# public APIs

def notify(text: str) -> int:
    """
    Notify user.
    thread safe
    """
    ...

# -----------------------------------------
# private APIs

def _notify_by_stream(text: str) -> int:
    ...

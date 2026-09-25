# Scripting

Users can use Python to extend Charxed. Current Python version is 3.14.7 with GIL.

## NOTE

Scripting functionaliy is at an early stage.

## IO Hijack

`sys.stdout` and `sys.stderr` are redirected to the Charxed peel panel.

If you really want to output to tty, there is a `chx.tty` to use.

## API

All APIs are in `chx` package. Most APIs are not thread safe, only some like `chx.notify` is thread safe and will be marked as thread safe.

See [.pyi file](../resource/python/chx/__init__.pyi) for all APIs.

# Troubleshooting

This document records some ways of how users can do troubleshooting when they meet some problems.

## Logging

The path of logging file is `%XDG_STATE_HOME/charxed/charxed.log`.

## Backtrace

If you meet a crash, send the backtrace to us to report the bug. E.g. use gdb:

```bash
gdb -q <chx-path> <core-path> \
  -ex "set pagination off" \
  -ex "thread apply all bt full" \
  -ex "quit" \
  > chx-backtrace.txt
```

In order to let OS be able to generate cores, you would like to first set core resource limit to a valid state.

```bash
ulimit -c unlimited
```

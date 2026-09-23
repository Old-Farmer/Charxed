# Debug

You can use whatever tools to debug code. Here is one example.

```bash
# find chx process
ps aux | grep chx

# attach a chx process
gdb -p <pid>
# or just gdb
gdb
# then attach a process
(gdb) attach <pid>

# make the prgm run
(gdb) continue

# detach if don't want monitor the process
(gdb) detach
```

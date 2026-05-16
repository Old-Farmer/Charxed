# Keymaps

A keymap is a key sequence which can trigger some operations. Sometime keymaps are called Shortcuts in other editors.

NOTE: Don't support self-defined keymaps now.

## Notation

- `<c-...>` means Ctrl + ...
- `<a-...>` means Alt + ...
- `<enter>` means Enter key
- `<esc>` means Escape key
- `<bs>` means Backspace key
- `<home>` means Home key
- `<end>` means End key
- `<left>` means Left arrow key
- `<right>` means Right arrow key

## Mode Reference

- **Normal**: Normal editing mode, like Vim's Normal mode
- **Insert**: Insert mode, for typing text
- **Select**: Character-wise selection mode
- **Select-L**: Line-wise selection mode
- **Op-Pend**: Operator pending mode
- **Command**: Command input mode (`:` prompt)
- **Search**: Search input mode (`/` or `?` prompt)
- **Show**: Peel show mode (multirow output display)

---

## General

| Key | Description | Mode(s) |
| --- | --- | --- |
| `<esc>` / `<c-[>` | Exit from current mode / close Peel | All modes |

## Navigation

| Key | Description | Mode(s) |
| --- | --- | --- |
| `h` | Move cursor left | Normal, Select, Select-L, Show |
| `l` | Move cursor right | Normal, Select, Select-L, Show |
| `k` | Move cursor up | Normal, Select, Select-L, Show |
| `j` | Move cursor down | Normal, Select, Select-L, Show |
| `b` | Move to beginning of word | Normal, Select, Select-L, Show |
| `e` | Move to end of word | Normal, Select, Select-L, Show |
| `w` | Move to beginning of next word | Normal, Select, Select-L |
| `0` | Move to beginning of line | Normal, Select, Select-L, Show |
| `$` | Move to end of line | Normal, Select, Select-L, Show |
| `<c-f>` | Move down one page | Normal, Select, Select-L, Show |
| `<c-b>` | Move up one page | Normal, Select, Select-L, Show |
| `<c-d>` | Move down half page | Normal, Select, Select-L, Show |
| `<c-u>` | Move up half page | Normal, Select, Select-L, Show |
| `gg` | Move to beginning of file | Normal, Select, Select-L |
| `G` | Move to end of file (or go to line {count}) | Normal, Select, Select-L |
| `gf` | Go to file at cursor | Normal, Select, Select-L |
| `<c-o>` | Jump to previous cursor position | Normal |
| `<c-i>` | Jump to next cursor position | Normal |
| `]b` | Go to next buffer | Normal |
| `[b` | Go to previous buffer | Normal |

## Selection

| Key | Description | Mode(s) |
| --- | --- | --- |
| `s` | Start line-wise selection | Normal |
| `S` | Start character-wise selection | Normal |

## Edit Operations

| Key | Description | Mode(s) |
| --- | --- | --- |
| `y` | Copy selection / prepare yank operation | Select, Select-L, Normal |
| `d` | Cut selection / prepare delete operation | Select, Select-L, Normal |
| `p` | Paste | Normal |
| `u` | Undo | Normal |
| `<c-r>` | Redo | Normal |
| `i` | Enter insert mode at cursor | Normal |
| `I` | Enter insert mode at first non-blank character | Normal |
| `a` | Enter insert mode after cursor | Normal |
| `A` | Enter insert mode at end of line | Normal |
| `o` | Create new line below and enter insert mode | Normal |
| `O` | Create new line above and enter insert mode | Normal |
| `<enter>` | Add newline | Insert |
| `<bs>` | Delete character before cursor | Insert |
| `<c-w>` | Delete word before cursor | Insert |

## Completion & History

| Key | Description | Mode(s) |
| --- | --- | --- |
| `<c-space>` | Trigger completion | Insert, Command |
| `<c-c>` | Trigger completion | Insert, Command |
| `<tab>` | Accept completion or insert tab | Insert, Command |
| `<c-n>` | Select next completion | Insert, Command |
| `<c-n>` | Select next history | Command, Search |
| `<c-p>` | Select prev completion | Insert, Command |
| `<c-p>` | Select prev history | Command, Search |

## Search & Command

| Key | Description | Mode(s) |
| --- | --- | --- |
| `/` | Start forward search | Normal |
| `?` | Start backward search | Normal |
| `n` | Go to next search match | Normal |
| `N` | Go to previous search match | Normal |
| `:` | Enter command mode | Normal |
| `<enter>` | Open Peel show mode | Normal |

## Peel Input (Command & Search)

| Key | Description | Mode(s) |
| --- | --- | --- |
| `<left>` | Move cursor left | Command, Search |
| `<right>` | Move cursor right | Command, Search |
| `<c-left>` | Move to previous word | Command, Search |
| `<c-right>` | Move to next word | Command, Search |
| `<home>` | Move to beginning | Command, Search |
| `<end>` | Move to end | Command, Search |
| `<bs>` | Delete character before cursor | Command, Search |
| `<c-w>` | Delete word before cursor | Command, Search |
| `<c-r>"` | Paste from clipboard | Command, Search |
| `<c-n>` | Next history item | Command, Search |
| `<c-p>` | Previous history item | Command, Search |
| `<enter>` | Execute command or search | Command, Search |



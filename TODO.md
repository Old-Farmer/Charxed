# TODO

## UI

- syntax highlight on more language(bash, go, java, markdown, markdown_inline, rust, etc.)
- Soft line break
- Multi-window
- Bufferline(low priority)
- Cursor color
- Show pending keys of keymaps

## LSP

- Json RPC protocal
- Lsp Client

## Modal Editing

- Multi cursor mode

## Usability

- Replace
- Options need a range limitaion
- Jump history should be adjusted when buffer changed.
- diff algorithm and diff view
- Better auto indent per laguages
- SIGTSTP & SIGCONT support
- Auto saving
- MacOS support(high priority)
- Windows support(high priority, especially for termbox2)
- Low lantency remote development

## Extensibiliy

- JS engine embbeded
- Plugin system
- API Design
    - Buffer API
    - Cursor API
    - UI API: Window
    - Option API
    - Keymap API
    - Command API

## Performance

- Big file support:
    - treesitter background thread parsing
    - File background thread saving

## Code Quality

- better clang-tidy

## Cross Platform

- Better CMake
    - Linux aarch64


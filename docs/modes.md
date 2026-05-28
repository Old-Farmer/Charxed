# Modes

There several modes in charxed:

- Normal: Do navigation and editing with semantic keymaps.
- Operator Pending: Waiting for motions or text object to complete an operator.
- Insert: Be able to input characters.
- Select/Select Line: Selection enabled.
- Command: Input and execute command.
- Search: Input searching pattern and search.
- Show: Navigate in multi-row Mango Peel.

# Context

Context represents the current **active panel or workspace**, determining which component handles key events.

## Available Contexts

- **Editor**: The text editor is active. Operations like cursor movement, text insertion, and editing commands apply to the currently open buffer.
- **Explorer**: The file explorer is active. Operations like cursor navigation and directory expansion/collapse apply to the file tree.

## Mode vs Context

- **Mode** controls *how* you interact (insert characters, run commands, navigate, etc.)
- **Context** controls *where* you interact (which panel receives the input)

Both work together:
- In Editor context + Normal mode: Navigation keys move the cursor in the editor
- In Explorer context + Normal mode: Navigation keys move the selection in the file tree
- In Editor context + Insert mode: Keys insert characters into the buffer
- In Explorer context + Insert mode: Not typically used (explorers don't insert text)

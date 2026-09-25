# kloc

**kloc** (*Kilo Line of Code*) is a lightweight, zero-dependency, terminal-based text editor written in modern C++ (C++17). It is a conceptual rewrite of Salvatore Sanfilippo's [`kilo`](https://github.com/antirez/kilo) text editor, re-engineered under a strict **1,000 Lines of Code (LOC)** constraint and outfitted with a user interface inspired by `nano`.

## Features

- **Zero External Dependencies:** Built strictly with POSIX ANSI escape sequences and `<termios.h>`. No `ncurses` or external UI framework required.
- **Nano-Inspired Interface:** Divided screen layout featuring an inverted header status bar and a footer keybinding guide.
- **Modern C++ Architecture:** Uses modern C++ features (`std::vector`, `std::string_view`, `std::optional`, Smart Memory Management) while maintaining deterministic performance and safety.
- **Bash Syntax Highlighting:** Integrated lexer supporting single-line comments (`#`), double/single-quoted strings, single and braced Bash variables (`$VAR`, `${VAR}`), numbers, and shell keywords.
- **Forward Search:** In-buffer search functionality with real-time highlighting and directional arrow navigation.
- **Safety Checks:** Exit confirmation prompts when attempting to quit with unsaved modifications.

### Keybindings

| Keybinding | Action | Description |
| :--- | :--- | :--- |
| `Ctrl + S` | **Save** | Writes the current text buffer to disk. Prompts for a filename if untitled. |
| `Ctrl + Q` | **Quit** | Exits the editor. Prompts `(Y)es / (N)o / (C)ancel` if unsaved changes exist. |
| `Ctrl + F` | **Find** | Initiates forward string search. Use `Left`/`Up` and `Right`/`Down` arrows to step matches. |
| `Ctrl + H` | **Help** | Displays short contextual keybinding information in the footer bar. |
| `Arrows` / `Home` / `End` | **Navigate** | Moves the cursor within the buffer with auto-scrolling capabilities. |
| `PgUp` / `PgDn` | **Scroll** | Scrolls up or down by a full terminal screen height. |

---

## Build Requirements

- C++17 compatible compiler (`g++` >= 7.0 or `clang++` >= 5.0)
- POSIX-compliant operating system (Linux, macOS, BSD)
- `make` build utility

## Compilation & Installation

Clone the repository and build using `make`:

```bash
git clone https://github.com/haithamaouati/kloc.git
cd kloc
make
```

To install system-wide (optional):
```
sudo cp kloc /usr/local/bin/
```
## Usage
​Launch kloc with an optional filename argument:

#### Open an empty buffer
```
./kloc
```

#### Open or create a file
```
./kloc script.sh
```

## License
​This project is licensed under the [MIT License](LICENSE). See the [LICENSE]() file for details. Inspired by the original [kilo]() editor by Salvatore Sanfilippo.

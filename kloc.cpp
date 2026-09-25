#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <optional>
#include <unordered_set>
#include <cctype>
#include <csignal>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

constexpr const char* KLOC_VERSION = "1.0.0";

// Syntax highlighting tokens
enum class Highlight : uint8_t {
    Normal = 0,
    Comment,
    Keyword,
    String,
    Variable,
    Number,
    Match
};

struct Row {
    std::string chars;
    std::string render;
    std::vector<Highlight> hl;
};

// Control key and terminal action mapping
enum class Key {
    None = 0,
    CtrlC = 3,
    CtrlF = 6,
    CtrlH = 8,
    Tab = 9,
    Enter = 13,
    CtrlQ = 17,
    CtrlS = 19,
    Esc = 27,
    Backspace = 127,
    ArrowLeft = 1000,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    DelKey,
    HomeKey,
    EndKey,
    PageUp,
    PageDown
};

const char* highlightToAnsi(Highlight hl) {
    switch (hl) {
        case Highlight::Comment:  return "\x1b[36m";   // Cyan
        case Highlight::Keyword:  return "\x1b[1;33m"; // Bold Yellow
        case Highlight::String:   return "\x1b[32m";   // Green
        case Highlight::Variable: return "\x1b[35m";   // Magenta
        case Highlight::Number:   return "\x1b[31m";   // Red
        case Highlight::Match:    return "\x1b[7m";    // Reverse Video
        default:                  return "\x1b[39m";   // Default Foreground
    }
}

// Bash Syntax Highlighting Scanner
void updateSyntax(Row& row) {
    row.hl.assign(row.render.size(), Highlight::Normal);
    const std::string& s = row.render;
    size_t n = s.size();
    if (n == 0) return;

    bool in_sq = false;
    bool in_dq = false;

    auto is_sep = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) || c == '\0' ||
               std::string_view("()[]{}<>;:=+*-/&|!,\"").find(c) != std::string_view::npos;
    };

    static const std::unordered_set<std::string_view> keywords = {
        "if", "then", "else", "elif", "fi", "case", "esac", "for", "select",
        "while", "until", "do", "done", "in", "function", "return", "exit",
        "local", "export", "set", "unset", "shift", "readonly", "trap",
        "echo", "printf", "alias", "eval", "exec", "source"
    };

    size_t i = 0;
    bool prev_sep = true;

    while (i < n) {
        char c = s[i];

        if (in_sq) {
            row.hl[i] = Highlight::String;
            if (c == '\'') in_sq = false;
            i++;
            prev_sep = false;
            continue;
        }

        if (in_dq) {
            row.hl[i] = Highlight::String;
            if (c == '\\' && i + 1 < n) {
                row.hl[i + 1] = Highlight::String;
                i += 2;
                continue;
            }
            if (c == '"') {
                in_dq = false;
            } else if (c == '$') {
                size_t var_start = i;
                i++;
                if (i < n && (s[i] == '{' || s[i] == '(')) {
                    char close_c = (s[i] == '{') ? '}' : ')';
                    i++;
                    while (i < n && s[i] != close_c) i++;
                    if (i < n) i++;
                } else if (i < n && (std::isdigit(static_cast<unsigned char>(s[i])) ||
                           s[i] == '?' || s[i] == '$' || s[i] == '#' || s[i] == '@' || s[i] == '*')) {
                    i++;
                } else {
                    while (i < n && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) i++;
                }
                for (size_t k = var_start; k < i && k < n; ++k) row.hl[k] = Highlight::Variable;
                continue;
            }
            i++;
            prev_sep = false;
            continue;
        }

        // Single line comment starting with '#'
        if (c == '#' && prev_sep) {
            for (size_t k = i; k < n; ++k) row.hl[k] = Highlight::Comment;
            break;
        }

        if (c == '\'') {
            in_sq = true;
            row.hl[i] = Highlight::String;
            i++;
            prev_sep = false;
            continue;
        }

        if (c == '"') {
            in_dq = true;
            row.hl[i] = Highlight::String;
            i++;
            prev_sep = false;
            continue;
        }

        // Bash variables ($VAR, ${VAR}, $1, etc.)
        if (c == '$') {
            size_t var_start = i;
            i++;
            if (i < n && (s[i] == '{' || s[i] == '(')) {
                char close_c = (s[i] == '{') ? '}' : ')';
                i++;
                while (i < n && s[i] != close_c) i++;
                if (i < n) i++;
            } else if (i < n && (std::isdigit(static_cast<unsigned char>(s[i])) ||
                       s[i] == '?' || s[i] == '$' || s[i] == '#' || s[i] == '@' || s[i] == '*')) {
                i++;
            } else {
                while (i < n && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) i++;
            }
            for (size_t k = var_start; k < i && k < n; ++k) row.hl[k] = Highlight::Variable;
            prev_sep = false;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) && prev_sep) {
            while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
                row.hl[i] = Highlight::Number;
                i++;
            }
            prev_sep = false;
            continue;
        }

        if (prev_sep) {
            size_t len = 0;
            while (i + len < n && !is_sep(s[i + len])) len++;
            std::string_view word(&s[i], len);
            if (keywords.find(word) != keywords.end()) {
                for (size_t k = 0; k < len; ++k) row.hl[i + k] = Highlight::Keyword;
                i += len;
                prev_sep = false;
                continue;
            }
        }

        prev_sep = is_sep(c);
        i++;
    }
}

void updateRow(Row& row) {
    row.render.clear();
    for (char c : row.chars) {
        if (c == '\t') {
            row.render.append(4 - (row.render.size() % 4), ' ');
        } else {
            row.render.push_back(c);
        }
    }
    updateSyntax(row);
}

int rxFromCx(const Row& row, int cx) {
    int rx = 0;
    for (int j = 0; j < cx && j < static_cast<int>(row.chars.size()); j++) {
        if (row.chars[j] == '\t') rx += 4 - (rx % 4);
        else rx++;
    }
    return rx;
}

class Editor {
public:
    static Editor* instance;

    int cx = 0, cy = 0;
    int rx = 0;
    int rowoff = 0, coloff = 0;
    int screenrows = 24, screencols = 80;
    std::vector<Row> rows;
    bool dirty = false;
    std::string filename;
    std::string status_msg;
    termios orig_termios{};
    bool raw_mode = false;

    Editor() { instance = this; }
    ~Editor() { disableRawMode(); }

    void enableRawMode() {
        if (raw_mode) return;
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
        termios raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return;
        raw_mode = true;
    }

    void disableRawMode() {
        if (raw_mode) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            raw_mode = false;
        }
    }

    void updateWindowSize() {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0) {
            screencols = ws.ws_col;
            screenrows = ws.ws_row;
        }
    }

    int readKey() {
        int nread;
        char c;
        while ((nread = read(STDIN_FILENO, &c, 1)) == 0) {}
        if (nread == -1) return static_cast<int>(Key::None);

        if (c == '\x1b') {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return static_cast<int>(Key::Esc);
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return static_cast<int>(Key::Esc);

            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    if (read(STDIN_FILENO, &seq[2], 1) != 1) return static_cast<int>(Key::Esc);
                    if (seq[2] == '~') {
                        switch (seq[1]) {
                            case '1': case '7': return static_cast<int>(Key::HomeKey);
                            case '3': return static_cast<int>(Key::DelKey);
                            case '4': case '8': return static_cast<int>(Key::EndKey);
                            case '5': return static_cast<int>(Key::PageUp);
                            case '6': return static_cast<int>(Key::PageDown);
                        }
                    }
                } else {
                    switch (seq[1]) {
                        case 'A': return static_cast<int>(Key::ArrowUp);
                        case 'B': return static_cast<int>(Key::ArrowDown);
                        case 'C': return static_cast<int>(Key::ArrowRight);
                        case 'D': return static_cast<int>(Key::ArrowLeft);
                        case 'H': return static_cast<int>(Key::HomeKey);
                        case 'F': return static_cast<int>(Key::EndKey);
                    }
                }
            } else if (seq[0] == 'O') {
                switch (seq[1]) {
                    case 'H': return static_cast<int>(Key::HomeKey);
                    case 'F': return static_cast<int>(Key::EndKey);
                }
            }
            return static_cast<int>(Key::Esc);
        }
        return static_cast<unsigned char>(c);
    }

    void insertChar(char c) {
        int filerow = cy + rowoff;
        if (filerow >= static_cast<int>(rows.size())) {
            while (static_cast<int>(rows.size()) <= filerow) {
                rows.push_back(Row{});
            }
        }
        Row& row = rows[filerow];
        if (cx < 0 || cx > static_cast<int>(row.chars.size())) {
            cx = row.chars.size();
        }
        row.chars.insert(cx, 1, c);
        updateRow(row);
        cx++;
        dirty = true;
    }

    void insertNewline() {
        int filerow = cy + rowoff;
        if (filerow >= static_cast<int>(rows.size())) {
            if (filerow == static_cast<int>(rows.size())) {
                rows.push_back(Row{});
            }
        } else {
            Row& row = rows[filerow];
            if (cx > static_cast<int>(row.chars.size())) cx = row.chars.size();
            std::string second_half = row.chars.substr(cx);
            row.chars.erase(cx);
            updateRow(row);

            Row new_row;
            new_row.chars = std::move(second_half);
            updateRow(new_row);
            rows.insert(rows.begin() + filerow + 1, std::move(new_row));
        }
        cy++;
        cx = 0;
        dirty = true;
    }

    void delChar() {
        int filerow = cy + rowoff;
        if (filerow >= static_cast<int>(rows.size())) return;
        Row& row = rows[filerow];

        if (cx > 0) {
            row.chars.erase(cx - 1, 1);
            updateRow(row);
            cx--;
            dirty = true;
        } else if (cx == 0 && filerow > 0) {
            cx = rows[filerow - 1].chars.size();
            rows[filerow - 1].chars += row.chars;
            updateRow(rows[filerow - 1]);
            rows.erase(rows.begin() + filerow);
            if (cy > 0) cy--;
            else if (rowoff > 0) rowoff--;
            dirty = true;
        }
    }

    void moveCursor(int key) {
        int text_rows = screenrows - 2;
        int filerow = cy + rowoff;
        int rowlen = (filerow < static_cast<int>(rows.size())) ? rows[filerow].chars.size() : 0;

        switch (static_cast<Key>(key)) {
            case Key::ArrowLeft:
                if (cx > 0) {
                    cx--;
                } else if (filerow > 0) {
                    cy--;
                    cx = rows[filerow - 1].chars.size();
                }
                break;
            case Key::ArrowRight:
                if (filerow < static_cast<int>(rows.size())) {
                    if (cx < rowlen) {
                        cx++;
                    } else if (cx == rowlen) {
                        cy++;
                        cx = 0;
                    }
                }
                break;
            case Key::ArrowUp:
                if (cy > 0) cy--;
                else if (rowoff > 0) rowoff--;
                break;
            case Key::ArrowDown:
                if (filerow < static_cast<int>(rows.size())) {
                    cy++;
                }
                break;
            case Key::HomeKey:
                cx = 0;
                break;
            case Key::EndKey:
                if (filerow < static_cast<int>(rows.size())) {
                    cx = rows[filerow].chars.size();
                }
                break;
            case Key::PageUp:
                cy = 0;
                rowoff = std::max(0, rowoff - text_rows);
                break;
            case Key::PageDown:
                cy = text_rows - 1;
                rowoff = std::min(static_cast<int>(rows.size()), rowoff + text_rows);
                break;
            default:
                break;
        }

        filerow = cy + rowoff;
        rowlen = (filerow < static_cast<int>(rows.size())) ? rows[filerow].chars.size() : 0;
        if (cx > rowlen) cx = rowlen;
    }

    void scroll() {
        int text_rows = screenrows - 2;
        int filerow = cy + rowoff;

        if (filerow < static_cast<int>(rows.size())) {
            rx = rxFromCx(rows[filerow], cx);
        } else {
            rx = 0;
        }

        if (cy < 0) {
            rowoff += cy;
            cy = 0;
        }
        if (cy >= text_rows) {
            rowoff += (cy - text_rows + 1);
            cy = text_rows - 1;
        }
        if (rx < coloff) {
            coloff = rx;
        }
        if (rx >= coloff + screencols) {
            coloff = rx - screencols + 1;
        }
    }

    void refreshScreen() {
        scroll();

        std::string ab;
        ab.reserve(4096);

        ab += "\x1b[?25l"; // Hide cursor
        ab += "\x1b[H";    // Go home

        int text_rows = screenrows - 2;

        // 1. Top Status Bar (Inverted header)
        ab += "\x1b[7m";
        std::string left_title = " kloc " + std::string(KLOC_VERSION);
        std::string fname_str = filename.empty() ? "[No Name]" : filename;
        std::string center_title = fname_str + (dirty ? " [Modified]" : "");

        std::string header(screencols, ' ');
        for (size_t i = 0; i < left_title.size() && i < header.size(); ++i) {
            header[i] = left_title[i];
        }
        if (screencols > static_cast<int>(center_title.size())) {
            size_t start = (screencols - center_title.size()) / 2;
            if (start > left_title.size()) {
                for (size_t i = 0; i < center_title.size(); ++i) {
                    header[start + i] = center_title[i];
                }
            }
        }
        ab += header;
        ab += "\x1b[m\r\n";

        // 2. Main Text Buffer Area
        for (int y = 0; y < text_rows; ++y) {
            int filerow = y + rowoff;
            if (filerow >= static_cast<int>(rows.size())) {
                if (rows.empty() && y == text_rows / 3) {
                    std::string welcome = "kloc editor -- version " + std::string(KLOC_VERSION);
                    size_t padding = (screencols > static_cast<int>(welcome.size())) ? (screencols - welcome.size()) / 2 : 0;
                    if (padding) { ab += "~"; padding--; }
                    while (padding--) ab += " ";
                    ab += welcome.substr(0, screencols);
                } else {
                    ab += "~";
                }
            } else {
                const Row& r = rows[filerow];
                int len = static_cast<int>(r.render.size()) - coloff;
                if (len < 0) len = 0;
                if (len > screencols) len = screencols;

                Highlight curr_hl = Highlight::Normal;
                for (int j = 0; j < len; ++j) {
                    int char_idx = coloff + j;
                    Highlight hl = r.hl[char_idx];
                    if (hl != curr_hl) {
                        curr_hl = hl;
                        ab += highlightToAnsi(hl);
                    }
                    ab += r.render[char_idx];
                }
                if (curr_hl != Highlight::Normal) {
                    ab += "\x1b[39m\x1b[m";
                }
            }
            ab += "\x1b[0K\r\n";
        }

        // 3. Bottom Command Bar (Footer)
        ab += "\x1b[0K";
        if (!status_msg.empty()) {
            ab += status_msg.substr(0, screencols);
        } else {
            ab += "\x1b[7m^Q\x1b[m Quit  \x1b[7m^S\x1b[m Save  \x1b[7m^F\x1b[m Find  \x1b[7m^H\x1b[m Help";
        }

        // 4. Cursor repositioning
        int term_row = cy + 2;
        int term_col = (rx - coloff) + 1;
        ab += "\x1b[" + std::to_string(term_row) + ";" + std::to_string(term_col) + "H";
        ab += "\x1b[?25h"; // Show cursor

        write(STDOUT_FILENO, ab.data(), ab.size());
    }

    std::optional<std::string> prompt(const std::string& prefix) {
        std::string buf;
        while (true) {
            status_msg = prefix + buf;
            refreshScreen();

            int c = readKey();
            if (c == static_cast<int>(Key::DelKey) || c == static_cast<int>(Key::Backspace) || c == 8) {
                if (!buf.empty()) buf.pop_back();
            } else if (c == static_cast<int>(Key::Esc)) {
                status_msg.clear();
                refreshScreen();
                return std::nullopt;
            } else if (c == static_cast<int>(Key::Enter)) {
                status_msg.clear();
                refreshScreen();
                return buf;
            } else if (!iscntrl(c) && c < 128) {
                buf.push_back(static_cast<char>(c));
            }
        }
    }

    bool save() {
        if (filename.empty()) {
            auto res = prompt("File Name to Save: ");
            if (!res.has_value() || res->empty()) {
                status_msg = "[ Save Canceled ]";
                return false;
            }
            filename = *res;
        }

        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open()) {
            status_msg = "[ Can't save file! I/O error ]";
            return false;
        }

        size_t total_bytes = 0;
        for (const auto& r : rows) {
            out << r.chars << "\n";
            total_bytes += r.chars.size() + 1;
        }

        out.close();
        dirty = false;
        status_msg = "[ Saved " + std::to_string(total_bytes) + " bytes to " + filename + " ]";
        return true;
    }

    void find() {
        int saved_cx = cx, saved_cy = cy;
        int saved_coloff = coloff, saved_rowoff = rowoff;

        int last_match = -1;
        int direction = 1;
        std::string query;

        auto search = [&](int key) {
            if (key == static_cast<int>(Key::ArrowRight) || key == static_cast<int>(Key::ArrowDown)) direction = 1;
            else if (key == static_cast<int>(Key::ArrowLeft) || key == static_cast<int>(Key::ArrowUp)) direction = -1;
            else { last_match = -1; direction = 1; }

            if (query.empty()) return;
            if (last_match == -1) direction = 1;

            int current = last_match;
            for (size_t i = 0; i < rows.size(); ++i) {
                current += direction;
                if (current == -1) current = static_cast<int>(rows.size()) - 1;
                else if (current == static_cast<int>(rows.size())) current = 0;

                size_t pos = rows[current].render.find(query);
                if (pos != std::string::npos) {
                    last_match = current;
                    rowoff = current;
                    cy = 0;

                    int char_pos = 0;
                    int r_idx = 0;
                    for (char c : rows[current].chars) {
                        if (r_idx >= static_cast<int>(pos)) break;
                        r_idx += (c == '\t') ? (4 - (r_idx % 4)) : 1;
                        char_pos++;
                    }
                    cx = char_pos;
                    coloff = 0;

                    for (size_t k = pos; k < pos + query.size() && k < rows[current].hl.size(); ++k) {
                        rows[current].hl[k] = Highlight::Match;
                    }
                    break;
                }
            }
        };

        while (true) {
            status_msg = "Search: " + query + " (ESC: Cancel, Enter: Accept, Arrows: Next/Prev)";
            refreshScreen();

            int c = readKey();
            if (c == static_cast<int>(Key::DelKey) || c == static_cast<int>(Key::Backspace) || c == 8) {
                if (!query.empty()) query.pop_back();
                for (auto& r : rows) updateSyntax(r);
                search(c);
            } else if (c == static_cast<int>(Key::Esc)) {
                cx = saved_cx; cy = saved_cy;
                coloff = saved_coloff; rowoff = saved_rowoff;
                for (auto& r : rows) updateSyntax(r);
                status_msg.clear();
                refreshScreen();
                return;
            } else if (c == static_cast<int>(Key::Enter)) {
                for (auto& r : rows) updateSyntax(r);
                status_msg.clear();
                refreshScreen();
                return;
            } else if (c == static_cast<int>(Key::ArrowDown) || c == static_cast<int>(Key::ArrowRight) ||
                       c == static_cast<int>(Key::ArrowUp) || c == static_cast<int>(Key::ArrowLeft)) {
                for (auto& r : rows) updateSyntax(r);
                search(c);
            } else if (!iscntrl(c) && c < 128) {
                query.push_back(static_cast<char>(c));
                for (auto& r : rows) updateSyntax(r);
                search(c);
            }
        }
    }

    void processQuit() {
        if (!dirty) {
            disableRawMode();
            write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
            exit(0);
        }

        while (true) {
            status_msg = "Save modified buffer? (Y)es / (N)o / (C)ancel";
            refreshScreen();

            int c = readKey();
            if (c == 'y' || c == 'Y') {
                if (save()) {
                    disableRawMode();
                    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
                    exit(0);
                } else {
                    break;
                }
            } else if (c == 'n' || c == 'N') {
                disableRawMode();
                write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
                exit(0);
            } else if (c == 'c' || c == 'C' || c == static_cast<int>(Key::Esc)) {
                status_msg.clear();
                refreshScreen();
                break;
            }
        }
    }

    void processKeypress() {
        int c = readKey();
        switch (static_cast<Key>(c)) {
            case Key::Enter:
                insertNewline();
                break;
            case Key::CtrlQ:
                processQuit();
                break;
            case Key::CtrlS:
                save();
                break;
            case Key::CtrlF:
                find();
                break;
            case Key::CtrlH:
                status_msg = "Help: ^Q Quit | ^S Save | ^F Find | ^H Help | Arrows: Navigate";
                break;
            case Key::Backspace:
            case Key::DelKey:
                delChar();
                break;
            case Key::ArrowUp:
            case Key::ArrowDown:
            case Key::ArrowLeft:
            case Key::ArrowRight:
            case Key::HomeKey:
            case Key::EndKey:
            case Key::PageUp:
            case Key::PageDown:
                moveCursor(c);
                break;
            default:
                if (!iscntrl(c) && c < 256) {
                    insertChar(static_cast<char>(c));
                }
                break;
        }
    }

    void openFile(const std::string& path) {
        filename = path;
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            Row r;
            r.chars = std::move(line);
            updateRow(r);
            rows.push_back(std::move(r));
        }
        dirty = false;
    }
};

Editor* Editor::instance = nullptr;

void handleSigWinCh(int) {
    if (Editor::instance) {
        Editor::instance->updateWindowSize();
        Editor::instance->refreshScreen();
    }
}

int main(int argc, char** argv) {
    Editor editor;
    editor.updateWindowSize();
    signal(SIGWINCH, handleSigWinCh);

    if (argc >= 2) {
        editor.openFile(argv[1]);
    }

    editor.enableRawMode();
    // Replicate nano visual startup screen initialization
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);

    while (true) {
        editor.refreshScreen();
        editor.processKeypress();
    }
    return 0;
}

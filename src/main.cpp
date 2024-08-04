#include <algorithm>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ncurses.h>
#include <unistd.h>
#include <vector>

#define KEY_ESC 27
#define REG_COLOR_NUM 1
#define CUS_COLOR_NUM 1

enum EditorMode { NORMAL, INSERT, COMMAND };
enum WrapMode { BREAK, SCROLL };

class MiniVim {
public:
  MiniVim(const std::string &filename, bool truncate, bool readOnly,
          WrapMode wrapMode)
      : filename(filename), truncate(truncate), readOnly(readOnly),
        wrapMode(wrapMode) {
    initscr();                    // Start ncurses mode
    raw();                        // Line buffering disabled
    keypad(stdscr, true);         // We get F1, F2, etc.
    noecho();                     // Don't echo while we do getch
    curs_set(1);                  // Make cursor visible
    getmaxyx(stdscr, rows, cols); // Get screen size

    if (rows <= 3) {
      fprintf(stderr, "Window is too small to display the content");
      endwin();
      exit(1);
    }

    // init color
    // start_color();
    // init_pair(CUS_COLOR_NUM, COLOR_YELLOW, COLOR_CYAN);

    wrefresh(stdscr);

    fileWindow = newwin(rows - 2, cols, 0, 0);
    // wbkgd(fileWindow, CUS_COLOR_NUM);
    wrefresh(fileWindow);

    infoWindow = newwin(1, cols, rows - 2, 0);
    // wbkgd(infoWindow, CUS_COLOR_NUM);
    wrefresh(infoWindow);

    commandWindow = newwin(1, cols, rows - 1, 0);
    wrefresh(commandWindow);

    cursorY = cursorX = 0;
    editorMode = NORMAL;

    loadFile();
  }

  ~MiniVim() {
    endwin(); // End ncurses mode
  }

  void run() {
    refreshScreen();

    int ch;
    while (ch = wgetch(stdscr)) {
      warnMessage = "";
      switch (editorMode) {
      case NORMAL:
        handleNormalEditorMode(ch);
        break;
      case INSERT:
        handleInsertEditorMode(ch);
        break;
      case COMMAND:
        handleCommandEditorMode(ch);
        break;
      default:
        break;
      }
      autoScroll();
      refreshScreen();
    }
  }

private:
  WINDOW *fileWindow;
  WINDOW *infoWindow;
  WINDOW *commandWindow;

  EditorMode editorMode;

  std::string filename;
  bool truncate;
  bool readOnly;
  bool isNewFile;
  bool modified;
  WrapMode wrapMode;

  int cursorY, cursorX;
  int rows, cols;
  int startLine;

  char kbCache = '\0';

  std::string warnMessage;

  std::vector<std::string> buffer;
  std::string commandBuffer;

  void loadFile() {
    if (truncate) {
      buffer.push_back(""); // Start with an empty buffer if truncating
      return;
    }

    std::ifstream file(filename);
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        buffer.push_back(line);
      }
      file.close();
      isNewFile = false;
    } else {
      buffer.push_back("");
      isNewFile = true;
      // Initialize with one empty line if file cannot be opened
    }

    modified = false;
  }

  void saveFile() {
    std::ofstream file(filename);
    if (file.is_open()) {
      for (const auto &line : buffer) {
        file << line << std::endl;
      }
      file.close();
      modified = false;
    } else {
      warnMessage = "Failed to save file!";
    }
  }

  void handleNormalEditorMode(int ch) {
    switch (ch) {
    case 'i':
      editorMode = INSERT;
      break;

    case ':':
      editorMode = COMMAND;
      commandBuffer.clear();
      break;

    case KEY_UP:
      if (cursorY + startLine > 0) {
        --cursorY;
        cursorX =
            std::min(cursorX, std::max(0, int(buffer[cursorY + startLine].length() - 1)));
      }
      break;

    case KEY_DOWN:
      if (cursorY + startLine < buffer.size() - 1) {
        ++cursorY;
        cursorX =
            std::min(cursorX, std::max(0, int(buffer[cursorY + startLine].length() - 1)));
      }
      break;

    case KEY_LEFT:
      if (cursorX > 0) {
        --cursorX;
      } else if (cursorY + startLine > 0) {
        --cursorY;
        cursorX = buffer[cursorY + startLine].length() - 1;
      }
      break;

    case 'b':
      while (cursorX > 0 && buffer[cursorY + startLine][--cursorX] == ' ')
        ;
      if (cursorX == 0 && cursorY + startLine > 0) {
        --cursorY;
        cursorX = buffer[cursorY + startLine].length() - 1;
      }
      break;

    case KEY_RIGHT:
      if (cursorX < buffer[cursorY + startLine].length()) {
        ++cursorX;
      } else if (cursorY + startLine < buffer.size() - 1) {
        ++cursorY;
        cursorX = 0;
      }
      break;

    case 'w':
      while (cursorX < buffer[cursorY + startLine].length() - 1 &&
             buffer[cursorY + startLine][++cursorX] == ' ')
        ;
      if (cursorX == buffer[cursorY + startLine].length() - 1 &&
          cursorY + startLine < buffer.size() - 1) {
        ++cursorY;
        cursorX = 0;
      }
      break;

    case '0':
    case KEY_HOME:
      cursorX = 0;
      break;

    case '$':
    case KEY_END:
      cursorX = buffer[cursorY + startLine].length() - 1;
      break;

    case 'd':
      if (kbCache == 'd') {
        buffer.erase(buffer.begin() + cursorY + startLine);
        if (cursorY + startLine < buffer.size()) {
          cursorX = 0;
          while (cursorX < buffer[cursorY + startLine].length() - 1 &&
                 buffer[cursorY + startLine][++cursorX] == ' ')
            ;
        } else {
          --cursorY;
          cursorX = 0;
          while (cursorX < buffer[cursorY + startLine].length() - 1 &&
                 buffer[cursorY + startLine][++cursorX] == ' ')
            ;
        }
        kbCache = '\0';
      } else {
        kbCache = 'd';
      }
    default:
      break;
    }
  }

  void handleInsertEditorMode(int ch) {
    switch (ch) {
    case KEY_ESC:
      editorMode = NORMAL;
      break;

    case KEY_BACKSPACE:
    case 127:
      modified = true;
      if (!buffer[cursorY + startLine].empty() && cursorX > 0) {
        buffer[cursorY + startLine].erase(cursorX - 1, 1);
        --cursorX;
      } else if (cursorY + startLine > 0) {
        buffer[cursorY - 1] += buffer[cursorY + startLine];
        buffer.erase(buffer.begin() + cursorY + startLine);
        --cursorY;
        cursorX = buffer[cursorY + startLine].length() - 1;
      }
      break;

    case KEY_DC:
      modified = true;
      if (!buffer[cursorY + startLine].empty() &&
          cursorX < buffer[cursorY + startLine].length() - 1) {
        buffer[cursorY + startLine].erase(cursorX + 1, 1);
        ++cursorX;
      } else if (cursorY + startLine < buffer.size() - 1) {
        buffer[cursorY + startLine] += buffer[cursorY + startLine + 1];
        buffer.erase(buffer.begin() + cursorY + startLine + 1);
      }
      break;

    case KEY_ENTER:
    case '\n':
      modified = true;
      buffer.insert(buffer.begin() + cursorY + startLine + 1,
                    buffer[cursorY + startLine].substr(cursorX));
      buffer[cursorY + startLine] =
          buffer[cursorY + startLine].substr(0, cursorX);
      ++cursorY;
      cursorX = 0;
      break;

    case KEY_UP:
      if (cursorY + startLine > 0) {
        --cursorY;
        cursorX = std::min(
            cursorX,
            std::max(0, int(buffer[cursorY + startLine].length() - 1)));
      }
      break;

    case KEY_DOWN:
      if (cursorY + startLine < buffer.size() - 1) {
        ++cursorY;
        cursorX = std::min(
            cursorX,
            std::max(0, int(buffer[cursorY + startLine].length() - 1)));
      }
      break;

    case KEY_LEFT:
      if (cursorX > 0) {
        --cursorX;
      } else if (cursorY + startLine > 0) {
        --cursorY;
        cursorX = buffer[cursorY + startLine].length() - 1;
      }
      break;

    case KEY_RIGHT:
      if (cursorX < buffer[cursorY + startLine].length()) {
        ++cursorX;
      } else if (cursorY + startLine < buffer.size() - 1) {
        ++cursorY;
        cursorX = 0;
      }
      break;

    case KEY_HOME:
      cursorX = 0;
      break;

    case KEY_END:
      cursorX = buffer[cursorY + startLine].length() - 1;
      break;

    case KEY_BTAB:
    case KEY_CTAB:
    case KEY_STAB:
    case KEY_CATAB:
    case '\t':
    case KEY_RESIZE:
      break;

    default:
      modified = true;
      buffer[cursorY + startLine].insert(cursorX, 1, ch);
      ++cursorX;
      break;
    }
  }

  void handleCommandEditorMode(int ch) {
    switch (ch) {
    case KEY_ESC:
      editorMode = NORMAL;
      break;

    case KEY_BACKSPACE:
    case 127:
      if (commandBuffer.length()) {
        commandBuffer.pop_back();
      }
      break;

    case KEY_ENTER:
    case '\n':
      if (commandBuffer == "w") {
        saveFile();
      } else if (commandBuffer == "q") {
        if (modified) {
          warnMessage = "No write since last change (add ! to override)";
        } else {
          endwin(); // Properly end ncurses mode
          exit(0);
        }
      } else if (commandBuffer == "q!") {
        endwin(); // Properly end ncurses mode
        exit(0);
      } else if (commandBuffer == "wq") {
        saveFile();
        endwin(); // Properly end ncurses mode
        exit(0);
      } else {
        warnMessage = "Command not found.";
      }
      editorMode = NORMAL;
      break;

    default:
      commandBuffer.push_back(ch);
      break;
    }
  }

  void autoScroll() {
    if (cursorY > (rows - 2) - 1) {
      startLine += cursorY - ((rows - 2) - 1);
      cursorY = (rows - 2) - 1;
    } else if (cursorY < 0 && startLine + cursorY >= 0) {
      startLine += cursorY;
      cursorY = 0;
    }
  }

  void refreshScreen() {
    int former_rows = rows;

    wrefresh(stdscr);
    getmaxyx(stdscr, rows, cols);

    int diff = former_rows - rows;

    if (diff + startLine <= 0 || cursorY - diff <= 0) {
      cursorY += startLine;
      startLine = 0;
    } else {
      startLine += diff;
      cursorY -= diff;
    }

    wclear(fileWindow);
    wresize(fileWindow, rows - 2, cols);
    wrefresh(fileWindow);

    wclear(infoWindow);
    mvwin(infoWindow, rows - 2, 0);
    wrefresh(infoWindow);

    wclear(commandWindow);
    mvwin(commandWindow, rows - 1, 0);
    wrefresh(commandWindow);

    wclear(infoWindow);
    if (warnMessage.length()) {
      mvwprintw(infoWindow, 0, 0, "[WARN]%s\n", warnMessage.c_str());
    } else {
      mvwprintw(infoWindow, 0, 0, "\"%s\" %s\tLine %d\tCol %d\n",
                filename.c_str(), isNewFile ? "(new file)" : "",
                cursorY + startLine + 1, cursorX + 1);
    }
    wrefresh(infoWindow);

    for (size_t i = 0; i < rows - 2 && i + startLine < buffer.size(); ++i) {
      mvwaddstr(fileWindow, i, 0, buffer[i + startLine].c_str());
      // Wrap mode todo...
    }

    wrefresh(fileWindow);

    if (editorMode == COMMAND) {
      mvwprintw(commandWindow, 0, 0, ":%s", commandBuffer.c_str());
      wrefresh(commandWindow);
    } else {
      if (editorMode == INSERT) {
        mvwprintw(commandWindow, 0, 0, "--INSERT--");
        wrefresh(commandWindow);
      } else {
        mvwprintw(commandWindow, 0, 0,
                  "MiniVim\t\t\t\tby Reqwey <reqwey05@sjtu.edu.cn>");
        wrefresh(commandWindow);
      }
      move(cursorY, cursorX);
    }
  }
};

int main(int argc, char **argv) {
  bool truncate = false;
  bool readOnly = false;
  WrapMode wrapMode = SCROLL;
  std::string filename;

  int opt;
  while ((opt = getopt(argc, argv, "tRW:")) != -1) {
    switch (opt) {
    case 't':
      truncate = true;
      break;
    case 'R':
      readOnly = true;
      break;
    case 'W':
      if (strcmp(optarg, "break") == 0) {
        wrapMode = BREAK;
      } else if (strcmp(optarg, "scroll") == 0) {
        wrapMode = SCROLL;
      } else {
        fprintf(stderr, "Invalid argument for -W: %s\n", optarg);
        return 1;
      }
      break;
    default:
      fprintf(stderr, "Usage: %s [-t] [-R] [-W break/scroll] <filename>\n",
              argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Expected filename argument\n");
    return 1;
  }

  filename = argv[optind];
  MiniVim editor(filename, truncate, readOnly, wrapMode);
  editor.run();
  return 0;
}

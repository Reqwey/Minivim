#include <csignal>
#include <ncurses.h>
#include <string>
#include <vector>

#define KEY_ESC 27
#define REG_COLOR_NUM 1
#define CUS_COLOR_NUM 1

enum EditorMode { NORMAL, INSERT, COMMAND };

class MiniVim {
public:
  MiniVim() {
    initscr();                    // Start ncurses EditorMode
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
    EditorMode = NORMAL;
  }

  ~MiniVim() {
    endwin(); // End ncurses EditorMode
  }

  void run() {
    int ch;
    while (ch = wgetch(stdscr)) {
      switch (EditorMode) {
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
  EditorMode EditorMode;
  int cursorY, cursorX;
  int rows, cols;
  int startLine;
  char kbCache = '\0';

  std::vector<std::string> buffer = {""};
  std::string commandBuffer;

  void handleNormalEditorMode(int ch) {
    switch (ch) {
    case 'i':
      EditorMode = INSERT;
      break;

    case ':':
      EditorMode = COMMAND;
      commandBuffer.clear();
      break;

    case KEY_UP:
      if (cursorY + startLine > 0) {
        --cursorY;
        cursorX =
            std::min(cursorX, int(buffer[cursorY + startLine].length() - 1));
      }
      break;

    case KEY_DOWN:
      if (cursorY + startLine < buffer.size() - 1) {
        ++cursorY;
        cursorX =
            std::min(cursorX, int(buffer[cursorY + startLine].length() - 1));
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
      EditorMode = NORMAL;
      break;

    case KEY_BACKSPACE:
    case 127:
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
      buffer[cursorY + startLine].insert(cursorX, 1, ch);
      ++cursorX;
      break;
    }
  }

  void handleCommandEditorMode(int ch) {
    switch (ch) {
    case KEY_ESC:
      EditorMode = NORMAL;
      break;
    case KEY_BACKSPACE:
    case 127:
      if (commandBuffer.length()) {
        commandBuffer.pop_back();
      }
      break;
    case KEY_ENTER:
    case '\n':
      if (commandBuffer == "q") {
        endwin();
        exit(0);
      }
      EditorMode = NORMAL;
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

    mvwprintw(infoWindow, 0, 0,
              "screen: %d %d, cursor: %d, %d, startline: %d\n", rows, cols,
              cursorY, cursorX, startLine);
    wrefresh(infoWindow);

    for (size_t i = 0; i < rows - 2 && i + startLine < buffer.size(); ++i) {
      mvwprintw(fileWindow, i, 0, "%s", buffer[i + startLine].c_str());
    }
    wrefresh(fileWindow);

    if (EditorMode == COMMAND) {
      mvwprintw(commandWindow, 0, 0, ":%s", commandBuffer.c_str());
      wrefresh(commandWindow);
    } else {
      if (EditorMode == INSERT) {

        mvwprintw(commandWindow, 0, 0, "--INSERT--");
        wrefresh(commandWindow);
      }
      move(cursorY, cursorX);
    }
  }
};

int main() {
  MiniVim editor;
  editor.run();
  return 0;
}

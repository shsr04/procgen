namespace nc {
#include <ncurses.h>
}

class Screen {
  nc::WINDOW *win_;

public:
  Screen(size_t w, size_t h, int x0, int y0);
  ~Screen();
  Screen(Screen const &) = delete;
  Screen(Screen &&) = default;

  void write(size_t x, size_t y, char c);

  void write(size_t x, size_t y, string s);

  void refresh() const;

  bool hasEnded() const;

  bool isPaused() const;
};

Screen::Screen(size_t w, size_t h, int x0 = 0, int y0 = 0) {
  nc::initscr();
  win_ = nc::newwin(int(h), int(w), x0, y0);
  nc::keypad(win_, true);
  nc::nodelay(win_, true);
  nc::noecho();
  nc::cbreak();
}

Screen::~Screen() {
  nc::delwin(win_);
  nc::endwin();
}

void Screen::write(size_t x, size_t y, char c) {
  nc::wmove(win_, int(y), int(x));
  nc::waddch(win_, static_cast<nc::chtype>(c));
}

void Screen::write(size_t x, size_t y, string s) {
  nc::wmove(win_, int(y), int(x));
  nc::wprintw(win_, s.c_str());
}

void Screen::refresh() const { nc::wrefresh(win_); }

bool Screen::hasEnded() const {
  int c = nc::wgetch(win_);
  return c == KEY_BACKSPACE;
}

bool Screen::isPaused() const {
  int c = nc::wgetch(win_);
  return c == 'P';
}

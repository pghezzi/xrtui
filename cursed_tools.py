import curses

UP = (curses.KEY_UP, ord('k'))
DOWN = (curses.KEY_DOWN, ord('j'))
LEFT = (curses.KEY_LEFT, ord('h'))
RIGHT = (curses.KEY_RIGHT, ord('l'))
ENTER = (curses.KEY_ENTER, 10, 13, ord('i'))

class SelectWidget:
    def __init__(self, row, func, update, selected = None):
        self.row = row
        self.data = func(row)
        self.selected_func = selected
        self.selected_row = 0
        self.update_func = update
        self.first = True

    def render(self, stdscr, start_y, start_x, width):
        height = len(self.data) + 2

        for x in range(start_x, start_x + width):
            stdscr.addch(start_y, x, curses.ACS_HLINE)
            stdscr.addch(start_y + height - 1, x, curses.ACS_HLINE)

        for y in range(start_y, start_y + height):
            stdscr.addch(y, start_x, curses.ACS_VLINE)
            stdscr.addch(y, start_x + width - 1, curses.ACS_VLINE)

        stdscr.addch(start_y, start_x, curses.ACS_ULCORNER)
        stdscr.addch(start_y, start_x + width - 1, curses.ACS_URCORNER)
        stdscr.addch(start_y + height - 1, start_x, curses.ACS_LLCORNER)
        stdscr.addch(start_y + height - 1, start_x + width - 1, curses.ACS_LRCORNER)
        
        self.selected = self.selected_func(self.row)
        for i, item in enumerate(self.data):
            text = str(item)
            y = start_y + 1 + i
            x = start_x + 1
            if item == self.selected:
                text = '*' + text
                if self.first:
                    self.selected_row = i
            if i == self.selected_row and (not self.first or item == self.selected):
                stdscr.attron(curses.A_REVERSE)
            stdscr.addstr(y, x, text + " " *(width - 2 - len(text)))
            if i == self.selected_row and (not self.first or item == self.selected):
                stdscr.attroff(curses.A_REVERSE)
        self.first = False
    
    def update(self, key):
        if key in UP and self.selected_row > 0:
            self.selected_row -= 1
        elif key in DOWN and self.selected_row < len(self.data) - 1:
            self.selected_row += 1
        if key in ENTER:
            self.update_func(self.row, self.data[self.selected_row])
        return False

class BufferWidget:
    def __init__(self, row, buff, update):
        self.row = row
        self.buffer = str(buff)
        self.update_func = update

    def render(self, stdscr, start_y, start_x, width):
        height = 3
        width = max(len(self.buffer)+3, width)

        for x in range(start_x, start_x + width):
            stdscr.addch(start_y, x, curses.ACS_HLINE)
            stdscr.addch(start_y + height - 1, x, curses.ACS_HLINE)

        for y in range(start_y, start_y + height):
            stdscr.addch(y, start_x, curses.ACS_VLINE)
            stdscr.addch(y, start_x + width - 1, curses.ACS_VLINE)

        stdscr.addch(start_y, start_x, curses.ACS_ULCORNER)
        stdscr.addch(start_y, start_x + width - 1, curses.ACS_URCORNER)
        stdscr.addch(start_y + height - 1, start_x, curses.ACS_LLCORNER)
        stdscr.addch(start_y + height - 1, start_x + width - 1, curses.ACS_LRCORNER)

        y = start_y + 1
        x = start_x + 1
        stdscr.addstr(y, x, self.buffer)
        x += len(self.buffer)
        stdscr.attron(curses.A_REVERSE)
        stdscr.addstr(y, x, " ")
        x += 1 
        stdscr.attroff(curses.A_REVERSE)
        stdscr.addstr(y, x, " " *(width - 3 - len(self.buffer)))
    
    def update(self, key):
        if key == 8:
            self.buffer = self.buffer[:-1]
        if chr(key) in ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]:
            self.buffer += chr(key)
        if key in ENTER:
            self.update_func(self.row, int(self.buffer))
            return True
        return False
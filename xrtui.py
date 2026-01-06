import curses
import subprocess
import re

UP = (curses.KEY_UP, ord('k'))
DOWN = (curses.KEY_DOWN, ord('j'))
LEFT = (curses.KEY_LEFT, ord('h'))
RIGHT = (curses.KEY_RIGHT, ord('l'))
ENTER = (curses.KEY_ENTER, 10, 13, ord('i'))

class Display:
    def __init__(self, name, primary, resolution=None, position_x=0, position_y=0):
        self.name = name
        self.primary = primary
        self.resolution=resolution
        self.position_x=position_x
        self.position_y=position_y
        self.refresh_rate: float | None = None
        self.modes_dict = {}
        self.updatsd = True

    def get_resolutions(self):
        return list(self.modes_dict.keys())

    def get_rates(self):
        return self.modes_dict[self.resolution]

    def set_actual_size(self, x, y):
        self.size_x = x
        self.size_y = y
    
    def set_resolution(self, resolution):
        self.resolution = resolution
        self.refresh_rate = self.modes_dict[self.resolution][0]
        self.updated = False

    def set_primary(self, primary):
        self.primary = primary
        self.updated = False

    def set_rate(self, rate):
        self.refresh_rate = rate
        self.updated = False

    def add_mode(self, resolution, modes):
        self.modes_dict[resolution] = modes

    def sync(self):
        if not self.modes_dict or self.updated:
            return

        cmd = [
            "xrandr",
            "--output", self.name,
            "--mode", self.resolution,
            "--rate", str(self.refresh_rate),
            "--pos", f"{self.position_x}x{self.position_y}"
        ]
        if self.primary:
            cmd.append("--primary")

        try:
            subprocess.run(cmd, check=True)
            self.updated = True
        except subprocess.CalledProcessError as e:
            pass

    def __str__(self):
        return f"{self.name} connected {'primary ' if self.primary else ''}{ self.resolution + '+' + str(self.position_x) + '+' + str(self.position_y) + ' @ ' + str(self.refresh_rate) + ' ' if self.resolution else ''}{self.size_x}mm x {self.size_y}mm"
    

class Widget:
    def __init__(self, row, func, update, selected = None):
        self.row = row
        self.data = func(row)
        self.selected_func = selected
        self.selected_row = 0
        self.update_func = update

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
            if text == self.selected:
                text = '*' + text
            if i == self.selected_row:
                stdscr.attron(curses.A_REVERSE)
            stdscr.addstr(y, x, text + " " *(width - 2 - len(text)))
            if i == self.selected_row:
                stdscr.attroff(curses.A_REVERSE)
    
    def update(self, key):
        if key in UP and self.selected_row > 0:
            self.selected_row -= 1
        elif key in DOWN and self.selected_row < len(self.data) - 1:
            self.selected_row += 1
        if key in ENTER:
            self.update_func(self.row, self.data[self.selected_row])

def prim(table, row, value):
    for x in table.data:
        x.primary = False
    row.set_primary(value)

class Displays_Table:
    def __init__(self):
        self.headers = ["Output", "Primary", "Resolution", "Refresh", "Position"]
        self.col_widths = [12, 12, 14, 12, 12]
        self.sum_col_widths = [sum(self.col_widths[:i]) for i in range(len(self.col_widths))]
        self.widgets = {
            "Resolution" : (lambda row : row.get_resolutions(), lambda row, value : row.set_resolution(value), lambda row: row.resolution), 
            "Refresh": (lambda row : row.get_rates(), lambda row, value : row.set_rate(value), lambda row: str(row.refresh_rate)),
            "Primary": (lambda row : [True, False], lambda row, value: prim(self, row, value), lambda row: str(row.primary))
        }
        self.data = []
        self.displays = []
        self.selected_row = 0
        self.selected_column = 0
        self.selected_mode = False
        self.x_offset = 2
        self.y_offset = 1
        self.wid = None

    def set_items(self, data):
        self.data = data
        self.displays = [d.name for d in data]

    def render(self, stdscr):
        x = self.x_offset
        for i, header in enumerate(self.headers):
            stdscr.addstr(self.y_offset, x, header)
            x += self.col_widths[i]
        
        for i, row in enumerate(self.data):
            x = 2
            y = i + 3
            if i == self.selected_row:
                stdscr.attron(curses.A_REVERSE)
            for j, cell in enumerate([row.name, str(row.primary), str(row.resolution), f"{row.refresh_rate:.2f}", f"+{row.position_x}+{row.position_y}"]):
                if i == self.selected_row and j == self.selected_column:
                    stdscr.attroff(curses.A_REVERSE)
                stdscr.addstr(y, x, " "*self.col_widths[j])
                stdscr.addstr(y, x, cell)
                x += self.col_widths[j]
                if i == self.selected_row and j == self.selected_column:
                    stdscr.attron(curses.A_REVERSE)
            if i == self.selected_row:
                stdscr.attroff(curses.A_REVERSE)

        if self.selected_mode:
            if self.wid is None:
                row = self.data[self.selected_row]
                pac = self.widgets.get(self.headers[self.selected_column], None)
                if pac is not None:
                    func, update, selected = pac
                    self.wid = Widget(row, func, update, selected)
                else:
                    self.selected_mode = False
                    self.wid = None
                    return
            self.wid.render(stdscr, self.selected_row + 2, self.x_offset + self.sum_col_widths[self.selected_column], self.col_widths[self.selected_column])
        if not self.selected_mode:
            self.wid = None


    def update(self, key):
        if not self.selected_mode:
            if key in UP and self.selected_row > 0:
                self.selected_row -= 1
            elif key in DOWN and self.selected_row < len(self.data) - 1:
                self.selected_row += 1
            elif key in LEFT and self.selected_column > 0:
                self.selected_column -= 1
            elif key in  RIGHT and self.selected_column < len(self.headers) - 1:
                self.selected_column += 1
            if key in ENTER:
                self.selected_mode = True
        else:
            self.wid.update(key)
            if key == 27:
                self.selected_mode = False
    
    def sync(self):
        for d in self.data:
            d.sync()


def get_randr_outputs():
    result = subprocess.run(
        ["xrandr", "--query"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )

    outputs = []
    current_output = None

    for line in result.stdout.splitlines():
        # Match connected output line
        modes = {}
        m = re.match(r"^(\S+) connected(?: (primary))?\s*(?:(\d+)x(\d+)\+(\d+)\+(\d+))?\s*\(normal left inverted right x axis y axis\) (\d+)mm x (\d+)mm", line)
        if m:
            name, primary, w, h, x, y, acc_x, acc_y = m.groups()

            current_output = Display(name, primary == "primary", f"{w}x{h}", x, y)
            current_output.set_actual_size(acc_x, acc_y)
            #current_output = {
            #    "name": name,
            #    "primary": primary if primary else "default",
            #    "resolution": f"{w}x{h}",
            #    "position": f"{x},{y}",
            #    "refresh": "?"
            #}
            outputs.append(current_output)
            continue

        # Match mode line with current refresh (*)
        if current_output:
            m = re.match(r"^\s+(\d+)x(\d+)\s+([\d.]+)\*", line)
            if m:
                current_output.set_rate(float(m.group(3)))
            m = re.match(r'\s*(\d+x\d+)\s+(.+)', line)
            if m:
                resolution = m.group(1)
                rates_part = m.group(2)
                rates = []
                active_rate = None

                for rate, flag in re.findall(r'(\d+\.\d+)([*+]?)', rates_part):
                    rate = float(rate)
                    rates.append(rate)
                    if '*' in flag:
                        active_rate = rate
                current_output.add_mode(resolution, rates)
    return outputs


def draw_table(stdscr, table):
    stdscr.clear()
    h, w = stdscr.getmaxyx()

    table.render(stdscr)

    stdscr.addstr(h - 2, 2, "h(left), j(down), k(up), l(right) move | ENTER edit | ESC exit edit | q quit | s sync monitors")
    stdscr.refresh()

def main(stdscr):
    curses.curs_set(0)
    stdscr.keypad(True)

    data = get_randr_outputs()
    table = Displays_Table()
    table.set_items(data)

    while True:
        draw_table(stdscr, table)
        key = stdscr.getch()
        if key == ord("q"):
            break
        if key == ord("s"):
            table.sync()
            data = get_randr_outputs()
            table.set_items(data)
        table.update(key)


if __name__ == "__main__":
    import os
    os.environ.setdefault('ESCDELAY', '0')
    curses.wrapper(main)

import subprocess
import randr
from cursed_tools import *

class Display:
    def __init__(self, data: dict):
        self._name: str = data.get("name")
        self.primary: bool = bool(data.get("primary", False))

        self.physical_size_mm = data.get("physical_size_mm")
        self.modes_dict: dict[str, list[float]] = self._parse_modes(data.get("modes", {}))

        self.resolution: str | None = data.get("resolution")
        self.refresh: float | None = self._float_format(data.get("refresh", 0.0))
        self.position: list[int, int] | None = self._parse_position(
            data.get("position")
        )
        self.active = self.resolution is not None
        self.updated = True

    @staticmethod
    def headers():
        return ["Output", "Active", "Primary", "Resolution", "Refresh", "X Position", "Y Position"]

    @staticmethod
    def widgets():
        return {
            "Resolution" : ("select", lambda row : row.get_resolutions(), lambda row, value : row.set_resolution(value), lambda row: row.resolution), 
            "Refresh": ("select", lambda row : row.get_rates(), lambda row, value : row.set_rate(value), lambda row: str(row.refresh)),
            "Primary": ("select", lambda row : [True, False], lambda row, value: prim(self, row, value), lambda row: row.primary),
            "Active": ("select", lambda row: [True, False], lambda row, value: row.set_active(value), lambda row: row.active),
            "X Position": ("buffer", lambda row: row.position[0], lambda row, value: row.set_pos(value, row.position[1])),
            "Y Position": ("buffer", lambda row: row.position[1], lambda row, value: row.set_pos(row.position[0], value))
        }

    @staticmethod
    def _float_format(val):
        if val:
            return f"{val:.2f}"
        return "0.0"

    @staticmethod
    def _parse_modes(modes):
        for k in modes:
            modes[k] = list(map(Display._float_format, modes[k]))
        return modes
    
    @staticmethod
    def _parse_position(pos):
        if not pos:
            return None
        try:
            x, y = pos.split(",")
            return [int(x), int(y)]
        except Exception:
            return [0, 0]

    @property
    def name(self) -> int | None:
        if self.updated:
            return self._name
        return "*" + self._name


    @property
    def width(self) -> int | None:
        if not self.resolution:
            return None
        return int(self.resolution.split("x")[0])

    @property
    def height(self) -> int | None:
        if not self.resolution:
            return None
        return int(self.resolution.split("x")[1])

    @property
    def physical_size(self) -> tuple[int, int] | None:
        if not self.physical_size_mm:
            return None
        try:
            w, h = self.physical_size_mm.split("x")
            return int(w), int(h)
        except Exception:
            return None

    def info_tuple(self):
        return self.name, str(self.active), str(self.primary), str(self.resolution), self.refresh, str(self.position[0]), str(self.position[1])

    def get_resolutions(self):
        return list(self.modes_dict.keys())

    def get_rates(self):
        return self.modes_dict.get(self.resolution, [])

    def set_pos(self, position_x, position_y):
        self.position[0]=position_x
        self.position[1]=position_y
        self.updated = False

    def set_actual_size(self, x, y):
        self.size_x = x
        self.size_y = y
    
    def set_resolution(self, resolution):
        self.resolution = resolution
        self.refresh = self.modes_dict[self.resolution][0]
        self.updated = False

    def set_primary(self, primary):
        self.primary = primary
        self.updated = False

    def set_active(self, active):
        self.active = active
        self.updated = False

    def set_rate(self, rate):
        self.refresh = rate
        self.updated = False

    def add_mode(self, resolution, modes):
        self.modes_dict[resolution] = modes

    def sync(self):
        
        if not self.modes_dict or self.updated:
            return
        if self.active:
            if self.resolution is None:
                cmd = [
                    "xrandr",
                    "--output", self._name,
                    "--auto",
                ]
            else:
                cmd = [
                    "xrandr",
                    "--output", self._name,
                    "--mode", self.resolution,
                    "--rate", str(self.refresh),
                    "--pos", f"{self.position[0]}x{self.position[1]}"
                ]
        else:
            cmd = [
                "xrandr",
                "--output", self._name,
                "--off"
            ]

        if self.primary:
            cmd.append("--primary")

        try:
            subprocess.run(cmd, check=True)
            self.updated = True
        except subprocess.CalledProcessError as e:
            pass

    def __repr__(self):
        return f"{self._name} connected {'primary ' if self.primary else ''}{ self.resolution + '+' + str(self.position_x) + '+' + str(self.position_y) + ' @ ' + str(self.refresh) + ' ' if self.resolution else ''}{self.size_x}mm x {self.size_y}mm"





def prim(table, row, value):
    if value:
        for x in table.data:
            x.primary = False    
            row.set_primary(value)

class Displays_Table:
    def __init__(self):
        
        self.data = []
        self.displays = []
        self.selected_row = 0
        self.selected_column = 0
        self.selected_mode = False
        self.x_offset = 2
        self.y_offset = 1
        self.wid = None

    def set_items(self, data):
        self.headers = Display.headers()
        self.widgets = Display.widgets()
        self.col_widths = [12] * len(self.headers)
        self.sum_col_widths = [sum(self.col_widths[:i]) for i in range(len(self.col_widths))]
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
            for j, cell in enumerate(row.info_tuple()):
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
                    ty= pac[0]
                    if ty == "select":
                        _, func, update, selected = pac
                        self.wid = SelectWidget(row, func, update, selected)
                    else:
                        _, buff, update = pac
                        self.wid = BufferWidget(row, buff(row), update)
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
            if self.wid.update(key) or key == 27:
                self.selected_mode = False
    
    def sync(self):
        for d in self.data:
            d.sync()


def get_randr_outputs():
    return [Display(o) for o in randr.get_randr_outputs()]

#def get_randr_outputs():
#    result = subprocess.run(
#        ["xrandr", "--query"],
#        stdout=subprocess.PIPE,
#        stderr=subprocess.DEVNULL,
#        text=True,
#        check=False,
#    )
#    outputs = []
#    current_output = None
#    for line in result.stdout.splitlines():
#        # Match connected output line
#        modes = {}
#        m = re.match(r"^(\S+) connected(?: (primary))?\s*(?:(\d+)x(\d+)\+(\d+)\+(\d+))?\s*\(normal left inverted right x axis y axis\)(?: (\d+)mm x (\d+)mm)?", line)
#        if m:
#            name, primary, w, h, x, y, acc_x, acc_y = m.groups()
#            res = f"{w}x{h}" if x and h else None
#            current_output = Display(name, primary == "primary", res, x, y)
#            current_output.set_actual_size(acc_x, acc_y)
#            #current_output = {
#            #    "name": name,
#            #    "primary": primary if primary else "default",
#            #    "resolution": f"{w}x{h}",
#            #    "position": f"{x},{y}",
#            #    "refresh": "?"
#            #}
#            outputs.append(current_output)
#            continue
#
#        # Match mode line with current refresh (*)
#        if current_output:
#            m = re.match(r"^\s+(\d+)x(\d+)\s+([\d.]+)\*", line)
#            if m:
#                current_output.set_rate(float(m.group(3)))
#            m = re.match(r'\s*(\d+x\d+)\s+(.+)', line)
#            if m:
#                resolution = m.group(1)
#                rates_part = m.group(2)
#                rates = []
#                active_rate = None
#                for rate, flag in re.findall(r'(\d+\.\d+)([*+]?)', rates_part):
#                    rate = float(rate)
#                    rates.append(rate)
#                    if '*' in flag:
#                        active_rate = rate
#                current_output.add_mode(resolution, rates)
#    return outputs

def draw_table(stdscr, table):
    stdscr.clear()
    h, w = stdscr.getmaxyx()

    table.render(stdscr)

    stdscr.addstr(h - 2, 2, "h(left), j(down), k(up), l(right) move | ENTER edit | ESC exit edit | q quit | s sync monitors | r reload")
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
        elif key == ord("s"):
            table.selected_mode = False
            table.sync()
            data = get_randr_outputs()
            table.set_items(data)
        elif key == ord("r"):
            table.selected_mode = False
            data = get_randr_outputs()
            table.set_items(data)
        table.update(key)


if __name__ == "__main__":
    import os
    os.environ.setdefault('ESCDELAY', '0')
    curses.wrapper(main)

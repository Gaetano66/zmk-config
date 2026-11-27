#!/usr/bin/env python3
"""Render the Corne keymap's active bindings (not its ASCII-art comments)."""
import argparse
from io import BytesIO
from pathlib import Path
import re
import stat
import subprocess
import sys
from tempfile import NamedTemporaryFile

KEYMAP = 'config/corne.keymap'
COMMANDS = 'dts/pomodoro_commands.h'
OUTPUT = 'docs/Keybindings.pdf'
ROOT = Path(__file__).resolve().parents[1]
NAMES = {'default_layer': 'Basis', 'lower_layer': 'Lower', 'raise_layer': 'Raise',
         'lighting_layer': 'Lighting'}
KEYS = {
    'BSPC': 'Lösch links', 'DEL': 'Entf', 'RET': 'Enter', 'TAB': 'Tab', 'ESC': 'Esc',
    'LCTRL': 'Strg', 'LSHFT': 'Shift', 'LGUI': 'Win', 'RALT': 'AltGr', 'SPACE': 'Leer',
    'Y': 'Z', 'Z': 'Y', 'LBKT': 'ü', 'SEMI': 'ö', 'SQT': 'ä', 'NUHS': '#', 'GRAVE': '^',
    'FSLH': '-', 'RBKT': '+', 'LS(FSLH)': '_', 'LS(RBKT)': '*', 'RA(RBKT)': '~',
    'COMMA': ',', 'DOT': '.', 'LS(N1)': '!', 'LS(N2)': '"', 'LS(N4)': '$', 'LS(N5)': '%',
    'LS(N7)': '/', 'LS(N8)': '(', 'LS(N9)': ')', 'LS(N0)': '=', 'LS(EQUAL)': '`',
    'RA(N7)': '{', 'RA(N8)': '[', 'RA(N9)': ']', 'RA(N0)': '}',
    'UP': 'Hoch', 'DOWN': 'Runter', 'LEFT': 'Links', 'RIGHT': 'Rechts',
    'C_VOL_UP': 'Lauter', 'C_VOL_DN': 'Leiser', 'C_PP': 'Play/Pause',
    'C_NEXT': 'Nächster', 'C_PREV': 'Voriger',
}
POMO = ['Pomo Start/Stopp', 'Pomo Neustart', 'Pomo Phase', 'Pomo kürzer', 'Pomo länger']


def strip_comments(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0].startswith('"') else ' ', text, flags=re.S)


def body(text, name):
    match = re.search(r'\b' + name + r'\s*\{', text)
    if not match:
        return ''
    start = match.end()
    depth = 1
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|[{}]', text[start:]):
        depth += (token[0] == '{') - (token[0] == '}')
        if not depth:
            return text[start:start + token.start()]
    raise ValueError(f'Nicht geschlossener DTS-Knoten: {name}')


def prop(node, name):
    match = re.search(r'(?<![\w-])' + name + r'\s*=\s*<([^<>]*)>\s*;', node)
    return match[1].strip() if match else ''


def parse(keymap, commands):
    keymap = strip_comments(keymap)
    defines = {k: int(v) for k, v in re.findall(r'^\s*#define\s+(POMO_\w+)\s+(\d+)\b',
                                               strip_comments(commands), re.M)}
    keys = body(keymap, 'keymap')
    raw_layers = re.findall(r'([\w-]+)\s*\{([^{}]*)\}', keys)
    names: list[str] = [NAMES[name] if name in NAMES else name for name, _ in raw_layers]

    def label(binding: str) -> str:
        verb, *args = binding.split(maxsplit=1)
        arg = args[0] if args else ''
        if verb == '&kp':
            arg = re.sub(r'\s+', '', arg)
            return KEYS.get(arg, arg[1:] if re.fullmatch(r'N[0-9]', arg) else arg)
        if verb == '&mo' and arg.isdecimal():
            return names[int(arg)]
        if verb == '&pomodoro':
            idx = defines.get(arg, int(arg) if arg.isdecimal() else -1)
            if 0 <= idx < len(POMO):
                return POMO[idx]
        if verb == '&bt':
            if arg == 'BT_CLR':
                return 'BT löschen'
            if re.fullmatch(r'BT_SEL \d+', arg):
                return f'BT {int(arg.split()[1]) + 1}'
        if verb == '&rgb_ug':
            return {'RGB_TOG': 'RGB an/aus', 'RGB_BRD': 'Dimmer', 'RGB_BRI': 'Heller'}.get(arg, binding)
        if verb == '&rgb_fx':
            return {'0': 'RGB-Effekt', '1': 'Farbe zurück', '2': 'Farbe weiter'}.get(arg, binding)
        return {'&trans': '·', '&none': '×', '&key_rgb': 'Tasten-LEDs'}.get(binding, binding)

    def labels(node):
        entries = re.findall(r'&[^&]+', prop(node, 'bindings'))
        return [label(' '.join(entry.split())) for entry in entries]

    if not raw_layers:
        raise ValueError('Kein Keymap-Layer gefunden')
    layers = [(name, labels(node)) for name, (_, node) in zip(names, raw_layers)]
    for name, entries in layers:
        if len(entries) != 42:
            raise ValueError(f'{name}: {len(entries)} statt 42 Tasten')

    combos = []
    for node in re.findall(r'\{([^{}]*)\}', body(keymap, 'combos')):
        chord = [int(p, 0) for p in prop(node, 'key-positions').split()]
        if len(chord) < 2 or not all(0 <= p < 42 for p in chord):
            raise ValueError(f'Ungültige Combo-Positionen: {chord}')
        scope = prop(node, 'layers').split()
        from_layers = ', '.join(names[int(i, 0)] for i in scope) if scope else 'Alle'
        combos.append((' + '.join(layers[0][1][p] for p in chord), ' / '.join(labels(node)),
                       from_layers, int(prop(node, 'timeout-ms') or '50', 0)))

    conditions = []
    for node in re.findall(r'\{([^{}]*)\}', body(keymap, 'conditional_layers')):
        combo = ' + '.join(names[int(i, 0)] for i in prop(node, 'if-layers').split())
        conditions.append((combo, names[int(prop(node, 'then-layer'), 0)]))
    return layers, combos, conditions


def render(layout):
    from reportlab.lib.colors import HexColor
    from reportlab.lib.pagesizes import A4, landscape
    from reportlab.pdfgen import canvas

    layers, combos, conditions = layout
    buffer = BytesIO()
    width, height = landscape(A4)
    pdf = canvas.Canvas(buffer, pagesize=(width, height), invariant=1)
    pdf.setTitle('Corne · Tastenbelegung')
    pdf.setAuthor('zmk-config')
    margin, gap = 30, 22
    panel = (width - 2 * margin - gap) / 2
    split = 7
    cell = (panel - split) / 12
    navy, ink, teal = map(HexColor, ('#14243A', '#22334A', '#006F71'))
    pale, soft, white = map(HexColor, ('#EBF2F5', '#F5F7FA', '#FFFFFF'))

    def text(x, y, value, size: float = 10, color=ink, bold=False, center=False):
        font = 'Helvetica-Bold' if bold else 'Helvetica'
        pdf.setFont(font, size)
        pdf.setFillColor(color)
        draw = pdf.drawCentredString if center else pdf.drawString
        draw(x, y, value)

    def draw_layer(name, entries, x0, y, row_height):
        text(x0, y, name, 13, teal, True)
        for row in range(4):
            for side in range(2):
                count = 6 if row < 3 else 3
                for col in range(count):
                    index = row * 12 + side * 6 + col if row < 3 else 36 + side * 3 + col
                    lane = col + (3 if row == 3 and side == 0 else 0)
                    x = x0 + side * (6 * cell + split) + lane * cell
                    yy = y - 9 - (row + 1) * row_height
                    value = entries[index]
                    dim = value in ('·', '×')
                    pdf.setFillColor(soft if dim else pale)
                    pdf.roundRect(x, yy, cell - 2, row_height - 3, 3, fill=1, stroke=0)
                    # Multiword actions get two centred lines; shorten the font if needed.
                    words = value.rsplit(' ', 1)
                    lines = words if len(words) == 2 and len(value) > 9 else [value]
                    for i, line in enumerate(lines):
                        size = min(8.2, 8.2 * (cell - 5) / max(1, pdf.stringWidth(line, 'Helvetica', 8.2)))
                        baseline = yy + (row_height - 3) / 2 - 2.5 + (3.8 if len(lines) == 2 else 0) - 7.6 * i
                        text(x + (cell - 2) / 2, baseline, line, size, color=ink if not dim else teal, center=True)

    pdf.setFillColor(navy)
    pdf.rect(0, height - 62, width, 62, fill=1, stroke=0)
    text(margin, height - 29, 'Corne · Tastenbelegung', 22, white, True)
    text(margin, height - 47, 'Aktive ZMK-Belegungen · Deutsch (QWERTZ)', 10, white)
    text(margin, 23, '· = übernimmt darunter    × = deaktiviert    ^ und ` sind ggf. Tottasten', 9)

    n_grid_rows = (len(layers) + 1) // 2
    n_combo_rows = (len(combos) + 1) // 2
    grid_top = height - 86
    combo_bottom = 44
    grid_bottom = combo_bottom + 16 * n_combo_rows + (32 if combos else 0) + 14 * len(conditions) + 10
    panel_height = (grid_top - grid_bottom) / n_grid_rows
    row_height = min(25, (panel_height - 30) / 4)
    if row_height < 17:
        raise ValueError('Tasten und Combos passen nicht lesbar auf eine A4-Seite')
    for i, (name, entries) in enumerate(layers):
        draw_layer(name, entries, margin + (i % 2) * (panel + gap), grid_top - (i // 2) * panel_height,
                   row_height)

    y = grid_bottom - 2
    for source, target in conditions:
        text(margin, y, f'{target}: {source} gleichzeitig halten.', 10, ink, True)
        y -= 14
    if combos:
        text(margin, y - 5, 'Combos · Tasten gleichzeitig drücken', 11, teal, True)
        for i, (chord, action, scope, timeout) in enumerate(combos):
            x = margin + (i % 2) * (panel + gap)
            yy = y - 24 - (i // 2) * 16
            contents = f'{chord}  >  {action}   ·   {scope}, {timeout} ms'
            pdf.setFillColor(soft)
            pdf.roundRect(x, yy - 4, panel, 14, 3, fill=1, stroke=0)
            size = min(9, 9 * (panel - 14) / max(1, pdf.stringWidth(contents, 'Helvetica', 9)))
            text(x + 6, yy, contents, size)
    pdf.showPage()
    pdf.save()
    return buffer.getvalue()


def output_mode(out):
    if out.parent.is_symlink():
        raise ValueError(f'{out.parent.name}: Symlink nicht erlaubt')
    try:
        mode = out.lstat().st_mode
    except FileNotFoundError:
        return None
    if stat.S_ISLNK(mode):
        raise ValueError(f'{OUTPUT}: Symlink nicht erlaubt')
    if not stat.S_ISREG(mode):
        raise ValueError(f'{OUTPUT}: keine reguläre Datei')
    return mode


def output_in_index():
    entries = subprocess.check_output(['git', 'ls-files', '--stage', '-z', '--', OUTPUT], cwd=ROOT)
    found = False
    for entry in entries.split(b'\0'):
        metadata, _, path = entry.partition(b'\t')
        if path != OUTPUT.encode():
            continue
        mode, _, stage = metadata.split()
        if mode == b'120000':
            raise ValueError(f'{OUTPUT}: Symlink im Git-Index nicht erlaubt')
        if mode not in (b'100644', b'100755') or stage != b'0':
            raise ValueError(f'{OUTPUT}: keine reguläre Datei im Git-Index')
        found = True
    return found


def write_pdf(out, data, mode):
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = None
    try:
        with NamedTemporaryFile(dir=out.parent, delete=False) as file:
            tmp = Path(file.name)
            file.write(data)
        if mode is not None:
            tmp.chmod(mode & 0o777)
        tmp.replace(out)
    finally:
        if tmp is not None:
            tmp.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true', help='read the Git index, not the working tree')
    parser.add_argument('--check', action='store_true', help='verify PDF without writing it')
    args = parser.parse_args()

    def read(path):
        if args.staged:
            return subprocess.check_output(['git', 'show', f':{path}'], cwd=ROOT)
        return (ROOT / path).read_bytes()

    try:
        out = ROOT / OUTPUT
        mode = output_mode(out)
        indexed = args.staged and output_in_index()
        data = render(parse(read(KEYMAP).decode(), read(COMMANDS).decode()))
        if args.check:
            if read(OUTPUT) != data:
                raise ValueError(f'{OUTPUT} ist nicht aktuell')
        elif mode is None or out.read_bytes() != data:
            if args.staged and mode is not None and not indexed:
                raise ValueError(f'{OUTPUT} ist nicht im Git-Index; wird nicht überschrieben')
            write_pdf(out, data, mode)
    except ImportError:
        sys.exit('ReportLab fehlt: Abhängigkeit aus scripts/requirements-keybindings.txt installieren.')
    except (OSError, subprocess.CalledProcessError, ValueError, IndexError) as error:
        sys.exit(str(error))


if __name__ == '__main__':
    main()

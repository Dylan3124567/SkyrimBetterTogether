"""Check the native strip hit-test against the page's .multiplayer-shortcut box at several screen heights (VS shell).

The page sizes the strip as calc(var(--sbt-u) * N) with --sbt-u = 100vh / 1080; Chromium paints it with its edges
snapped to the nearest pixel. The compiled OverlayService::IsMenuShortcut must cover every painted pixel, and may only
add the partially covered pixel on an edge, never a whole pixel outside the CSS box.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
scss = (root / 'Code/skyrim_ui/src/app/components/root/root.component.scss').read_text()
strip = scss[scss.index('app-window.multiplayer-shortcut {'):]
strip = strip[:strip.index('&.shown')]
assert re.search(r'--sbt-u:\s*calc\(100vh / 1080\);', strip), 'strip must define --sbt-u: calc(100vh / 1080)'
css = {}
for side in ('left', 'top', 'width', 'height'):
    match = re.search(rf'\n\s*{side}:\s*calc\(var\(--sbt-u\) \* (\d+)\);', strip)
    assert match, f'strip {side} must be calc(var(--sbt-u) * N)'
    css[side] = int(match.group(1))
assert (css['left'], css['top'], css['width'], css['height']) == (16, 96, 160, 180), css

source = (root / 'Code/client/Services/Generic/OverlayService.cpp').read_text()
render = source[source.index('// The game\'s cursor'):source.index('m_pOverlay->GetClient()->Render();')]
assert render.index('if (shortcut') < render.index('GetViewRect')
start = source.index('bool OverlayService::IsMenuShortcut')
depth, end = 0, source.index('{', start)
while True:
    depth += {'{': 1, '}': -1}.get(source[end], 0)
    end += 1
    if depth == 0:
        break
body = source[start:end]

heights = (720, 768, 900, 1080, 1440, 2160)
code = '''
#include <cstdint>
#include <cstdio>
#include "Systems/UiRules.h"
struct OverlayService { bool m_menuShortcut=true,m_active=false; int m_viewHeight=1080; bool IsMenuShortcut(uint16_t,uint16_t) const noexcept; };
''' + body + '''
// Prints the hit-test's edges per height as: height left right top bottom (right and bottom exclusive), then the
// result inside the box while the overlay is active and while the strip is hidden.
int main(){ OverlayService s;
for (int h : {''' + ', '.join(map(str, heights)) + '''}) {
  s.m_viewHeight = h;
  const int cx = (''' + str(css['left'] * 2 + css['width']) + ''' * h) / 2160, cy = (''' + str(css['top'] * 2 + css['height']) + ''' * h) / 2160;
  int l = -1, r = -1, t = -1, b = -1;
  for (int x = 0; x < 4000; ++x) if (s.IsMenuShortcut(x, cy)) { if (l < 0) l = x; r = x + 1; }
  for (int y = 0; y < 4000; ++y) if (s.IsMenuShortcut(cx, y)) { if (t < 0) t = y; b = y + 1; }
  std::printf("%d %d %d %d %d\\n", h, l, r, t, b);
}
s.m_viewHeight = 1080; s.m_active = true; const bool active = s.IsMenuShortcut(20, 100);
s.m_active = false; s.m_menuShortcut = false; const bool hidden = s.IsMenuShortcut(20, 100);
std::printf("%d %d\\n", active, hidden);
}
'''
with tempfile.TemporaryDirectory(prefix='menu-bounds-') as directory:
    p = Path(directory)
    (p / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++20', f'/I{root / "Code/client"}', 'check.cpp', '/Fe:check.exe'], cwd=p, check=True, stdout=subprocess.DEVNULL)
    lines = subprocess.run([str(p / 'check.exe')], cwd=p, check=True, capture_output=True, text=True).stdout.split('\n')

for line in lines[:len(heights)]:
    h, left, right, top, bottom = map(int, line.split())
    u = h / 1080
    box = (css['left'] * u, css['left'] * u + css['width'] * u, css['top'] * u, css['top'] * u + css['height'] * u)
    painted = [int((int(v * 64) + 32) // 64) for v in box]  # LayoutUnit (1/64 px) edges snapped to the nearest pixel
    native = (left, right, top, bottom)
    assert left <= painted[0] and right >= painted[1] and top <= painted[2] and bottom >= painted[3], (h, native, painted)
    assert left > box[0] - 1 and right < box[1] + 1 and top > box[2] - 1 and bottom < box[3] + 1, (h, native, box)
    print(f'{h}p: native x[{left},{right}) y[{top},{bottom})  page paints x[{painted[0]},{painted[1]}) y[{painted[2]},{painted[3]})')
assert lines[len(heights)].split() == ['0', '0'], 'hit-test must be off while the overlay is active or the strip hidden'
print('PASS strip hit-test covers the CSS box at every height, off while active or hidden')

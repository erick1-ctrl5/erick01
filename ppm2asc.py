#!/usr/bin/env python3
import sys

f = open(sys.argv[1], 'rb').read()
parts = f.split(b'\n', 3)
w, h = map(int, parts[1].split())
data = parts[3]
cols = int(sys.argv[2]) if len(sys.argv) > 2 else 110
gain = float(sys.argv[3]) if len(sys.argv) > 3 else 1.25
gamma = float(sys.argv[4]) if len(sys.argv) > 4 else 0.85
rows = max(1, round(cols * h / w / 2.1))
ramp = " .:-=+*coaA#%@"
xw, yh = w / cols, h / rows
for ry in range(rows):
    line = ""
    for rx in range(cols):
        x0, x1 = int(rx * xw), max(int(rx * xw) + 1, int((rx + 1) * xw))
        y0, y1 = int(ry * yh), max(int(ry * yh) + 1, int((ry + 1) * yh))
        r = g = b = n = 0
        for y in range(y0, min(y1, h)):
            for x in range(x0, min(x1, w)):
                i = (y * w + x) * 3
                r += data[i]; g += data[i + 1]; b += data[i + 2]; n += 1
        lum = (0.3 * r + 0.6 * g + 0.1 * b) / (n * 255)
        lum = min(1.0, lum * gain) ** gamma
        line += ramp[min(len(ramp) - 1, int(lum * len(ramp)))]
    print(line)

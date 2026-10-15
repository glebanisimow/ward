"""Draw the Ward icon (ECG complex on a monitor grid) as a PNG, stdlib only."""
import math
import sys

from make_icon import BG, S, png

GREEN, GRID = (53, 224, 111), (24, 40, 30)


def ecg(x):
    """Lead II beat over x in [0, 1]: P, QRS, T."""
    g = lambda mu, w: math.exp(-0.5 * ((x - mu) / w) ** 2)
    return 0.1 * g(0.3, 0.035) - 0.12 * g(0.455, 0.012) + 1.0 * g(0.48, 0.016) \
        - 0.25 * g(0.51, 0.013) + 0.24 * g(0.7, 0.05)


def main(path):
    px = bytearray(S * S * 4)
    corner = S * 0.18
    # polyline of the trace in pixels
    pts = [(S * (0.08 + 0.84 * k / 800), S * (0.66 - 0.46 * ecg(k / 800))) for k in range(801)]
    for y in range(S):
        for x in range(S):
            dx = max(abs(x + 0.5 - S / 2) - (S / 2 - corner), 0)
            dy = max(abs(y + 0.5 - S / 2) - (S / 2 - corner), 0)
            a = max(0.0, min(1.0, corner - math.hypot(dx, dy)))
            r, g, b = BG
            if x % 64 < 2 or y % 64 < 2:
                r, g, b = GRID
            px[(y * S + x) * 4:(y * S + x) * 4 + 4] = bytes((r, g, b, int(a * 255)))
    # stroke: distance to the polyline, with a glow
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        for y in range(int(min(y0, y1)) - 14, int(max(y0, y1)) + 15):
            for x in range(int(min(x0, x1)) - 14, int(max(x0, x1)) + 15):
                if not (0 <= x < S and 0 <= y < S):
                    continue
                vx, vy = x1 - x0, y1 - y0
                t = max(0.0, min(1.0, ((x - x0) * vx + (y - y0) * vy) / (vx * vx + vy * vy + 1e-9)))
                d = math.hypot(x - (x0 + t * vx), y - (y0 + t * vy))
                k = max(0.0, min(1.0, 7.0 - d)) + 0.35 * math.exp(-(d / 9.0) ** 2)
                i = (y * S + x) * 4
                if px[i + 3] == 0:
                    continue
                c = [px[i + j] + (GREEN[j] - px[i + j]) * min(1.0, k) for j in range(3)]
                px[i:i + 3] = bytes(int(v) for v in c)
    png(path, px)


if __name__ == "__main__":
    main(sys.argv[1])

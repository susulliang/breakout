"use strict";

const fs = require("node:fs");
const path = require("node:path");
const zlib = require("node:zlib");

const FRAME = 64;
const FRAME_COUNT = 4;
const SCALE = 4;
const WIDTH = FRAME * FRAME_COUNT;
const HEIGHT = FRAME;
const PIXEL_WIDTH = WIDTH * SCALE;
const PIXEL_HEIGHT = HEIGHT * SCALE;

const PALETTE = {
    player: {
        outline: [18, 31, 51, 255],
        shadow: [25, 47, 76, 255],
        dark: [25, 65, 107, 255],
        body: [42, 113, 188, 255],
        light: [93, 165, 226, 255],
        shine: [163, 215, 248, 255],
        visor: [12, 31, 48, 255],
        accent: [244, 157, 55, 255],
        limb: [76, 113, 145, 255],
        boot: [35, 48, 66, 255],
        glow: [118, 229, 255, 255]
    },
    enemy: {
        outline: [37, 25, 34, 255],
        shadow: [64, 34, 42, 255],
        dark: [83, 39, 52, 255],
        body: [160, 57, 68, 255],
        light: [216, 91, 82, 255],
        shine: [245, 155, 111, 255],
        visor: [48, 24, 36, 255],
        accent: [249, 177, 67, 255],
        limb: [116, 67, 74, 255],
        boot: [48, 39, 47, 255],
        glow: [255, 96, 75, 255]
    }
};

function makeCanvas() {
    return Buffer.alloc(PIXEL_WIDTH * PIXEL_HEIGHT * 4);
}

function putPixel(canvas, x, y, color) {
    if (x < 0 || x >= PIXEL_WIDTH || y < 0 || y >= PIXEL_HEIGHT) return;
    const index = (y * PIXEL_WIDTH + x) * 4;
    canvas[index] = color[0];
    canvas[index + 1] = color[1];
    canvas[index + 2] = color[2];
    canvas[index + 3] = color[3];
}

function logicalBounds(frame, x0, y0, x1, y1) {
    return {
        left: Math.max(0, Math.floor((frame * FRAME + x0) * SCALE)),
        top: Math.max(0, Math.floor(y0 * SCALE)),
        right: Math.min(PIXEL_WIDTH, Math.ceil((frame * FRAME + x1) * SCALE)),
        bottom: Math.min(PIXEL_HEIGHT, Math.ceil(y1 * SCALE))
    };
}

function fillEllipse(canvas, frame, cx, cy, rx, ry, color) {
    const bounds = logicalBounds(frame, cx - rx, cy - ry, cx + rx, cy + ry);
    for (let y = bounds.top; y < bounds.bottom; y += 1) {
        const py = ((y + 0.5) / SCALE) - cy;
        for (let x = bounds.left; x < bounds.right; x += 1) {
            const px = ((x + 0.5) / SCALE) - (frame * FRAME + cx);
            if ((px * px) / (rx * rx) + (py * py) / (ry * ry) <= 1) {
                putPixel(canvas, x, y, color);
            }
        }
    }
}

function fillPolygon(canvas, frame, points, color) {
    const xs = points.map(point => point[0]);
    const ys = points.map(point => point[1]);
    const bounds = logicalBounds(frame, Math.min(...xs), Math.min(...ys),
        Math.max(...xs), Math.max(...ys));
    for (let y = bounds.top; y < bounds.bottom; y += 1) {
        const py = ((y + 0.5) / SCALE);
        for (let x = bounds.left; x < bounds.right; x += 1) {
            const px = ((x + 0.5) / SCALE);
            let inside = false;
            for (let i = 0, j = points.length - 1; i < points.length; j = i, i += 1) {
                const xi = (frame * FRAME + points[i][0]) * SCALE;
                const yi = points[i][1] * SCALE;
                const xj = (frame * FRAME + points[j][0]) * SCALE;
                const yj = points[j][1] * SCALE;
                const sx = px * SCALE;
                const sy = py * SCALE;
                if ((yi > sy) !== (yj > sy) &&
                    sx < ((xj - xi) * (sy - yi)) / (yj - yi) + xi) {
                    inside = !inside;
                }
            }
            if (inside) putPixel(canvas, x, y, color);
        }
    }
}

function strokeLine(canvas, frame, x1, y1, x2, y2, width, color) {
    const radius = width * 0.5;
    const bounds = logicalBounds(frame,
        Math.min(x1, x2) - radius, Math.min(y1, y2) - radius,
        Math.max(x1, x2) + radius, Math.max(y1, y2) + radius);
    const ax = (frame * FRAME + x1) * SCALE;
    const ay = y1 * SCALE;
    const bx = (frame * FRAME + x2) * SCALE;
    const by = y2 * SCALE;
    const vx = bx - ax;
    const vy = by - ay;
    const lengthSquared = vx * vx + vy * vy;
    const radiusSquared = (radius * SCALE) ** 2;

    for (let y = bounds.top; y < bounds.bottom; y += 1) {
        for (let x = bounds.left; x < bounds.right; x += 1) {
            const px = x + 0.5 - ax;
            const py = y + 0.5 - ay;
            const t = lengthSquared === 0
                ? 0
                : Math.max(0, Math.min(1, (px * vx + py * vy) / lengthSquared));
            const dx = px - vx * t;
            const dy = py - vy * t;
            if (dx * dx + dy * dy <= radiusSquared) {
                putPixel(canvas, x, y, color);
            }
        }
    }
}

function polyline(canvas, frame, points, width, color, closed = false) {
    for (let i = 0; i < points.length - 1; i += 1) {
        strokeLine(canvas, frame, points[i][0], points[i][1],
            points[i + 1][0], points[i + 1][1], width, color);
    }
    if (closed && points.length > 2) {
        strokeLine(canvas, frame, points[points.length - 1][0],
            points[points.length - 1][1], points[0][0], points[0][1], width, color);
    }
}

function drawLimb(canvas, frame, start, joint, end, palette, width = 5) {
    strokeLine(canvas, frame, start[0], start[1], joint[0], joint[1],
        width, palette.outline);
    strokeLine(canvas, frame, joint[0], joint[1], end[0], end[1],
        width, palette.outline);
    strokeLine(canvas, frame, start[0], start[1], joint[0], joint[1],
        width * 0.48, palette.limb);
    strokeLine(canvas, frame, joint[0], joint[1], end[0], end[1],
        width * 0.48, palette.limb);
    fillEllipse(canvas, frame, joint[0], joint[1], width * 0.64, width * 0.64,
        palette.outline);
    fillEllipse(canvas, frame, joint[0], joint[1], width * 0.32, width * 0.32,
        palette.light);
    fillEllipse(canvas, frame, end[0], end[1], width * 0.48, width * 0.48,
        palette.outline);
}

function drawBoot(canvas, frame, x, y, palette) {
    const boot = [[x - 3, y - 1], [x + 1, y - 1], [x + 4, y + 1],
        [x + 4, y + 3], [x - 3, y + 3]];
    fillPolygon(canvas, frame, boot, palette.outline);
    fillPolygon(canvas, frame, boot.map(([px, py]) => [px + 0.6, py + 0.5]),
        palette.boot);
    strokeLine(canvas, frame, x - 1.5, y + 2.2, x + 3, y + 2.2,
        0.7, palette.accent);
}

function drawBackpack(canvas, frame, palette) {
    const shape = [[22, 25], [17, 27], [16, 37], [19, 41], [25, 39], [26, 29]];
    fillPolygon(canvas, frame, shape, palette.outline);
    fillPolygon(canvas, frame, [[22, 27], [19, 28], [18, 36], [20, 38],
        [23, 37], [24, 30]], palette.dark);
    strokeLine(canvas, frame, 19, 30, 23, 29, 1.5, palette.light);
    strokeLine(canvas, frame, 19, 35, 23, 34, 1.1, palette.accent);
}

function drawHelmet(canvas, frame, palette, bob, isEnemy) {
    const cy = 13.5 + bob;
    fillEllipse(canvas, frame, 32, cy, 10.4, 9.6, palette.outline);
    fillEllipse(canvas, frame, 31.7, cy - 0.6, 8.8, 7.9, palette.body);
    fillEllipse(canvas, frame, 29.1, cy - 4.0, 4.6, 2.1, palette.light);
    fillEllipse(canvas, frame, 27.7, cy - 4.6, 1.6, 0.8, palette.shine);

    if (isEnemy) {
        fillPolygon(canvas, frame, [[23, 13 + bob], [39, 11 + bob],
            [40, 16 + bob], [25, 18 + bob]], palette.visor);
        strokeLine(canvas, frame, 27, 15 + bob, 37, 13.7 + bob,
            1.8, palette.glow);
        fillPolygon(canvas, frame, [[39, 10 + bob], [43, 13 + bob],
            [39, 14 + bob]], palette.accent);
    } else {
        fillPolygon(canvas, frame, [[23, 14 + bob], [38, 12.5 + bob],
            [40, 16 + bob], [25, 18 + bob]], palette.visor);
        strokeLine(canvas, frame, 26, 15.6 + bob, 37, 14.5 + bob,
            1.7, palette.accent);
        fillEllipse(canvas, frame, 36.5, 14.5 + bob, 1.2, 0.8, palette.glow);
    }
}

function drawTorso(canvas, frame, palette, bob, isEnemy) {
    const outer = [[24, 25 + bob], [37, 24 + bob], [42, 29 + bob],
        [40, 39 + bob], [36, 44 + bob], [27, 43 + bob], [22, 36 + bob]];
    fillPolygon(canvas, frame, outer, palette.outline);
    fillPolygon(canvas, frame, [[25, 27 + bob], [36, 26 + bob],
        [39, 30 + bob], [37, 38 + bob], [34, 41 + bob],
        [28, 40 + bob], [25, 35 + bob]], palette.body);
    fillPolygon(canvas, frame, [[26, 28 + bob], [32, 27 + bob],
        [32, 39 + bob], [28, 39 + bob], [26, 35 + bob]], palette.light);
    fillPolygon(canvas, frame, [[33, 28 + bob], [36, 28 + bob],
        [38, 31 + bob], [36, 36 + bob], [33, 37 + bob]], palette.dark);
    strokeLine(canvas, frame, 27, 31 + bob, 37, 30 + bob, 1.2, palette.shine);

    if (isEnemy) {
        fillPolygon(canvas, frame, [[28, 33 + bob], [35, 32 + bob],
            [36, 37 + bob], [29, 38 + bob]], palette.dark);
        strokeLine(canvas, frame, 29, 35 + bob, 35, 34 + bob,
            1.8, palette.accent);
    } else {
        fillPolygon(canvas, frame, [[28, 33 + bob], [35, 32 + bob],
            [36, 37 + bob], [29, 38 + bob]], palette.dark);
        fillPolygon(canvas, frame, [[29, 33 + bob], [31.5, 32.5 + bob],
            [31.5, 37.5 + bob], [29.5, 37 + bob]], palette.body);
        strokeLine(canvas, frame, 33, 34 + bob, 36, 33.5 + bob,
            1.3, palette.accent);
    }
    strokeLine(canvas, frame, 27, 41 + bob, 36, 40 + bob, 1.7, palette.outline);
}

function drawCharacter(canvas, frame, role) {
    const isEnemy = role === "enemy";
    const palette = PALETTE[role];
    const gait = [
        { bob: 0, leftKnee: [27, 49], leftFoot: [25, 57], rightKnee: [37, 49], rightFoot: [39, 57] },
        { bob: -0.8, leftKnee: [25, 48], leftFoot: [21, 56], rightKnee: [38, 49], rightFoot: [42, 57] },
        { bob: 0, leftKnee: [28, 49], leftFoot: [29, 58], rightKnee: [36, 48], rightFoot: [34, 56] },
        { bob: -0.8, leftKnee: [27, 48], leftFoot: [25, 57], rightKnee: [39, 49], rightFoot: [42, 56] }
    ][frame];
    const bob = gait.bob;
    const hipLeft = [29, 41 + bob];
    const hipRight = [35, 41 + bob];
    const ankleLeft = [gait.leftFoot[0], gait.leftFoot[1] - 2];
    const ankleRight = [gait.rightFoot[0], gait.rightFoot[1] - 2];

    drawLimb(canvas, frame, hipLeft, gait.leftKnee, ankleLeft, palette, 5.2);
    drawLimb(canvas, frame, hipRight, gait.rightKnee, ankleRight, palette, 5.2);
    drawBoot(canvas, frame, gait.leftFoot[0], gait.leftFoot[1], palette);
    drawBoot(canvas, frame, gait.rightFoot[0], gait.rightFoot[1], palette);

    drawBackpack(canvas, frame, palette);

    const leftShoulder = [25, 29 + bob];
    const leftElbow = [22 + (frame % 2 ? -1 : 1), 34 + bob];
    const leftHand = [24 + (frame % 2 ? -1 : 1), 38 + bob];
    drawLimb(canvas, frame, leftShoulder, leftElbow, leftHand, palette, 4.8);

    const rightShoulder = [38, 29 + bob];
    const rightElbow = isEnemy ? [42, 33 + bob] : [43, 32 + bob];
    const rightHand = isEnemy ? [45, 37 + bob] : [47, 30 + bob];
    drawLimb(canvas, frame, rightShoulder, rightElbow, rightHand, palette, 4.8);

    drawTorso(canvas, frame, palette, bob, isEnemy);
    drawHelmet(canvas, frame, palette, bob, isEnemy);

    if (isEnemy) {
        fillPolygon(canvas, frame, [[23, 28 + bob], [20, 30 + bob],
            [22, 33 + bob], [26, 31 + bob]], palette.body);
        fillPolygon(canvas, frame, [[37, 28 + bob], [41, 27 + bob],
            [43, 30 + bob], [39, 33 + bob]], palette.light);
        fillEllipse(canvas, frame, 45, 37 + bob, 2.2, 2.0, palette.glow);
    } else {
        fillPolygon(canvas, frame, [[23, 28 + bob], [20, 30 + bob],
            [22, 33 + bob], [26, 31 + bob]], palette.body);
        fillPolygon(canvas, frame, [[36, 27 + bob], [40, 27 + bob],
            [42, 30 + bob], [39, 33 + bob]], palette.light);

        strokeLine(canvas, frame, 47, 31 + bob, 49, 37 + bob, 3.4, palette.outline);
        strokeLine(canvas, frame, 46, 28 + bob, 48, 32 + bob, 3.2, palette.outline);
        strokeLine(canvas, frame, 52, 27 + bob, 49, 32 + bob, 3.2, palette.outline);
        strokeLine(canvas, frame, 47, 31 + bob, 49, 37 + bob, 1.5, palette.accent);
        strokeLine(canvas, frame, 46, 28 + bob, 48, 32 + bob, 1.3, palette.light);
        strokeLine(canvas, frame, 52, 27 + bob, 49, 32 + bob, 1.3, palette.light);
        strokeLine(canvas, frame, 46.5, 29 + bob, 50, 34 + bob, 0.9, palette.glow);
    }
}

function drawWeaponIcon(canvas, frame, type) {
    const metal = [48, 59, 73, 255];
    const dark = [22, 29, 40, 255];
    const light = [151, 177, 198, 255];
    const accent = [244, 157, 55, 255];
    const cyan = [118, 229, 255, 255];
    if (type === 0) {
        strokeLine(canvas, frame, 29, 19, 34, 39, 8, dark);
        strokeLine(canvas, frame, 29, 19, 23, 12, 7, dark);
        strokeLine(canvas, frame, 29, 19, 38, 11, 7, dark);
        strokeLine(canvas, frame, 29, 19, 34, 39, 4, [139, 91, 54, 255]);
        strokeLine(canvas, frame, 29, 18, 24, 12, 3, accent);
        strokeLine(canvas, frame, 30, 18, 37, 12, 3, accent);
        fillEllipse(canvas, frame, 32, 35, 4.5, 3.2, cyan);
    } else if (type === 1) {
        fillPolygon(canvas, frame, [[12, 23], [47, 21], [52, 25], [48, 29],
            [23, 29], [18, 34], [13, 33]], dark);
        fillPolygon(canvas, frame, [[15, 23], [46, 22], [49, 25], [45, 27],
            [22, 27], [18, 31], [15, 30]], metal);
        fillPolygon(canvas, frame, [[30, 28], [39, 28], [36, 39], [30, 39]], dark);
        strokeLine(canvas, frame, 39, 24, 51, 24, 4, light);
        strokeLine(canvas, frame, 23, 23, 31, 23, 2, accent);
        strokeLine(canvas, frame, 22, 31, 32, 31, 2, accent);
    } else if (type === 2) {
        fillPolygon(canvas, frame, [[8, 24], [45, 22], [55, 24], [55, 28],
            [24, 30], [18, 35], [12, 34]], dark);
        fillPolygon(canvas, frame, [[10, 24], [45, 23], [52, 25], [48, 26],
            [23, 28], [17, 32], [13, 31]], metal);
        fillPolygon(canvas, frame, [[32, 28], [39, 28], [36, 39], [31, 39]], dark);
        fillPolygon(canvas, frame, [[27, 18], [39, 17], [41, 22], [29, 23]], dark);
        fillPolygon(canvas, frame, [[29, 19], [38, 18], [39, 21], [30, 22]], cyan);
        strokeLine(canvas, frame, 44, 24, 56, 23, 1.8, light);
        strokeLine(canvas, frame, 16, 26, 25, 25, 1.4, accent);
    } else {
        fillPolygon(canvas, frame, [[10, 24], [43, 22], [53, 24], [53, 29],
            [22, 31], [17, 36], [11, 34]], dark);
        fillPolygon(canvas, frame, [[12, 24], [42, 23], [50, 25], [47, 27],
            [22, 29], [16, 33], [13, 31]], metal);
        fillPolygon(canvas, frame, [[30, 29], [38, 28], [36, 39], [30, 39]], dark);
        for (let i = 0; i < 4; i += 1) {
            const x = 39 + i * 3;
            fillPolygon(canvas, frame, [[x, 19], [x + 2, 18], [x + 3, 25], [x + 1, 26]], light);
        }
        fillEllipse(canvas, frame, 26, 30, 8, 7, dark);
        fillEllipse(canvas, frame, 26, 30, 5.2, 4.8, accent);
        fillEllipse(canvas, frame, 26, 30, 2.2, 2.0, metal);
    }
}

function drawEnemyPart(canvas, frame, part) {
    const p = PALETTE.enemy;
    if (part === 0) {
        fillEllipse(canvas, frame, 32, 30, 17, 15, p.outline);
        fillEllipse(canvas, frame, 31, 27, 13, 11, p.body);
        fillPolygon(canvas, frame, [[17, 28], [46, 25], [48, 33], [19, 37]], p.visor);
        strokeLine(canvas, frame, 22, 31, 43, 29, 2.5, p.glow);
        fillPolygon(canvas, frame, [[18, 21], [27, 14], [36, 16], [44, 25],
            [36, 22], [24, 25]], p.light);
    } else if (part === 1) {
        fillPolygon(canvas, frame, [[19, 12], [42, 10], [51, 20], [47, 46],
            [38, 54], [24, 50], [14, 31]], p.outline);
        fillPolygon(canvas, frame, [[22, 15], [40, 14], [46, 22], [42, 42],
            [36, 48], [26, 45], [19, 30]], p.body);
        fillPolygon(canvas, frame, [[23, 18], [31, 16], [31, 43], [25, 41],
            [21, 29]], p.light);
        strokeLine(canvas, frame, 24, 31, 42, 29, 3, p.accent);
    } else if (part === 2) {
        strokeLine(canvas, frame, 18, 17, 31, 32, 12, p.outline);
        strokeLine(canvas, frame, 31, 32, 47, 47, 10, p.outline);
        strokeLine(canvas, frame, 18, 17, 31, 32, 6, p.limb);
        strokeLine(canvas, frame, 31, 32, 47, 47, 5, p.light);
        fillEllipse(canvas, frame, 31, 32, 6, 6, p.accent);
    } else {
        strokeLine(canvas, frame, 22, 12, 30, 30, 11, p.outline);
        strokeLine(canvas, frame, 30, 30, 39, 48, 10, p.outline);
        strokeLine(canvas, frame, 22, 12, 30, 30, 5.5, p.limb);
        strokeLine(canvas, frame, 30, 30, 39, 48, 5, p.boot);
        strokeLine(canvas, frame, 34, 45, 45, 45, 4, p.accent);
    }
}

function downsample(canvas) {
    const output = Buffer.alloc(WIDTH * HEIGHT * 4);
    const sampleCount = SCALE * SCALE;
    for (let y = 0; y < HEIGHT; y += 1) {
        for (let x = 0; x < WIDTH; x += 1) {
            let red = 0;
            let green = 0;
            let blue = 0;
            let alpha = 0;
            for (let dy = 0; dy < SCALE; dy += 1) {
                for (let dx = 0; dx < SCALE; dx += 1) {
                    const source = (((y * SCALE + dy) * PIXEL_WIDTH) +
                        (x * SCALE + dx)) * 4;
                    const a = canvas[source + 3];
                    red += canvas[source] * a;
                    green += canvas[source + 1] * a;
                    blue += canvas[source + 2] * a;
                    alpha += a;
                }
            }
            const target = (y * WIDTH + x) * 4;
            if (alpha > 0) {
                output[target] = Math.round(red / alpha);
                output[target + 1] = Math.round(green / alpha);
                output[target + 2] = Math.round(blue / alpha);
            }
            output[target + 3] = Math.round(alpha / sampleCount);
        }
    }
    return output;
}

const crcTable = new Uint32Array(256);
for (let n = 0; n < 256; n += 1) {
    let value = n;
    for (let bit = 0; bit < 8; bit += 1) {
        value = (value & 1) ? (0xedb88320 ^ (value >>> 1)) : (value >>> 1);
    }
    crcTable[n] = value >>> 0;
}

function crc32(buffer) {
    let value = 0xffffffff;
    for (const byte of buffer) {
        value = crcTable[(value ^ byte) & 0xff] ^ (value >>> 8);
    }
    return (value ^ 0xffffffff) >>> 0;
}

function pngChunk(type, data) {
    const name = Buffer.from(type, "ascii");
    const body = Buffer.concat([name, data]);
    const chunk = Buffer.alloc(data.length + 12);
    chunk.writeUInt32BE(data.length, 0);
    body.copy(chunk, 4);
    chunk.writeUInt32BE(crc32(body), data.length + 8);
    return chunk;
}

function encodePng(rgba) {
    const rowBytes = WIDTH * 4;
    const raw = Buffer.alloc((rowBytes + 1) * HEIGHT);
    for (let y = 0; y < HEIGHT; y += 1) {
        const row = y * (rowBytes + 1);
        raw[row] = 0;
        rgba.copy(raw, row + 1, y * rowBytes, (y + 1) * rowBytes);
    }

    const header = Buffer.alloc(13);
    header.writeUInt32BE(WIDTH, 0);
    header.writeUInt32BE(HEIGHT, 4);
    header[8] = 8;
    header[9] = 6;
    header[10] = 0;
    header[11] = 0;
    header[12] = 0;

    return Buffer.concat([
        Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
        pngChunk("IHDR", header),
        pngChunk("IDAT", zlib.deflateSync(raw, { level: 9 })),
        pngChunk("IEND", Buffer.alloc(0))
    ]);
}

function generateSheet(role) {
    const canvas = makeCanvas();
    for (let frame = 0; frame < FRAME_COUNT; frame += 1) {
        drawCharacter(canvas, frame, role);
    }
    return encodePng(downsample(canvas));
}

function generateWeaponSheet() {
    const canvas = makeCanvas();
    for (let frame = 0; frame < FRAME_COUNT; frame += 1) {
        drawWeaponIcon(canvas, frame, frame);
    }
    return encodePng(downsample(canvas));
}

function generateEnemyPartsSheet() {
    const canvas = makeCanvas();
    for (let frame = 0; frame < FRAME_COUNT; frame += 1) {
        drawEnemyPart(canvas, frame, frame);
    }
    return encodePng(downsample(canvas));
}

const spriteDirectory = path.resolve(__dirname, "..", "assets", "sprites");
fs.mkdirSync(spriteDirectory, { recursive: true });
for (const role of ["player", "enemy"]) {
    const output = path.join(spriteDirectory, `${role}.png`);
    fs.writeFileSync(output, generateSheet(role));
    console.log(`Generated ${output} (${WIDTH}x${HEIGHT}, RGBA PNG)`);
}
for (const [name, image] of [
    ["weapons", generateWeaponSheet()],
    ["enemy_parts", generateEnemyPartsSheet()]
]) {
    const output = path.join(spriteDirectory, `${name}.png`);
    fs.writeFileSync(output, image);
    console.log(`Generated ${output} (${WIDTH}x${HEIGHT}, RGBA PNG)`);
}

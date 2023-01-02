/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2022 John Maloney, Bernat Romagosa, and Jens Mönig

// boardieTftPrims.cpp - Microblocks TFT primitives simulated on JS Canvas
// Bernat Romagosa, November 2022

#include <stdio.h>
#include <stdlib.h>

#include <emscripten.h>

#include "mem.h"
#include "interp.h"

#define DEFAULT_WIDTH 240
#define DEFAULT_HEIGHT 240

static int touchEnabled = false;
static int tftEnabled = false;
static int tftShouldUpdate = false;

void EMSCRIPTEN_KEEPALIVE tftChanged() { tftShouldUpdate = true; }

void tftClear() {
	tftInit();
	EM_ASM_({
		window.ctx.fillStyle = "#000";
		window.ctx.fillRect(0, 0, $0, $1);
	}, DEFAULT_WIDTH, DEFAULT_HEIGHT);
	tftChanged();
}

void tftInit() {
	if (!tftEnabled) {
		tftEnabled = true;
		EM_ASM_({
			// initialize an offscreen canvas
			window.offscreenCanvas = document.createElement('canvas');
			window.offscreenCanvas.width = $0;
			window.offscreenCanvas.height = $1;
			window.ctx = window.offscreenCanvas.getContext('2d', { alpha: false });
			window.ctx.imageSmoothingEnabled = false;

			var adafruitFont = new FontFace('adafruit', 'url(adafruit_font.ttf)');

			adafruitFont.load().then((font) => { document.fonts.add(font); });

			window.rgbFrom24b = function (color24b) {
				return 'rgb(' + ((color24b >> 16) & 255) + ',' +
					((color24b >> 8) & 255) + ',' + (color24b & 255) + ')';
			};
		}, DEFAULT_WIDTH, DEFAULT_HEIGHT);
	}
}

void updateMicrobitDisplay() {
	// transfer the offscreen canvas image to the main canvas
	if (tftEnabled && tftShouldUpdate) {
		EM_ASM_({
			if (!window.mainCtx) {
				window.mainCtx =
					document.querySelector('#screen').getContext(
						'2d',
						{ alpha: false }
					);
				window.mainCtx.imageSmoothingEnabled = false;
			}
			window.mainCtx.drawImage(window.offscreenCanvas, 0, 0);
		});
		tftShouldUpdate = false;
	}
}

// TFT Primitives

static OBJ primEnableDisplay(int argCount, OBJ *args) {
	if (trueObj == args[0]) {
		tftInit();
	} else {
		tftClear();
	}
	return falseObj;
}

static OBJ primGetWidth(int argCount, OBJ *args) {
	return int2obj(DEFAULT_WIDTH);
}

static OBJ primGetHeight(int argCount, OBJ *args) {
	return int2obj(DEFAULT_HEIGHT);
}

static OBJ primSetPixel(int argCount, OBJ *args) {
	tftInit();
	EM_ASM_({
			window.ctx.fillStyle = window.rgbFrom24b($2);
			window.ctx.fillRect($0, $1, 1, 1);
		},
		obj2int(args[0]), // x
		obj2int(args[1]), // y
		obj2int(args[2]) // color
	);
	tftChanged();
	return falseObj;
}

static OBJ primLine(int argCount, OBJ *args) {
	tftInit();
	EM_ASM_({
			window.ctx.strokeStyle = window.rgbFrom24b($4);
			window.ctx.beginPath();
			window.ctx.moveTo($0, $1);
			window.ctx.lineTo($2, $3);
			window.ctx.stroke();
		},
		obj2int(args[0]), // x0
		obj2int(args[1]), // y0
		obj2int(args[2]), // x1
		obj2int(args[3]), // y1
		obj2int(args[4]) // color
	);
	tftChanged();
	return falseObj;
}

static OBJ primRect(int argCount, OBJ *args) {
	tftInit();
	EM_ASM_({
			if ($5) {
				window.ctx.fillStyle = window.rgbFrom24b($4);
				window.ctx.fillRect($0, $1, $2, $3);
			} else {
				window.ctx.strokeStyle = window.rgbFrom24b($4);
				window.ctx.strokeRect($0, $1, $2, $3);
			}
		},
		obj2int(args[0]), // x
		obj2int(args[1]), // y
		obj2int(args[2]), // width
		obj2int(args[3]), // height
		obj2int(args[4]), // color
		(argCount > 5) ? (trueObj == args[5]) : true // fill
	);
	tftChanged();
	return falseObj;
}

static OBJ primCircle(int argCount, OBJ *args) {
	tftInit();
	EM_ASM_({
			var x = $0;
			var y = $1;
			var r = $2;
			var fill = $4;
			if (fill) {
				window.ctx.fillStyle = window.rgbFrom24b($3);
				window.ctx.beginPath();
				window.ctx.arc(x, y, r, 0, 2 * Math.PI);
				window.ctx.fill();
			} else {
				window.ctx.strokeStyle = window.rgbFrom24b($3);
				window.ctx.beginPath();
				window.ctx.arc(x, y, r, 0, 2 * Math.PI);
				window.ctx.stroke();
			}
		},
		obj2int(args[0]), // x
		obj2int(args[1]), // y
		obj2int(args[2]), // radius
		obj2int(args[3]), // color
		(argCount > 4) ? (trueObj == args[4]) : true // fill
	);
	tftChanged();
	return falseObj;
}

static OBJ primRoundedRect(int argCount, OBJ *args) {
	tftInit();
	int width = obj2int(args[2]);
	int height = obj2int(args[3]);
	int radius = obj2int(args[4]);

	if (2 * radius >= height) {
		radius = height / 2 - 1;
	}
	if (2 * radius >= width) {
		radius = width / 2 - 1;
	}

	EM_ASM_({
			if ($6) {
				window.ctx.fillStyle = window.rgbFrom24b($5);
			} else {
				window.ctx.strokeStyle = window.rgbFrom24b($5);
			}
			var x = $0;
			var y = $1;
			var w = $2;
			var h = $3;
			var r = $4;

			window.ctx.beginPath();
			window.ctx.moveTo(x + r, y);

			window.ctx.lineTo(x + w - r, y); // top
			window.ctx.arc(x + w - r, y + r, r, 3 * Math.PI / 2, 0, false); // t-r

			window.ctx.lineTo(x + w, y + h - r); // right
			window.ctx.arc(x + w - r, y + h - r, r, 0, Math.PI / 2, false); // b-r

			window.ctx.lineTo(x + r, y + h); // bottom
			window.ctx.arc(x + r, y + h - r, r, Math.PI / 2, Math.PI, false); // b-l

			window.ctx.lineTo(x, y + r); // left
			window.ctx.arc(x + r, y + r, r, Math.PI , 3 * Math.PI / 2, false); // t-l

			if ($6) {
				window.ctx.fill();
			} else {
				window.ctx.stroke();
			}
		},
		obj2int(args[0]), // x
		obj2int(args[1]), // y
		width, // (2)
		height, // (3)
		radius, // (4)
		obj2int(args[5]), // color
		(argCount > 6) ? (trueObj == args[6]) : true // fill (6)
	);
	tftChanged();
	return falseObj;
}

static OBJ primTriangle(int argCount, OBJ *args) {
	tftInit();

	// draw triangle
	EM_ASM_({
			if ($7) {
				window.ctx.fillStyle = window.rgbFrom24b($6);
			} else {
				window.ctx.strokeStyle = window.rgbFrom24b($6);
			}
			window.ctx.beginPath();
			window.ctx.moveTo($0, $1);
			window.ctx.lineTo($2, $3);
			window.ctx.lineTo($3, $4);
			window.ctx.closePath();
			if ($7) {
				window.ctx.fill();
			} else {
				window.ctx.stroke();
			}
		},
		obj2int(args[0]), // x0
		obj2int(args[1]), // y0
		obj2int(args[2]), // x1
		obj2int(args[3]), // y1
		obj2int(args[4]), // x2
		obj2int(args[5]), // y2
		obj2int(args[6]), // color
		(argCount > 7) ? (trueObj == args[7]) : true // fill
	);
	tftChanged();
	return falseObj;
}

static OBJ primText(int argCount, OBJ *args) {
	tftInit();
	OBJ value = args[0];
	char text[256];

	if (IS_TYPE(value, StringType)) {
		sprintf(text, "%s", obj2str(value));
	} else if (trueObj == value) {
		sprintf(text, "true");
	} else if (falseObj == value) {
		sprintf(text, "false");
	} else if (isInt(value)) {
		sprintf(text, "%d", obj2int(value));
	}

	// draw text
	EM_ASM_({
			var text = UTF8ToString($0);
			// subtract a little bit from x, proportional to font scale, to make
			// it match the font on physical boards
			var x = $1 - $4;
			var y = $2 - $4;
			// there is a weird rounding artifact at scale 3
			var fontSize = ($4 == 3) ? ($4 * 10.5) : ($4 * 11);
			window.ctx.font = fontSize + 'px adafruit';
			window.ctx.fillStyle = window.rgbFrom24b($3);
			window.ctx.textBaseline = 'top';
			text.split("").forEach(
				(c) => {
					window.ctx.fillText(c, x, y);
					x += $4 * 6;
					if ($5 && (x + $4 * 6 >= window.ctx.canvas.width)) {
						// wrap
						x = 0 - $4;
						y += fontSize * 3 / 4;
					}
				}
			);
		},
		text, // text
		obj2int(args[1]), // x
		obj2int(args[2]), // y
		obj2int(args[3]), // color
		(argCount > 4) ? obj2int(args[4]) : 2, // scale
		(argCount > 5) ? (trueObj == args[5]) : true // wrap
	);

	tftChanged();
	return falseObj;
}

// 8-bit graphics primitives

static OBJ primVGAPalette(int argCount, OBJ *args) {
	OBJ palette = newObj(ListType, 257, zeroObj);
	FIELD(palette, 0) = int2obj(256);
	int colors[256] = {
		0x000000, 0x0000aa, 0x00aa00, 0x00aaaa, 0xaa0000, 0xaa00aa, 0xaa5500,
		0xaaaaaa, 0x555555, 0x5555ff, 0x55ff55, 0x55ffff, 0xff5555, 0xff55ff,
		0xffff55, 0xffffff, 0x000000, 0x141414, 0x202020, 0x2c2c2c, 0x383838,
		0x454545, 0x515151, 0x616161, 0x717171, 0x828282, 0x929292, 0xa2a2a2,
		0xb6b6b6, 0xcbcbcb, 0xe3e3e3, 0xffffff, 0x0000ff, 0x4100ff, 0x7d00ff,
		0xbe00ff, 0xff00ff, 0xff00be, 0xff007d, 0xff0041, 0xff0000, 0xff4100,
		0xff7d00, 0xffbe00, 0xffff00, 0xbeff00, 0x7dff00, 0x41ff00, 0x00ff00,
		0x00ff41, 0x00ff7d, 0x00ffbe, 0x00ffff, 0x00beff, 0x007dff, 0x0041ff,
		0x7d7dff, 0x9e7dff, 0xbe7dff, 0xdf7dff, 0xff7dff, 0xff7ddf, 0xff7dbe,
		0xff7d9e, 0xff7d7d, 0xff9e7d, 0xffbe7d, 0xffdf7d, 0xffff7d, 0xdfff7d,
		0xbeff7d, 0x9eff7d, 0x7dff7d, 0x7dff9e, 0x7dffbe, 0x7dffdf, 0x7dffff,
		0x7ddfff, 0x7dbeff, 0x7d9eff, 0xb6b6ff, 0xc7b6ff, 0xdbb6ff, 0xebb6ff,
		0xffb6ff, 0xffb6eb, 0xffb6db, 0xffb6c7, 0xffb6b6, 0xffc7b6, 0xffdbb6,
		0xffebb6, 0xffffb6, 0xebffb6, 0xdbffb6, 0xc7ffb6, 0xb6ffb6, 0xb6ffc7,
		0xb6ffdb, 0xb6ffeb, 0xb6ffff, 0xb6ebff, 0xb6dbff, 0xb6c7ff, 0x000071,
		0x1c0071, 0x380071, 0x550071, 0x710071, 0x710055, 0x710038, 0x71001c,
		0x710000, 0x711c00, 0x713800, 0x715500, 0x717100, 0x557100, 0x387100,
		0x1c7100, 0x007100, 0x00711c, 0x007138, 0x007155, 0x007171, 0x005571,
		0x003871, 0x001c71, 0x383871, 0x453871, 0x553871, 0x613871, 0x713871,
		0x713861, 0x713855, 0x713845, 0x713838, 0x714538, 0x715538, 0x716138,
		0x717138, 0x617138, 0x557138, 0x457138, 0x387138, 0x387145, 0x387155,
		0x387161, 0x387171, 0x386171, 0x385571, 0x384571, 0x515171, 0x595171,
		0x615171, 0x695171, 0x715171, 0x715169, 0x715161, 0x715159, 0x715151,
		0x715951, 0x716151, 0x716951, 0x717151, 0x697151, 0x617151, 0x597151,
		0x517151, 0x517159, 0x517161, 0x517169, 0x517171, 0x516971, 0x516171,
		0x515971, 0x000041, 0x100041, 0x200041, 0x300041, 0x410041, 0x410030,
		0x410020, 0x410010, 0x410000, 0x411000, 0x412000, 0x413000, 0x414100,
		0x304100, 0x204100, 0x104100, 0x004100, 0x004110, 0x004120, 0x004130,
		0x004141, 0x003041, 0x002041, 0x001041, 0x202041, 0x282041, 0x302041,
		0x382041, 0x412041, 0x412038, 0x412030, 0x412028, 0x412020, 0x412820,
		0x413020, 0x413820, 0x414120, 0x384120, 0x304120, 0x284120, 0x204120,
		0x204128, 0x204130, 0x204138, 0x204141, 0x203841, 0x203041, 0x202841,
		0x2c2c41, 0x302c41, 0x342c41, 0x3c2c41, 0x412c41, 0x412c3c, 0x412c34,
		0x412c30, 0x412c2c, 0x41302c, 0x41342c, 0x413c2c, 0x41412c, 0x3c412c,
		0x34412c, 0x30412c, 0x2c412c, 0x2c4130, 0x2c4134, 0x2c413c, 0x2c4141,
		0x2c3c41, 0x2c3441, 0x2c3041, 0x000000, 0x000000, 0x000000, 0x000000,
		0x000000, 0x000000, 0x000000, 0x000000
	};
	for (int i = 0; i < 255; i++) FIELD(palette, i + 1) = int2obj(colors[i]);
	return palette;
}

static OBJ primSetPalette(int argCount, OBJ *args) {
	OBJ list = args[0];
	EM_ASM_({ window.palette = []; });
	for (int i = 0; i < obj2int(FIELD(list, 0)); i++) {
		EM_ASM_(
			{ window.palette[$1] = $0;},
			obj2int(FIELD(list, i + 1)),	// $0, list item
			i								// $1, index
		);
	}
	EM_ASM_({ window['palette'] = window.palette; });
	return falseObj;
}

static OBJ primPalette(int argCount, OBJ *args) {
	// This fails after a bunch of times
	int size =
		EM_ASM_INT({ return window.palette ? window.palette.length : 0 });
	OBJ palette = newObj(ListType, size + 1, 0);
	FIELD(palette, 0) = int2obj(size);
	for (int i = 1; i <= size; i++) {
		FIELD(palette, i) =
			int2obj(EM_ASM_INT({ return window.palette[$0] }, i - 1));
	}
	return palette;
};

static OBJ primCroppedBitmap(int argCount, OBJ *args) {
	int width = obj2int(args[4]);
	int height = obj2int(args[5]);
	int originX = obj2int(args[1]);
	int originY = obj2int(args[2]);

	// Size is incorrect!
	OBJ newBitmap =
		newObj(
			ByteArrayType,
			((width - originX) * (height - originY) + 3) / 4,
			0
		);

	EM_ASM_({
			for (var y = 0; y < $5; y++) {
				for (var x = 0; x < $4; x++) {
					HEAPU8[$6 + y * $4 + x] =
						HEAPU8[$0 + ($2 + y) * $3 + $1 + x];
				}
			}
		},
		(uint8 *) &FIELD(args[0], 0),	// $0, bitmap
		originX,						// $1, origin x
		originY,						// $2, origin y
		obj2int(args[3]),				// $3, origin width
		width,							// $4, crop width
		height,							// $5, crop height
		(uint8 *) &FIELD(newBitmap, 0) 	// $6, cropped bitmap
	);
	return newBitmap;
}

static OBJ primMergeBitmap(int argCount, OBJ *args) {
	EM_ASM_({
			var bitmapHeight = $2 / $1;
			for (var y = 0; y < bitmapHeight; y++) {
				for (var x = 0; x < $1; x++) {
					var bufIndex =
						($7 + y) * (window.ctx.canvas.width / $4) + x + $6;
					var pixelValue = HEAPU8[$0 + y * $1 + x];
					if (pixelValue !== $5) {
						HEAPU8[$3 + bufIndex] = pixelValue;
					}
				}
			}
		},
		(uint8 *) &FIELD(args[0], 0),	// $0, bitmap
		obj2int(args[1]),				// $1, bitmap width
		BYTES(args[0]),					// $2, bitmap byte size
		(uint8 *) &FIELD(args[2], 0),	// $3, buffer
		obj2int(args[3]),				// $4, scale
		obj2int(args[4]),				// $5, transparent color index
		obj2int(args[5]),				// $6, destination x
		obj2int(args[6])				// $7, destination y
	);
	return falseObj;
}

static OBJ primDrawBuffer(int argCount, OBJ *args) {
	tftInit();
	EM_ASM_({
			var scale = $1 || 1;
			var scaledWidth = Math.ceil(window.ctx.canvas.width / scale);
			var scaledHeight = Math.ceil(window.ctx.canvas.height / scale);
			var imgData = window.ctx.createImageData(scaledWidth, scaledHeight);
			var pixelIndex = 0;

			// Make an extra canvas where we'll draw the buffer first, so we can
			// later scale it up to the offscreen canvas.
			var canvas = document.createElement('canvas');
			canvas.width = scaledWidth;
			canvas.height = scaledHeight;

			// Read the indices from the buffer and turn them into color values
			// from the palette, and transfer them into the imgData object.
			for (var y = 0; y < scaledHeight; y ++) {
				for (var x = 0; x < scaledWidth; x ++) {
					var index = HEAPU8[$0 + y * scaledHeight + x];
					if (index != $2) { // if not transparent
						var color =
							window.palette[index];
						imgData.data[pixelIndex] = color >> 16; // R
						imgData.data[pixelIndex + 1] = (color >> 8) & 255; // G
						imgData.data[pixelIndex + 2] = color & 255; // B
						imgData.data[pixelIndex + 3] = 255; // A
					}
					pixelIndex += 4;
				}
			}
			
			// Draw the image data into the canvas, then draw it scaled to the
			// offscreen canvas.
			// Note: we could do the same without an extra canvas by using
			// createImageBitmap, but that returns a promise and we need this
			// operation to be synchronous. Additionally, we've measured this
			// method to be pretty much as fast as the other one.
			canvas.getContext('2d').putImageData(imgData, 0, 0);
			window.ctx.drawImage(
				canvas,
				0,
				0,
				window.ctx.canvas.width,
				window.ctx.canvas.height
			);
			_tftChanged();
		},
		(uint8 *) &FIELD(args[0], 0),	// $0, buffer
		obj2int(args[1]),				// $1, scale
		obj2int(args[2])				// $2, transparent color index
	);
	yield();
	return falseObj;
}

// Simulating a 5x5 LED Matrix

void tftSetHugePixel(int x, int y, int state) {
	// simulate a 5x5 array of square pixels like the micro:bit LED array
	tftInit();
	int minDimension, xInset = 0, yInset = 0;
	if (DEFAULT_WIDTH > DEFAULT_HEIGHT) {
		minDimension = DEFAULT_HEIGHT;
		xInset = (DEFAULT_WIDTH - DEFAULT_HEIGHT) / 2;
	} else {
		minDimension = DEFAULT_WIDTH;
		yInset = (DEFAULT_HEIGHT - DEFAULT_WIDTH) / 2;
	}
	int lineWidth = (minDimension > 60) ? 3 : 1;
	int squareSize = (minDimension - (6 * lineWidth)) / 5;

	EM_ASM_({
			window.ctx.fillStyle = $3 == 0 ? '#000' : '#0F0';
			window.ctx.fillRect($0, $1, $2, $2);
		},
		xInset + ((x - 1) * squareSize) + (x * lineWidth), // x
		yInset + ((y - 1) * squareSize) + (y * lineWidth), // y
		squareSize,
		state
	);

	tftChanged();
}

void tftSetHugePixelBits(int bits) {
	if (0 == bits) {
		tftClear();
	} else {
		for (int x = 1; x <= 5; x++) {
			for (int y = 1; y <= 5; y++) {
				tftSetHugePixel(x, y, bits & (1 << ((5 * (y - 1) + x) - 1)));
			}
		}
	}
}

// TFT Touch Primitives

// Mouse support
void initMouseHandler() {
	if (!touchEnabled) {
		EM_ASM_({
			var screen = window.document.querySelector('#screen');
			window.mouseX = 0;
			window.mouseY = 0;
			window.mouseDown = false;
			window.mouseDownTime = 0;
			screen.addEventListener('pointermove', function (event) {
				window.mouseX = Math.floor(event.clientX - screen.offsetLeft);
				window.mouseY = Math.floor(event.clientY - screen.offsetTop);
			}, false);
			screen.addEventListener('pointerdown', function (event) {
				window.mouseDown = true;
				window.mouseDownTime = Date.now();
			}, false);
			screen.addEventListener('pointerup', function (event) {
				window.mouseDown = false;
				window.mouseDownTime = 0;
			}, false);
		});
	touchEnabled = true;
	}
}

static OBJ primTftTouched(int argCount, OBJ *args) {
	initMouseHandler();
	return EM_ASM_INT({ return window.mouseDown }) ? trueObj : falseObj;
}

static OBJ primTftTouchX(int argCount, OBJ *args) {
	initMouseHandler();
	return int2obj(EM_ASM_INT({
		return window.mouseDown ? window.mouseX : -1
	}));
}

static OBJ primTftTouchY(int argCount, OBJ *args) {
	initMouseHandler();
	return int2obj(EM_ASM_INT({
		return window.mouseDown ? window.mouseY : -1
	}));
}

static OBJ primTftTouchPressure(int argCount, OBJ *args) {
	initMouseHandler();
	return int2obj(EM_ASM_INT({
		if (window.mouseDown) {
			var mousePressure = (Date.now() - window.mouseDownTime);
			if (mousePressure > 4095) { mousePressure = 4095; }
		} else {
			mousePressure = -1;
		}
		return mousePressure;
	}));
}

// Primitives

static PrimEntry entries[] = {
	{"enableDisplay", primEnableDisplay},
	{"getWidth", primGetWidth},
	{"getHeight", primGetHeight},
	{"setPixel", primSetPixel},
	{"line", primLine},
	{"rect", primRect},
	{"roundedRect", primRoundedRect},
	{"circle", primCircle},
	{"triangle", primTriangle},
	{"text", primText},

	{"setPalette", primSetPalette},
	{"vgaPalette", primVGAPalette},
	{"currentPalette", primPalette},
	{"croppedBitmap", primCroppedBitmap},
	{"mergeBitmap", primMergeBitmap},
	{"drawBuffer", primDrawBuffer},

	{"tftTouched", primTftTouched},
	{"tftTouchX", primTftTouchX},
	{"tftTouchY", primTftTouchY},
	{"tftTouchPressure", primTftTouchPressure},
};

void addTFTPrims() {
	addPrimitiveSet("tft", sizeof(entries) / sizeof(PrimEntry), entries);
}

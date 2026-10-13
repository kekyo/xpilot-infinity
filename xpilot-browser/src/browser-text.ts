// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** Measurements supplied by a browser text layout. */
export interface BrowserTextMetrics {
  /** Logical advance in CSS pixels. */
  readonly width: number;
  /** Ink distance to the left of the alignment origin. */
  readonly actualBoundingBoxLeft: number;
  /** Ink distance to the right of the alignment origin. */
  readonly actualBoundingBoxRight: number;
  /** Ink distance above the alphabetic baseline. */
  readonly actualBoundingBoxAscent: number;
  /** Ink distance below the alphabetic baseline. */
  readonly actualBoundingBoxDescent: number;
}

/** Pixel storage and logical placement of a baked text line. */
export interface TextRasterBounds {
  /** Logical text advance, independent of visible ink. */
  readonly advance: number;
  /** Width of the RGBA bitmap in physical pixels. */
  readonly pixelWidth: number;
  /** Height of the RGBA bitmap in physical pixels. */
  readonly pixelHeight: number;
  /** Logical bitmap left relative to the text origin. */
  readonly offsetX: number;
  /** Logical bitmap top relative to the alphabetic baseline. */
  readonly offsetY: number;
  /** Horizontal drawing origin inside the bitmap, in physical pixels. */
  readonly baselineX: number;
  /** Vertical drawing origin inside the bitmap, in physical pixels. */
  readonly baselineY: number;
  /** Physical pixels per logical CSS pixel. */
  readonly scale: number;
}

/**
 * Allocate an outward-rounded text rectangle including italic overhangs.
 * @param metrics Measurements from the same layout that will be drawn.
 * @param scale Physical pixels per logical pixel; must be finite and positive.
 * @returns Bitmap bounds with one transparent physical pixel of padding.
 * @remarks Padding protects antialiased edges when the texture is filtered.
 * Logical advance is retained independently of the bitmap's ink bounds.
 */
export const calculateTextRasterBounds = (
  metrics: BrowserTextMetrics,
  scale: number
): TextRasterBounds => {
  if (
    !Number.isFinite(scale) ||
    scale <= 0 ||
    ![
      metrics.width,
      metrics.actualBoundingBoxLeft,
      metrics.actualBoundingBoxRight,
      metrics.actualBoundingBoxAscent,
      metrics.actualBoundingBoxDescent,
    ].every(Number.isFinite) ||
    metrics.width < 0
  ) {
    throw new RangeError('Invalid text measurements or raster scale');
  }

  const left =
    Math.floor(Math.min(0, -metrics.actualBoundingBoxLeft) * scale) - 1;
  const right =
    Math.ceil(Math.max(metrics.width, metrics.actualBoundingBoxRight) * scale) +
    1;
  const top = Math.floor(-metrics.actualBoundingBoxAscent * scale) - 1;
  const bottom = Math.ceil(metrics.actualBoundingBoxDescent * scale) + 1;
  const pixelWidth = right - left;
  const pixelHeight = bottom - top;
  if (
    !Number.isSafeInteger(pixelWidth) ||
    !Number.isSafeInteger(pixelHeight) ||
    pixelWidth <= 0 ||
    pixelHeight <= 0
  ) {
    throw new RangeError(
      'Text bitmap dimensions are outside the supported range'
    );
  }

  return {
    advance: metrics.width,
    pixelWidth,
    pixelHeight,
    offsetX: left / scale,
    offsetY: top / scale,
    baselineX: -left,
    baselineY: -top,
    scale,
  };
};

/** One line requested from XPilot's browser font provider. */
export interface BrowserTextRequest {
  /** Complete Unicode line, including any combining characters. */
  readonly text: string;
  /** Logical CSS pixel height. */
  readonly size: number;
  /** Select the bundled monospace or oblique face. */
  readonly face: 'mono' | 'oblique';
  /** Physical pixels per logical pixel. */
  readonly scale: number;
}

/** An immutable-by-contract RGBA raster and its logical placement. */
export interface BrowserTextRaster extends TextRasterBounds {
  /** Unpremultiplied RGBA8 pixels, with rows starting at the top left. */
  readonly pixels: Uint8ClampedArray;
}

/** Browser text service owning its loaded font faces and bounded raster cache. */
export interface BrowserTextProvider {
  /**
   * Measure and rasterize one complete line, reusing an existing cached result.
   * @param request Font and line to render.
   * @returns Raster borrowed from the service; callers must not mutate it.
   */
  readonly rasterize: (request: BrowserTextRequest) => BrowserTextRaster;
  /** Release cached pixels and unregister this service's loaded fonts. */
  readonly dispose: () => void;
}

/**
 * Load XPilot's bundled faces before creating a synchronous text provider.
 * @param monoUrl Same-origin URL of VeraMoBd.ttf or its web conversion.
 * @param obliqueUrl Same-origin URL of FreeSansBoldOblique.ttf or its conversion.
 * @returns A provider that asks the browser to supply missing glyphs.
 * @remarks A complete line is shaped together so combining sequences and
 * kerning are preserved. Cache entries include raster scale and are limited
 * by both entry count and pixel memory. Multiline layout belongs to the caller.
 */
export const createBrowserTextProvider = async (
  monoUrl: string,
  obliqueUrl: string
): Promise<BrowserTextProvider> => {
  const mono = new FontFace('XPilotMono', `url(${JSON.stringify(monoUrl)})`, {
    weight: '700',
  });
  const oblique = new FontFace(
    'XPilotOblique',
    `url(${JSON.stringify(obliqueUrl)})`,
    { weight: '700', style: 'oblique' }
  );
  await Promise.all([mono.load(), oblique.load()]);
  document.fonts.add(mono);
  document.fonts.add(oblique);

  const canvas = document.createElement('canvas');
  const context = canvas.getContext('2d', { willReadFrequently: true });
  if (context === null) {
    document.fonts.delete(mono);
    document.fonts.delete(oblique);
    throw new Error('Canvas text rendering is unavailable');
  }
  const cache = new Map<string, BrowserTextRaster>();
  const maximumBytes = 8 * 1024 * 1024;
  let cachedBytes = 0;
  let disposed = false;

  const rasterize = (request: BrowserTextRequest): BrowserTextRaster => {
    if (disposed) throw new Error('The text provider has been disposed');
    if (
      !Number.isFinite(request.size) ||
      request.size <= 0 ||
      request.size > 512 ||
      !Number.isFinite(request.scale) ||
      request.scale <= 0 ||
      request.scale > 8 ||
      request.text.length > 2048 ||
      /[\r\n]/u.test(request.text)
    ) {
      throw new RangeError('Invalid text line or font size');
    }
    const key = JSON.stringify([
      request.text,
      request.size,
      request.face,
      request.scale,
    ]);
    const cached = cache.get(key);
    if (cached !== undefined) {
      cache.delete(key);
      cache.set(key, cached);
      return cached;
    }
    const font =
      request.face === 'mono'
        ? `700 ${request.size}px "XPilotMono", monospace`
        : `oblique 700 ${request.size}px "XPilotOblique", sans-serif`;
    context.font = font;
    context.textAlign = 'left';
    context.textBaseline = 'alphabetic';
    context.direction = 'ltr';
    const bounds = calculateTextRasterBounds(
      context.measureText(request.text),
      request.scale
    );
    const bytes = bounds.pixelWidth * bounds.pixelHeight * 4;
    if (
      bytes > maximumBytes ||
      bounds.pixelWidth > 8192 ||
      bounds.pixelHeight > 8192
    ) {
      throw new RangeError('Text raster exceeds the bitmap limit');
    }
    canvas.width = bounds.pixelWidth;
    canvas.height = bounds.pixelHeight;
    // Resizing resets Canvas state, so measurement and drawing use the same font.
    context.font = font;
    context.textAlign = 'left';
    context.textBaseline = 'alphabetic';
    context.direction = 'ltr';
    context.fillStyle = 'white';
    context.setTransform(
      request.scale,
      0,
      0,
      request.scale,
      bounds.baselineX,
      bounds.baselineY
    );
    context.fillText(request.text, 0, 0);
    const raster = {
      ...bounds,
      pixels: context.getImageData(0, 0, bounds.pixelWidth, bounds.pixelHeight)
        .data,
    };
    while (cache.size >= 128 || cachedBytes + bytes > maximumBytes) {
      const oldest = cache.entries().next().value;
      if (oldest === undefined) break;
      cache.delete(oldest[0]);
      cachedBytes -= oldest[1].pixels.byteLength;
    }
    cache.set(key, raster);
    cachedBytes += bytes;
    return raster;
  };

  return {
    rasterize,
    dispose: () => {
      disposed = true;
      cache.clear();
      cachedBytes = 0;
      document.fonts.delete(mono);
      document.fonts.delete(oblique);
    },
  };
};

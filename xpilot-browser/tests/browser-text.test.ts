// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { describe, expect, it } from 'vitest';
import { calculateTextRasterBounds } from '../src/browser-text';

describe('text raster bounds', () => {
  it('keeps italic overhangs and descenders without changing the advance', () => {
    const bounds = calculateTextRasterBounds(
      {
        width: 10.25,
        actualBoundingBoxLeft: 2.5,
        actualBoundingBoxRight: 12.75,
        actualBoundingBoxAscent: 8.2,
        actualBoundingBoxDescent: 2.1,
      },
      2
    );

    expect(bounds).toEqual({
      advance: 10.25,
      pixelWidth: 33,
      pixelHeight: 24,
      offsetX: -3,
      offsetY: -9,
      baselineX: 6,
      baselineY: 18,
      scale: 2,
    });
  });

  it('preserves whitespace advance independently from its empty ink bounds', () => {
    const bounds = calculateTextRasterBounds(
      {
        width: 14,
        actualBoundingBoxLeft: 0,
        actualBoundingBoxRight: 0,
        actualBoundingBoxAscent: 0,
        actualBoundingBoxDescent: 0,
      },
      1
    );

    expect(bounds.advance).toBe(14);
    expect(bounds.pixelWidth).toBe(16);
    expect(bounds.pixelHeight).toBe(2);
  });

  it('contains fractional ink bounds at a fractional device scale', () => {
    const bounds = calculateTextRasterBounds(
      {
        width: 7.3,
        actualBoundingBoxLeft: -1.2,
        actualBoundingBoxRight: 6.7,
        actualBoundingBoxAscent: 5.1,
        actualBoundingBoxDescent: 1.4,
      },
      1.25
    );

    expect(bounds.offsetX).toBeLessThanOrEqual(1.2);
    expect(bounds.offsetY).toBeLessThanOrEqual(-5.1);
    expect(
      bounds.offsetX + bounds.pixelWidth / bounds.scale
    ).toBeGreaterThanOrEqual(7.3);
    expect(
      bounds.offsetY + bounds.pixelHeight / bounds.scale
    ).toBeGreaterThanOrEqual(1.4);
    expect(Number.isInteger(bounds.pixelWidth)).toBe(true);
    expect(Number.isInteger(bounds.pixelHeight)).toBe(true);
  });

  it.each([0, -1, Infinity, NaN])(
    'rejects an invalid raster scale (%s)',
    (scale) => {
      expect(() =>
        calculateTextRasterBounds(
          {
            width: 1,
            actualBoundingBoxLeft: 0,
            actualBoundingBoxRight: 1,
            actualBoundingBoxAscent: 1,
            actualBoundingBoxDescent: 0,
          },
          scale
        )
      ).toThrow(RangeError);
    }
  );

  it('validates non-enumerable browser measurements', () => {
    const metrics = Object.create(null) as TextMetrics;
    Object.defineProperties(metrics, {
      width: { value: 1 },
      actualBoundingBoxLeft: { value: 0 },
      actualBoundingBoxRight: { value: 1 },
      actualBoundingBoxAscent: { value: Infinity },
      actualBoundingBoxDescent: { value: 0 },
    });
    expect(() => calculateTextRasterBounds(metrics, 1)).toThrow(
      'Invalid text measurements'
    );
  });
});

// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

const checksum = (bytes: Uint8Array): number => {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let sum = 0;
  for (let offset = 0; offset < bytes.length; offset += 4) {
    sum = (sum + view.getUint32(offset)) >>> 0;
  }
  return sum;
};

/**
 * Repack a bundled TrueType font without empty tables rejected by browsers.
 * @param source Single-font SFNT bytes; never modified.
 * @returns A standalone TrueType font with preserved nonempty tables and fresh checksums.
 * @remarks This is a build-time repair for XPilot's bundled fonts, not a general
 * font sanitizer. Glyphs, shaping tables, names and embedded license text are
 * copied unchanged. Only the directory, padding and head.checkSumAdjustment change.
 * See https://learn.microsoft.com/en-us/typography/opentype/spec/otff.
 */
export const normalizeWebFont = (source: Uint8Array): Uint8Array => {
  const input = new DataView(
    source.buffer,
    source.byteOffset,
    source.byteLength
  );
  if (source.length < 12 || input.getUint32(0) !== 0x00010000) {
    throw new Error('Expected a standalone TrueType font');
  }
  const count = input.getUint16(4);
  if (12 + count * 16 > source.length)
    throw new Error('Truncated font directory');
  const tables: { tag: number; offset: number; length: number }[] = [];
  for (let index = 0; index < count; index++) {
    const position = 12 + index * 16;
    const tag = input.getUint32(position);
    const offset = input.getUint32(position + 8);
    const length = input.getUint32(position + 12);
    if (offset + length > source.length)
      throw new Error('Truncated font table');
    if (length !== 0) tables.push({ tag, offset, length });
  }
  tables.sort((left, right) => left.tag - right.tag);
  const headTag = 0x68656164;
  if (!tables.some((table) => table.tag === headTag && table.length >= 12)) {
    throw new Error('Missing font header');
  }
  for (let index = 1; index < tables.length; index++) {
    if (tables[index]!.tag === tables[index - 1]!.tag)
      throw new Error('Duplicate font table');
  }
  // Keep already usable fonts byte-for-byte, including the bundled Vera face.
  if (tables.length === count) return source.slice();
  const aligned = (length: number) => Math.ceil(length / 4) * 4;
  let offset = 12 + tables.length * 16;
  const result = new Uint8Array(
    offset + tables.reduce((sum, table) => sum + aligned(table.length), 0)
  );
  const output = new DataView(result.buffer);
  output.setUint32(0, 0x00010000);
  output.setUint16(4, tables.length);
  const selector = Math.floor(Math.log2(tables.length));
  output.setUint16(6, 2 ** selector * 16);
  output.setUint16(8, selector);
  output.setUint16(10, tables.length * 16 - 2 ** selector * 16);
  let adjustment = 0;
  for (const [index, table] of tables.entries()) {
    result.set(
      source.subarray(table.offset, table.offset + table.length),
      offset
    );
    if (table.tag === headTag) {
      adjustment = offset + 8;
      output.setUint32(adjustment, 0);
    }
    const position = 12 + index * 16;
    output.setUint32(position, table.tag);
    output.setUint32(
      position + 4,
      checksum(result.subarray(offset, offset + aligned(table.length)))
    );
    output.setUint32(position + 8, offset);
    output.setUint32(position + 12, table.length);
    offset += aligned(table.length);
  }
  output.setUint32(adjustment, (0xb1b0afba - checksum(result)) >>> 0);
  return result;
};

// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** Public filesystem boundary used to decode the bundled WAV effects. */
export interface AudioFiles {
  /** Read a complete asset; the service copies bytes before decoding. */
  readFile: (path: string) => Uint8Array;
}

/** One page owns one audio service across successive C game instances. */
export interface BrowserAudio {
  /** Resume within a user gesture; autoplay rejection remains recoverable. */
  unlock: () => Promise<void>;
  /** Set mute state and stop currently playing effects when muting. */
  mute: (muted: boolean) => void;
  /** Play the existing filename,gain,loop definition from caudio.c. */
  play: (
    definition: string,
    type: number,
    volume: number,
    frame: number
  ) => void;
  /** Expire loops that the next game frame did not renew. */
  update: (frame: number) => void;
  /** Release all sources and gain nodes, retaining bounded decoded assets. */
  stop: () => void;
  /** Close the context when the page is discarded. */
  dispose: () => Promise<void>;
  /** Current context state, live node count and largest measured PCM amplitude. */
  stats: () => { state: string; active: number; peak: number };
}

/**
 * Decode the bundled effect mappings without blocking the browser main loop.
 * @param files Read-only game assets.
 * @returns A page-owned service with at most 16 simultaneous effects.
 */
export const createBrowserAudio = async (
  files: AudioFiles
): Promise<BrowserAudio> => {
  const context = new AudioContext();
  const output = context.createGain();
  const analyser = context.createAnalyser();
  analyser.fftSize = 256;
  output.connect(analyser);
  analyser.connect(context.destination);
  const buffers = new Map<string, AudioBuffer>();
  const definitions = new TextDecoder().decode(
    files.readFile('/assets/sound/sounds.txt')
  );
  try {
    for (const line of definitions.split('\n')) {
      const tokens = line.trim().split(/\s+/);
      if (!tokens[0] || tokens[0].startsWith('#') || !tokens[1]) continue;
      for (const item of tokens[1].split('|')) {
        const file = `/assets/sound/${item.split(',')[0]}`;
        if (buffers.has(file)) continue;
        const data = Uint8Array.from(files.readFile(file));
        buffers.set(file, await context.decodeAudioData(data.buffer));
      }
    }
  } catch (error) {
    await context.close();
    throw error;
  }
  interface Voice {
    source: AudioBufferSourceNode;
    gain: GainNode;
    type: number;
    frame: number;
  }
  const voices = new Set<Voice>();
  let muted = false;
  let peak = 0;
  const samples = new Float32Array(256);
  const release = (voice: Voice) => {
    if (!voices.delete(voice)) return;
    voice.source.onended = null;
    voice.source.stop();
    voice.source.disconnect();
    voice.gain.disconnect();
  };
  const stop = () => {
    for (const voice of voices) release(voice);
  };
  return {
    unlock: async () => {
      if (context.state !== 'closed') await context.resume();
    },
    mute: (value) => {
      muted = value;
      output.gain.value = value ? 0 : 1;
      if (value) stop();
    },
    play: (definition, type, volume, frame) => {
      if (muted || context.state !== 'running') return;
      const [file, gainValue, looping] = definition.split(',');
      const buffer = buffers.get(file!);
      if (!buffer) return;
      const gain = Math.max(
        0,
        Math.min(1, (volume / 100) * Number(gainValue ?? 1))
      );
      for (const voice of voices) {
        if (looping === '1' && voice.type === type && voice.source.loop) {
          voice.frame = frame;
          voice.gain.gain.value = gain;
          return;
        }
      }
      if (voices.size >= 16) release(voices.values().next().value!);
      const source = context.createBufferSource();
      const volumeNode = context.createGain();
      volumeNode.gain.value = gain;
      source.buffer = buffer;
      source.loop = looping === '1';
      source.connect(volumeNode);
      volumeNode.connect(output);
      const voice = { source, gain: volumeNode, type, frame };
      voices.add(voice);
      source.onended = () => release(voice);
      source.start();
    },
    update: (frame) => {
      for (const voice of voices)
        if (voice.source.loop && voice.frame < frame - 1) release(voice);
    },
    stop,
    dispose: async () => {
      stop();
      output.disconnect();
      analyser.disconnect();
      await context.close();
      buffers.clear();
    },
    stats: () => {
      analyser.getFloatTimeDomainData(samples);
      for (const sample of samples) peak = Math.max(peak, Math.abs(sample));
      return { state: context.state, active: voices.size, peak };
    },
  };
};

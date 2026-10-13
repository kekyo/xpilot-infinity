import { expect, it } from 'vitest';
import { execFile, spawn, type ChildProcess } from 'node:child_process';
import { promisify } from 'node:util';
import { once } from 'node:events';
import { randomUUID } from 'node:crypto';

it.runIf(
  process.env.XPILOT_CONTAINER_IMAGE !== undefined &&
    process.env.XPILOT_CLIENT_BINARY !== undefined
)(
  'plays over UDP, TCP and WebSocket by adding port mappings to the same image',
  async () => {
    const engine = process.env.CONTAINER_ENGINE ?? 'podman';
    const image = process.env.XPILOT_CONTAINER_IMAGE!;
    const platform =
      process.env.XPILOT_CONTAINER_PLATFORM ??
      `linux/${process.arch === 'x64' ? 'amd64' : process.arch}`;
    const name = `xpilot-container-transports-${randomUUID()}`;
    const exec = promisify(execFile);
    const run = async (args: string[]) =>
      (await exec(engine, args, { timeout: 180000 })).stdout.trim();
    const clients: ChildProcess[] = [];
    const waitFor = async (child: ChildProcess, marker: string) =>
      await new Promise<string>((done, reject) => {
        let output = '';
        const collect = (chunk: Buffer) => {
          output += chunk.toString();
          if (output.includes(marker)) done(output);
        };
        child.stdout!.on('data', collect);
        child.stderr!.on('data', collect);
        child.once('error', reject);
        child.once('exit', (code) =>
          reject(new Error(`Exited (${code}): ${output}`))
        );
      });
    try {
      await run([
        'run',
        '-d',
        '--name',
        name,
        '--platform',
        platform,
        '--read-only',
        '--cap-drop=all',
        '--security-opt=no-new-privileges',
        '-p',
        '127.0.0.1::15345/udp',
        '-p',
        '127.0.0.1:15400-15499:15400-15499/udp',
        '-p',
        '127.0.0.1::15346/tcp',
        '-p',
        '127.0.0.1::15347/tcp',
        image,
        '-maxClientsPerIP',
        '8',
      ]);
      const logs = spawn(engine, ['logs', '--follow', name], {
        timeout: 180000,
      });
      try {
        await waitFor(logs, 'Server runs at');
      } finally {
        const closed = once(logs, 'close');
        logs.kill('SIGTERM');
        await closed;
      }
      for (const [transport, port] of [
        ['udp', '15345/udp'],
        ['tcp', '15346/tcp'],
        ['ws', '15347/tcp'],
      ]) {
        const address = await run(['port', name, port]);
        const client = spawn(
          'xvfb-run',
          [
            '-a',
            'env',
            'LIBGL_ALWAYS_SOFTWARE=1',
            'SDL_VIDEODRIVER=x11',
            'stdbuf',
            '-oL',
            process.env.XPILOT_CLIENT_BINARY!,
            '-geometry',
            '800x600',
            '-join',
            '-name',
            `Container${transport}`,
            '-messagesToStdout',
            '2',
            `${transport}://${address}`,
          ],
          { detached: true }
        );
        clients.push(client);
        expect(await waitFor(client, 'Game frame ready')).toContain(
          'Game frame ready'
        );
      }
      for (const client of clients) expect(client.exitCode).toBeNull();
    } finally {
      for (const client of clients)
        if (
          client.exitCode === null &&
          client.signalCode === null &&
          client.pid !== undefined
        ) {
          const closed = once(client, 'exit');
          process.kill(-client.pid, 'SIGTERM');
          await closed;
        }
      await run(['stop', '--ignore', '--time', '60', name]);
      await run(['rm', '--force', '--ignore', name]);
    }
  },
  300000
);

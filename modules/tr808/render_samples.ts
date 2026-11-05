// Renders the 13 TR-808 voices to samples/<id>.wav with the TS synth from the
// Bun WibWob-DOS microapp (microapps/wibwob-tr808/audio.ts), at default params.
//
//   bun modules/tr808/render_samples.ts [path/to/wibandwob-dos]
//
// Default source repo: ~/Repos/wibandwob-dos. The synth adds random noise, so
// each run gives slightly different hats, snares and claps.
import { join, resolve } from "path";
import { homedir } from "os";

const repo = resolve(process.argv[2] ?? join(homedir(), "Repos", "wibandwob-dos"));
const app = join(repo, "microapps", "wibwob-tr808");
const { TR808Audio } = await import(join(app, "audio.ts"));
const { INSTRUMENTS } = await import(join(app, "engine.ts"));

const out = join(import.meta.dir, "samples");
const audio = new TR808Audio();
// renderSamples writes into the instance's sampleDir; point it at our folder.
(audio as unknown as { sampleDir: string }).sampleDir = out;
await Bun.write(join(out, ".keep"), "");

const params: Record<string, Record<string, number>> = {};
for (const inst of INSTRUMENTS) {
  params[inst.id] = Object.fromEntries(inst.params.map((p: { id: string; default: number }) => [p.id, p.default]));
}
audio.renderSamples(params);
console.log(`wrote ${INSTRUMENTS.length} samples to ${out}`);

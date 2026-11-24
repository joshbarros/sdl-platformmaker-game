// Sorceress asset generator.
//
// Calls the Sorceress API, waits for the job and writes the resulting image to
// disk. The API key comes from SORCERESS_API_KEY or from the local key file, so
// it never lives in the repo.
//
//   node tools/gen-asset.mjs --prompt "..." --out assets/art/hero.png \
//     [--ratio 1:1] [--model gpt-image-1.5] [--background transparent] [--quality low]

import fs from "node:fs";
import path from "node:path";

const API = "https://sorceress.games/api/v1";

function arg(name, fallback = undefined) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return fallback;
  return process.argv[i + 1];
}

function readKey() {
  if (process.env.SORCERESS_API_KEY) return process.env.SORCERESS_API_KEY.trim();
  const file = path.join(process.env.LOCALAPPDATA || "", "opencode", "sorceress.key");
  if (fs.existsSync(file)) return fs.readFileSync(file, "utf8").trim();
  throw new Error("no Sorceress API key: set SORCERESS_API_KEY or create the key file");
}

const key = readKey();
const headers = { Authorization: `Bearer ${key}`, "Content-Type": "application/json" };

async function post(url, body) {
  const res = await fetch(API + url, { method: "POST", headers, body: JSON.stringify(body) });
  const text = await res.text();
  if (!res.ok) throw new Error(`${url} -> ${res.status} ${text.slice(0, 400)}`);
  return JSON.parse(text);
}

async function get(url) {
  const res = await fetch(API + url, { headers });
  const text = await res.text();
  if (!res.ok) throw new Error(`${url} -> ${res.status} ${text.slice(0, 400)}`);
  return JSON.parse(text);
}

function findImages(node, out = []) {
  if (typeof node === "string") {
    if (/^https?:\/\/\S+\.(png|jpe?g|webp)(\?\S*)?$/i.test(node)) out.push(node);
  } else if (Array.isArray(node)) {
    for (const item of node) findImages(item, out);
  } else if (node && typeof node === "object") {
    for (const value of Object.values(node)) findImages(value, out);
  }
  return out;
}

const prompt = arg("prompt");
const out = arg("out");
if (!prompt || !out) {
  console.error("usage: node tools/gen-asset.mjs --prompt <text> --out <file.png> [--ratio 1:1]");
  process.exit(2);
}

const model = arg("model", "gpt-image-1.5");
const params = {};
if (arg("background")) params.background = arg("background");
if (arg("quality")) params.quality = arg("quality");

const body = { model, prompt, aspectRatio: arg("ratio", "1:1") };
if (Object.keys(params).length) body.params = params;

console.log(`generate: ${model} ${body.aspectRatio} ${JSON.stringify(params)}`);
const started = await post("/tools/image_generate", body);
const jobId =
  started?.data?.jobId || started?.jobId || started?.data?.id || started?.id || started?.job?.id;
if (!jobId) throw new Error("no job id in response: " + JSON.stringify(started).slice(0, 400));
console.log("job:", jobId);

const deadline = Date.now() + 5 * 60 * 1000;
let result = null;
while (Date.now() < deadline) {
  await new Promise((r) => setTimeout(r, 2500));
  const job = await get(`/jobs/${jobId}`);
  const status = (job?.data?.status || job?.status || "").toLowerCase();
  if (status === "failed" || status === "error") {
    throw new Error("job failed: " + JSON.stringify(job).slice(0, 600));
  }
  if (status === "succeeded" || status === "success" || status === "completed" || status === "done") {
    result = job;
    break;
  }
}
if (!result) throw new Error("timed out waiting for job " + jobId);

const images = findImages(result);
if (!images.length) throw new Error("no image url in result: " + JSON.stringify(result).slice(0, 800));

fs.mkdirSync(path.dirname(out), { recursive: true });
const response = await fetch(images[0]);
if (!response.ok) throw new Error(`download failed: ${response.status}`);
const bytes = Buffer.from(await response.arrayBuffer());
fs.writeFileSync(out, bytes);
const credits = result?.data?.creditsCharged ?? result?.creditsCharged ?? "?";
console.log(`saved ${out} (${bytes.length} bytes, ${credits} credits)`);

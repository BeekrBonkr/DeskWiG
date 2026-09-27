import { build } from "esbuild";
import { gzipSync } from "node:zlib";
import { writeFileSync, mkdirSync, statSync } from "node:fs";

const out = await build({
  entryPoints: ["entry.js"], bundle: true, minify: true, format: "iife", target: "es2018",
  write: false, legalComments: "none", logLevel: "warning"
});
const js = out.outputFiles[0].contents;
mkdirSync("../../web", { recursive: true });
writeFileSync("../../web/cm.js", js);
writeFileSync("../../web/cm.js.gz", gzipSync(js, { level: 9 }));
console.log("web/cm.js", js.length, "bytes; web/cm.js.gz", statSync("../../web/cm.js.gz").size, "bytes");

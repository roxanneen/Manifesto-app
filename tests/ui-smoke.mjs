#!/usr/bin/env node
// Headless smoke test for manifesto_ui. No network. It signs with the app's own key.
//
// Launches the module builder's standalone host with an isolated user dir and
// drives the view through the QML inspector (logos-qt-mcp). It checks that the
// bundled text and its hash reach the view, that the statement names that
// hash, that the app makes its own key (owner-only file, never shown), signs
// in one click and saves a receipt, and the whole Check a signature tab
// against receipts made by an independent implementation
// (tests/make_fixtures.py). The signed receipt is written to
// WORK/signed-receipt.json so tests/check_receipt.py can verify it with
// eth-account.
//
//   MANIFESTO_RUN=./result-ui-dev/bin/run-logos-standalone-ui \
//   MANIFESTO_WORK=$PWD/tests/work/smoke MANIFESTO_FIXTURES=$PWD/tests/work/fixtures.json \
//   node tests/ui-smoke.mjs [--keep]

import net from "node:net";
import fs from "node:fs";
import path from "node:path";
import { spawn, execSync } from "node:child_process";

const RUN = process.env.MANIFESTO_RUN;
const WORK = process.env.MANIFESTO_WORK || path.resolve("tests/work/smoke");
const FIX = process.env.MANIFESTO_FIXTURES;
const PORT = parseInt(process.env.MANIFESTO_INSPECTOR_PORT || "3783", 10);
const KEEP = process.argv.includes("--keep");
if (!RUN || !FIX) { console.error("MANIFESTO_RUN and MANIFESTO_FIXTURES are required"); process.exit(2); }
const fx = JSON.parse(fs.readFileSync(FIX, "utf-8"));
fs.mkdirSync(WORK, { recursive: true });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const stamp = () => new Date().toISOString().slice(11, 19);
const log = (...a) => console.log(`[${stamp()}]`, ...a);

class Inspector {
  constructor(port) { this.port = port; this.sock = null; this.id = 0; this.pending = new Map(); this.buf = ""; }
  async connect(timeoutMs = 90000) {
    const start = Date.now();
    for (;;) {
      try {
        await new Promise((res, rej) => {
          const s = net.createConnection({ host: "127.0.0.1", port: this.port });
          s.once("connect", () => { this.sock = s; res(); });
          s.once("error", rej);
          s.on("data", (c) => { this.buf += c.toString("utf-8"); this._drain(); });
          s.on("close", () => { this.sock = null; for (const [, p] of this.pending) p.reject(new Error("closed")); this.pending.clear(); });
        });
        return;
      } catch (e) {
        if (Date.now() - start > timeoutMs) throw new Error(`inspector :${this.port} not reachable: ${e.message}`);
        await sleep(500);
      }
    }
  }
  _drain() {
    let i;
    while ((i = this.buf.indexOf("\n")) !== -1) {
      const line = this.buf.slice(0, i).trim(); this.buf = this.buf.slice(i + 1);
      if (!line) continue;
      try { const m = JSON.parse(line); const p = this.pending.get(String(m.id)); if (p) { clearTimeout(p.t); this.pending.delete(String(m.id)); p.resolve(m); } } catch {}
    }
  }
  send(command, params = {}) {
    if (!this.sock) return Promise.reject(new Error("not connected"));
    const id = ++this.id;
    return new Promise((resolve, reject) => {
      const t = setTimeout(() => { this.pending.delete(String(id)); reject(new Error(`timeout ${command}`)); }, 20000);
      this.pending.set(String(id), { resolve, reject, t });
      this.sock.write(JSON.stringify({ id, command, params }) + "\n");
    });
  }
  close() { if (this.sock) this.sock.destroy(); }
}

const home = path.join(WORK, "home");
const userDir = path.join(home, "basecamp");
const logPath = path.join(WORK, "app.log");
let proc = null;
const insp = new Inspector(PORT);

async function find(objectName) {
  const r = await insp.send("findByProperty", { property: "objectName", value: objectName });
  if (r.error || !r.matches || !r.matches.length) throw new Error(`no object "${objectName}"`);
  return r.matches[0].id;
}
async function props(objectName) {
  const r = await insp.send("getProperties", { objectId: await find(objectName) });
  if (r.error) throw new Error(`getProperties(${objectName}): ${r.error}`);
  const o = {};
  for (const p of r.properties || []) o[p.name] = p.value;
  return o;
}
async function prop(objectName, name) { return (await props(objectName))[name]; }
async function rootProp(name) { return prop("manifestoRoot", name); }
async function set(objectName, property, value) {
  const r = await insp.send("setProperty", { objectId: await find(objectName), property, value });
  if (r.error) throw new Error(`set ${objectName}.${property}: ${r.error}`);
}
async function click(objectName) {
  const r = await insp.send("click", { objectId: await find(objectName) });
  if (r.error) throw new Error(`click ${objectName}: ${r.error}`);
}
async function waitFor(fn, timeoutMs, what) {
  const start = Date.now();
  let last;
  while (Date.now() - start < timeoutMs) {
    try { if (await fn()) return; } catch (e) { last = e; }
    await sleep(500);
  }
  throw new Error(`timed out waiting for ${what}${last ? ` (${last.message})` : ""}`);
}
async function screenshot(name) {
  const r = await insp.send("screenshot", {});
  const data = r.data || r.image || r.png || r.base64;
  if (data) fs.writeFileSync(path.join(WORK, name), Buffer.from(String(data).replace(/^data:image\/png;base64,/, ""), "base64"));
  else log(`screenshot ${name}: no image in reply (${Object.keys(r).join(",")})`);
}
function children() {
  try {
    const pattern = userDir.replace(/^(.)/, "[$1]");
    const out = execSync(`pgrep -f ${JSON.stringify(pattern)} || true`, { encoding: "utf-8" });
    return out.split("\n").map((x) => parseInt(x, 10)).filter((n) => Number.isFinite(n) && n !== process.pid);
  } catch { return []; }
}
async function stop() {
  insp.close();
  insp.sock = null; insp.buf = "";
  if (proc && proc.pid) { try { process.kill(-proc.pid, "SIGTERM"); } catch { try { proc.kill("SIGTERM"); } catch {} } }
  for (let i = 0; i < 20; i++) {
    const kids = children();
    if (!kids.length) break;
    for (const pid of kids) { try { process.kill(pid, i < 5 ? "SIGTERM" : "SIGKILL"); } catch {} }
    await sleep(500);
  }
}

const results = [];
async function step(name, fn) {
  try { await fn(); results.push([name, true]); log(`ok   ${name}`); }
  catch (e) { results.push([name, false, e.message]); log(`FAIL ${name}: ${e.message}`); }
}
async function check(receipt, want) {
  // Start every check from "none", so a result left by the previous check can
  // never satisfy this one.
  await click("clearButton");
  await waitFor(async () => (await prop("verifyMarker", "text")) === "none", 5000, "cleared");
  await set("verifyArea", "text", typeof receipt === "string" ? receipt : JSON.stringify(receipt));
  await waitFor(async () => (await prop("verifyButton", "enabled")) === true, 5000, "check enabled");
  await click("verifyButton");
  let last = "";
  try {
    await waitFor(async () => { last = await prop("verifyMarker", "text"); return last === want; }, 8000, `verify = ${want}`);
  } catch (e) { throw new Error(`${e.message}; marker is "${last}"`); }
}

(async () => {
  // Every run starts from an empty profile (WORK/home is this script's own
  // folder), so the first-run path that makes the key is what gets tested.
  fs.rmSync(home, { recursive: true, force: true });
  fs.mkdirSync(home, { recursive: true });
  const env = {
    ...process.env,
    HOME: home,
    LOGOS_USER_DIR: userDir,
    QML_INSPECTOR_PORT: String(PORT),
    QT_QPA_PLATFORM: process.env.MANIFESTO_PLATFORM || "offscreen",
    ...(process.env.MANIFESTO_QML ? { DEV_QML_PATH: process.env.MANIFESTO_QML } : { LOGOS_QML_HOT_RELOAD: "0" }),
    MANIFESTO_DATE: "2026-11-26",
  };
  let launches = 0;
  async function launch() {
    const out = fs.openSync(logPath, launches++ ? "a" : "w");
    fs.writeSync(out, `\n===== launch at ${new Date().toISOString()} =====\n`);
    proc = spawn(RUN, ["--user-dir", userDir], { env, stdio: ["ignore", out, out], cwd: WORK, detached: true });
    proc.on("exit", (code, sig) => { log(`app exited code=${code} sig=${sig}`); });
    await insp.connect();
    await waitFor(async () => (await rootProp("ready")) === true, 60000, "ready");
  }
  async function relaunch() {
    await stop();
    await sleep(1000);
    await launch();
  }

  try {
    await step("view ready", () => launch());
    await step("backend reports build id", async () => {
      const id = await rootProp("buildId");
      if (!/^manifesto_ui\//.test(id)) throw new Error(`buildId = ${id}`);
    });
    await step("bundled text hash matches src/manifesto.txt", async () => {
      const h = await prop("hashMarker", "text");
      if (h !== fx.hash) throw new Error(`view ${h}, file ${fx.hash}`);
    });
    await step("manifesto text is shown", async () => {
      const t = await prop("manifestoBody", "text");
      if (!t.startsWith("Logos: A Declaration of Independence in Cyberspace") || !t.trim().endsWith("can experience it."))
        throw new Error(`body is ${t.length} chars`);
    });
    await step("the reading view keeps every paragraph, in order, with nothing added", async () => {
      const kinds = (await prop("blocksMarker", "text")).split(",");
      const want = ["title", "author", "abstract", "keywords"];
      if (want.some((k, i) => kinds[i] !== k)) throw new Error(`starts ${kinds.slice(0, 4)}`);
      if (kinds[kinds.length - 1] !== "closing") throw new Error(`ends ${kinds[kinds.length - 1]}`);
      if (!kinds.includes("question") || kinds.filter((k) => k === "credo").length < 6) throw new Error(`kinds ${kinds}`);
      // 31 paragraphs after the title block, plus title and author.
      if (kinds.length !== 33) throw new Error(`${kinds.length} blocks`);
    });
    await step("the manifesto scrolls: its content is taller than the view and moves", async () => {
      const before = await props("manifestoScroll");
      const contentH = Number(before.contentHeight), viewH = Number(before.height);
      if (!(contentH > viewH * 2)) throw new Error(`content ${contentH} px in a ${viewH} px view`);
      // Scroll the way a wheel or a drag does: move the view's content.
      const flick = (await insp.send("findByProperty", { property: "objectName", value: "manifestoScroll" })).matches[0].id;
      const r = await insp.send("setProperty", { objectId: flick, property: "contentItem.contentY", value: 600 });
      let y = NaN;
      if (!r.error) y = Number((await props("manifestoScroll"))["contentItem.contentY"] ?? NaN);
      log(`reading view: content ${contentH} px, view ${viewH} px${Number.isFinite(y) ? `, scrolled to ${y}` : ""}`);
    });
    await step("statement is one line naming the hash and the date", async () => {
      const s = await prop("statementMarker", "text");
      const want = `I sign the Logos manifesto, "Logos: A Declaration of Independence in Cyberspace". Text sha256: ${fx.hash}. Signed on 2026-11-26.`;
      if (s !== want) throw new Error(s);
    });
    await step("the app is ready to sign with its own key", () => waitFor(async () =>
      (await prop("signerMarker", "text")) === "ready"
      && /^0x[0-9a-fA-F]{40}$/.test(await prop("ownAddressMarker", "text")), 30000, "ready with a key"));
    const ownAddress = await prop("ownAddressMarker", "text");
    const keyFile = await rootProp("appKeyFile");
    log(`own key ${ownAddress} in ${keyFile}; ${await rootProp("signerInfo")}`);
    await step("the key file is inside the test profile and readable by its owner only", async () => {
      if (!keyFile.startsWith(userDir)) throw new Error(`key at ${keyFile}`);
      const mode = fs.statSync(keyFile).mode & 0o777;
      if (mode !== 0o600) throw new Error(`mode ${mode.toString(8)}`);
      if (!/^[0-9a-f]{64}\n$/.test(fs.readFileSync(keyFile, "utf-8"))) throw new Error("not 64 hex characters");
    });
    await screenshot("01-read-and-sign.png");
    await step("no receipt exists before signing", async () => {
      if ((await prop("receiptsMarker", "text")) !== "0") throw new Error("a receipt exists");
      if ((await rootProp("ownSigned")) !== false || (await prop("signButton", "text")) !== "Sign the manifesto")
        throw new Error("marked as signed before signing");
    });
    await step("Sign makes a receipt in one click", async () => {
      await waitFor(async () => (await prop("signButton", "enabled")) === true, 5000, "sign enabled");
      await click("signButton");
      let last = "";
      try {
        await waitFor(async () => { last = await prop("signMarker", "text"); return last === "signed"; }, 10000, "signed");
      } catch (e) { throw new Error(`${e.message}; signMarker is "${last}": ${await rootProp("signInfo")}`); }
      await waitFor(async () => (await prop("receiptsMarker", "text")) === "1", 5000, "one receipt");
      await waitFor(async () => (await rootProp("ownSigned")) === true && (await prop("signButton", "text")) === "Sign again", 5000, "button says Sign again");
    });
    let saved = null;
    await step("the saved receipt names the own key and today's statement", async () => {
      const dir = path.join(path.dirname(keyFile), "receipts");
      const files = fs.readdirSync(dir).filter((f) => f.endsWith(".json"));
      if (files.length !== 1) throw new Error(`${files.length} receipt files`);
      saved = JSON.parse(fs.readFileSync(path.join(dir, files[0]), "utf-8"));
      fs.writeFileSync(path.join(WORK, "signed-receipt.json"), JSON.stringify(saved, null, 2));
      if (saved.address !== ownAddress) throw new Error(`address ${saved.address}`);
      if (saved.statement !== await prop("statementMarker", "text")) throw new Error(saved.statement);
      if (!/^0x[0-9a-f]{130}$/.test(saved.signature)) throw new Error("signature shape");
    });
    await screenshot("02-signed.png");
    await step("signing again the same day rewrites the one receipt, it does not add one", async () => {
      const dir = path.join(path.dirname(keyFile), "receipts");
      const file = path.join(dir, fs.readdirSync(dir).filter((f) => f.endsWith(".json"))[0]);
      const before = fs.statSync(file).ino;   // the save replaces the file, so its inode changes
      await click("signButton");
      await waitFor(async () => fs.statSync(file).ino !== before, 5000, "receipt rewritten");
      await waitFor(async () => (await prop("signMarker", "text")) === "signed", 5000, "signed again");
      const n = fs.readdirSync(dir).filter((f) => f.endsWith(".json")).length;
      if (n !== 1 || (await prop("receiptsMarker", "text")) !== "1") throw new Error(`${n} receipt files`);
    });
    await step("after signing, the key is in no view property, log line or receipt", async () => {
      const key = fs.readFileSync(keyFile, "utf-8").trim().toLowerCase();
      const names = ["manifestoRoot", "statementText", "manifestoBody", "signNotice", "signerBadge", "ownAddressMarker", "statementMarker"];
      for (const n of names)
        if (JSON.stringify(await props(n)).toLowerCase().includes(key)) throw new Error(`key in ${n}`);
      if (fs.readFileSync(logPath, "utf-8").toLowerCase().includes(key)) throw new Error("key in the log");
      const dir = path.join(path.dirname(keyFile), "receipts");
      for (const f of fs.readdirSync(dir))
        if (fs.readFileSync(path.join(dir, f), "utf-8").toLowerCase().includes(key)) throw new Error("key in a receipt");
    });

    await set("tabs", "currentIndex", 2);
    await step("the app's own receipt passes its own check", () => check(saved, "valid"));
    await step("the same receipt with one signature character changed is refused", async () => {
      const c = saved.signature[20] === "a" ? "b" : "a";
      await check({ ...saved, signature: saved.signature.slice(0, 20) + c + saved.signature.slice(21) }, "invalid");
    });
    await set("tabs", "currentIndex", 2);
    await step("a receipt from an independent signer is valid", () => check(fx.valid, "valid"));
    await step("the address shown is the signer's", async () => {
      const shown = await prop("verifyAddressMarker", "text");
      if (shown !== fx.address) throw new Error(`shown ${shown}, signer ${fx.address}`);
    });
    await screenshot("03-valid.png");
    await step("a signature over another text version is flagged", () => check(fx.otherText, "valid-other-text"));
    await step("a receipt naming another address is refused", () => check(fx.wrongAddress, "invalid"));
    await step("an edited statement is refused", () => check(fx.edited, "invalid"));
    await step("a signature over unrelated text is refused", () => check(fx.notOurs, "invalid"));
    await step("text that is not a receipt is refused, and says so", async () => {
      await check("hello", "invalid");
      const err = await prop("verifyErrorMarker", "text");
      if (!err.includes("No signed line")) throw new Error(`reason shown: ${err}`);
    });
    // Receipts damaged in transit (the way they arrive from a chat or an email).
    await step("a clean receipt is not reported as repaired", async () => {
      await check(fx.valid, "valid");
      if ((await prop("verifyRepairedMarker", "text")) !== "no") throw new Error("marked repaired");
    });
    for (const [name, text] of Object.entries(fx.loose.good)) {
      await step(`damaged but genuine (${name}) verifies, as the right address`, async () => {
        await check(text, "valid");
        const shown = await prop("verifyAddressMarker", "text");
        if (shown !== fx.address) throw new Error(`shown ${shown}, signer ${fx.address}`);
        if ((await prop("verifyRepairedMarker", "text")) !== "yes") throw new Error("went through the strict path");
      });
    }
    const why = {
      wrongAddress: "not made by the address", edited: "not made by the address", badSignature: "",
      noAddress: "no address", noAddressRandomSignature: "no address", noAddressEdited: "no address",
      noAddressBadSignature: "no address", randomSignature: "", decoyField: "not made by the address",
      uppercasePrefixDecoy: "not made by the address", signatureOnly: "signed line is missing", statementOnly: "signature is missing",
    };
    for (const [name, text] of Object.entries(fx.loose.bad)) {
      await step(`not genuine (${name}) is refused`, async () => {
        await check(text, "invalid");
        const err = await prop("verifyErrorMarker", "text");
        if (!(name in why)) throw new Error("no expected reason for this fixture");
        if (why[name] && !err.includes(why[name])) throw new Error(`reason shown: ${err}`);
      });
    }
    await screenshot("04-invalid.png");
    await step("checking receipts did not add or remove any", async () => {
      if ((await prop("receiptsMarker", "text")) !== "1") throw new Error(`${await prop("receiptsMarker", "text")} receipts`);
    });

    // --- restarts -----------------------------------------------------------
    const keyBytes = fs.readFileSync(keyFile);
    await step("after a restart the app has the same key and still lists the receipt", async () => {
      await relaunch();
      await waitFor(async () => (await prop("signerMarker", "text")) === "ready", 30000, "ready");
      const again = await prop("ownAddressMarker", "text");
      if (again !== ownAddress) throw new Error(`address changed to ${again}`);
      if (!fs.readFileSync(keyFile).equals(keyBytes)) throw new Error("key file changed");
      await waitFor(async () => (await prop("receiptsMarker", "text")) === "1", 5000, "one receipt");
      await waitFor(async () => (await rootProp("ownSigned")) === true, 5000, "still marked as signed");
    });
    const damaged = Buffer.from("this is not a key\n");
    await step("a damaged key file is left untouched and signing switches off", async () => {
      await stop();
      fs.writeFileSync(keyFile, damaged);
      await sleep(1000);
      await launch();
      await waitFor(async () => (await prop("signerMarker", "text")) === "error", 30000, "signer error");
      if (!fs.readFileSync(keyFile).equals(damaged)) throw new Error("the damaged file was rewritten");
      if ((await prop("ownAddressMarker", "text")) !== "") throw new Error("an address is shown");
      if ((await prop("signButton", "enabled")) !== false) throw new Error("sign is enabled");
      log(`signerInfo: ${await rootProp("signerInfo")}`);
    });
    await step("with the key file damaged, the earlier receipt is still listed", async () => {
      if ((await prop("receiptsMarker", "text")) !== "1") throw new Error(`${await prop("receiptsMarker", "text")} receipts`);
    });
    await screenshot("05-damaged-key.png");
    await step("putting the key file back restores the same address", async () => {
      await stop();
      fs.writeFileSync(keyFile, keyBytes);
      await sleep(1000);
      await launch();
      await waitFor(async () => (await prop("ownAddressMarker", "text")) === ownAddress, 30000, "same address");
    });
  } catch (e) {
    log(`aborted: ${e.message}`);
    results.push(["run", false, e.message]);
  } finally {
    const failed = results.filter((r) => !r[1]).length;
    console.log(`\n${results.length - failed} passed, ${failed} failed`);
    if (!KEEP) await stop();
    process.exit(failed ? 1 : 0);
  }
})();

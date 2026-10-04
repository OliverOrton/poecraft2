import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {mkdirSync, readFileSync, writeFileSync, renameSync, existsSync} from "node:fs";
import {resolve} from "node:path";
import {execFileSync} from "node:child_process";

/** Optional immutable raw capture for the existing native qualification calls.
 * It changes no request, trial count, cap, price, scope or native output. */
export function retainNativeUIEvidence(name: string, payload: unknown): void {
    const folder = process.env.POECRAFT_UI_NATIVE_EVIDENCE_DIR;
    if (!folder) return;
    assert.match(name, /^[a-z0-9-]+$/);
    const root = new URL("../../../", import.meta.url);
    const build = JSON.parse(readFileSync(new URL("apps/web/src/generated/build-info.json", root), "utf8"));
    const sha = (path: string) => createHash("sha256").update(readFileSync(new URL(path, root))).digest("hex");
    assert.equal(build.repository_commit, execFileSync("git", ["rev-parse", "HEAD"], {cwd: root, encoding: "utf8"}).trim());
    assert.equal(build.working_tree_dirty, false, "Native UI evidence requires committed product inputs");
    assert.equal(build.engine.wasm_sha256, sha("bindings/wasm/dist/poecraft_engine.wasm"));
    assert.equal(build.engine.loader_sha256, sha("bindings/wasm/dist/poecraft_engine.mjs"));
    const lock = JSON.parse(readFileSync(new URL("apps/web/runtime.lock.json", root), "utf8"));
    assert.equal(build.runtime.manifest_sha256, lock.manifest_sha256);
    const destination = resolve(folder, name + ".json");
    mkdirSync(resolve(folder), {recursive: true});
    assert.equal(existsSync(destination), false, "Do not overwrite an earlier native capture");
    const temporary = destination + ".tmp";
    writeFileSync(temporary, JSON.stringify({kind: "native_ui_evidence_v1", name, build, payload}, null, 2) + "\n", {flag: "wx"});
    renameSync(temporary, destination);
}

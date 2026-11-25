/**
 * Proves the combat documentation gate does its job:
 * it passes on agreeing numbers, fails on drift, and ignores prose.
 *
 * Run from the repository root: node --test tools/pi/
 * Only Node standard-library modules are used.
 */
import assert from "node:assert/strict";
import { cpSync, existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCRIPT = join(REPO_ROOT, "tools", "pi", "check_combat_docs.mjs");
const DOC = join(REPO_ROOT, "Document", "30-design", "combat-system.md");
const CONFIG = join(REPO_ROOT, "Document", "50-worklog", "combat", "config.json");

const hasConfig = existsSync(CONFIG);
const skip = hasConfig ? false : "run the Client.Combat automation tests first (config.json)";

function withCopies(body) {
	const dir = mkdtempSync(join(tmpdir(), "combat-docs-"));
	try {
		const doc = join(dir, "combat-system.md");
		const config = join(dir, "config.json");
		cpSync(DOC, doc);
		cpSync(CONFIG, config);
		return body({ doc, config, edit: (from, to) => writeFileSync(doc, readFileSync(doc, "utf8").replace(from, to)) });
	} finally {
		rmSync(dir, { recursive: true, force: true });
	}
}

function run(doc, config) {
	const result = spawnSync(process.execPath, [SCRIPT, doc, config], { encoding: "utf8" });
	return { code: result.status, stdout: result.stdout, stderr: result.stderr };
}

test("combat docs gate passes when the tables match the code", { skip }, () => {
	withCopies(({ doc, config }) => {
		const result = run(doc, config);
		assert.equal(result.code, 0, result.stderr);
		assert.match(result.stdout, /9 weapon states/);
	});
});

test("combat docs gate reports table drift with the state and field", { skip }, () => {
	withCopies(({ doc, config, edit }) => {
		edit("| 0.5° | 0.2° | 3.0° |", "| 0.9° | 0.2° | 3.0° |");
		const result = run(doc, config);
		assert.equal(result.code, 1);
		assert.match(result.stderr, /Stand_Idle baseDeg is 0\.9 in the doc, 0\.5 in config\.json/);
	});
});

test("combat docs gate ignores prose added inside a table section", { skip }, () => {
	withCopies(({ doc, config, edit }) => {
		edit("| `Roll` | 발사 불가 | 발사 불가 | 발사 불가 |", "| `Roll` | 발사 불가 | 발사 불가 | 발사 불가 |\n\n표의 값은 각 상태의 상한이다.");
		const result = run(doc, config);
		assert.equal(result.code, 0, result.stderr);
	});
});

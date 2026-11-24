/**
 * Structural checks for the Document/ vault and its AGENTS.md rules.
 *
 * Run from the repository root: node --test tools/pi/vault.test.mjs
 * Only Node standard-library modules are used.
 */
import assert from "node:assert/strict";
import { existsSync, readdirSync, readFileSync, realpathSync, statSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const VAULT = join(REPO_ROOT, "Document");
const HOME = join(VAULT, "Home.md");
const LINK = /\[[^\]\n]*\]\(([^()\s]+)\)/g;
const EXTERNAL = /^[a-z][a-z0-9+.-]*:/i;

function readNote(file) {
	return new TextDecoder("utf-8", { fatal: true }).decode(readFileSync(file));
}

function vaultPath(file) {
	return relative(VAULT, file).split(sep).join("/");
}

function inside(root, file) {
	const path = relative(root, file);
	return path !== "" && path !== ".." && !path.startsWith(`..${sep}`) && !isAbsolute(path);
}

function notesUnder(dir) {
	const found = [];
	for (const entry of readdirSync(dir, { withFileTypes: true })) {
		if (entry.name.startsWith(".")) continue;
		const full = join(dir, entry.name);
		if (entry.isDirectory()) found.push(...notesUnder(full));
		else if (entry.isFile() && entry.name.endsWith(".md")) found.push(full);
	}
	return found;
}

function linksIn(file) {
	return [...readNote(file).matchAll(LINK)].map((match) => match[1]);
}

/** Crawl relative Markdown links from Home.md; every edge is checked, reachable notes are returned. */
function crawlVault() {
	const queue = [HOME];
	const visited = new Set(queue);
	const edges = [];
	while (queue.length > 0) {
		const file = queue.shift();
		for (const target of linksIn(file)) {
			if (EXTERNAL.test(target) || target.startsWith("#")) continue;
			const withoutAnchor = target.split("#")[0];
			if (withoutAnchor === "") continue;
			const absolute = resolve(dirname(file), withoutAnchor);
			edges.push({ from: file, target, absolute });
			if (!inside(VAULT, absolute) || !absolute.toLowerCase().endsWith(".md") || !existsSync(absolute)) continue;
			if (!inside(realpathSync(VAULT), realpathSync(absolute)) || !statSync(absolute).isFile()) continue;
			if (visited.has(absolute)) continue;
			visited.add(absolute);
			queue.push(absolute);
		}
	}
	return { notes: visited, edges };
}

/** Declared repository evidence: backticked paths on a line that starts with "근거 경로:". */
function evidencePaths(text) {
	const declared = [];
	for (const line of text.split(/\r?\n/)) {
		const match = /^\s*근거 경로:\s*(.+)$/.exec(line);
		if (!match) continue;
		for (const quoted of match[1].matchAll(/`([^`]+)`/g)) declared.push(quoted[1]);
	}
	return declared;
}

function sectionsByHeading(text) {
	const sections = new Map();
	let current = null;
	for (const line of text.split(/\r?\n/)) {
		const heading = /^##\s+(.+?)\s*$/.exec(line);
		if (heading) {
			current = heading[1];
			sections.set(current, []);
			continue;
		}
		if (current) sections.get(current).push(line);
	}
	return new Map([...sections].map(([heading, lines]) => [heading, lines.join("\n")]));
}

function sectionMatching(sections, pattern) {
	const heading = [...sections.keys()].find((name) => pattern.test(name));
	assert.ok(heading, `AGENTS.md has no section matching ${pattern}`);
	return sections.get(heading);
}

test("Home.md is a readable UTF-8 note that starts with a heading", () => {
	const text = readNote(HOME);
	assert.ok(text.trim().length > 0, "Home.md is empty");
	assert.match(text, /^#\s+\S/m, "Home.md has no top-level heading");
});

test("Home.md links every area index, and each area index links back to Home", () => {
	const areaDirs = readdirSync(VAULT, { withFileTypes: true })
		.filter((entry) => entry.isDirectory() && /^\d{2}-/.test(entry.name))
		.map((entry) => entry.name)
		.sort();
	for (const initialArea of ["00-meta", "10-architecture", "20-decisions", "40-guides", "50-worklog"]) {
		assert.ok(areaDirs.includes(initialArea), `missing required area ${initialArea}`);
	}

	const homeTargets = linksIn(HOME).map((target) => target.split("#")[0]);
	for (const area of areaDirs) {
		const index = join(VAULT, area, "index.md");
		assert.ok(existsSync(index), `missing area index ${vaultPath(index)}`);
		assert.ok(homeTargets.includes(`${area}/index.md`), `Home.md does not link ${area}/index.md`);

		const backToHome = linksIn(index).some((target) => resolve(dirname(index), target.split("#")[0]) === HOME);
		assert.ok(backToHome, `${area}/index.md does not link back to Home.md`);
	}
});

test("every relative link in the reachable notes resolves inside the vault", () => {
	const { edges } = crawlVault();
	assert.ok(edges.length > 0, "no links found in the vault");
	for (const edge of edges) {
		const where = `${vaultPath(edge.from)} -> ${edge.target}`;
		assert.equal(isAbsolute(edge.target), false, `${where}: link is absolute, not vault-relative`);
		assert.ok(inside(VAULT, edge.absolute), `${where}: link escapes the vault`);
		assert.ok(existsSync(edge.absolute) && statSync(edge.absolute).isFile(), `${where}: link target is missing`);
		assert.ok(inside(realpathSync(VAULT), realpathSync(edge.absolute)), `${where}: linked file is outside the vault`);
	}
});

test("every note is reachable from Home.md", () => {
	const { notes } = crawlVault();
	const orphans = notesUnder(VAULT)
		.map((file) => resolve(file))
		.filter((file) => !notes.has(file));
	assert.deepEqual(orphans.map(vaultPath), [], "notes are not reachable from Home.md");
});

test("declared evidence paths exist in the repository", () => {
	const { notes } = crawlVault();
	let declaredCount = 0;
	for (const note of notes) {
		const declared = evidencePaths(readNote(note));
		declaredCount += declared.length;
		for (const declaredPath of declared) {
			const where = `${vaultPath(note)}: 근거 경로 ${declaredPath}`;
			assert.equal(isAbsolute(declaredPath), false, `${where}: evidence path is absolute, not repository-relative`);
			const absolute = resolve(REPO_ROOT, declaredPath);
			assert.ok(inside(REPO_ROOT, absolute), `${where}: evidence path escapes the repository`);
			assert.ok(existsSync(absolute), `${where}: evidence path does not exist`);
		}
		if (note === HOME || note.endsWith(`${sep}index.md`)) continue;
		assert.ok(declared.length > 0, `${vaultPath(note)}: no 근거 경로 declaration`);
	}
	assert.ok(declaredCount >= 5, `only ${declaredCount} evidence paths declared in the vault`);
});

test("Obsidian can open Document/ without a plugin, and its local state stays uncommitted", () => {
	const home = readNote(HOME);
	const sections = sectionsByHeading(home);
	const opening = [...sections.keys()].find((heading) => /obsidian/i.test(heading));
	assert.ok(opening, "Home.md has no Obsidian opening section");
	const body = sections.get(opening);
	assert.match(body, /Open folder as vault|폴더를 Vault로 열기/i, "Obsidian section does not give the folder-opening action");
	assert.match(body, /Document\//, "Obsidian section does not identify the vault folder");
	assert.match(body, /Home\.md/, "Obsidian section does not point at Home.md");

	const ignore = readNote(join(VAULT, ".gitignore")).split(/\r?\n/).map((line) => line.trim());
	assert.ok(ignore.includes("/.obsidian/"), "Document/.gitignore does not ignore /.obsidian/");
});

test("AGENTS.md states the read, ownership, and hand-off rules", () => {
	const sections = sectionsByHeading(readNote(join(REPO_ROOT, "AGENTS.md")));

	const before = sectionMatching(sections, /작업 전/);
	assert.match(before, /Document\/Home\.md/, "작업 전 does not name the entry note");
	assert.match(before, /관련된 노트를 읽/, "작업 전 does not require reading relevant notes");
	assert.match(before, /차단/, "작업 전 does not describe a blocked session");
	assert.match(before, /우회하지\s*않는다/, "작업 전 does not forbid working around a blocked note");

	const delegation = sectionMatching(sections, /위임/);
	assert.match(delegation, /노트/, "위임 section does not hand off note paths");
	assert.match(delegation, /경로/, "위임 section does not name paths");
	assert.match(delegation, /사실/, "위임 section does not hand off verified facts");
	assert.match(delegation, /하위 세션.*가정하지 않는다/, "위임 section does not distinguish the main guard from child sessions");

	const closing = sectionMatching(sections, /작업을 마칠 때/);
	assert.match(closing, /메인 세션.*(갱신|고친)/, "작업을 마칠 때 does not assign updates to the main session");
	assert.match(closing, /Document\//, "작업을 마칠 때 does not name the Document/ vault");
	assert.match(closing, /바뀌/, "작업을 마칠 때 does not gate updates on changed knowledge");
	assert.match(closing, /(갱신|고친)/, "작업을 마칠 때 does not describe updating notes");
	assert.match(closing, /그대로 두/, "작업을 마칠 때 does not allow leaving the vault untouched");
});

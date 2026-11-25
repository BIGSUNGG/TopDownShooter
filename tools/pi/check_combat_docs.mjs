/**
 * Fails when the shooting tables in Document/30-design/combat-system.md drift
 * from the numbers the game code actually uses.
 *
 * The source of truth is the C++ tuning table; the Client.Combat.Tuning
 * automation test dumps it to Document/50-worklog/combat/config.json.
 *
 * Run from the repository root: node tools/pi/check_combat_docs.mjs
 * Optional arguments override the two files, so tests can point it at fixtures.
 * Only Node standard-library modules are used.
 */
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const DOC = process.argv[2] ?? join(REPO_ROOT, "Document", "30-design", "combat-system.md");
const CONFIG = process.argv[3] ?? join(REPO_ROOT, "Document", "50-worklog", "combat", "config.json");

const SPREAD_HEADING = "## 탄퍼짐";
const MUZZLE_HEADING = "## 총구 지연(벌어짐)";

/** Rows of the markdown table that follows the given heading. */
function tableRows(markdown, heading) {
	const start = markdown.indexOf(heading);
	if (start < 0) throw new Error(`missing heading: ${heading}`);
	const rest = markdown.slice(start + heading.length);
	const next = rest.indexOf("\n## ");
	const section = next < 0 ? rest : rest.slice(0, next);
	return section
		.split("\n")
		.filter((line) => line.trimStart().startsWith("|"))
		.map((line) => line.split("|").slice(1, -1).map((cell) => cell.trim()))
		.filter((cells) => cells.length > 0 && !cells.every((cell) => /^-{3,}$/.test(cell.replace(/:/g, ""))))
		.slice(1);
}

function number(cell) {
	const text = cell.replace(/[`°,\s]|°\/s/g, "");
	const value = Number.parseFloat(text);
	return Number.isFinite(value) ? value : null;
}

function stateName(cell) {
	return cell.replace(/`/g, "").trim();
}

function close(actual, expected) {
	return Math.abs(actual - expected) < 1e-6;
}

/**
 * Roll is written as "발사 불가" instead of numbers, so that literal is accepted for its cells as
 * long as the code really uses 0 for them (firing never happens in a roll).
 */
function compareCell(table, name, cell, field, actual) {
	const value = number(cell);
	if (value === null) {
		if (name === "Roll" && cell === "발사 불가") {
			if (!close(0, actual)) {
				problems.push(`${table}: ${name} ${field} is 발사 불가 in the doc, ${actual} in config.json`);
			}
			return;
		}
		problems.push(`${table}: ${name} ${field} is not a number in the doc`);
		return;
	}
	if (!close(value, actual)) {
		problems.push(`${table}: ${name} ${field} is ${value} in the doc, ${actual} in config.json`);
	}
}

const problems = [];

const markdown = readFileSync(DOC, "utf8");
if (!existsSync(CONFIG)) {
	console.error(`missing ${CONFIG}: run the Client.Combat automation tests first`);
	process.exit(1);
}
const config = JSON.parse(readFileSync(CONFIG, "utf8"));
const dumped = new Map(config.states.map((state) => [state.state, state]));

const spreadRows = tableRows(markdown, SPREAD_HEADING);
const muzzleRows = tableRows(markdown, MUZZLE_HEADING);

if (spreadRows.length !== 9) problems.push(`탄퍼짐 table has ${spreadRows.length} rows, expected 9`);
if (muzzleRows.length !== 9) problems.push(`총구 지연 table has ${muzzleRows.length} rows, expected 9`);

const states = config.states.map((state) => state.state);
if (states.length !== 9) problems.push(`config.json has ${states.length} states, expected 9`);

for (const [index, cells] of spreadRows.entries()) {
	const name = stateName(cells[0]);
	const state = dumped.get(name);
	if (!state) {
		problems.push(`탄퍼짐: ${name} is not in config.json`);
		continue;
	}
	if (states[index] !== name) problems.push(`탄퍼짐: row ${index + 1} is ${name}, config.json has ${states[index]}`);
	for (const [cell, field] of [
		[cells[1], "baseDeg"],
		[cells[2], "perShotDeg"],
		[cells[3], "maxDeg"],
	]) {
		compareCell("탄퍼짐", name, cell, field, state.spread[field]);
	}
}

for (const [index, cells] of muzzleRows.entries()) {
	const name = stateName(cells[0]);
	const state = dumped.get(name);
	if (!state) {
		problems.push(`총구 지연: ${name} is not in config.json`);
		continue;
	}
	if (states[index] !== name) problems.push(`총구 지연: row ${index + 1} is ${name}, config.json has ${states[index]}`);
	for (const [cell, field] of [
		[cells[1], "maxDeg"],
		[cells[2], "speedDegPerSec"],
	]) {
		compareCell("총구 지연", name, cell, field, state.muzzle[field]);
	}
}

if (problems.length > 0) {
	console.error(`combat-system.md and config.json disagree (${problems.length}):`);
	for (const problem of problems) console.error(`- ${problem}`);
	process.exit(1);
}

process.stdout.write(`combat-system.md matches config.json for ${states.length} weapon states\n`);

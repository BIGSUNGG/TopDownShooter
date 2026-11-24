/**
 * Deterministic harness for tools/pi/notes-guard.e2e.test.mjs.
 *
 * Uses only Node standard-library modules plus the installed Pi CLI. Nothing reaches the
 * network beyond a loopback OpenAI-compatible stub provider, and every scenario runs in its
 * own temp fixture copied from this repository (AGENTS.md, Document/, .pi/extensions/notes-guard.ts).
 *
 * Windows: `pi` on PATH is usually a .cmd shim that Node cannot spawn without a shell, so this
 * harness spawns the installed JavaScript entry directly on Windows. Set PI_CLI_JS to
 * the package's `bin.pi` entry (for example, dist/bundle/cli.js) if discovery fails.
 *
 * Evidence per scenario: rpc.jsonl (every stdout record), requests.jsonl (raw provider request
 * bodies), stderr.log, observations.json, plus the whole fixture directory.
 */
import { spawn } from "node:child_process";
import { createHash } from "node:crypto";
import {
	appendFileSync,
	cpSync,
	existsSync,
	mkdirSync,
	readFileSync,
	readdirSync,
	writeFileSync,
} from "node:fs";
import http from "node:http";
import { dirname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

export const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const PROVIDER_ID = "stub";
export const MODEL_ID = "stub-model";
export const CLOSING_CHECK_TYPE = "project-notes-closing-check";

/** Locate the installed Pi CLI. Returns { js } when it must run through node, else { binary }. */
export function resolvePiCli() {
	const explicit = process.env.PI_CLI_JS || process.env.PI_BIN;
	const candidates = explicit ? [resolve(explicit)] : [];
	const globalModuleDirs = [
		process.env.APPDATA && join(process.env.APPDATA, "npm", "node_modules"),
		join(dirname(process.execPath), "node_modules"),
		join(dirname(process.execPath), "..", "lib", "node_modules"),
	];
	if (!explicit) {
		for (const modules of globalModuleDirs.filter(Boolean)) {
			const packageDir = join(modules, "@earendil-works", "pi-coding-agent");
			const entry = readManifest(packageDir)?.bin?.pi;
			if (entry) candidates.push(resolve(packageDir, entry));
		}
	}
	for (const candidate of candidates) {
		if (!existsSync(candidate)) continue;
		if (/\.m?js$/i.test(candidate)) return { js: candidate, label: candidate };
		if (process.platform === "win32") {
			throw new Error(
				`notes-guard e2e: ${candidate} is not a JavaScript entry point. On Windows set PI_CLI_JS to the installed package's bin.pi entry.`, 
			);
		}
		return { binary: candidate, label: candidate };
	}
	throw new Error("notes-guard e2e: installed Pi CLI not found. Set PI_CLI_JS to the package's bin.pi entry.");
}

function readManifest(directory) {
	try {
		return JSON.parse(readFileSync(join(directory, "package.json"), "utf8"));
	} catch {
		return null;
	}
}

/** Record the installed Pi version for both dist/cli.js and dist/bundle/cli.js layouts. */
export function readPiVersion(cli) {
	if (!cli.js) return "unknown (binary CLI)";
	for (let dir = dirname(cli.js); dir !== dirname(dir); dir = dirname(dir)) {
		const manifest = readManifest(dir);
		if (manifest?.name === "@earendil-works/pi-coding-agent") return String(manifest.version);
	}
	return "unknown";
}

export function writeEvidence(dir, name, value) {
	mkdirSync(dir, { recursive: true });
	writeFileSync(join(dir, name), typeof value === "string" ? value : `${JSON.stringify(value, null, 2)}\n`);
}

export function listFiles(root) {
	const files = [];
	for (const entry of readdirSync(root, { withFileTypes: true })) {
		const full = join(root, entry.name);
		if (entry.isDirectory()) files.push(...listFiles(full));
		else if (entry.isFile()) files.push(full);
	}
	return files;
}

export function hashTree(root) {
	const hashes = {};
	for (const file of listFiles(root)) {
		hashes[relative(root, file).split(sep).join("/")] = createHash("sha256").update(readFileSync(file)).digest("hex");
	}
	return hashes;
}

/** Every string value in a parsed JSON structure, so assertions survive JSON escaping. */
export function flattenStrings(value) {
	if (typeof value === "string") return [value];
	if (Array.isArray(value)) return value.flatMap(flattenStrings);
	if (value && typeof value === "object") return Object.values(value).flatMap(flattenStrings);
	return [];
}

export function countOccurrences(haystacks, needle) {
	return haystacks.reduce((total, text) => total + text.split(needle).length - 1, 0);
}

export function isClosingCheckEntry(entry) {
	return (entry?.customType ?? entry?.message?.customType) === CLOSING_CHECK_TYPE;
}

/** Copy AGENTS.md, the Document vault, and the real extension into a fresh fixture. */
export function createFixture({ root, repoRoot = REPO_ROOT }) {
	mkdirSync(root, { recursive: true });
	cpSync(join(repoRoot, "AGENTS.md"), join(root, "AGENTS.md"));
	cpSync(join(repoRoot, "Document"), join(root, "Document"), {
		recursive: true,
		filter: (source) => !source.split(sep).includes(".obsidian"),
	});
	mkdirSync(join(root, ".pi", "extensions"), { recursive: true });
	cpSync(join(repoRoot, ".pi", "extensions", "notes-guard.ts"), join(root, ".pi", "extensions", "notes-guard.ts"));
	return {
		root,
		home: join(root, "Document", "Home.md"),
		vault: join(root, "Document"),
		extension: join(root, ".pi", "extensions", "notes-guard.ts"),
		markers: join(root, "markers"),
	};
}

/** models.json for one loopback provider; no credentials and no built-in provider is used. */
export function writeProfile({ dir, port }) {
	mkdirSync(dir, { recursive: true });
	const models = {
		providers: {
			[PROVIDER_ID]: {
				name: "Loopback stub",
				baseUrl: `http://127.0.0.1:${port}/v1`,
				api: "openai-completions",
				apiKey: "stub-key",
				models: [{ id: MODEL_ID, name: MODEL_ID, contextWindow: 200000, maxTokens: 4096 }],
			},
		},
	};
	writeFileSync(join(dir, "models.json"), `${JSON.stringify(models, null, 2)}\n`);
}

/**
 * Project-local simulation tools: a harmless extension tool, a delegation-shaped tool, and a
 * slow probe. They only write marker files inside the temp fixture; nothing is delegated.
 */
export function writeSimulationExtension({ root, markerDir }) {
	const source = `/**
 * E2E simulation tools for the notes-guard harness. Harmless local probes only.
 */
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { setTimeout } from "node:timers/promises";
import { Type } from "typebox";

const MARKER_DIR = ${JSON.stringify(markerDir.split(sep).join("/"))};

function mark(name) {
	mkdirSync(MARKER_DIR, { recursive: true });
	writeFileSync(join(MARKER_DIR, name), "executed\\n");
}

function result(text) {
	return { content: [{ type: "text", text: text }], details: {} };
}

export default function simulationTools(pi) {
	pi.registerTool({
		name: "probe_extension",
		label: "Probe extension tool",
		description: "Harmless extension tool used by the notes-guard E2E test; writes a marker file.",
		parameters: Type.Object({}),
		async execute() {
			mark("extension-tool.txt");
			return result("probe_extension executed");
		},
	});
	pi.registerTool({
		name: "delegate_task",
		label: "Delegation-shaped tool",
		description: "Delegation-shaped tool used by the notes-guard E2E test; records instead of delegating.",
		parameters: Type.Object({ task: Type.String() }),
		async execute(_toolCallId, params) {
			mark("delegation-tool.txt");
			return result("delegate_task recorded: " + params.task);
		},
	});
	pi.registerTool({
		name: "slow_probe",
		label: "Slow probe tool",
		description: "Sleeps so the E2E test can abort a run while a tool is genuinely executing.",
		parameters: Type.Object({}),
		async execute(_toolCallId, _params, signal) {
			mark("slow-probe-started.txt");
			await setTimeout(30000, undefined, { signal });
			mark("slow-probe-finished.txt");
			return result("slow_probe finished");
		},
	});
}
`;
	mkdirSync(join(root, ".pi", "extensions"), { recursive: true });
	const file = join(root, ".pi", "extensions", "zz-simulation-tools.ts");
	writeFileSync(file, source);
	return file;
}

function chunk(res, payload) {
	res.write(`data: ${JSON.stringify(payload)}\n\n`);
}

/**
 * Loopback OpenAI-compatible SSE provider. `turns(requestIndex, body)` returns
 * { text?, toolCalls?: [{ name, arguments }] }; every request body is kept for evidence.
 */
export async function startStubProvider({ turns }) {
	const requests = [];
	const server = http.createServer((request, response) => {
		const chunks = [];
		request.on("data", (data) => chunks.push(data));
		request.on("end", () => {
			const raw = Buffer.concat(chunks).toString("utf8");
			let body;
			try {
				body = JSON.parse(raw);
			} catch {
				body = { unparsed: raw };
			}
			const index = requests.length;
			requests.push(body);
			let turn;
			try {
				turn = turns(index, body) ?? {};
			} catch (error) {
				response.writeHead(400, { "content-type": "application/json" });
				response.end(JSON.stringify({ error: { message: String(error) } }));
				return;
			}
			const calls = turn.toolCalls ?? [];
			const base = { id: `chatcmpl-stub-${index}`, object: "chat.completion.chunk", created: 1, model: MODEL_ID };
			response.writeHead(200, { "content-type": "text/event-stream", "cache-control": "no-cache", connection: "close" });
			chunk(response, { ...base, choices: [{ index: 0, delta: { role: "assistant", content: "" }, finish_reason: null }] });
			if (turn.text) chunk(response, { ...base, choices: [{ index: 0, delta: { content: turn.text }, finish_reason: null }] });
			calls.forEach((call, position) => {
				chunk(response, {
					...base,
					choices: [
						{
							index: 0,
							delta: {
								tool_calls: [
									{
										index: position,
										id: `call_${index}_${position}`,
										type: "function",
										function: { name: call.name, arguments: JSON.stringify(call.arguments ?? {}) },
									},
								],
							},
							finish_reason: null,
						},
					],
				});
			});
			chunk(response, {
				...base,
				choices: [{ index: 0, delta: {}, finish_reason: calls.length > 0 ? "tool_calls" : (turn.finishReason ?? "stop") }],
				usage: { prompt_tokens: 1, completion_tokens: 1, total_tokens: 2 },
			});
			response.write("data: [DONE]\n\n");
			response.end();
		});
	});
	await new Promise((done) => server.listen(0, "127.0.0.1", done));
	const { port } = server.address();
	return {
		port,
		requests,
		close: async () => {
			server.closeAllConnections?.();
			await new Promise((done) => server.close(done));
		},
	};
}

/** RPC client: JSONL on stdin/stdout, records kept in memory and in evidence/rpc.jsonl. */
export function startRpcSession({ cli, cwd, profileDir, sessionDir, evidenceDir }) {
	mkdirSync(evidenceDir, { recursive: true });
	mkdirSync(sessionDir, { recursive: true });
	writeFileSync(join(evidenceDir, "rpc.jsonl"), "");
	const args = [
		"--mode",
		"rpc",
		"--approve",
		"--provider",
		PROVIDER_ID,
		"--model",
		MODEL_ID,
		"--session-dir",
		sessionDir,
	];
	const child = spawn(cli.js ? process.execPath : cli.binary, cli.js ? [cli.js, ...args] : args, {
		cwd,
		env: {
			...process.env,
			PI_CODING_AGENT_DIR: profileDir,
			PI_CODING_AGENT_SESSION_DIR: sessionDir,
			PI_OFFLINE: "1",
			PI_SKIP_VERSION_CHECK: "1",
			PI_TELEMETRY: "0",
		},
		stdio: ["pipe", "pipe", "pipe"],
		windowsHide: true,
		shell: false,
	});

	const records = [];
	const waiters = [];
	let buffer = "";
	let stderrText = "";
	let closed = false;
	let commandSeq = 0;

	child.stdin.on("error", () => {});
	child.stderr.setEncoding("utf8");
	child.stderr.on("data", (text) => {
		stderrText = `${stderrText}${text}`.slice(-8192);
		appendFileSync(join(evidenceDir, "stderr.log"), text);
	});

	function stderrTail() {
		const tail = stderrText.trim().split(/\r?\n/).slice(-5).join("\n");
		return tail ? `\npi stderr:\n${tail}` : "";
	}

	function pump() {
		for (const waiter of [...waiters]) {
			const hit = records.slice(waiter.from).find(waiter.predicate);
			if (hit) waiter.resolve(hit);
		}
	}

	function waitFor(predicate, { timeoutMs = 30000, label = "record", from = 0 } = {}) {
		const existing = records.slice(from).find(predicate);
		if (existing) return Promise.resolve(existing);
		return new Promise((resolvePromise, rejectPromise) => {
			let timer;
			const waiter = {
				predicate,
				from,
				resolve(record) {
					clearTimeout(timer);
					const position = waiters.indexOf(waiter);
					if (position !== -1) waiters.splice(position, 1);
					resolvePromise(record);
				},
				reject(error) {
					clearTimeout(timer);
					rejectPromise(error);
				},
			};
			timer = setTimeout(() => {
				const position = waiters.indexOf(waiter);
				if (position !== -1) waiters.splice(position, 1);
				rejectPromise(new Error(`timed out after ${timeoutMs}ms waiting for ${label}${stderrTail()}`));
			}, timeoutMs);
			timer.unref?.();
			waiters.push(waiter);
		});
	}

	child.stdout.setEncoding("utf8");
	child.stdout.on("data", (text) => {
		buffer += text;
		let newline = buffer.indexOf("\n");
		while (newline !== -1) {
			const line = buffer.slice(0, newline).replace(/\r$/, "");
			buffer = buffer.slice(newline + 1);
			newline = buffer.indexOf("\n");
			if (!line.trim()) continue;
			let record;
			try {
				record = JSON.parse(line);
			} catch {
				record = { type: "unparsed", line };
			}
			records.push(record);
			appendFileSync(join(evidenceDir, "rpc.jsonl"), `${line}\n`);
			pump();
		}
	});

	child.on("exit", (code, signal) => {
		for (const waiter of [...waiters]) {
			waiters.splice(waiters.indexOf(waiter), 1);
			waiter.reject(new Error(`pi exited (code=${code} signal=${signal}) while waiting${stderrTail()}`));
		}
	});

	async function command(payload, timeoutMs = 30000) {
		const id = `cmd-${++commandSeq}`;
		const response = waitFor((record) => record.type === "response" && record.id === id, {
			timeoutMs,
			label: `response:${payload.type}`,
			from: records.length,
		});
		response.catch(() => {});
		child.stdin.write(`${JSON.stringify({ ...payload, id })}\n`);
		return response;
	}

	async function prompt(message, timeoutMs = 90000) {
		const settled = waitFor((record) => record.type === "agent_settled", {
			timeoutMs,
			label: "agent_settled",
			from: records.length,
		});
		settled.catch(() => {});
		const response = await command({ type: "prompt", message }, timeoutMs);
		if (!response.success || response.data?.disposition !== "started") {
			throw new Error(`Expected Pi to start a new task: ${JSON.stringify(response)}`);
		}
		const record = await settled;
		if (record.aborted !== false) throw new Error(`Expected normal settlement: ${JSON.stringify(record)}`);
		return record;
	}

	async function entries() {
		const response = await command({ type: "get_entries" });
		if (!response.success || !Array.isArray(response.data?.entries)) {
			throw new Error(`Could not read Pi session entries: ${JSON.stringify(response)}`);
		}
		return response.data.entries;
	}

	async function close(timeoutMs = 15000) {
		if (closed) return null;
		closed = true;
		if (child.exitCode !== null || child.signalCode !== null) {
			return { code: child.exitCode, signal: child.signalCode };
		}
		const exited = new Promise((done) => child.once("exit", (code, signal) => done({ code, signal })));
		child.stdin.end();
		const stalled = new Promise((done) => {
			const handle = setTimeout(() => done(null), timeoutMs);
			handle.unref?.();
		});
		const outcome = await Promise.race([exited, stalled]);
		if (outcome === null) {
			child.kill("SIGKILL");
			await exited;
			return { code: "killed", signal: "SIGKILL" };
		}
		return outcome;
	}

	return {
		child,
		records,
		waitFor,
		command,
		prompt,
		entries,
		close,
		stderrText: () => stderrText,
	};
}

/**
 * End-to-end proof for .pi/extensions/notes-guard.ts against the installed Pi CLI.
 *
 * Run from the repository root: node --test tools/pi/notes-guard.e2e.test.mjs
 * Optional: set PI_CLI_JS to the installed package's bin.pi entry, and
 * NOTES_GUARD_EVIDENCE_DIR to a fresh directory to keep requests, RPC JSONL, and fixtures.
 *
 * Every assertion reads raw provider requests, RPC records, session entries, or files.
 * Model replies are never treated as evidence.
 */
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { after, test } from "node:test";
import { setTimeout as delay } from "node:timers/promises";
import {
	countOccurrences,
	createFixture,
	flattenStrings,
	hashTree,
	isClosingCheckEntry,
	readPiVersion,
	resolvePiCli,
	startRpcSession,
	startStubProvider,
	writeEvidence,
	writeProfile,
	writeSimulationExtension,
} from "./harness.mjs";

const cli = resolvePiCli();
const KEEP_EVIDENCE = Boolean(process.env.NOTES_GUARD_EVIDENCE_DIR);
const EVIDENCE_ROOT = KEEP_EVIDENCE
	? resolve(process.env.NOTES_GUARD_EVIDENCE_DIR)
	: mkdtempSync(join(tmpdir(), "notes-guard-e2e-"));
let failed = false;

after(() => {
	if (failed || KEEP_EVIDENCE) {
		console.error(`notes-guard e2e evidence kept at: ${EVIDENCE_ROOT}`);
		return;
	}
	rmSync(EVIDENCE_ROOT, { recursive: true, force: true });
});

function textOf(value) {
	return flattenStrings(value).join("\n");
}

const CLOSING = "Project notes closing check (once per task, main agent only).";

function lastUserText({ messages }) {
	const last = messages?.at(-1);
	if (last?.role !== "user") return "";
	if (typeof last.content === "string") return last.content;
	return last.content?.filter((part) => part.type === "text").map((part) => part.text).join("\n") ?? "";
}

function assertNote(request, diskBody) {
	const system = request.messages.filter((message) => message.role === "system");
	const allMessages = flattenStrings(request.messages);
	assert.equal(countOccurrences(allMessages, "<project_notes>"), 1, "expected a single notes section in the request");
	assert.equal(countOccurrences(allMessages, diskBody), 1, "expected exactly one copy of the disk body in context");
	assert.equal(countOccurrences(flattenStrings(system), `<project_notes>\n${diskBody}\n</project_notes>`), 1,
		"the system section did not contain the exact disk body");
}

function closingRequests(requests) {
	return requests.filter((request) => lastUserText(request).startsWith(CLOSING));
}

function toolEnds(session, toolName) {
	return session.records.filter((record) => record.type === "tool_execution_end" && record.toolName === toolName);
}

function toolStarts(session, toolName) {
	return session.records.filter((record) => record.type === "tool_execution_start" && record.toolName === toolName);
}

function closingChecks(entries) {
	return entries.filter(isClosingCheckEntry);
}

async function withScenario(name, turns, body) {
	const dir = join(EVIDENCE_ROOT, name);
	mkdirSync(EVIDENCE_ROOT, { recursive: true });
	mkdirSync(dir); // Never reuse markers, profiles, or receipts from a previous run.
	const fixture = createFixture({ root: join(dir, "project") });
	mkdirSync(fixture.markers, { recursive: true });
	writeSimulationExtension({ root: fixture.root, markerDir: fixture.markers });
	const provider = await startStubProvider({ turns: (index, request) => {
		assert.ok(index < 12, `Unexpected provider loop in ${name}`);
		return turns(index, request);
	} });
	const profileDir = join(dir, "profile");
	writeProfile({ dir: profileDir, port: provider.port });
	const observations = {
		scenario: name,
		piCli: cli.label,
		piVersion: readPiVersion(cli),
		node: process.version,
		provider: "stub/stub-model",
	};
	let session = null;
	try {
		session = startRpcSession({
			cli,
			cwd: fixture.root,
			profileDir,
			sessionDir: join(dir, "sessions"),
			evidenceDir: dir,
		});
		await body({ dir, fixture, provider, session, observations });
		writeEvidence(dir, "observations.json", { ...observations, ok: true, stderr: session.stderrText().slice(-2000) });
	} catch (error) {
		failed = true;
		writeEvidence(dir, "observations.json", {
			...observations,
			ok: false,
			error: String(error?.stack ?? error),
			stderr: session?.stderrText?.() ?? "",
		});
		throw error;
	} finally {
		writeEvidence(dir, "requests.jsonl", `${provider.requests.map((request, index) => JSON.stringify({ index, request })).join("\n")}\n`);
		await session?.close().catch(() => {});
		await provider.close().catch(() => {});
	}
}

const READY_ONE = "Task one: probe the vault.";
const READY_TWO = "Task two: read the linked notes.";

const readyTurns = (_index, body) => {
	const prompt = lastUserText(body);
	if (prompt === READY_ONE) return { toolCalls: [
		{ name: "write", arguments: { path: "markers/builtin.txt", content: "builtin executed\n" } },
		{ name: "probe_extension", arguments: {} },
		{ name: "delegate_task", arguments: { task: "summarize the notes" } },
	] };
	if (prompt === READY_TWO) return { toolCalls: [{ name: "read", arguments: { path: "Document/00-meta/index.md" } }] };
	return { text: "No further tools or vault changes needed." };
};

test("ready vault: exact note injected once per request, all tool kinds execute, one check per tool task", { timeout: 240000 }, async () => {
	await withScenario("ready-injection-and-tools", readyTurns, async ({ dir, fixture, provider, session, observations }) => {
		const firstHome = `<!-- synthetic first revision -->\n${readFileSync(fixture.home, "utf8")}`;
		writeFileSync(fixture.home, firstHome);
		const before = hashTree(fixture.vault);
		await session.prompt(READY_ONE);
		const firstRequests = provider.requests.slice();
		assert.equal(lastUserText(firstRequests[0]), READY_ONE);
		firstRequests.forEach((request) => assertNote(request, firstHome));
		assert.equal(closingRequests(firstRequests).length, 1);
		assert.equal(closingChecks(await session.entries()).length, 1);
		const after = hashTree(fixture.vault);
		assert.deepEqual(after, before, "a no-change closing check wrote to the vault");
		writeEvidence(dir, "unchanged-vault-hashes.json", { before, after });

		const freshHome = firstHome.replace("synthetic first revision", "synthetic second revision");
		writeFileSync(fixture.home, freshHome);
		await session.prompt(READY_TWO);
		const secondEnd = provider.requests.length;
		const secondRequests = provider.requests.slice(firstRequests.length, secondEnd);
		assert.equal(lastUserText(secondRequests[0]), READY_TWO);
		for (const request of secondRequests) {
			assertNote(request, freshHome);
			assert.ok(!textOf(request.messages).includes("synthetic first revision"), "previous note revision leaked into context");
		}
		assert.equal(closingRequests(secondRequests).length, 1);
		assert.equal(closingChecks(await session.entries()).length, 2);

		await session.prompt("Task three: answer without tools.");
		const textRequests = provider.requests.slice(secondEnd);
		assert.equal(textRequests.length, 1, "text-only task started an extra model turn");
		assertNote(textRequests[0], freshHome);
		assert.equal(closingRequests(textRequests).length, 0);
		assert.equal(closingChecks(await session.entries()).length, 2);

		// Every tool category really executed (marker files + durationMs evidence).
		for (const marker of ["builtin.txt", "extension-tool.txt", "delegation-tool.txt"]) {
			assert.ok(existsSync(join(fixture.markers, marker)), `${marker} was not written`);
		}
		for (const name of ["write", "probe_extension", "delegate_task"]) {
			const ends = toolEnds(session, name);
			assert.equal(ends.length, 1, `${name} did not finalize exactly once`);
			assert.equal(typeof ends[0].durationMs, "number", `${name} ran without a duration`);
			assert.equal(ends[0].isError, false, `${name} failed`);
		}

		assert.equal(toolEnds(session, "read").length, 1, "the second task did not call read");
		assert.equal(toolEnds(session, "read")[0].isError, false, "the second task could not read the linked index");
		observations.requests = [firstRequests.length, secondRequests.length, textRequests.length];
		observations.closingChecks = [1, 1, 0];
		observations.changedFilesAfterFirstCheck = [];
		observations.executed = ["write", "probe_extension", "delegate_task", "read"];
	});
});

test("closing check that edits one note changes only that note and stays a single check", { timeout: 240000 }, async () => {
	const prompt = "Task: read the linked worklog index.";
	const turns = (_index, request) => {
		if (lastUserText(request) === prompt) {
			return { toolCalls: [{ name: "read", arguments: { path: "Document/50-worklog/index.md" } }] };
		}
		if (lastUserText(request).startsWith(CLOSING))
			return {
				toolCalls: [
					{
						name: "write",
						arguments: { path: "Document/50-worklog/handoff.md", content: "# handoff\n\nE2E: durable knowledge recorded.\n" },
					},
				],
			};
		return { text: "Check finished without further tools." };
	};
	await withScenario("ready-closing-check-diff", turns, async ({ dir, fixture, provider, session, observations }) => {
		const before = hashTree(fixture.vault);
		await session.prompt(prompt);
		const after = hashTree(fixture.vault);
		const entries = await session.entries();
		const requests = provider.requests;

		assert.equal(closingRequests(requests).length, 1, "expected exactly one provider turn triggered by the check");
		assert.equal(closingChecks(entries).length, 1, "expected exactly one persisted closing check");
		const read = toolEnds(session, "read");
		const writes = toolEnds(session, "write");
		assert.equal(read.length, 1, "the ordinary task did not read the linked worklog index");
		assert.equal(read[0].isError, false);
		assert.equal(writes.length, 1, "the check-run edit never executed or executed more than once");
		assert.equal(typeof writes[0].durationMs, "number", "the check-run edit did not execute");
		assert.ok(requests.at(-1).messages.at(-1).role === "tool" && textOf(requests.at(-1).messages.at(-1)).includes("handoff.md"),
			"the model did not receive the closing-check tool result");

		const changed = [...new Set([...Object.keys(before), ...Object.keys(after)])].filter((file) => before[file] !== after[file]);
		assert.deepEqual(changed, ["50-worklog/handoff.md"], `unexpected vault changes: ${changed.join(", ")}`);
		writeEvidence(dir, "changed-vault-hashes.json", { before, after });
		observations.changedFiles = changed;
		observations.requests = requests.length;
		observations.closingChecks = closingChecks(entries).length;
	});
});

const blockedTurns = (_index, body) => {
	const prompt = lastUserText(body);
	if (prompt.includes("write a file"))
		return { toolCalls: [{ name: "write", arguments: { path: "markers/builtin-blocked.txt", content: "must not run\n" } }] };
	if (prompt.includes("extension probe")) return { toolCalls: [{ name: "probe_extension", arguments: {} }] };
	if (prompt.includes("delegate")) return { toolCalls: [{ name: "delegate_task", arguments: { task: "must not delegate" } }] };
	return { text: "Still blocked." };
};

async function assertAllBlocked({ fixture, provider, session, observations, reasonNeedle }) {
	// One independent prompt per tool kind, so a terminating block cannot hide the others.
	await session.prompt("Blocked task one: write a file.");
	await session.prompt("Blocked task two: call the extension probe.");
	await session.prompt("Blocked task three: delegate the work.");

	const entries = await session.entries();
	const ends = session.records.filter((record) => record.type === "tool_execution_end");
	assert.equal(ends.length, 3, "each category must finalize exactly one call");
	assert.equal(toolStarts(session, "write").length, 1, "write was never preflighted");
	assert.equal(toolStarts(session, "probe_extension").length, 1, "probe_extension was never preflighted");
	assert.equal(toolStarts(session, "delegate_task").length, 1, "delegate_task was never preflighted");
	for (const end of ends) {
		assert.equal(end.isError, true, `${end.toolName} was not blocked`);
		assert.equal(end.durationMs, undefined, `${end.toolName} executed despite the blocked note`);
		const evidence = textOf(end.result);
		assert.ok(evidence.includes("Project notes guard"), "the block reason is missing");
		assert.ok(evidence.includes(fixture.home), "the block reason omits the absolute note path");
		assert.ok(evidence.includes("UTF-8"), "the block reason omits the UTF-8 recovery instruction");
		assert.ok(evidence.includes("same session"), "the block reason omits the same-session retry instruction");
		assert.ok(evidence.includes(reasonNeedle), `the block reason omits ${reasonNeedle}`);
	}

	assert.deepEqual(readdirSync(fixture.markers), [], "a blocked tool still wrote a marker file");
	assert.equal(closingChecks(entries).length, 0, "a blocked-only task requested a closing check");
	assert.equal(closingRequests(provider.requests).length, 0, "a blocked-only task started a closing turn");

	observations.requests = provider.requests.length;
	observations.blockedCalls = ends.map((end) => end.toolName);
	observations.durationMs = ends.map((end) => end.durationMs ?? null);
	observations.closingChecks = 0;
}

test("missing entry note blocks builtin, extension, and delegation calls with recovery guidance", { timeout: 240000 }, async () => {
	await withScenario("blocked-missing-note", blockedTurns, async ({ fixture, provider, session, observations }) => {
		rmSync(fixture.home);
		await assertAllBlocked({ fixture, provider, session, observations, reasonNeedle: "ENOENT" });
	});
});

test("unreadable entry note (directory at Home.md) blocks every tool category", { timeout: 240000 }, async () => {
	await withScenario("blocked-read-error-note", blockedTurns, async ({ fixture, provider, session, observations }) => {
		rmSync(fixture.home);
		mkdirSync(fixture.home);
		assert.equal(statSync(fixture.home).isDirectory(), true, "fixture did not replace Home.md with a directory");
		await assertAllBlocked({ fixture, provider, session, observations, reasonNeedle: "EISDIR" });
	});
});

test("preflight rechecks the file, and restoration takes effect with the next task in the same session", { timeout: 240000 }, async () => {
	const remove = "Remove the note after the initial request.";
	const restore = "Restore the note before the blocked tool call.";
	const retry = "Retry now that the note is readable.";
	let home;
	let originalHome;
	const turns = (_index, body) => {
		switch (lastUserText(body)) {
			case remove:
				assertNote(body, originalHome);
				rmSync(home);
				return { toolCalls: [{ name: "write", arguments: { path: "markers/removed-after-start.txt", content: "must not run\n" } }] };
			case restore:
				assert.ok(!textOf(body.messages).includes(originalHome), "Pi treated the missing startup note as loaded");
				writeFileSync(home, originalHome);
				return { toolCalls: [{ name: "write", arguments: { path: "markers/restored-mid-task.txt", content: "must not run\n" } }] };
			case retry:
				return { toolCalls: [{ name: "write", arguments: { path: "markers/recovered.txt", content: "recovered\n" } }] };
			default:
				return { text: "No more work." };
		}
	};
	await withScenario("recovery-same-session", turns, async ({ fixture, provider, session, observations }) => {
		home = fixture.home;
		originalHome = readFileSync(home, "utf8");
		await session.prompt(remove);
		assert.ok(!existsSync(home), "the fixture did not remove the startup note");
		await session.prompt(restore);
		assert.equal(readFileSync(home, "utf8"), originalHome, "the fixture did not restore the startup note");
		assert.deepEqual(readdirSync(fixture.markers), [], "a preflight-blocked call wrote a marker");
		const denied = toolEnds(session, "write");
		assert.equal(denied.length, 2);
		for (const end of denied) {
			assert.equal(end.isError, true);
			assert.equal(end.durationMs, undefined, "the blocked call executed");
			assert.ok(textOf(end.result).includes("ENOENT"));
		}
		assert.equal(closingChecks(await session.entries()).length, 0);

		const restoredStart = provider.requests.length;
		await session.prompt(retry);
		assertNote(provider.requests[restoredStart], originalHome);
		assert.equal(readFileSync(join(fixture.markers, "recovered.txt"), "utf8"), "recovered\n");
		const recovered = toolEnds(session, "write");
		assert.equal(recovered.length, 3, "the recovery task did not finalize its call");
		assert.equal(typeof recovered[2].durationMs, "number", "the recovery call did not execute");

		assert.equal(closingChecks(await session.entries()).length, 1, "expected exactly one closing check after recovery");
		assert.equal(closingRequests(provider.requests.slice(restoredStart)).length, 1);
		assert.equal(closingRequests(provider.requests.slice(0, restoredStart)).length, 0);
		observations.requests = provider.requests.length;
		observations.blocked = ["removed after injection", "restored after failed startup"];
		observations.recovered = "write";
		observations.closingChecks = [0, 0, 1];
	});
});

test("abort while a tool is running starts no closing check and no extra provider request", { timeout: 240000 }, async () => {
	const turns = (index) => {
		if (index === 0) return { toolCalls: [{ name: "probe_extension", arguments: {} }] };
		if (index === 1) return { toolCalls: [{ name: "slow_probe", arguments: {} }] };
		return { text: "Unexpected follow-up." };
	};
	await withScenario("abort-no-check", turns, async ({ fixture, provider, session, observations }) => {
		const settled = session.waitFor((record) => record.type === "agent_settled", { timeoutMs: 40000, label: "agent_settled" });
		settled.catch(() => {});
		const prompt = await session.command({ type: "prompt", message: "Run a quick probe then the slow probe." });
		assert.equal(prompt.success && prompt.data?.disposition, "started");
		const started = join(fixture.markers, "slow-probe-started.txt");
		const deadline = Date.now() + 15000;
		while (!existsSync(started) && Date.now() < deadline) await delay(25);
		assert.ok(existsSync(started), "the slow tool never entered its execute callback");
		const completed = toolEnds(session, "probe_extension")[0];
		assert.equal(typeof completed?.durationMs, "number", "no tool completed before the abort");
		assert.equal(completed.isError, false);
		const requestsAtAbort = provider.requests.length;
		assert.equal(requestsAtAbort, 2);
		assert.equal((await session.command({ type: "abort" })).success, true);
		const settledRecord = await settled;
		assert.equal(settledRecord.aborted, true, "the run was not recorded as aborted");
		assert.equal(provider.requests.length, requestsAtAbort, "a provider request ran after the abort");
		assert.ok(!existsSync(join(fixture.markers, "slow-probe-finished.txt")), "the probe finished instead of being aborted");
		assert.equal(closingRequests(provider.requests).length, 0);

		const entries = await session.entries();
		assert.equal(closingChecks(entries).length, 0, "an aborted run requested a closing check");
		observations.requests = provider.requests.length;
		observations.aborted = settledRecord.aborted;
	});
});

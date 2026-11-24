/**
 * Project notes guard - project-local Pi extension for the Document/ vault.
 *
 * 1. before_agent_start: inject the full entry note into one system-prompt section.
 * 2. tool_call: fail closed for every model tool while the entry note cannot be read.
 * 3. agent_before_settle: ask the main agent once whether reusable knowledge changed.
 *
 * Scope: model tool calls of the session that loaded this project-local extension.
 * Shell subprocesses, other sessions, and agents that did not load it are not covered.
 *
 * shortcut: Home.md is injected in full; move details to linked notes if it grows large.
 */
import { readFile } from "node:fs/promises";
import { join } from "node:path";
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";

const NOTES_PATH = "Document/Home.md";
const SECTION_NAME = "project_notes";
const CLOSING_CHECK_TYPE = "project-notes-closing-check";

const CLOSING_CHECK_TEXT = [
	"Project notes closing check (once per task, main agent only).",
	"Decide whether this task produced durable project knowledge that the next task should reuse:",
	"a verified fact, a decision with its reason, a changed procedure, or a new constraint.",
	"If yes, update only the related notes under Document/ (and their index when the note",
	"structure changed) and name the paths in your reply.",
	"If nothing durable changed, write no files: leaving Document/ untouched is the correct",
	"outcome. Do not create routine work logs.",
].join(" ");

type NoteRead = { ok: true; text: string } | { ok: false; detail: string };

function notePath(cwd: string): string {
	return join(cwd, NOTES_PATH);
}

/** Read the entry note strictly: bytes must decode as UTF-8 and contain non-whitespace. */
async function readEntryNote(cwd: string): Promise<NoteRead> {
	let bytes: Buffer;
	try {
		bytes = await readFile(notePath(cwd));
	} catch (error) {
		const code = (error as { code?: string }).code ?? "read failed";
		return { ok: false, detail: `${NOTES_PATH} cannot be read (${code})` };
	}
	let text: string;
	try {
		text = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(bytes);
	} catch {
		return { ok: false, detail: `${NOTES_PATH} is not valid UTF-8` };
	}
	if (text.trim().length === 0) return { ok: false, detail: `${NOTES_PATH} is empty` };
	return { ok: true, text };
}

function recoveryReason(cwd: string, detail: string): string {
	return [
		`Project notes guard: ${detail} at ${notePath(cwd)}.`,
		"All model tools are blocked for this task.",
		`Restore ${notePath(cwd)} from outside Pi as a non-empty UTF-8 Markdown file, then send the request again in the same session.`,
	].join(" ");
}

export default function notesGuard(pi: ExtensionAPI) {
	let executedTool = false;
	let checkRequested = false;
	let startupFailure: string | null = "the vault entry note was not loaded for this task";

	pi.on("before_agent_start", async (event, ctx) => {
		executedTool = false;
		checkRequested = false;
		const note = await readEntryNote(ctx.cwd);
		// A failed start stays sticky until Pi prepares a new task, not a closing continuation.
		startupFailure = note.ok ? null : note.detail;
		event.systemPromptOptions.sections[SECTION_NAME] = note.ok ? note.text : recoveryReason(ctx.cwd, note.detail);
	});

	pi.on("tool_call", async (_event, ctx) => {
		if (startupFailure) {
			return { block: true, terminate: true, reason: recoveryReason(ctx.cwd, startupFailure) };
		}
		// Re-read at the call: the note may have become unreadable after a successful start.
		const note = await readEntryNote(ctx.cwd);
		if (!note.ok) {
			return { block: true, terminate: true, reason: recoveryReason(ctx.cwd, note.detail) };
		}
		return;
	});

	pi.on("tool_execution_end", (event) => {
		// durationMs exists only when execute() ran; tool_execution_start also fires for blocked calls.
		if (event.durationMs !== undefined) executedTool = true;
	});

	pi.on("agent_before_settle", (event) => {
		// One check per task: skip tasks without an actual execution, repeats, and aborted runs.
		if (!executedTool || checkRequested || event.outcome !== "completed") return;
		checkRequested = true; // set before returning so the check run cannot request another
		return {
			entries: [
				...event.entries,
				{
					type: "custom_message",
					customType: CLOSING_CHECK_TYPE,
					content: CLOSING_CHECK_TEXT,
					display: false,
				},
			],
			continue: true,
		};
	});
}

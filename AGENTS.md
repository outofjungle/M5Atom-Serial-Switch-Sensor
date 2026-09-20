# Agent Instructions

This file tells an AI agent how to write and work in this project.

## Writing rules for all documents

This project uses **Simplified Technical English**. Follow these rules in every file under
`docs/`, in this file, and in code comments.

1. Write one idea per sentence.
2. Keep each sentence under 20 words.
3. Use active voice. Say "the device sends the frame", not "the frame is sent".
4. Use present tense for how the system behaves.
5. Name each thing the same way every time. Do not use a different word for variety.
   Example: always write "frame", never switch to "packet" or "message" for the same object.
6. Define each term once, in `docs/00-overview.md`. Use only that term after it is defined.
7. Do not use metaphor, idiom, or figures of speech. Say what happens, in plain terms.
   Banned examples: "bites you", "hot loop", "ceremony", "pays off", "under the hood",
   "out of the box", "boils down to", "at the end of the day".
8. Do not use vague adjectives such as "fast", "small", "robust", or "efficient" alone.
   Give a number, a name, or a file path instead.
9. Put a table of facts in a table. Do not describe a table in a paragraph.
10. Keep a paragraph to 6 sentences or fewer.
11. Write a procedure as a numbered list. One action per step.
12. Spell out an abbreviation the first time it appears in a document, even if a different
    document already defined it. Example: "Cyclic Redundancy Check (CRC)".
13. Write code identifiers, file names, and commands in fixed-width text (backticks).

## Project summary

This project builds firmware for the M5Stack AtomS3 Lite board (ESP32-S3). The device connects
to a computer by USB. The computer sends commands. The device sends back sensor data. See
`docs/00-overview.md` for the full description.

## Documents

Read `docs/00-overview.md` first. It lists every other document in `docs/` and what each one
covers.

## Build commands

The build system is not yet written. This section will list the `make` targets once
Phase 2 of the project plan adds them: `make build`, `make flash`, `make monitor`,
`make clean`, `make menuconfig`.

## Git workflow

- Claude does not run `git push`. The user pushes to the remote by hand.
- Ask the user before creating a commit.
- Use this commit message format:
  - `feat:` — a new feature
  - `fix:` — a bug fix
  - `docs:` — a documentation change
  - `refactor:` — a code change with no behavior change
  - `WIP:` — a work-in-progress commit made during development

## Order of work

1. Write and review the documents in `docs/`. Do not write firmware code before this step is
   done and the user has approved the documents.
2. Write the build files (`Dockerfile`, `docker-compose.yml`, `Makefile`, `CMakeLists.txt`).
3. Write the firmware in `main/`.
4. Run the tests in `docs/11-test-plan.md`.

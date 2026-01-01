import test from "node:test";
import assert from "assert";
import path from "path";
import fs from "fs/promises";
import child_process from "child_process";

Error.stackTraceLimit = Infinity;

const executable = process.argv[2];
const files = await fs.readdir(import.meta.dirname);
const scripts = process.env.FILE ? process.env.FILE.split(",").filter(Boolean) : files.filter((f) => f.endsWith(".cy"));

for (const filename of scripts) {
  await test(filename, async () => {
    const fullPath = path.join(import.meta.dirname, filename);
    const text = await fs.readFile(fullPath, "utf-8");
    const expectedLogs = text
      .split("\n")
      .filter((line) => line.startsWith("#") && !line.startsWith("#!"))
      .map((line) =>
        line
          .substring(line.indexOf("#") + 1)
          .replaceAll("\r", "")
          .replaceAll("\\0", "\0")
          .replaceAll("\\t", "\t")
          .replaceAll("\\b", "\b")
          .replaceAll("\\r", "\r")
          .replaceAll("\\f", "\f")
          .trimStart()
      );

    for (let i = 0; i < expectedLogs.length; i++) {
      if (expectedLogs[i].includes("\\n")) {
        const parts = expectedLogs[i].split("\\n");
        expectedLogs.splice(i, 1, ...parts);
        i += parts.length - 1;
      }
    }


    const expectedErrors = text
      .split("\n")
      .filter((line) => line.startsWith("#!"))
      .map((line) => {
        const matches = line.match(
          /^#!\s*([0-9]+):([0-9]+)-([0-9]+):([0-9]+) (.+)/
        );
        return {
          startLineNumber: parseInt(matches[1]),
          startColumn: parseInt(matches[2]),
          endLineNumber: parseInt(matches[3]),
          endColumn: parseInt(matches[4]),
          message: matches[5].replaceAll("\r", ""),
        };
      });

    const process = child_process.spawnSync(executable, ["-"], { input: text });
    const status = process.status;
    const output = process.stdout.toString();
    const errors = process.stderr
      .toString()
      .trim()
      .split("\n")
      .filter(Boolean)
      .map((line) => {
        const matches = line.match(
          /^([0-9]+):([0-9]+)-([0-9]+):([0-9]+): error: (.+)/
        );

        return matches
          ? {
            startLineNumber: parseInt(matches[1]),
            startColumn: parseInt(matches[2]),
            endLineNumber: parseInt(matches[3]),
            endColumn: parseInt(matches[4]),
            message: matches[5].replaceAll("\r", ""),
          }
          : line;
      });

    const logs = [];

    for (let i = 0, j = 0; i < output.length; i += 1) {
      if (output[i] == "\n") {
        logs.push(output.slice(j, i))
        j = i + 1;

      }
    }

    assert.deepStrictEqual(errors, expectedErrors);
    assert.deepStrictEqual(logs, expectedLogs);

    if (errors.length === 0)
      assert.deepStrictEqual(status, 0);
    else
      assert.notDeepStrictEqual(status, 0);
  });
}

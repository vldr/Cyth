import test from "node:test";
import assert from "assert";
import path from "path";
import fs from "fs/promises";
import { spawnSync } from "child_process";

Error.stackTraceLimit = Infinity;

const executable = process.argv[2];
const decoder = new TextDecoder("utf-8");
const files = await fs.readdir(import.meta.dirname);
const scripts = files.filter((f) => f.endsWith(".cy"));

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
          .replaceAll("\\n", "\n")
          .replaceAll("\\f", "\f")
          .replaceAll("\\r", "\r")
          .trimStart()
      );

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

    const process = spawnSync(executable, [], { input: text });
    const bytecode = process.stdout;
    const error = process.stderr;
    const status = process.status;

    if (error.byteLength) {
      const errors = error.toString()
        .trim()
        .split("\n")
        .map((line) => {
          const matches = line.match(
            /^\(null\):([0-9]+):([0-9]+)-([0-9]+):([0-9]+): error: (.+)/
          );

          return matches
            ? {
              startLineNumber: parseInt(matches[1]),
              startColumn: parseInt(matches[2]),
              endLineNumber: parseInt(matches[3]),
              endColumn: parseInt(matches[4]),
              message: matches[5].replaceAll("\r", ""),
            }
            : { message: line };
        });

      assert.deepStrictEqual([], expectedLogs);
      assert.deepStrictEqual(errors, expectedErrors);
      assert.notDeepStrictEqual(status, 0);
    } else {
      const logs = [];

      function log(output) {
        if (typeof output === "object") {
          const at = result.instance.exports["string.at"];
          const length = result.instance.exports["string.length"];
          const array = new Uint8Array(length(output));
          for (let i = 0; i < array.byteLength; i++) {
            array[i] = at(output, i);
          }
          logs.push(decoder.decode(array));
        } else {
          logs.push(String(output));
        }
      }

      const kv = {};
      const result = await WebAssembly.instantiate(bytecode, {
        env: {
          "log": log,
          "log<bool>": log,
          "log<int>": log,
          "log<float>": log,
          "log<string>": log,
          "log<char>": log,

          set: (key, value) => { kv[key] = value; },
          get: (key) => { return kv[key]; },
        },
      });

      result.instance.exports["<start>"]();

      assert.deepStrictEqual([], expectedErrors);
      assert.deepStrictEqual(logs, expectedLogs);
      assert.deepStrictEqual(status, 0);
    }
  });
}

import test from "node:test";
import assert from "assert";
import path from "path";
import fs from "fs/promises";
import { spawn } from "child_process";

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
          .replaceAll("\\0", "\0")
          .replaceAll("\\t", "\t")
          .replaceAll("\\b", "\b")
          .replaceAll("\\n", "\n")
          .replaceAll("\\r", "\r")
          .replaceAll("\\f", "\f")
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
          message: matches[5].replace(/\r/g, ''),
        };
      });

    const bytecode = await new Promise((resolve, reject) => {
      const process = spawn(executable, [], { stdio: ["pipe", "pipe", "pipe"] });
      const chunks = [];
      let errorText = "";

      process.stdout.on("data", (chunk) => chunks.push(chunk));
      process.stderr.on("data", (chunk) => (errorText += chunk.toString("utf8")));

      process.on("error", reject);
      process.on("close", (code) => {
        if (errorText.length > 0) {
          const errors = errorText
            .trim()
            .split("\n")
            .filter(Boolean)
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
                  message: matches[5].replace(/\r/g, ''),
                }
                : { message: line };
            });

          resolve({ errors, code });
        } else {
          resolve({ bytecode: Buffer.concat(chunks), code });
        }
      });

      process.stdin.end(text);
    });

    if (bytecode.errors) {
      assert.notDeepStrictEqual(bytecode.code, 0);
      assert.deepStrictEqual(bytecode.errors, expectedErrors);
      assert.deepStrictEqual([], expectedLogs);
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
      const result = await WebAssembly.instantiate(bytecode.bytecode, {
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

      assert.deepStrictEqual(bytecode.code, 0);
      assert.deepStrictEqual([], expectedErrors);
      assert.deepStrictEqual(logs, expectedLogs);
    }
  });
}

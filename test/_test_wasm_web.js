import test from "node:test";
import assert from "assert";
import path from "path";
import fs from "fs/promises";
import cyth from "../editor/cyth.js";

Error.stackTraceLimit = Infinity;

await new Promise((resolve) => (cyth["onRuntimeInitialized"] = resolve));

let errors = [];
let logs = [];
let bytecode;

cyth._set_result_callback(
  cyth.addFunction(
    (size, data, sourceMapSize, sourceMap) =>
      (bytecode = cyth.HEAPU8.subarray(data, data + size)),
    "viiii"
  )
);

cyth._set_error_callback(
  cyth.addFunction(
    (startLineNumber, startColumn, endLineNumber, endColumn, message) =>
      errors.push({
        startLineNumber,
        startColumn,
        endLineNumber,
        endColumn,
        message: cyth.UTF8ToString(message),
      }),
    "viiiii"
  )
);

const encoder = new TextEncoder();
const decoder = new TextDecoder("utf-8");

let lastEncodedText;

function encodeText(text) {
  const data = encoder.encode(text);
  const offset = cyth._memory_alloc(data.byteLength + 1);
  cyth.HEAPU8.set(data, offset);
  cyth.HEAPU8[offset + data.byteLength] = 0;

  if (lastEncodedText) assert.strictEqual(offset, lastEncodedText);
  lastEncodedText = offset;

  return offset;
}

const files = await fs.readdir(import.meta.dirname);
const scripts = process.env.FILE ? process.env.FILE.split(",").filter(Boolean) : files.filter((f) => f.endsWith(".cy"));

for (const filename of scripts) {
  await test(filename + " (wasm)", async () => {
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
        const matches = line.match(/^#!\s*([0-9]+):([0-9]+)-([0-9]+):([0-9]+) (.+)$/);

        return {
          startLineNumber: parseInt(matches[1]),
          startColumn: parseInt(matches[2]),
          endLineNumber: parseInt(matches[3]),
          endColumn: parseInt(matches[4]),
          message: matches[5]
        }
      });

    errors.length = 0;
    logs.length = 0;

    cyth._run(encodeText(text), true);

    if (errors.length === 0) {
      function log(output) {
        if (typeof output === "object") {
          const at = result.instance.exports["string.at"];
          const length = result.instance.exports["string.length"];

          const array = new Uint8Array(length(output));
          for (let i = 0; i < array.byteLength; i++) {
            array[i] = at(output, i);
          }

          logs.push(decoder.decode(array));
        } else if (typeof output == "undefined") {
          logs.push("");
        } else {
          logs.push(String(output));
        }
      }

      const result = await WebAssembly.instantiate(bytecode, {
        env: {
          "log": log,

          "log<bool>": log,
          "log<int>": log,
          "log<float>": log,
          "log<string>": log,
          "log<char>": log,

          "log()": log,
          "log(int)": log,
          "log(float)": log,
          "log(string)": log,
          "log(char)": log,
          "log(bool)": log,
        },
      });

      result.instance.exports["<start>"]();
    }

    assert.deepStrictEqual(errors, expectedErrors);
    assert.deepStrictEqual(logs, expectedLogs);
  });
}

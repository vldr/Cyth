import { expect, test } from "bun:test";
import { Glob } from "bun";
import Module from "../editor/cyth.js";

await new Promise((resolve) => (Module["onRuntimeInitialized"] = resolve));

let errors = [];
let logs = [];
let bytecode;

Module._set_result_callback(
  Module.addFunction(
    (size, data, sourceMapSize, sourceMap) =>
      (bytecode = Module.HEAPU8.subarray(data, data + size)),
    "viiii"
  )
);

Module._set_error_callback(
  Module.addFunction(
    (startLineNumber, startColumn, endLineNumber, endColumn, message) =>
      errors.push({
        startLineNumber,
        startColumn,
        endLineNumber,
        endColumn,
        message: Module.UTF8ToString(message),
      }),
    "viiiii"
  )
);

const glob = new Glob(import.meta.dir + "/*.cy");
const encoder = new TextEncoder();
const decoder = new TextDecoder("utf-8");

let lastEncodedText;

function encodeText(text) {
  const data = encoder.encode(text);
  const offset = Module._memory_alloc(data.byteLength + 1);
  Module.HEAPU8.set(data, offset);
  Module.HEAPU8[offset + data.byteLength] = 0;

  if (lastEncodedText) expect(lastEncodedText).toEqual(offset);

  lastEncodedText = offset;

  return offset;
}

for await (const path of glob.scan(".")) {
  const kv = {};

  test(path, async () => {
    const file = Bun.file(path);
    const text = await file.text();
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

    Module._run(encodeText(text), true);

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

          set: (key, value) => { kv[key] = value; },
          get: (key) => { return kv[key]; },
        },
      });

      result.instance.exports["<start>"]();
    }

    expect(errors).toEqual(expectedErrors);
    expect(logs).toEqual(expectedLogs);
  });
}

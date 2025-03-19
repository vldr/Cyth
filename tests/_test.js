import { expect, test } from "bun:test";
import { Glob } from "bun";
import Module from "../output/cyth.js";

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
  test(path, async () => {
    const file = Bun.file(path);
    const text = await file.text();
    const expectedLogs = text
      .split("\n")
      .filter((line) => line.includes("#"))
      .map((line) => line.substring(line.indexOf("#") + 1).trimStart());

    errors.length = 0;
    logs.length = 0;

    Module._run(encodeText(text), true);

    if (errors.length === 0) {
      const result = await WebAssembly.instantiate(bytecode, {
        env: {
          log: (output) => {
            if (typeof output === "object") {
              const at = result.instance.exports["string.at"];
              const length = result.instance.exports["string.length"];

              let text = "";
              for (let i = 0; i < length(output); i++)
                text += String.fromCharCode(at(output, i));

              logs.push(text);
            } else {
              logs.push(String(output));
            }
          },
        },
      });

      result.instance.exports["<start>"]();
    }

    expect(errors).toBeEmpty();
    expect(logs).toEqual(expectedLogs);
  });
}

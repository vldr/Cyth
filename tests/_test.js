import { expect, test } from "bun:test";
import { Glob } from "bun";
import Module from "../output/cyth.js";

await new Promise(resolve => Module["onRuntimeInitialized"] = resolve);

let errors = [];
let logs = [];
let bytecode;

Module.set_error_callback = Module.cwrap("set_error_callback", null, [
  "number",
]);

Module.set_result_callback = Module.cwrap("set_result_callback", null, [
  "number",
]);

Module.set_result_callback(Module.addFunction((size, data) => bytecode = Module.HEAPU8.subarray(data, data + size), "vii"));
Module.set_error_callback(Module.addFunction((startLineNumber, startColumn, endLineNumber, endColumn, message) => errors.push({
  startLineNumber,
  startColumn,
  endLineNumber,
  endColumn,
  message: Module.UTF8ToString(message)
}), "viiiii"));

const glob = new Glob(import.meta.dir + "/*.cy");
const run = Module.cwrap("run", null, ["string", "number"]);

for await (const path of glob.scan(".")) {
  test(path, async () => {
    const file = Bun.file(path);
    const text = await file.text();
    const expectedLogs = text
                        .split("\n") 
                        .filter(line => line.includes("#"))
                        .map(line => line.substring(line.indexOf("#") + 1).trim());

    errors.length = 0;
    logs.length = 0;

    run(text, true);

    await WebAssembly.instantiate(bytecode, { 
      env: {
        log: (text) => { logs.push(String(text)); }
      }
    });

    expect(errors).toBeEmpty();
    expect(logs).toEqual(expectedLogs);

    Module._free(bytecode);
  });
}

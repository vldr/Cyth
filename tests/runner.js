import Module from "../output/cyth.js";
import { expect, test } from "bun:test";
import { Glob } from "bun";

await new Promise(resolve => Module["onRuntimeInitialized"] = resolve);

let errors = [];
let logs = [];
let resolve;

Module.print = () => {};

Module.set_error_callback = Module.cwrap("set_error_callback", null, [
  "number",
]);

Module.set_result_callback = Module.cwrap("set_result_callback", null, [
  "number",
]);

Module.set_result_callback(Module.addFunction(async (size, data) => {
  const bytecode = Module.HEAPU8.subarray(data, data + size);
  const env = {
    log: (text) => { logs.push(text); } 
  };

  await WebAssembly.instantiate(bytecode, { env });

  resolve();
  Module._free(data);
}, "vii"));

Module.set_error_callback(Module.addFunction((startLineNumber, startColumn, endLineNumber, endColumn, message) => errors.push({
  startLineNumber,
  startColumn,
  endLineNumber,
  endColumn,
  message: Module.UTF8ToString(message)
}), "viiiii"));

const glob = new Glob("input/*.cy");
const run = Module.cwrap("run", null, ["string", "number"]);

for await (const path of glob.scan(".")) {
  test(path, async () => {
    const file = Bun.file(path);
    const text = await file.text();
    const expectedLogs = text
                        .split("\n") 
                        .filter(line => line.startsWith("#"))
                        .map(line => Number(line.substring(1).trim())); 

    const promise = new Promise(resolve_ => resolve = resolve_);

    errors.length = 0;
    logs.length = 0;

    run(text, true);

    await promise;

    expect(errors).toBeEmpty();
    expect(logs).toEqual(expectedLogs);
  });
}

onmessage = (event) => 
{
    const data = event.data;

    switch (data.type)
    {
        case "start": 
        {
            try 
            {
                const module = new WebAssembly.Module(data.bytecode);
                const instance = new WebAssembly.Instance(module, {
                    env: {
                        log: function(output) {
                            if (typeof(output) === "object")
                            {
                                const length = instance.exports["string.length"];
                                const at = instance.exports["string.at"];

                                let text = String();
                                for (let i = 0; i < length(output); i++)
                                    text += String.fromCharCode(at(output, i));

                                postMessage({ type: "print", text }); 
                            }
                            else 
                            {
                                postMessage({ type: "print", text: output });
                            }
                        }
                    }
                });

                instance.exports["~start"]();

                postMessage({ type: "stop" });
            }
            catch (error)
            {
                postMessage({ type: "error", message: error.message ? error.message : "Internal error." });
            }

            break;
        }
    }
}
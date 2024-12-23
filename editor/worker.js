onmessage = async (event) => 
{
    const data = event.data;

    switch (data.type)
    {
        case "start": 
        {
            try 
            {
                const env = { 
                    log: (text) => {
                        postMessage({ type: "print", text});
                    } 
                }

                await WebAssembly.instantiate(data.bytecode, { env });

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
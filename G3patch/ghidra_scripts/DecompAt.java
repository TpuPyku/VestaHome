// DecompAt.java - decompile the function containing each provided address.
// GhidraScript args: addresses (e.g. 0x005f7e94).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompAt extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        for (String a : getScriptArgs()) {
            Address addr = currentProgram.getAddressFactory().getAddress(a);
            if (addr == null) {
                println("bad address: " + a);
                continue;
            }
            Function f = getFunctionContaining(addr);
            if (f == null) {
                println("no function containing " + a);
                continue;
            }
            println("======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
            DecompileResults r = di.decompileFunction(f, 90, monitor);
            if (r != null && r.decompileCompleted()) {
                println(r.getDecompiledFunction().getC());
            } else {
                println("decompile failed for " + f.getName());
            }
            println("");
        }
    }
}

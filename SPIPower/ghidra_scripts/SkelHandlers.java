// SkelHandlers.java - the shell's handlers of the MCU's inbound messages: for each skeleton (SPApi_SetSkelonton id)
// its vtable's slot 1 (interpretCommand calls skel->vtable[1](&buf[1]), buf = [id_hi][id_lo][len][payload]) is
// decompiled, with the functions it calls directly (one level: the handler often hands each code to a method).
// Into %OUTF%. GhidraScript args: id:vtable pairs in hex, e.g. 6:0077d8dc 8:00781f40.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.LinkedHashSet;
import java.util.Set;

public class SkelHandlers extends GhidraScript {

    private DecompInterface di;

    private String decomp(Function f) {
        DecompileResults r = di.decompileFunction(f, 120, monitor);
        return r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// (failed)\n";
    }

    @Override
    public void run() throws Exception {
        java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(System.getenv("OUTF")));
        di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            String[] p = arg.split(":");
            Address vt = toAddr(Long.parseLong(p[1], 16));
            Address h = toAddr(getInt(vt.add(4)) & 0xffffffffL);
            Function f = getFunctionAt(h);
            if (f == null)
                f = createFunction(h, null);
            w.println("######## SKEL " + Integer.parseInt(p[0], 16) + " vtable " + vt + " handler " + h);
            if (f == null) {
                w.println("// no function");
                continue;
            }
            w.println(decomp(f));
            Set<Function> callees = new LinkedHashSet<Function>(f.getCalledFunctions(monitor));
            for (Function c : callees) {
                if (c.isExternal() || c.isThunk() || c.getBody().getNumAddresses() < 24)
                    continue;
                w.println("//// callee " + c.getName() + " @ " + c.getEntryPoint());
                w.println(decomp(c));
            }
        }
        w.close();
    }
}

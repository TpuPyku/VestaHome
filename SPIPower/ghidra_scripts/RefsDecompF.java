// RefsDecompF.java - for each address (data or function): every function that refers to it, decompiled once,
// into %OUTF%. GhidraScript args: addresses (0x...).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.util.LinkedHashSet;
import java.util.Set;

public class RefsDecompF extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(System.getenv("OUTF")));
        Set<Function> done = new LinkedHashSet<Function>();
        for (String a : getScriptArgs()) {
            Address t = currentProgram.getAddressFactory().getAddress(a);
            w.println("######## refs to " + a);
            for (Reference r : getReferencesTo(t)) {
                Function f = getFunctionContaining(r.getFromAddress());
                w.println("  " + r.getFromAddress() + " " + r.getReferenceType() + " in " + (f == null ? "?" : f.getName()));
                if (f == null || !done.add(f))
                    continue;
                DecompileResults d = di.decompileFunction(f, 120, monitor);
                w.println("======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
                w.println(d != null && d.decompileCompleted() ? d.getDecompiledFunction().getC() : "// failed");
            }
        }
        w.close();
    }
}

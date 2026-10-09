// Callers.java - list and decompile the callers of the given functions.
// GhidraScript args: addresses inside the functions of interest (e.g. 0x2284c).
// Both code references and data references are reported: CE/MFC code reaches
// handlers through vtables, message maps and ON_COMMAND tables, which show up
// as data references from the vtable/map, not as direct calls.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class Callers extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);

        java.util.Set<Function> fns = new java.util.LinkedHashSet<>();
        for (String a : getScriptArgs()) {
            Address addr = currentProgram.getAddressFactory().getAddress(a);
            Function f = (addr == null) ? null : getFunctionContaining(addr);
            if (f == null) {
                println("no function containing " + a);
                continue;
            }
            println("== " + f.getName() + " @ " + f.getEntryPoint() + "   (from " + a + ")");
            for (Reference r : getReferencesTo(f.getEntryPoint())) {
                Address fa = r.getFromAddress();
                Function c = getFunctionContaining(fa);
                println("   " + r.getReferenceType() + " " + fa + " in " + (c == null ? "<data>" : c.getName()));
                if (c != null) fns.add(c);
            }
        }

        for (Function f : fns) {
            println("\n======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
            DecompileResults r = di.decompileFunction(f, 180, monitor);
            if (r != null && r.decompileCompleted()) {
                println(r.getDecompiledFunction().getC());
            } else {
                println("decompile failed");
            }
        }
    }
}

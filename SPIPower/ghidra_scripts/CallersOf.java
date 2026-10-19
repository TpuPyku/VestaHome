// CallersOf.java - for each import (IAT address, e.g. 0x006f097c) or function address: the functions that
// call it (through its thunk too), each decompiled once. GhidraScript args: addresses.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.util.LinkedHashSet;
import java.util.Set;

public class CallersOf extends GhidraScript {

    private void addCallers(Address target, Set<Function> out, int depth) {
        for (Reference r : getReferencesTo(target)) {
            Function f = getFunctionContaining(r.getFromAddress());
            if (f == null)
                continue;
            // a thunk (or a tiny stub that only loads the IAT entry): its own callers are the real ones
            if (depth < 2 && (f.isThunk() || f.getBody().getNumAddresses() <= 16)) {
                println("  via stub " + f.getName() + " @ " + f.getEntryPoint());
                addCallers(f.getEntryPoint(), out, depth + 1);
            } else {
                out.add(f);
            }
        }
    }

    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        Set<Function> done = new LinkedHashSet<Function>();
        for (String a : getScriptArgs()) {
            Address addr = currentProgram.getAddressFactory().getAddress(a);
            Set<Function> callers = new LinkedHashSet<Function>();
            addCallers(addr, callers, 0);
            println("######## " + a + ": " + callers.size() + " caller(s)");
            for (Function f : callers) {
                println("  " + f.getName() + " @ " + f.getEntryPoint());
                if (!done.add(f))
                    continue;
                DecompileResults r = di.decompileFunction(f, 90, monitor);
                println("======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
                println(r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC() : "decompile failed");
            }
        }
    }
}

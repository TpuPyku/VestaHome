// RefsTo.java - decompile the functions that reference a symbol (imported function, label) by name.
// GhidraScript args: substrings of symbol names (e.g. writeDatagram). Each matching symbol's references
// are listed and their containing functions decompiled once.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.util.HashSet;
import java.util.Set;

public class RefsTo extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        Set<Function> done = new HashSet<Function>();
        for (String key : getScriptArgs()) {
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (!s.getName().contains(key))
                    continue;
                println("#### symbol " + s.getName(true) + " @ " + s.getAddress());
                for (Reference r : s.getReferences()) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    println("  ref from " + r.getFromAddress() + (f != null ? " in " + f.getName() : ""));
                    if (f == null || done.contains(f))
                        continue;
                    done.add(f);
                    // thunks: follow one level up to the real callers
                    if (f.isThunk()) {
                        for (Reference r2 : getReferencesTo(f.getEntryPoint())) {
                            Function g = getFunctionContaining(r2.getFromAddress());
                            if (g != null && !done.contains(g)) {
                                done.add(g);
                                decompile(di, g);
                            }
                        }
                        continue;
                    }
                    decompile(di, f);
                }
            }
        }
    }

    private void decompile(DecompInterface di, Function f) {
        println("======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
        DecompileResults r = di.decompileFunction(f, 90, monitor);
        if (r != null && r.decompileCompleted())
            println(r.getDecompiledFunction().getC());
        else
            println("decompile failed for " + f.getName());
    }
}

// ExtCallersF.java - the callers of a library's imports (by ordinal or name), decompiled, into %OUTF%.
// GhidraScript args: library (e.g. btapi.dll) then import names (e.g. Ordinal_244 Ordinal_246).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.ExternalLocationIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class ExtCallersF extends GhidraScript {
    private void callers(Symbol s, Set<Function> out, int depth) {
        for (Reference r : getReferencesTo(s.getAddress())) {
            Function f = getFunctionContaining(r.getFromAddress());
            if (f == null) {   // an IAT slot: who reads it
                for (Reference r2 : getReferencesTo(r.getFromAddress())) {
                    Function g = getFunctionContaining(r2.getFromAddress());
                    if (g != null)
                        out.add(g);
                }
                continue;
            }
            if (depth < 1 && f.isThunk()) {
                callers(f.getSymbol(), out, depth + 1);
                continue;
            }
            out.add(f);
        }
    }

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        List<String> want = Arrays.asList(a).subList(1, a.length);
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(System.getenv("OUTF")));
        ExternalLocationIterator it = currentProgram.getExternalManager().getExternalLocations(a[0]);
        Set<Function> done = new LinkedHashSet<Function>();
        while (it.hasNext()) {
            ExternalLocation el = it.next();
            if (!want.contains(el.getLabel()))
                continue;
            Set<Function> fs = new LinkedHashSet<Function>();
            callers(el.getSymbol(), fs, 0);
            Function ef = el.getFunction();
            if (ef != null)
                for (Function t : ef.getCallingFunctions(monitor))
                    if (t.isThunk())
                        for (Function c : t.getCallingFunctions(monitor))
                            fs.add(c);
                    else
                        fs.add(t);
            w.println("######## " + a[0] + " " + el.getLabel() + ": " + fs.size() + " caller(s)");
            for (Function f : fs) {
                w.println("  " + f.getName() + " @ " + f.getEntryPoint());
                if (!done.add(f))
                    continue;
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                w.println("======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
                w.println(r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed");
            }
        }
        w.close();
    }
}

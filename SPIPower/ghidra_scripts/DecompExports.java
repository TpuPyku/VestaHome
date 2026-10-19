// DecompExports.java - decompile the exported functions whose names contain any of the given parts
// (case-insensitive), e.g. "SV_Video". GhidraScript args: name parts.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class DecompExports extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        String[] parts = getScriptArgs();
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        while (it.hasNext()) {
            Symbol s = it.next();
            if (!s.isExternalEntryPoint())
                continue;
            boolean want = parts.length == 0;
            for (String p : parts)
                want |= s.getName().toLowerCase().contains(p.toLowerCase());
            if (!want)
                continue;
            Function f = getFunctionAt(s.getAddress());
            if (f == null)
                continue;
            DecompileResults r = di.decompileFunction(f, 90, monitor);
            println("======== " + s.getName() + " @ " + f.getEntryPoint() + " ========");
            println(r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC() : "decompile failed");
        }
    }
}

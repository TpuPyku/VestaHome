// CallSites.java - every call of the named functions (imports or not) with its constant arguments, as the
// decompiler shows them, and the calling function; tab-separated lines into %OUTF%: callee, args, caller,
// caller address and the strings the caller uses (a hint of its screen: picture names, debug lines). For
// tables like "which SendKeyEvent code is sent from where".
// GhidraScript args: callee name parts (e.g. SPApi_SendKeyEvent SPApi_SendExtendKeyEvent).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CallSites extends GhidraScript {

    private void callers(Address target, Set<Function> out, int depth) {
        for (Reference r : getReferencesTo(target)) {
            Function f = getFunctionContaining(r.getFromAddress());
            if (f == null)
                continue;
            if (depth < 2 && f.getBody().getNumAddresses() <= 16) {   // a thunk: its callers
                callers(f.getEntryPoint(), out, depth + 1);
                continue;
            }
            out.add(f);
        }
    }

    private String strings(Function f) {   // the strings f refers to, up to 6, short
        StringBuilder b = new StringBuilder();
        int n = 0;
        AddressIterator ai = f.getBody().getAddresses(true);
        while (ai.hasNext() && n < 6) {
            Address a = ai.next();
            for (Reference r : getReferencesFrom(a)) {
                Data d = getDataAt(r.getToAddress());
                if (d == null || !d.hasStringValue())
                    continue;
                String v = String.valueOf(d.getValue()).replaceAll("[\t\r\n]", " ");
                if (v.length() > 60)
                    v = v.substring(0, 60);
                b.append(n > 0 ? " | " : "").append(v);
                n++;
            }
        }
        return b.toString();
    }

    @Override
    public void run() throws Exception {
        java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(System.getenv("OUTF")));
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        Map<Function, String> cache = new HashMap<Function, String>();
        for (String part : getScriptArgs()) {
            Set<String> names = new LinkedHashSet<String>();
            Set<Function> fs = new LinkedHashSet<Function>();
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (!s.getName().contains(part))
                    continue;
                names.add(s.getName().replaceAll("^PTR_", "").replaceAll("_[0-9a-f]{8}$", ""));
                callers(s.getAddress(), fs, 0);
            }
            for (Function f : fs) {
                String c = cache.get(f);
                if (c == null) {
                    DecompileResults r = di.decompileFunction(f, 120, monitor);
                    c = r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC() : "";
                    cache.put(f, c);
                }
                for (String n : names) {
                    Matcher m = Pattern.compile(Pattern.quote(n) + "\\(([^;]*?)\\);").matcher(c);
                    String hint = null;
                    while (m.find()) {
                        if (hint == null)
                            hint = strings(f);
                        w.println(part + "\t" + m.group(1).replaceAll("\\s+", " ") + "\t" + f.getName() + "\t"
                                  + f.getEntryPoint() + "\t" + hint);
                    }
                }
            }
        }
        w.close();
    }
}

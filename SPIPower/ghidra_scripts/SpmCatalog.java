// SpmCatalog.java - the catalogue of SPIPowerMgr.dll's exports: for each one the MCU message it sends and the
// kinds of its arguments. An export is a wrapper around one inner function, which packs the arguments one by one
// (pack(buf, kind, &arg); kind 0xd = a byte) and sends the buffer with the message id (send(proxy, id, buf)).
// Output: tab-separated lines "export  id  kinds  inner" into the file named by %OUTF% (Ghidra's console log
// does not work when its path has non-ASCII letters). GhidraScript args: none.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class SpmCatalog extends GhidraScript {

    private DecompInterface di;

    private String decomp(Function f) {
        DecompileResults r = di.decompileFunction(f, 60, monitor);
        return r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC() : "";
    }

    @Override
    public void run() throws Exception {
        java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(System.getenv("OUTF")));
        di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        Pattern call = Pattern.compile("(FUN_[0-9a-f]{8})\\(");
        Pattern send = Pattern.compile("FUN_[0-9a-f]{8}\\(\\w+,(0x[0-9a-f]+),\\(int\\)\\w+\\)");
        Pattern pack = Pattern.compile("FUN_[0-9a-f]{8}\\(\\(int\\)\\w+,(0x[0-9a-f]+|\\d+),");
        List<String> out = new ArrayList<String>();
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        while (it.hasNext()) {
            Symbol s = it.next();
            if (!s.isExternalEntryPoint())
                continue;
            Function f = getFunctionAt(s.getAddress());
            if (f == null || s.getName().startsWith("FUN_") || s.getName().equals("entry"))
                continue;
            String c = decomp(f);
            String id = "-", kinds = "", inner = "-";
            Matcher m = call.matcher(c);
            if (m.find()) {
                inner = m.group(1);
                Address a = currentProgram.getAddressFactory().getAddress(inner.substring(4));
                Function g = a != null ? getFunctionAt(a) : null;
                String ic = g != null ? decomp(g) : "";
                Matcher ms = send.matcher(ic);
                if (ms.find())
                    id = ms.group(1);
                Matcher mp = pack.matcher(ic);
                while (mp.find())
                    kinds += (kinds.length() > 0 ? "," : "") + mp.group(1);
            }
            String sig = f.getSignature().getPrototypeString().replace('\t', ' ');
            out.add(s.getName() + "\t" + id + "\t" + kinds + "\t" + inner + "\t" + sig);
        }
        Collections.sort(out);
        for (String l : out)
            w.println(l);
        w.close();
    }
}

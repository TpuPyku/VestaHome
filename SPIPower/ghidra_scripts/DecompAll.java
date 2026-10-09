// DecompAll.java - decompile every (non-thunk, non-external) function.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.PrintWriter;

public class DecompAll extends GhidraScript {

    @Override
    public void run() throws Exception {
        String outPath = getScriptArgs().length > 0
                ? getScriptArgs()[0]
                : "E:\\GitHub\\VestaMMC\\Vesta\\SPIPower\\ghidra\\decomp.txt";
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        int n = 0, ok = 0;
        try (PrintWriter pw = new PrintWriter(outPath, "UTF-8")) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext()) {
                Function f = it.next();
                if (f.isThunk() || f.isExternal()) {
                    continue;
                }
                n++;
                pw.println("\n======== " + f.getName() + " @ " + f.getEntryPoint() + " ========");
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                if (r != null && r.decompileCompleted()) {
                    pw.println(r.getDecompiledFunction().getC());
                    ok++;
                } else {
                    pw.println("decompile failed");
                }
                pw.flush();
            }
        }
        println("DecompAll: " + ok + "/" + n + " functions -> " + outPath);
    }
}

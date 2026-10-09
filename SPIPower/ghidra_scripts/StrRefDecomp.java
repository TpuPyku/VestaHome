// StrRefDecomp.java - for each keyword, find the literal string in memory,
// list code references, and decompile the containing functions.
// Both encodings are searched: ascii/latin1 bytes and utf-16le (CE binaries mix
// them - e.g. SwDnl.exe keeps config keys and paths only as utf-16le).
// GhidraScript args: substrings to search for (e.g. CreateProcess SwUpdate).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class StrRefDecomp extends GhidraScript {

    private byte[] utf16(String s) {
        byte[] out = new byte[s.length() * 2];
        for (int i = 0; i < s.length(); i++) {
            out[i * 2] = (byte) (s.charAt(i) & 0xFF);
            out[i * 2 + 1] = (byte) ((s.charAt(i) >> 8) & 0xFF);
        }
        return out;
    }

    private boolean printable(Address a, boolean wide) {
        try {
            int b = currentProgram.getMemory().getByte(a) & 0xFF;
            if (wide && (currentProgram.getMemory().getByte(a.add(1)) & 0xFF) != 0) return false;
            return b >= 0x20 && b < 0x7F;
        } catch (Exception e) {
            return false;
        }
    }

    // A keyword can match inside a longer string while the code references the
    // string's first character: walk back to the start of the run of characters.
    private Address startOf(Address a, boolean wide) {
        long step = wide ? 2 : 1;
        Address cur = a;
        for (int i = 0; i < 512; i++) {
            Address prev = cur.subtract(step);
            if (prev == null || prev.compareTo(currentProgram.getMinAddress()) < 0) break;
            if (!printable(prev, wide)) break;
            cur = prev;
        }
        return cur;
    }

    private void refsOf(Address a, java.util.Set<Function> fns) {
        for (Reference r : getReferencesTo(a)) {
            Address fa = r.getFromAddress();
            Function f = getFunctionContaining(fa);
            if (f != null) {
                println("   ref " + fa + " in " + f.getName() + " @ " + f.getEntryPoint());
                fns.add(f);
            } else {
                println("   ref " + fa + " (no function)");
            }
        }
    }

    private void scan(String k, byte[] pat, String enc, java.util.Set<Function> fns) throws Exception {
        boolean wide = enc.equals("utf16");
        Address a = currentProgram.getMinAddress();
        while (a != null) {
            Address found = currentProgram.getMemory().findBytes(a, pat, null, true, monitor);
            if (found == null) break;
            Address start = startOf(found, wide);
            println("STR " + found + " start " + start + " [" + enc + "] : " + k);
            refsOf(found, fns);
            if (!start.equals(found)) refsOf(start, fns);
            a = found.add(1);
        }
    }

    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);

        java.util.Set<Function> fns = new java.util.LinkedHashSet<>();
        for (String k : getScriptArgs()) {
            scan(k, k.getBytes("latin1"), "ascii", fns);
            scan(k, utf16(k), "utf16", fns);
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

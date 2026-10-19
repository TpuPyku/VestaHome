// FindPattern.java - search initialized memory for byte patterns and report hits,
// their containing function and inbound references.
// GhidraScript args: each arg is either "a:<ascii>" or "h:<hexbytes>" or a bare ascii string.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class FindPattern extends GhidraScript {

    private int findFrom(byte[] data, byte[] pat, int from) {
        outer:
        for (int i = from; i <= data.length - pat.length; i++) {
            for (int j = 0; j < pat.length; j++) {
                if (data[i + j] != pat[j]) {
                    continue outer;
                }
            }
            return i;
        }
        return -1;
    }

    private static byte[] hex(String s) {
        s = s.replace(" ", "");
        byte[] b = new byte[s.length() / 2];
        for (int i = 0; i < b.length; i++) {
            b[i] = (byte) Integer.parseInt(s.substring(2 * i, 2 * i + 2), 16);
        }
        return b;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        for (MemoryBlock blk : currentProgram.getMemory().getBlocks()) {
            if (!blk.isInitialized()) {
                continue;
            }
            long sz = blk.getSize();
            if (sz > 16_000_000L) {
                continue;
            }
            byte[] data = new byte[(int) sz];
            blk.getBytes(blk.getStart(), data);
            for (String a : args) {
                byte[] pat;
                if (a.startsWith("h:")) {
                    pat = hex(a.substring(2));
                } else {
                    String t = a.startsWith("a:") ? a.substring(2) : a;
                    pat = t.getBytes("US-ASCII");
                }
                int idx = findFrom(data, pat, 0);
                while (idx >= 0) {
                    Address addr = blk.getStart().add(idx);
                    StringBuilder sb = new StringBuilder();
                    sb.append("HIT ").append(a).append(" @ ").append(addr.toString());
                    Function f = getFunctionContaining(addr);
                    if (f != null) {
                        sb.append("  in ").append(f.getName());
                    }
                    Reference[] refs = getReferencesTo(addr);
                    for (int r = 0; r < refs.length && r < 6; r++) {
                        sb.append("  <- ").append(refs[r].getFromAddress().toString());
                    }
                    println(sb.toString());
                    idx = findFrom(data, pat, idx + 1);
                }
            }
        }
    }
}

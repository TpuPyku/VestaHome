// InsnInfo.java - dump the instructions of a function/range with PC-relative
// literal-pool references resolved (target address + 4-byte value + float form).
// Used to pin the exact file offset of a compiler-emitted float constant.
// GhidraScript args: <startVA> [endVA]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;

public class InsnInfo extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("usage: InsnInfo <startVA> [endVA]");
            return;
        }
        Address start = toAddr(args[0]);
        Address end = args.length > 1 ? toAddr(args[1]) : null;
        Listing lst = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();
        for (Instruction insn : lst.getInstructions(start, true)) {
            Address a = insn.getAddress();
            if (end != null && a.compareTo(end) > 0) {
                break;
            }
            StringBuilder sb = new StringBuilder();
            sb.append(String.format("%08x", a.getOffset()));
            sb.append("  ");
            sb.append(String.format("%-46s", insn.toString()));
            for (Reference r : insn.getReferencesFrom()) {
                Address t = r.getToAddress();
                if (!mem.contains(t)) {
                    continue;
                }
                try {
                    int w = mem.getInt(t);
                    float f = Float.intBitsToFloat(w);
                    sb.append(String.format("  ->[%08x]=%08x f=%s", t.getOffset(), w, f));
                } catch (Exception e) {
                    // target not a readable word (code ref, etc.)
                }
            }
            println(sb.toString());
        }
    }
}

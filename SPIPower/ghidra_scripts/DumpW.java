// DumpW.java - print the utf-16 string (and its length) starting at each address.
// Used to read literals the decompiler renders as &DAT_xxxxxx, where the file
// offset has to be derived by hand otherwise.
// GhidraScript args: addresses (e.g. 0x414f8).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpW extends GhidraScript {

    @Override
    public void run() throws Exception {
        for (String a : getScriptArgs()) {
            Address addr = currentProgram.getAddressFactory().getAddress(a);
            if (addr == null) {
                println("bad address: " + a);
                continue;
            }
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < 120; i++) {
                int c = currentProgram.getMemory().getShort(addr.add(i * 2L)) & 0xFFFF;
                if (c == 0) break;
                sb.append((char) c);
            }
            println(a + "  len=" + sb.length() + "  '" + sb + "'");
        }
    }
}

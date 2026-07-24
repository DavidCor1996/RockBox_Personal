package ipod6g.frameworktest;

import android.util.Pair;
import dalvikExecTest.HelloWorld;

/** Minimal proof that Dalvik loads and executes code from framework.jar. */
public final class FrameworkHello {
    private FrameworkHello() {
    }

    public static void main(String[] args) {
        // Resolve and execute the checksum-pinned official Dalvik fixture first.
        HelloWorld.main(args);
        Pair<String, String> pair = Pair.create("Android", "Framework");
        if (!"Android".equals(pair.first) || !"Framework".equals(pair.second)) {
            throw new AssertionError("framework Pair contract failed");
        }
        System.out.println("Android Framework Java OK!");
    }
}

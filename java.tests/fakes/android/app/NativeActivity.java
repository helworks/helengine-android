package android.app;
import android.os.Bundle;
/** Supplies lifecycle methods so the actual activity gate can be tested without loading native graphics code. */
public class NativeActivity extends Activity {
    /** Last trim level passed to the actual Activity superclass callback. */
    public int LastTrimLevel = -1;
    /** Desktop memory callback records superclass forwarding without a native glue thread. */
    public void onTrimMemory(int level) { LastTrimLevel = level; }
    /** Desktop lifecycle setup has no Android glue thread. */
    protected void onCreate(Bundle savedInstanceState) {}
    /** Desktop resume has no Android glue thread. */
    protected void onResume() {}
    /** Desktop suspension has no Android glue thread. */
    protected void onPause() {}
    /** Desktop destruction has no Android glue thread. */
    protected void onDestroy() {}
}

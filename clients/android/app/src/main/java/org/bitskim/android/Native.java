package org.bitskim.android;
final class Native {
    static { System.loadLibrary("bitskim"); }
    static native long create(String name, String id, String trust, String received, int port, boolean discovery, int security);
    static native void destroy(long handle);
    static native String tick(long handle);
    static native boolean action(long handle, int command, long serial, String argument);
    static native boolean queue(long handle, String id, String address, int port, String path);
    static native String selfTest(String root);
    private Native() {}
}

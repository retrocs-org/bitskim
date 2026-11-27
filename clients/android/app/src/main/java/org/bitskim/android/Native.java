package org.bitskim.android;
final class Native {
    static { System.loadLibrary("bitskim"); }
    static native String buildId();
    static native String retainedItem(long handle,long reviewed,int index);
    static native long create(String name, String id, String trust, String received, int port, boolean discovery, int security);
    static native void destroy(long handle);
    static native String tick(long handle);
    static native String queuePath(long handle,int index);
    static native boolean action(long handle, int command, long serial, String argument);
    static native boolean queue(long handle, String id, String address, int port, String path);
    static native String invitation(long handle,String address);
    static native String parseInvitation(String text);
    static native boolean queueInvitation(long handle,String invitation,String path);
    static native String selfTest(String root);
    private Native() {}
}

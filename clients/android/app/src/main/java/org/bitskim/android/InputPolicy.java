package org.bitskim.android;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
final class InputPolicy {
    static final long MAX_FILE = 2147483647L;
    static final class Target {
        final String id, name, address; final int port;
        Target(String id,String name,String address,int port) { this.id=id;this.name=name;this.address=address;this.port=port; }
        String endpoint() { return address+":"+port; }
    }
    static Target target(String value) {
        String[] parts=value.trim().split(":",-1);if(parts.length>2)throw new IllegalArgumentException("Enter an IPv4 address, optionally followed by :port.");
        String[] octets=parts[0].split("\\.",-1);if(octets.length!=4)throw new IllegalArgumentException("Enter an IPv4 address.");
        StringBuilder ip=new StringBuilder();for(String part:octets){if(!part.matches("[0-9]{1,3}"))throw new IllegalArgumentException("Invalid IPv4 address.");int n=Integer.parseInt(part);if(n>255)throw new IllegalArgumentException("Invalid IPv4 address.");if(ip.length()>0)ip.append('.');ip.append(n);}
        int port=46440;if(parts.length==2){if(!parts[1].matches("[0-9]{1,5}"))throw new IllegalArgumentException("Invalid port.");port=Integer.parseInt(parts[1]);if(port<1 || port>65535)throw new IllegalArgumentException("Port must be between 1 and 65535.");}
        String address=ip.toString();return new Target("",address+":"+port,address,port);
    }
    static String filename(String name) {
        if(name==null || name.trim().isEmpty() || name.equals(".") || name.equals("..") || name.startsWith(".") || name.contains("/") || name.contains("\\") || name.getBytes(StandardCharsets.UTF_8).length>255)throw new IllegalArgumentException("Choose a filename without path separators, no longer than 255 UTF-8 bytes.");
        for(int i=0;i<name.length();i++)if(Character.isISOControl(name.charAt(i)))throw new IllegalArgumentException("Control characters are not allowed in filenames.");return name;
    }
    static File child(File root,File value) throws IOException {
        File base=root.getCanonicalFile(),file=value.getCanonicalFile();if(!file.getPath().startsWith(base.getPath()+File.separator) || Files.isSymbolicLink(value.toPath()))throw new IOException("File is outside the Received folder.");return file;
    }
    static void rename(File root,File value,String name) throws IOException {
        File source=child(root,value),destination=new File(source.getParentFile(),filename(name));if(destination.equals(source))return;if(destination.exists())throw new IOException("A file with that name already exists.");Files.move(source.toPath(),destination.toPath());
    }
    static void delete(File root,File value) throws IOException {
        File path=child(root,value);if(path.isDirectory()){File[] children=path.listFiles();if(children==null)throw new IOException("Cannot read folder.");for(File item:children)delete(root,item);}Files.delete(path.toPath());
    }
    static String deviceName(String text) {
        StringBuilder out=new StringBuilder();for(int i=0;i<text.length();){int cp=text.codePointAt(i);String next=new String(Character.toChars(cp));if((out.toString()+next).getBytes(StandardCharsets.UTF_8).length>63)break;out.append(next);i+=Character.charCount(cp);}return out.toString();
    }
}

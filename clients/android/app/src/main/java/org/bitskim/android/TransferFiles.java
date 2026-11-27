package org.bitskim.android;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileTime;
import java.security.*;
import java.util.*;
import java.util.function.BooleanSupplier;

/** Read-only provider staging and bounded, content-checked transfer manifests. */
final class TransferFiles {
    static final int MAX_ENTRIES=32760;
    static final class Entry {
        final String path; final boolean directory; final long bytes;
        Entry(String path,boolean directory,long bytes){this.path=path;this.directory=directory;this.bytes=bytes;}
    }
    static final class Snapshot {
        final File root; final List<Entry> entries; final long total; final byte[] signature;
        Snapshot(File root,List<Entry> entries,long total,byte[] signature){this.root=root;this.entries=Collections.unmodifiableList(entries);this.total=total;this.signature=signature;}
        boolean matches(Snapshot other){return root.equals(other.root)&&MessageDigest.isEqual(signature,other.signature);}
    }
    static final class Node {
        final String id,name; final boolean directory; final long size,modifiedMillis;
        Node(String id,String name,boolean directory,long size){this(id,name,directory,size,-1);}
        Node(String id,String name,boolean directory,long size,long modifiedMillis){this.id=id;this.name=name;this.directory=directory;this.size=size;this.modifiedMillis=modifiedMillis;}
    }
    interface Source { List<Node> children(Node directory)throws IOException; InputStream open(Node file)throws IOException; }
    private static MessageDigest digest(){try{return MessageDigest.getInstance("SHA-256");}catch(NoSuchAlgorithmException e){throw new AssertionError(e);}}
    static String name(String value)throws IOException {
        if(value==null||value.isEmpty()||value.equals(".")||value.equals("..")||value.contains("/")||value.contains("\\")||!StandardCharsets.UTF_8.newEncoder().canEncode(value)||value.getBytes(StandardCharsets.UTF_8).length>255)throw new IOException("The provider supplied an unsupported filename.");
        for(int i=0;i<value.length();i++)if(Character.isISOControl(value.charAt(i)))throw new IOException("Control characters are not allowed in filenames.");return value;
    }
    private static void bounds(String path,boolean directory,int depth,int count)throws IOException {
        if(path.getBytes(StandardCharsets.UTF_8).length+(directory?1:0)>255||depth>16||(directory&&depth>=16)||count>=MAX_ENTRIES)throw new IOException("The selection exceeds portable filename, folder-depth or item-count limits.");
    }
    static Snapshot inspect(File file)throws IOException {
        List<Entry> entries=new ArrayList<>();MessageDigest digest=digest();long[] total={0};File root=file.getAbsoluteFile();
        inspect(root,name(root.getName()),1,entries,digest,total);
        return new Snapshot(root,entries,total[0],digest.digest());
    }
    private static void inspect(File file,String relative,int depth,List<Entry> entries,MessageDigest digest,long[] total)throws IOException {
        BasicFileAttributes attributes=Files.readAttributes(file.toPath(),BasicFileAttributes.class,LinkOption.NOFOLLOW_LINKS);
        if(attributes.isSymbolicLink()||(!attributes.isDirectory()&&!attributes.isRegularFile()))throw new IOException("Links and special files cannot be sent.");
        name(file.getName());bounds(relative,attributes.isDirectory(),depth,entries.size());
        if(file.getPath().getBytes(StandardCharsets.UTF_8).length>1023)throw new IOException("Choose a shorter local path.");
        if(attributes.isDirectory()){
            entries.add(new Entry(relative+"/",true,0));digest.update(("D\0"+relative+"\0").getBytes(StandardCharsets.UTF_8));
            File[] children=file.listFiles();if(children==null)throw new IOException("Cannot read the selected folder.");Arrays.sort(children,Comparator.comparing(File::getName));
            for(File child:children)inspect(child,relative+"/"+child.getName(),depth+1,entries,digest,total);
        }else{
            long size=attributes.size();if(size<0||size>InputPolicy.MAX_FILE)throw new IOException("Files must be no larger than 2 GiB minus one byte.");
            digest.update(("F\0"+relative+"\0"+size+"\0").getBytes(StandardCharsets.UTF_8));long count=0;
            try(InputStream input=Files.newInputStream(file.toPath(),LinkOption.NOFOLLOW_LINKS)){byte[] buffer=new byte[65536];int n;while((n=input.read(buffer))!=-1){if(Thread.currentThread().isInterrupted())throw new IOException("Preparation cancelled.");count+=n;if(count>InputPolicy.MAX_FILE)throw new IOException("A selected file grew beyond the transfer limit.");digest.update(buffer,0,n);}}
            if(count!=size)throw new IOException("The selected files changed. Review them again.");total[0]+=count;entries.add(new Entry(relative,false,count));
        }
    }
    static File stage(Source source,Node node,File pending,BooleanSupplier cancelled)throws IOException {
        File temporary=new File(pending,".copy-"+UUID.randomUUID());if(!temporary.mkdir())throw new IOException("Cannot create staging folder.");
        try{
            String leaf=name(node.name);File payload=new File(temporary,leaf);copy(source,node,payload,leaf,1,new HashSet<>(),new int[]{0},cancelled);
            File committed=new File(pending,UUID.randomUUID().toString());Files.move(temporary.toPath(),committed.toPath(),StandardCopyOption.ATOMIC_MOVE);return new File(committed,leaf);
        }catch(IOException|RuntimeException e){InputPolicy.delete(pending,temporary);throw e;}
    }
    private static void copy(Source source,Node node,File output,String relative,int depth,Set<String> ancestors,int[] count,BooleanSupplier cancelled)throws IOException {
        if(cancelled.getAsBoolean())throw new IOException("Import cancelled.");name(node.name);bounds(relative,node.directory,depth,count[0]++);
        if(node.directory){
            if(!ancestors.add(node.id))throw new IOException("The provider returned a folder loop.");
            try{
                Files.createDirectory(output.toPath());List<Node> children=new ArrayList<>(source.children(node));children.sort(Comparator.comparing(child->child.name));Set<String> names=new HashSet<>();
                for(Node child:children){name(child.name);if(!names.add(child.name))throw new IOException("The provider returned duplicate filenames.");copy(source,child,new File(output,child.name),relative+"/"+child.name,depth+1,ancestors,count,cancelled);}
            }finally{ancestors.remove(node.id);}
        }else{
            if(node.size>InputPolicy.MAX_FILE)throw new IOException("Files must be no larger than 2 GiB minus one byte.");long bytes=0;
            try(InputStream input=source.open(node);OutputStream out=Files.newOutputStream(output.toPath(),StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE)){
                if(input==null)throw new IOException("The provider could not supply file contents. Download the folder locally and try again.");
                byte[] buffer=new byte[65536];int n;while((n=input.read(buffer))!=-1){if(cancelled.getAsBoolean())throw new IOException("Import cancelled.");bytes+=n;if(bytes>InputPolicy.MAX_FILE)throw new IOException("A file exceeds the transfer limit.");out.write(buffer,0,n);}
                if(node.size>=0&&bytes!=node.size)throw new IOException("A provider file changed or was incomplete. Choose the folder again.");
            }
            preserveDate(output,node.modifiedMillis);
        }
    }
    static void preserveDate(File file,long modifiedMillis)throws IOException {
        if(modifiedMillis<0)return;
        Files.setLastModifiedTime(file.toPath(),FileTime.fromMillis(modifiedMillis));
        if(Math.abs(Files.getLastModifiedTime(file.toPath()).toMillis()-modifiedMillis)>1000)throw new IOException("Cannot preserve the provider file date.");
    }
    static File batch(File pending,File file)throws IOException {
        Path base=pending.getCanonicalFile().toPath(),path=file.getCanonicalFile().toPath();if(!path.startsWith(base))return null;Path relative=base.relativize(path);
        if(relative.getNameCount()<2)return null;String id=relative.getName(0).toString();try{if(!UUID.fromString(id).toString().equals(id))return null;}catch(IllegalArgumentException e){return null;}return new File(pending,id);
    }
    static void markQueued(File pending,File file)throws IOException {File batch=batch(pending,file);if(batch!=null){File marker=new File(pending,".queued-"+batch.getName());try(FileOutputStream output=new FileOutputStream(marker)){output.write(1);output.getFD().sync();}}}
    static void unmark(File pending,File batch)throws IOException {Files.deleteIfExists(new File(pending,".queued-"+batch.getName()).toPath());}
    static void removeBatch(File pending,File batch)throws IOException {InputPolicy.delete(pending,batch);unmark(pending,batch);}
    static void clearFinished(File pending)throws IOException {
        File[] markers=pending.listFiles();if(markers==null)return;for(File marker:markers){String n=marker.getName();if(!n.startsWith(".queued-")||Files.isSymbolicLink(marker.toPath())||!marker.isFile())continue;String id=n.substring(8);try{if(!UUID.fromString(id).toString().equals(id))continue;}catch(IllegalArgumentException e){continue;}File batch=new File(pending,id);if(batch.exists())InputPolicy.delete(pending,batch);Files.deleteIfExists(marker.toPath());}
    }
}

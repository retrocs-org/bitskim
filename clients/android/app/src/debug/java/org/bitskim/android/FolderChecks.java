package org.bitskim.android;
import android.app.Instrumentation;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import org.json.JSONObject;
import java.io.*;
import java.net.ServerSocket;
import java.nio.file.Files;
import java.util.*;

/** Actual document-provider staging and JNI folder transfer, using private fixtures. */
public final class FolderChecks extends Instrumentation {
    @Override public void onCreate(Bundle arguments){super.onCreate(arguments);start();}
    private int port()throws IOException {try(ServerSocket socket=new ServerSocket(0)){return socket.getLocalPort();}}
    @Override public void onStart(){Bundle result=new Bundle();File root=null;long sender=0,receiver=0;
        try{
            root=Files.createTempDirectory(getTargetContext().getCacheDir().toPath(),"folder-check-").toFile();File source=new File(root,"Tree"),pending=new File(root,"Pending"),destination=new File(root,"Received"),trust=new File(root,"Trust");source.mkdir();pending.mkdir();destination.mkdir();trust.mkdir();new File(source,"Sub/Empty").mkdirs();
            byte[] bytes="Native document-provider folder bytes".getBytes("UTF-8");Files.write(new File(source,"Sub/Möte.txt").toPath(),bytes);for(int i=0;i<96;i++)Files.write(new File(source,"file-"+i).toPath(),new byte[]{(byte)i});
            FolderProviderChecks.fixture=source;Uri uri=DocumentsContract.buildTreeDocumentUri(getTargetContext().getPackageName()+".folderchecks","root");ProviderFolder provider=new ProviderFolder(getTargetContext().getContentResolver(),uri);
            File staged=TransferFiles.stage(provider,provider.root(),pending,()->false);TransferFiles.Snapshot snapshot=TransferFiles.inspect(staged);
            if(!snapshot.matches(TransferFiles.inspect(staged))||!Arrays.equals(snapshot.signature,TransferFiles.inspect(source).signature))throw new IOException("Provider snapshot mismatch");
            int destinationPort=port();receiver=Native.create("Folder receiver","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",trust.getPath(),destination.getPath(),destinationPort,false,1);sender=Native.create("Folder sender","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",trust.getPath(),destination.getPath(),port(),false,1);
            Native.action(sender,14,0,"");if(!Native.queue(sender,"","127.0.0.1",destinationPort,staged.getPath()))throw new IOException("Folder queue failed");TransferFiles.markQueued(pending,staged);Native.action(sender,8,0,"");boolean approved=false,bounded=false;long deadline=System.currentTimeMillis()+60000;
            while(System.currentTimeMillis()<deadline){JSONObject sent=new JSONObject(Native.tick(sender)),received=new JSONObject(Native.tick(receiver));if(sent.has("error")&&sent.optString("error").contains("capacity"))throw new IOException("Large folder report overflow");if(sent.optInt("queued")>16){bounded=true;if(sent.getJSONArray("queuedPaths").length()>16)throw new IOException("Unbounded queue report");}
                if(!approved&&received.optBoolean("awaiting")){if(!new File(staged,"Sub/Möte.txt").isFile())throw new IOException("Staged children removed too early");if(!Native.action(receiver,3,received.getLong("serial"),""))throw new IOException("Folder approval failed");approved=true;}
                if(approved&&!sent.optBoolean("active")&&!received.optBoolean("active")&&sent.optInt("queued")==0&&!sent.optBoolean("scanning"))break;Thread.sleep(2);}
            if(!approved||!bounded)throw new IOException("Folder review or bounded report not exercised");
            if(!Arrays.equals(Files.readAllBytes(new File(destination,"Tree/Sub/Möte.txt").toPath()),bytes)||!new File(destination,"Tree/Sub/Empty").isDirectory())throw new IOException("Folder bytes or empty directory missing");
            if(!Arrays.equals(snapshot.signature,TransferFiles.inspect(new File(destination,"Tree")).signature))throw new IOException("Received tree differs");
            TransferFiles.clearFinished(pending);if(pending.listFiles().length!=0||!Arrays.equals(snapshot.signature,TransferFiles.inspect(source).signature))throw new IOException("Private cleanup changed provider originals");
            result.putString("stream","\nPASS native folder provider, Unicode/empty directories, full manifest, 96-file JNI TLS transfer, bounded status, originals and staging cleanup\n");finish(-1,result);
        }catch(Throwable failure){result.putString("stream","\nFAIL "+failure+"\n");finish(0,result);}
        finally{FolderProviderChecks.fixture=null;if(sender!=0)Native.destroy(sender);if(receiver!=0)Native.destroy(receiver);if(root!=null)try{for(File child:root.listFiles())InputPolicy.delete(root,child);root.delete();}catch(Exception ignored){}}
    }
}

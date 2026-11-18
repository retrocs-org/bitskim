package org.bitskim.android;
import android.app.*;
import android.content.*;
import android.os.Bundle;
import android.service.notification.StatusBarNotification;
import org.json.JSONObject;
import java.io.*;
import java.lang.reflect.Field;
import java.util.*;
import java.util.concurrent.*;

/** Debug-only native transfer with the real activity minimized and notification cancelled. */
public final class BackgroundChecks extends Instrumentation {
    @Override public void onCreate(Bundle arguments){super.onCreate(arguments);start();}
    @Override public void onStart(){
        Bundle result=new Bundle();long peer=0;File root=null;ClientService client=null;boolean ownsQueue=false;ScheduledExecutorService engine=null;
        try{
            Context context=getTargetContext();
            context.getSharedPreferences("bitskim",0).edit().putBoolean("backgroundTransfers",true).putBoolean("notificationAsked",true).apply();
            startActivitySync(new Intent(context,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            client=ClientService.shared(context);
            Field field=ClientService.class.getDeclaredField("engine");field.setAccessible(true);engine=(ScheduledExecutorService)field.get(client);
            long deadline=System.currentTimeMillis()+10000;
            while(client.state.optString("diagnostics").isEmpty()&&System.currentTimeMillis()<deadline)Thread.sleep(100);
            if(client.state.optString("diagnostics").isEmpty())throw new IOException("Local network permission or listener unavailable");
            if(!client.state.optBoolean("active")&&client.state.optInt("queued")>0){org.json.JSONArray paths=client.state.optJSONArray("queuedPaths");boolean onlyFixtures=paths!=null&&paths.length()>0;for(int i=0;paths!=null&&i<paths.length();i++)onlyFixtures &= paths.optString(i).endsWith("/Background-check.bin");if(onlyFixtures){client.command(2,0,"");engine.submit(()->{}).get();Thread.sleep(300);}}
            if(client.state.optBoolean("active")||client.state.optInt("queued")>0)throw new IOException("Existing transfer work; test left it unchanged");
            root=new File(context.getCacheDir(),"background-check-"+UUID.randomUUID());root.mkdir();File fixture=root;
            peer=engine.submit(()->Native.create("Test receiver","eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",new File(context.getFilesDir(),"Trust").getPath(),fixture.getPath(),46445,false,1)).get();long receiver=peer;
            if(!engine.submit(()->Native.action(receiver,12,262144,"")).get())throw new IOException("Cannot set test rate");
            File staging=new File(client.pending,UUID.randomUUID().toString());staging.mkdir();File source=new File(staging,"Background-check.bin");
            try(OutputStream out=new FileOutputStream(source)){byte[] buffer=new byte[65536];for(int i=0;i<128;i++)out.write(buffer);}
            String invitation=engine.submit(()->Native.invitation(receiver,"127.0.0.1")).get();ownsQueue=true;
            client.send(new InputPolicy.Target("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","Test receiver","127.0.0.1",46445,invitation),Collections.singletonList(source),Collections.singletonList(TransferFiles.inspect(source)),()->{});
            deadline=System.currentTimeMillis()+10000;boolean accepted=false;
            while(System.currentTimeMillis()<deadline){JSONObject state=new JSONObject(engine.submit(()->Native.tick(receiver)).get());if(state.optBoolean("awaiting")){long serial=state.optLong("serial");if(!engine.submit(()->Native.action(receiver,3,serial,"")).get())throw new IOException("Cannot approve fixture");accepted=true;break;}Thread.sleep(20);}
            if(!accepted)throw new IOException("No outgoing offer: "+client.state.optString("status")+" / "+client.state.optString("error"));
            deadline=System.currentTimeMillis()+8000;while(client.state.optLong("done")==0&&System.currentTimeMillis()<deadline){engine.submit(()->Native.tick(receiver)).get();Thread.sleep(30);}
            long before=client.state.optLong("done");if(before==0)throw new IOException("Transfer did not start");
            getTargetContext().startActivity(new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            long wait=System.currentTimeMillis()+1800;while(System.currentTimeMillis()<wait){engine.submit(()->Native.tick(receiver)).get();Thread.sleep(30);}
            if(client.foreground||!client.state.optBoolean("active")||client.state.optLong("done")<=before)throw new IOException("Transfer failed to advance while minimized");
            StatusBarNotification[] notices=context.getSystemService(NotificationManager.class).getActiveNotifications();Notification.Action cancel=null;
            for(StatusBarNotification notice:notices)if(notice.getId()==46440&&notice.getNotification().actions!=null)for(Notification.Action action:notice.getNotification().actions)if(action.title.toString().contains("Cancel"))cancel=action;
            if(cancel==null)throw new IOException("Visible transfer notification with Cancel missing");cancel.actionIntent.send();
            deadline=System.currentTimeMillis()+8000;while(System.currentTimeMillis()<deadline){JSONObject remote=new JSONObject(engine.submit(()->Native.tick(receiver)).get());if(!client.state.optBoolean("active")&&!remote.optBoolean("active"))break;Thread.sleep(30);}
            if(client.state.optBoolean("active")||new File(root,"Background-check.bin").exists())throw new IOException("Notification cancellation failed");
            for(File file:root.listFiles())if(file.getName().endsWith(".part"))throw new IOException("Partial fixture was retained");
            result.putString("stream","\nPASS minimized native transfer advanced, visible foreground notification, Cancel and partial cleanup\n");finish(-1,result);
        }catch(Throwable error){result.putString("stream","\nFAIL "+error+"\n");finish(0,result);}
        finally{try{if(ownsQueue&&client!=null){client.command(2,0,"");if(engine!=null)engine.submit(()->{}).get();}if(peer!=0&&engine!=null){long receiver=peer;engine.submit(()->Native.destroy(receiver)).get();}if(root!=null){File[] files=root.listFiles();if(files!=null)for(File file:files)file.delete();root.delete();}}catch(Exception ignored){}}
    }
}

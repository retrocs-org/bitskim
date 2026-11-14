package org.bitskim.android;
import android.content.*;
import android.database.Cursor;
import android.net.Uri;
import android.os.*;
import android.provider.OpenableColumns;
import org.json.*;
import java.io.*;
import java.net.*;
import java.util.*;
import java.util.concurrent.*;

final class ClientService {
    interface Listener { void changed(JSONObject state); void error(String text); }
    final File received,pending;
    final SharedPreferences preferences;
    private final Context context;
    private final Listener listener;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final ScheduledExecutorService engine=Executors.newSingleThreadScheduledExecutor();
    private final ExecutorService copies=Executors.newSingleThreadExecutor();
    private final Set<File> sending=ConcurrentHashMap.newKeySet();
    private final List<Long> plan=new ArrayList<>();
    private long handle;
    private boolean uiHeld;
    private int ticks;
    private String last="";
    volatile JSONObject state=new JSONObject();
    volatile boolean foreground;
    volatile boolean closed;
    ClientService(Context context,Listener listener) {
        this.context=context.getApplicationContext();this.listener=listener;preferences=context.getSharedPreferences("bitskim",Context.MODE_PRIVATE);
        received=new File(context.getFilesDir(),"Received");pending=new File(context.getFilesDir(),"Pending");received.mkdirs();pending.mkdirs();
        engine.scheduleAtFixedRate(()->{if(handle!=0){try{Native.action(handle,9,0,"");if(++ticks%10==0)poll();}catch(Exception e){fail(e);}}},0,20,TimeUnit.MILLISECONDS);
    }
    void start() { engine.execute(()->{if(closed || handle!=0)return;try{
        File trust=new File(context.getFilesDir(),"Trust");if(!trust.isDirectory()&&!trust.mkdirs())throw new IOException("Cannot create identity storage.");
        String id=preferences.getString("installationID",null);if(id==null){id=UUID.randomUUID().toString().replace("-","");preferences.edit().putString("installationID",id).apply();}
        handle=Native.create(InputPolicy.deviceName(Build.MANUFACTURER+" "+Build.MODEL),id,trust.getPath(),received.getPath(),46440,BuildConfig.DISCOVERY,preferences.getInt("security",1));
        if(uiHeld)Native.action(handle,7,0,"");last="";poll();
    }catch(Exception e){fail(e);}}); }
    void stop() { engine.execute(()->{uiHeld=false;if(handle!=0){Native.destroy(handle);handle=0;}cleanSending();state=new JSONObject();last="";}); }
    void close() { closed=true;engine.execute(()->{if(handle!=0){Native.destroy(handle);handle=0;}cleanSending();});engine.shutdown();copies.shutdown(); }
    void command(int command,long serial,String argument) { engine.execute(()->{if(handle==0)return;try{if(!Native.action(handle,command,serial,argument))throw new IOException("Action is unavailable or the offer expired.");if(command==6)preferences.edit().putInt("security",(int)serial).apply();poll();}catch(Exception e){fail(e);}}); }
    void pauseForUi(Runnable ready) { engine.execute(()->{try{if(handle!=0 && !Native.action(handle,7,0,""))throw new IOException("Finish or cancel the current transfer first.");uiHeld=true;main.post(ready);}catch(Exception e){fail(e);}}); }
    void releaseUi() { if(closed)return;engine.execute(()->{uiHeld=false;if(handle!=0)Native.action(handle,7,1,"");}); }
    List<File> selectedFiles() {
        List<File> files=new ArrayList<>();File[] folders=pending.listFiles();if(folders!=null)for(File folder:folders){if(folder.getName().startsWith("."))continue;File[] list=folder.listFiles();if(list!=null)for(File file:list)if(file.isFile()&&!sending.contains(file))files.add(file);}
        files.sort(Comparator.comparing(File::getName,String.CASE_INSENSITIVE_ORDER));return files;
    }
    interface FileAction { void run() throws Exception; }
    void manageFiles(FileAction action,Runnable ready) {copies.execute(()->{boolean success=false;try{action.run();success=true;}catch(Exception e){fail(e);}finally{releaseUi();}if(success)main.post(()->{if(!closed)ready.run();});});}
    void discardSelection(Runnable ready) {copies.execute(()->{for(File file:selectedFiles())removeCopy(file);main.post(()->{if(!closed)ready.run();});});}
    void importFiles(List<Uri> uris,Runnable ready) {
        if(uris.isEmpty() || uris.size()>32){listener.error("Choose between 1 and 32 files.");releaseUi();return;}
        copies.execute(()->{List<File> staged=new ArrayList<>();try{
            for(Uri uri:uris){if(!"content".equals(uri.getScheme()))throw new IOException("Choose files through the Android document picker.");
                String name="File";long hint=-1;
                try(Cursor cursor=context.getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE},null,null,null)){if(cursor!=null&&cursor.moveToFirst()){int n=cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME),s=cursor.getColumnIndex(OpenableColumns.SIZE);if(n>=0&&!cursor.isNull(n))name=cursor.getString(n);if(s>=0&&!cursor.isNull(s))hint=cursor.getLong(s);}}
                InputPolicy.filename(name);if(hint>InputPolicy.MAX_FILE)throw new IOException("Files must be smaller than 2 GiB.");
                File directory=new File(pending,".copy-"+UUID.randomUUID());if(!directory.mkdir())throw new IOException("Cannot create staging folder.");File file=new File(directory,name);staged.add(file);
                try(InputStream input=context.getContentResolver().openInputStream(uri);OutputStream output=new FileOutputStream(file)){
                    if(input==null)throw new IOException("Provider could not open this file.");byte[] buffer=new byte[65536];long count=0;int n;while((n=input.read(buffer))!=-1){if(closed)throw new IOException("Import cancelled.");count+=n;if(count>InputPolicy.MAX_FILE)throw new IOException("Files must be smaller than 2 GiB.");output.write(buffer,0,n);}
                }
                File committed=new File(pending,UUID.randomUUID().toString());if(!directory.renameTo(committed))throw new IOException("Cannot commit staged file.");staged.set(staged.size()-1,new File(committed,name));
            }
            main.post(()->{releaseUi();if(!closed)ready.run();});
        }catch(Exception e){for(File file:staged)removeCopy(file);main.post(()->{releaseUi();if(!closed)listener.error(e.getMessage()==null?"Cannot import files.":e.getMessage());});}});
    }
    void send(InputPolicy.Target target,List<File> files,long[] sizes,Runnable started) {
        engine.execute(()->{try{
            if(handle==0)throw new IOException("Allow local network access and open the client first.");
            if(state.optBoolean("active")||state.optInt("queued")>0)throw new IOException("Finish, retry or cancel the queued transfer first.");
            if(files.isEmpty())throw new IOException("Choose files first.");
            for(int i=0;i<files.size();i++){File f=files.get(i);if(!f.isFile()||f.length()!=sizes[i]||f.length()>InputPolicy.MAX_FILE)throw new IOException("Selected files changed. Review them again.");}
            plan.clear();for(long size:sizes)plan.add(size);
            for(File file:files){if(!Native.queue(handle,target.id,target.address,target.port,file.getPath()))throw new IOException("Cannot queue the file. Refresh the client or check its address.");sending.add(file);}
            uiHeld=false;Native.action(handle,7,1,"");poll();main.post(started);
        }catch(Exception e){uiHeld=false;if(handle!=0){Native.action(handle,2,0,"");Native.action(handle,7,1,"");}sending.clear();plan.clear();fail(e);}});
    }
    private void poll() throws JSONException {
        JSONObject next=new JSONObject(Native.tick(handle));
        long count=next.optLong("count"),position=next.optLong("position");
        if(!next.optBoolean("incoming")&&plan.size()==count&&position>0){long all=0,done=next.optLong("done");for(int i=0;i<plan.size();i++){all+=plan.get(i);if(i<position-1)done+=plan.get(i);}next.put("overallTotal",all);next.put("overallDone",Math.min(all,done));}
        if(!next.optBoolean("active")&&next.optInt("queued")==0)cleanSending();
        state=next;String text=next.toString();if(!last.equals(text)){last=text;main.post(()->{if(!closed)listener.changed(next);});}
    }
    private void cleanSending() { for(File file:sending)removeCopy(file);sending.clear(); }
    private void removeCopy(File file) { try{if(file.getParentFile().getCanonicalPath().startsWith(pending.getCanonicalPath()+File.separator)){file.delete();file.getParentFile().delete();}}catch(IOException ignored){} }
    private void fail(Exception e) { String message=e.getMessage()==null?"Transfer engine error":e.getMessage();main.post(()->{if(!closed)listener.error(message);}); }
    static String addresses() {
        List<String> values=new ArrayList<>();try{Enumeration<NetworkInterface> interfaces=NetworkInterface.getNetworkInterfaces();while(interfaces.hasMoreElements()){NetworkInterface nic=interfaces.nextElement();if(!nic.isUp()||nic.isLoopback())continue;Enumeration<InetAddress> addresses=nic.getInetAddresses();while(addresses.hasMoreElements()){InetAddress a=addresses.nextElement();if(a instanceof Inet4Address&&!a.isLoopbackAddress())values.add(a.getHostAddress()+":46440");}}}catch(SocketException ignored){}return String.join("\n",values);
    }
}

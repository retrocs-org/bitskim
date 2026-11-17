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
    private final Set<Listener> listeners=new CopyOnWriteArraySet<>();
    private static ClientService shared;
    static synchronized ClientService shared(Context context){if(shared==null)shared=new ClientService(context,null);return shared;}
    void attach(Listener listener){listeners.add(listener);if(!closed)listener.changed(state);}
    void detach(Listener listener){listeners.remove(listener);}
    private void changed(JSONObject value){for(Listener listener:listeners)listener.changed(value);}
    private void reportError(String text){for(Listener listener:listeners)listener.error(text);}
    void backgroundUnavailable(){preferences.edit().putBoolean("backgroundTransfers",false).apply();reportError("Android could not keep the transfer running in the background. Keep BitSkim open.");if(!foreground)stop();}
    boolean backgroundEnabled(){return preferences.getBoolean("backgroundTransfers",true);}
    void leaveForeground(){foreground=false;if(backgroundEnabled()&&state.optBoolean("active")&&!state.optBoolean("awaiting")){TransferService.ensure(context);return;}stop();}
    void serviceStopped(){if(!foreground)stop();}

    private final Handler main=new Handler(Looper.getMainLooper());
    private final ScheduledExecutorService engine=Executors.newSingleThreadScheduledExecutor();
    private final ExecutorService copies=Executors.newSingleThreadExecutor();
    private final Set<File> sending=ConcurrentHashMap.newKeySet();
    private final List<Long> plan=new ArrayList<>();
    private long handle;
    private boolean uiHeld;
    private int ticks;
    private ScheduledFuture<?> pump;
    private String last="";
    volatile JSONObject state=new JSONObject();
    volatile boolean foreground;
    volatile boolean closed;
    ClientService(Context context,Listener listener) {
        this.context=context.getApplicationContext();if(listener!=null)listeners.add(listener);preferences=context.getSharedPreferences("bitskim",Context.MODE_PRIVATE);
        received=new File(context.getFilesDir(),"Received");pending=new File(context.getFilesDir(),"Pending");received.mkdirs();pending.mkdirs();

    }
    private void startPump(){if(pump==null)pump=engine.scheduleAtFixedRate(()->{if(handle!=0){try{Native.action(handle,9,0,"");if(++ticks%10==0)poll();}catch(Exception failure){fail(failure);}}},0,20,TimeUnit.MILLISECONDS);}
    private void stopPump(){if(pump!=null){pump.cancel(false);pump=null;}}
    void start() { engine.execute(()->{if(closed || handle!=0)return;try{
        File trust=new File(context.getFilesDir(),"Trust");if(!trust.isDirectory()&&!trust.mkdirs())throw new IOException("Cannot create identity storage.");
        String id=preferences.getString("installationID",null);if(id==null){id=UUID.randomUUID().toString().replace("-","");preferences.edit().putString("installationID",id).apply();}
        handle=Native.create(InputPolicy.deviceName(Build.MANUFACTURER+" "+Build.MODEL),id,trust.getPath(),received.getPath(),46440,BuildConfig.DISCOVERY,preferences.getInt("security",1));
        Native.action(handle,11,preferences.getBoolean("retroMetadata",false)?1:0,"");if(uiHeld)Native.action(handle,7,0,"");last="";startPump();poll();
    }catch(Exception e){fail(e);}}); }
    void stop() { engine.execute(()->{stopPump();uiHeld=false;if(handle!=0){Native.destroy(handle);handle=0;}sending.clear();state=new JSONObject();last="";}); }
    void close() { closed=true;engine.execute(()->{stopPump();if(handle!=0){Native.destroy(handle);handle=0;}sending.clear();});engine.shutdown();copies.shutdown(); }
    void showInvitation(String address,java.util.function.Consumer<String> ready){engine.execute(()->{try{if(handle==0)throw new IOException("Open the local network client first.");String text=Native.invitation(handle,address);if(text.isEmpty())throw new IOException("Enable receiving and certificate encryption before inviting.");main.post(()->ready.accept(text));}catch(Exception failure){fail(failure);}});}
    void command(int command,long serial,String argument) { engine.execute(()->{if(handle==0)return;try{if(!Native.action(handle,command,serial,argument))throw new IOException("Action is unavailable or the offer expired.");if(command==6)preferences.edit().putInt("security",(int)serial).apply();if(command==11)preferences.edit().putBoolean("retroMetadata",serial!=0).apply();poll();}catch(Exception e){fail(e);}}); }
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
        if(uris.isEmpty() || uris.size()>32){reportError("Choose between 1 and 32 files.");releaseUi();return;}
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
        }catch(Exception e){for(File file:staged)removeCopy(file);main.post(()->{releaseUi();if(!closed)reportError(e.getMessage()==null?"Cannot import files.":e.getMessage());});}});
    }
    void send(InputPolicy.Target target,List<File> files,long[] sizes,Runnable started) {
        engine.execute(()->{try{
            if(handle==0)throw new IOException("Allow local network access and open the client first.");
            if(state.optBoolean("active")||state.optInt("queued")>0)throw new IOException("Finish, retry or cancel the queued transfer first.");
            if(files.isEmpty())throw new IOException("Choose files first.");
            for(int i=0;i<files.size();i++){File f=files.get(i);if(!f.isFile()||f.length()!=sizes[i]||f.length()>InputPolicy.MAX_FILE)throw new IOException("Selected files changed. Review them again.");}
            plan.clear();for(long size:sizes)plan.add(size);
            for(File file:files){if(!(target.invitation.isEmpty()?Native.queue(handle,target.id,target.address,target.port,file.getPath()):Native.queueInvitation(handle,target.invitation,file.getPath())))throw new IOException("Cannot queue the file. Refresh the client or check its address.");sending.add(file);}
            uiHeld=false;Native.action(handle,7,1,"");poll();main.post(started);
        }catch(Exception e){uiHeld=false;if(handle!=0){Native.action(handle,2,0,"");Native.action(handle,7,1,"");}sending.clear();plan.clear();fail(e);}});
    }
    private void poll() throws JSONException {
        JSONObject next=new JSONObject(Native.tick(handle));
        if(!next.has("active")||!next.has("queued"))throw new JSONException(next.optString("error","Incomplete engine report"));
        long count=next.optLong("count"),position=next.optLong("position");
        if(!next.optBoolean("incoming")&&plan.size()==count&&position>0){long all=0,done=next.optLong("done");for(int i=0;i<plan.size();i++){all+=plan.get(i);if(i<position-1)done+=plan.get(i);}next.put("overallTotal",all);next.put("overallDone",Math.min(all,done));}
        Set<String> queuedPaths=new HashSet<>();JSONArray paths=next.optJSONArray("queuedPaths");if(paths!=null)for(int i=0;i<paths.length();i++)queuedPaths.add(paths.optString(i));if(paths!=null)for(int i=0;i<paths.length();i++){File file=new File(paths.optString(i));try{if(file.getCanonicalPath().startsWith(pending.getCanonicalPath()+File.separator))sending.add(file);}catch(IOException ignored){}}
        JSONArray completed=next.optJSONArray("completedPaths");if(completed!=null)for(int i=0;i<completed.length();i++){String path=completed.optString(i);if(!queuedPaths.contains(path)){File file=new File(path);sending.remove(file);removeCopy(file);}}
        if(!next.optBoolean("active")&&next.optInt("queued")==0)cleanSending();
        state=next;
        if(next.optBoolean("active")&&!next.optBoolean("awaiting")&&foreground&&backgroundEnabled())main.post(()->TransferService.ensure(context));
        String text=next.toString();if(!last.equals(text)){last=text;main.post(()->{if(!closed)changed(next);});}
    }
    private void cleanSending() { for(File file:sending)removeCopy(file);sending.clear(); }
    private void removeCopy(File file) { try{if(file.getParentFile().getCanonicalPath().startsWith(pending.getCanonicalPath()+File.separator)){file.delete();file.getParentFile().delete();}}catch(IOException ignored){} }
    private void fail(Exception e) { String message=e.getMessage()==null?"Transfer engine error":e.getMessage();main.post(()->{if(!closed)reportError(message);}); }
    static String addresses() {
        List<String> values=new ArrayList<>();try{Enumeration<NetworkInterface> interfaces=NetworkInterface.getNetworkInterfaces();while(interfaces.hasMoreElements()){NetworkInterface nic=interfaces.nextElement();if(!nic.isUp()||nic.isLoopback())continue;Enumeration<InetAddress> addresses=nic.getInetAddresses();while(addresses.hasMoreElements()){InetAddress a=addresses.nextElement();if(a instanceof Inet4Address&&!a.isLoopbackAddress())values.add(a.getHostAddress()+":46440");}}}catch(SocketException ignored){}return String.join("\n",values);
    }
}

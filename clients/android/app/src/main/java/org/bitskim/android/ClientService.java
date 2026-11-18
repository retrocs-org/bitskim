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
    private boolean cleanupScheduled;
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
        Set<File> restored=new HashSet<>();for(int index=0;;index++){String path=Native.queuePath(handle,index);if(path.isEmpty())break;File file=new File(path),batch=TransferFiles.batch(pending,file);if(batch!=null&&restored.add(batch))TransferFiles.markQueued(pending,file);}
        Native.action(handle,11,preferences.getBoolean("retroMetadata",false)?1:0,"");if(uiHeld)Native.action(handle,7,0,"");last="";startPump();poll();
    }catch(Exception e){fail(e);}}); }
    void stop() { engine.execute(()->{stopPump();uiHeld=false;if(handle!=0){Native.destroy(handle);handle=0;}sending.clear();state=new JSONObject();last="";}); }
    void close() { closed=true;engine.execute(()->{stopPump();if(handle!=0){Native.destroy(handle);handle=0;}sending.clear();});engine.shutdown();copies.shutdown(); }
    void showInvitation(String address,java.util.function.Consumer<String> ready){engine.execute(()->{try{if(handle==0)throw new IOException("Open the local network client first.");String text=Native.invitation(handle,address);if(text.isEmpty())throw new IOException("Enable receiving and certificate encryption before inviting.");main.post(()->ready.accept(text));}catch(Exception failure){fail(failure);}});}
    void command(int command,long serial,String argument) { engine.execute(()->{if(handle==0)return;try{if(!Native.action(handle,command,serial,argument))throw new IOException("Action is unavailable or the offer expired.");if(command==6)preferences.edit().putInt("security",(int)serial).apply();if(command==11)preferences.edit().putBoolean("retroMetadata",serial!=0).apply();poll();}catch(Exception e){fail(e);}}); }
    void pauseForUi(Runnable ready) { pauseForUi(ready,()->{}); }
    void pauseForUi(Runnable ready,Runnable failed) { engine.execute(()->{try{if(handle!=0 && !Native.action(handle,7,0,""))throw new IOException("Finish or cancel the current transfer first.");uiHeld=true;main.post(ready);}catch(Exception e){main.post(failed);fail(e);}}); }
    void releaseUi() { if(closed)return;engine.execute(()->{uiHeld=false;if(handle!=0)Native.action(handle,7,1,"");}); }
    List<File> selectedFiles() {
        List<File> files=new ArrayList<>();File[] folders=pending.listFiles();if(folders!=null)for(File folder:folders){if(folder.getName().startsWith("."))continue;File[] list=folder.listFiles();if(list!=null)for(File file:list)if((file.isFile()||file.isDirectory())&&!new File(pending,".queued-"+folder.getName()).exists())files.add(file);}
        files.sort(Comparator.comparing(File::getName,String.CASE_INSENSITIVE_ORDER));return files;
    }
    interface FileAction { void run() throws Exception; }
    void manageFiles(FileAction action,Runnable ready) {copies.execute(()->{boolean success=false;try{action.run();success=true;}catch(Exception e){fail(e);}finally{releaseUi();}if(success)main.post(()->{if(!closed)ready.run();});});}
    void discardSelection(Runnable ready) {copies.execute(()->{for(File file:selectedFiles())removeCopy(file);main.post(()->{if(!closed)ready.run();});});}
    void importFiles(List<Uri> uris,java.util.function.Consumer<Boolean> ready) {
        if(uris.isEmpty() || uris.size()>32){reportError("Choose between 1 and 32 files.");releaseUi();ready.accept(false);return;}
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
            main.post(()->{releaseUi();if(!closed)ready.accept(true);});
        }catch(Exception e){for(File file:staged)removeCopy(file);main.post(()->{releaseUi();if(!closed){ready.accept(false);reportError(e.getMessage()==null?"Cannot import files.":e.getMessage());}});}});
    }
    void importFolder(Uri uri,java.util.function.Consumer<Boolean> ready) {
        copies.execute(()->{try{
            ProviderFolder provider=new ProviderFolder(context.getContentResolver(),uri);
            TransferFiles.stage(provider,provider.root(),pending,()->closed||Thread.currentThread().isInterrupted());
            main.post(()->{releaseUi();if(!closed)ready.accept(true);});
        }catch(Exception failure){main.post(()->{releaseUi();if(!closed){ready.accept(false);reportError(failure.getMessage()==null?"Cannot import this folder.":failure.getMessage());}});}});
    }
    void review(List<File> files,java.util.function.Consumer<List<TransferFiles.Snapshot>> ready) {
        copies.execute(()->{try{
            List<TransferFiles.Snapshot> snapshots=snapshots(files);main.post(()->{if(!closed)ready.accept(snapshots);});
        }catch(Exception failure){releaseUi();main.post(()->{if(!closed)ready.accept(null);});fail(failure);}});
    }
    private static List<TransferFiles.Snapshot> snapshots(List<File> files)throws IOException {
        if(files.isEmpty()||files.size()>32)throw new IOException("Choose between 1 and 32 files or folders.");
        List<TransferFiles.Snapshot> result=new ArrayList<>();Set<String> unique=new HashSet<>();int count=0;
        for(File file:files){if(!unique.add(file.getAbsolutePath()))throw new IOException("Choose each item only once.");TransferFiles.Snapshot snapshot=TransferFiles.inspect(file);count+=snapshot.entries.size();if(count>TransferFiles.MAX_ENTRIES)throw new IOException("Choose no more than 32,760 files and folder entries.");result.add(snapshot);}return result;
    }
    void send(InputPolicy.Target target,List<File> files,List<TransferFiles.Snapshot> reviewed,Runnable started) {
        copies.execute(()->{try{
            List<TransferFiles.Snapshot> fresh=snapshots(files);if(fresh.size()!=reviewed.size())throw new IOException("The selection changed. Review it again.");
            for(int i=0;i<fresh.size();i++)if(!fresh.get(i).matches(reviewed.get(i)))throw new IOException("The selected files or folder contents changed. Review them again.");
            engine.execute(()->{try{
                if(handle==0)throw new IOException("Allow local network access and open the client first.");
                if(state.optBoolean("active")||state.optInt("queued")>0||state.optBoolean("scanning"))throw new IOException("Finish, retry or cancel the queued transfer first.");
                Native.action(handle,14,0,"");plan.clear();for(TransferFiles.Snapshot snapshot:fresh)for(TransferFiles.Entry entry:snapshot.entries)plan.add(entry.bytes);
                for(File file:files)if(!(target.invitation.isEmpty()?Native.queue(handle,target.id,target.address,target.port,file.getPath()):Native.queueInvitation(handle,target.invitation,file.getPath())))throw new IOException("Cannot queue the selection. Refresh the client or check its address.");
                if(!Native.action(handle,13,0,""))throw new IOException("Cannot save the outgoing queue.");
                for(File file:files){TransferFiles.markQueued(pending,file);sending.add(file);}
                Native.action(handle,8,0,"");uiHeld=false;Native.action(handle,7,1,"");poll();main.post(started);
            }catch(Exception failure){if(handle!=0){Native.action(handle,2,0,"");Native.action(handle,13,0,"");Native.action(handle,7,1,"");}uiHeld=false;sending.clear();plan.clear();for(File file:files)try{File batch=TransferFiles.batch(pending,file);if(batch!=null)TransferFiles.unmark(pending,batch);}catch(IOException ignored){}fail(failure);}});
        }catch(Exception failure){releaseUi();fail(failure);}});
    }
    private void poll() throws JSONException {
        JSONObject next=new JSONObject(Native.tick(handle));
        if(!next.has("active")||!next.has("queued"))throw new JSONException(next.optString("error","Incomplete engine report"));
        long count=next.optLong("count"),position=next.optLong("position");
        if(!next.optBoolean("incoming")&&plan.size()==count&&position>0){long all=0,done=next.optLong("done");for(int i=0;i<plan.size();i++){all+=plan.get(i);if(i<position-1)done+=plan.get(i);}next.put("overallTotal",all);next.put("overallDone",Math.min(all,done));}
        if(!next.optBoolean("active")&&!next.optBoolean("scanning")&&next.optInt("queued")==0)cleanSending();
        state=next;
        if(next.optBoolean("active")&&!next.optBoolean("awaiting")&&foreground&&backgroundEnabled())main.post(()->TransferService.ensure(context));
        String text=next.toString();if(!last.equals(text)){last=text;main.post(()->{if(!closed)changed(next);});}
    }
    private void cleanSending() {
        if(cleanupScheduled)return;sending.clear();File[] all=pending.listFiles();if(all==null)return;List<File> batches=new ArrayList<>();
        for(File marker:all){String n=marker.getName();if(!n.startsWith(".queued-")||java.nio.file.Files.isSymbolicLink(marker.toPath())||!marker.isFile())continue;String id=n.substring(8);try{if(UUID.fromString(id).toString().equals(id))batches.add(new File(pending,id));}catch(IllegalArgumentException ignored){}}
        if(batches.isEmpty())return;cleanupScheduled=true;copies.execute(()->{for(File batch:batches)try{if(batch.exists())TransferFiles.removeBatch(pending,batch);else TransferFiles.unmark(pending,batch);}catch(IOException failure){fail(failure);}if(!closed)engine.execute(()->cleanupScheduled=false);});
    }
    private void removeCopy(File file) { try{if(file.getParentFile().getCanonicalPath().startsWith(pending.getCanonicalPath()+File.separator)){InputPolicy.delete(pending,file);file.getParentFile().delete();}}catch(IOException ignored){} }
    private void fail(Exception e) { String message=e.getMessage()==null?"Transfer engine error":e.getMessage();main.post(()->{if(!closed)reportError(message);}); }
    static String addresses() {
        List<String> values=new ArrayList<>();try{Enumeration<NetworkInterface> interfaces=NetworkInterface.getNetworkInterfaces();while(interfaces.hasMoreElements()){NetworkInterface nic=interfaces.nextElement();if(!nic.isUp()||nic.isLoopback())continue;Enumeration<InetAddress> addresses=nic.getInetAddresses();while(addresses.hasMoreElements()){InetAddress a=addresses.nextElement();if(a instanceof Inet4Address&&!a.isLoopbackAddress())values.add(a.getHostAddress()+":46440");}}}catch(SocketException ignored){}return String.join("\n",values);
    }
}

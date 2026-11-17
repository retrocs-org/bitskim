package org.bitskim.android;
import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.os.*;
import androidx.core.app.NotificationCompat;
import org.json.JSONObject;

/** Lifetime is limited to an explicitly approved active transfer, never idle listening. */
public final class TransferService extends Service implements ClientService.Listener {
    private static final int NOTICE=46440;
    private static boolean requested;
    private ClientService client;
    private PowerManager.WakeLock wake;
    private long updated,renewed;
    static synchronized void ensure(Context context){
        if(requested)return;if(Build.VERSION.SDK_INT>=33&&context.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)!=android.content.pm.PackageManager.PERMISSION_GRANTED){ClientService.shared(context).backgroundUnavailable();return;}requested=true;
        try{context.startForegroundService(new Intent(context,TransferService.class));}
        catch(RuntimeException failure){requested=false;ClientService.shared(context).backgroundUnavailable();}
    }
    @Override public void onCreate(){
        super.onCreate();client=ClientService.shared(this);
        NotificationManager manager=getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("transfers","Active file transfers",NotificationManager.IMPORTANCE_LOW));
        if(Build.VERSION.SDK_INT>=29)startForeground(NOTICE,notice(client.state),ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);else startForeground(NOTICE,notice(client.state));
        wake=((PowerManager)getSystemService(POWER_SERVICE)).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,getPackageName()+":transfer");wake.setReferenceCounted(false);wake.acquire(600000);renewed=SystemClock.elapsedRealtime();client.attach(this);
    }
    private Notification notice(JSONObject state){
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent cancel=PendingIntent.getService(this,1,new Intent(this,TransferService.class).setAction("cancel"),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        long done=state.optLong("done"),total=state.optLong("total");
        return new NotificationCompat.Builder(this,"transfers").setSmallIcon(org.bitskim.android.R.drawable.bitskim).setContentTitle(state.optString("filename","BitSkim transfer")).setContentText(state.optString("status","Transferring file")).setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true).setProgress(1000,total>0?(int)Math.min(1000,done*1000/total):0,total==0).addAction(0,"Cancel transfer",cancel).build();
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId){if(intent!=null&&"cancel".equals(intent.getAction()))client.command(2,0,"");return START_NOT_STICKY;}
    @Override public void changed(JSONObject state){
        if((!state.optBoolean("active")&&(state.optInt("queued")==0||state.optBoolean("paused")))||state.optBoolean("awaiting")){stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();return;}
        long now=SystemClock.elapsedRealtime();if(wake!=null&&now-renewed>60000){wake.acquire(600000);renewed=now;}
        if(now-updated>=1000){getSystemService(NotificationManager.class).notify(NOTICE,notice(state));updated=now;}
    }
    @Override public void error(String text){}
    @Override public void onTaskRemoved(Intent rootIntent){if(!client.state.optBoolean("active"))stopSelf();}
    @Override public void onDestroy(){client.detach(this);if(wake!=null&&wake.isHeld())wake.release();synchronized(TransferService.class){requested=false;}client.serviceStopped();super.onDestroy();}
    @Override public IBinder onBind(Intent intent){return null;}
}

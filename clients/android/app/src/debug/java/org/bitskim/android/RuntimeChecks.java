package org.bitskim.android;
import android.app.Instrumentation;
import android.os.Bundle;
import java.io.File;
import java.nio.file.Files;

/** Debug-only native runtime checks; creates and removes its own private fixtures. */
public final class RuntimeChecks extends Instrumentation {
    @Override public void onCreate(Bundle arguments){super.onCreate(arguments);start();}
    @Override public void onStart(){
        Bundle result=new Bundle();File root=null;
        try{
            root=Files.createTempDirectory(getTargetContext().getCacheDir().toPath(),"native-check-").toFile();
            String report=Native.selfTest(root.getPath());if(!report.startsWith("PASS "))throw new IllegalStateException(report);
            InputPolicy.Target target=InputPolicy.target("192.168.1.2:46441");if(target.port!=46441)throw new IllegalStateException("Port parsing");
            String invitation="bitskim://192.168.1.2:46440/"+String.format(java.util.Locale.ROOT,"%032d",0).replace('0','a')+"/"+String.format(java.util.Locale.ROOT,"%064d",0).replace('0','b')+"/"+(System.currentTimeMillis()/1000+300)+"/"+String.format(java.util.Locale.ROOT,"%032d",0).replace('0','c');
            com.google.zxing.common.BitMatrix matrix=new com.google.zxing.qrcode.QRCodeWriter().encode(invitation,com.google.zxing.BarcodeFormat.QR_CODE,640,640);int[] pixels=new int[640*640];for(int y=0;y<640;y++)for(int x=0;x<640;x++)pixels[y*640+x]=matrix.get(x,y)?0xff000000:0xffffffff;
            String decoded=new com.google.zxing.qrcode.QRCodeReader().decode(new com.google.zxing.BinaryBitmap(new com.google.zxing.common.HybridBinarizer(new com.google.zxing.RGBLuminanceSource(640,640,pixels)))).getText();if(!decoded.equals(invitation)||!Native.parseInvitation(decoded).contains("192.168.1.2"))throw new IllegalStateException("QR round trip");
            File file=new File(root,"received.txt");InputPolicy.rename(root,file,"Renamed.txt");if(!new File(root,"Renamed.txt").isFile())throw new IllegalStateException("Rename failed");
            result.putString("stream","\n"+report+"\nPASS Android runtime checks\n");finish(-1,result);
        }catch(Throwable error){result.putString("stream","\nFAIL "+error+"\n");finish(0,result);}
        finally{if(root!=null)try{File[] files=root.listFiles();if(files!=null)for(File file:files)InputPolicy.delete(root,file);root.delete();}catch(Exception ignored){}}
    }
}

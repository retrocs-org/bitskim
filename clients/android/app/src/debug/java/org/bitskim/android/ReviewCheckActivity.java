package org.bitskim.android;
import android.app.Activity;import android.os.Bundle;import android.content.res.Configuration;import android.view.ContextThemeWrapper;import android.widget.TextView;import java.util.*;

/** Debug-only host for the actual review dialog; never starts a network client. */
public final class ReviewCheckActivity extends Activity {
    TransferReviewDialog review;TextView outcome;
    @Override public void onCreate(Bundle state){super.onCreate(state);outcome=new TextView(this);outcome.setText("Review not submitted");setContentView(outcome);
        boolean light=getIntent().getBooleanExtra("light",false);ContextThemeWrapper themed=new ContextThemeWrapper(this,light?android.R.style.Theme_Material_Light_NoActionBar:android.R.style.Theme_Material_NoActionBar);
        Configuration config=new Configuration(getResources().getConfiguration());config.fontScale=getIntent().getFloatExtra("fontScale",1);themed.applyOverrideConfiguration(config);
        List<String> rows=new ArrayList<>();rows.add("Folder: Tree/Empty/");rows.add("File: Tree/"+String.join("",Collections.nCopies(9,"Long filename möte-"))+".txt · 12 B");for(int i=0;i<120;i++)rows.add("File: Tree/File-"+i+".txt · 12 B");
        review=new TransferReviewDialog(themed,"Test recipient","192.0.2.1:46440","Encryption preferred.",rows,1452,light?0xffeff1f5:0xff1e1e2e,light?0xffccd0da:0xff313244,light?0xff4c4f69:0xffcdd6f4,light?0xff8839ef:0xffcba6f7,()->outcome.setText("Send selected"),()->outcome.setText("Review cancelled"));review.show();
    }
    @Override protected void onDestroy(){if(review!=null)review.dismiss();super.onDestroy();}
}

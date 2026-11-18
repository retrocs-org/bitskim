package org.bitskim.android;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.*;
import android.widget.*;
import java.util.*;

/** A virtualized review whose actions remain outside the scrolling content. */
final class TransferReviewDialog extends Dialog {
    final ListView entries;
    final Button cancelButton,sendButton;
    private final Context context;
    private final int base,text,accent,surface;
    private int dp(int n){return Math.round(n*context.getResources().getDisplayMetrics().density);}
    private TextView label(String value,int size){TextView v=new TextView(context);v.setText(value);v.setTextSize(size);v.setTextColor(text);v.setPadding(dp(16),dp(12),dp(16),dp(12));v.setSingleLine(false);v.setEllipsize(null);return v;}
    private Button action(String label){Button b=new Button(context);b.setText(label);b.setAllCaps(false);b.setMinHeight(dp(48));b.setMinWidth(dp(48));b.setTextColor(accent);b.setBackgroundTintList(android.content.res.ColorStateList.valueOf(surface));b.setId(View.generateViewId());return b;}
    TransferReviewDialog(Context context,String recipient,String endpoint,String protection,List<String> rows,long total,int base,int surface,int text,int accent,Runnable send,Runnable cancel){
        super(context);this.context=context;this.base=base;this.surface=surface;this.text=text;this.accent=accent;
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout root=new LinearLayout(context);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(base);root.setPadding(dp(8),dp(8),dp(8),dp(8));
        entries=new ListView(context);entries.setBackgroundColor(base);entries.setId(View.generateViewId());
        TextView title=label("Review transfer",22);if(Build.VERSION.SDK_INT>=28)title.setAccessibilityHeading(true);entries.addHeaderView(title,null,false);
        entries.addHeaderView(label(recipient+"\n"+endpoint+"\n"+rows.size()+" items · "+MobileSize.format(total)+"\n"+protection+"\nThe recipient must approve. Keep BitSkim open.",16),null,false);
        entries.setAdapter(new BaseAdapter(){public int getCount(){return rows.size();}public Object getItem(int i){return rows.get(i);}public long getItemId(int i){return i;}public View getView(int i,View recycled,ViewGroup parent){TextView row=recycled instanceof TextView?(TextView)recycled:label("",16);row.setText(rows.get(i));row.setMinHeight(dp(48));row.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);return row;}});
        root.addView(entries,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout footer=new LinearLayout(context);footer.setPadding(dp(8),dp(8),dp(8),0);footer.setOrientation(LinearLayout.HORIZONTAL);
        cancelButton=action("Cancel");sendButton=action("Send");cancelButton.setContentDescription("Cancel transfer review");sendButton.setContentDescription("Send "+rows.size()+" items to "+recipient);
        LinearLayout.LayoutParams left=new LinearLayout.LayoutParams(0,-2,1);left.setMarginEnd(dp(8));footer.addView(cancelButton,left);footer.addView(sendButton,new LinearLayout.LayoutParams(0,-2,1));root.addView(footer);
        sendButton.setAccessibilityTraversalAfter(cancelButton.getId());cancelButton.setOnClickListener(v->cancel());sendButton.setOnClickListener(v->{sendButton.setEnabled(false);dismiss();send.run();});
        setOnCancelListener(d->cancel.run());setCanceledOnTouchOutside(false);setContentView(root);
        setOnKeyListener((d,key,event)->{if(key==KeyEvent.KEYCODE_ESCAPE&&event.getAction()==KeyEvent.ACTION_UP){cancel();return true;}return false;});
    }
    @Override public void show(){super.show();Window window=getWindow();if(window==null)return;int height=context.getResources().getDisplayMetrics().heightPixels;
        if(Build.VERSION.SDK_INT>=30){WindowMetrics metrics=window.getWindowManager().getCurrentWindowMetrics();android.graphics.Insets insets=metrics.getWindowInsets().getInsetsIgnoringVisibility(WindowInsets.Type.systemBars());height=metrics.getBounds().height()-insets.top-insets.bottom;}
        window.setLayout(WindowManager.LayoutParams.MATCH_PARENT,Math.max(dp(160),(int)(height*.9f)));window.setBackgroundDrawableResource(android.R.color.transparent);
    }
    private static final class MobileSize {static String format(long bytes){if(bytes<1024)return bytes+" B";if(bytes<1048576)return String.format(java.util.Locale.ROOT,"%.1f KiB",bytes/1024.0);return String.format(java.util.Locale.ROOT,"%.1f MiB",bytes/1048576.0);}}
}

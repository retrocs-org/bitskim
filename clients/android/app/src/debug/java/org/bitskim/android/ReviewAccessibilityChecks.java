package org.bitskim.android;
import android.app.*;import android.content.*;import android.graphics.Rect;import android.os.Bundle;import android.view.*;import android.view.accessibility.AccessibilityNodeInfo;import android.widget.TextView;import java.io.*;

/** Native layout/accessibility checks with synthetic rows and no file/network access. */
public final class ReviewAccessibilityChecks extends Instrumentation {
 @Override public void onCreate(Bundle args){super.onCreate(args);start();}
 private void check(boolean value,String what){if(!value)throw new AssertionError(what);}
 private void onUi(Runnable action){java.util.concurrent.atomic.AtomicReference<Throwable> failure=new java.util.concurrent.atomic.AtomicReference<>();runOnMainSync(()->{try{action.run();}catch(Throwable problem){failure.set(problem);}});if(failure.get()!=null)throw new RuntimeException(failure.get());}
 private void visible(View view,String label){Rect area=new Rect();check(view.getGlobalVisibleRect(area)&&area.width()>=view.getWidth()&&area.height()>=view.getHeight(),label+" clipped");check(view.getHeight()>=48*view.getResources().getDisplayMetrics().density,label+" touch target");}
 @Override public void onStart(){Bundle result=new Bundle();ReviewCheckActivity activity=null;try{
  for(boolean light:new boolean[]{false,true})for(float scale:new float[]{1,2}){
   Intent launch=new Intent(getTargetContext(),ReviewCheckActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).putExtra("light",light).putExtra("fontScale",scale);
   activity=(ReviewCheckActivity)startActivitySync(launch);waitForIdleSync();ReviewCheckActivity current=activity;
   if(scale==2){android.graphics.Bitmap screenshot=getUiAutomation().takeScreenshot();if(screenshot==null)throw new IOException("No native review screenshot");try(OutputStream output=new FileOutputStream(new File(getTargetContext().getCacheDir(),"review-accessibility-"+(light?"latte":"mocha")+".png"))){screenshot.compress(android.graphics.Bitmap.CompressFormat.PNG,100,output);}screenshot.recycle();}
   onUi(()->{visible(current.review.cancelButton,"Cancel");visible(current.review.sendButton,"Send");check(current.review.sendButton.getContentDescription().toString().contains("122 items to Test recipient"),"Send accessibility label");check(current.review.entries.canScrollVertically(1),"Content not scrollable");current.review.entries.setSelection(3);});waitForIdleSync();
   onUi(()->{View first=current.review.entries.getChildAt(0);check(first instanceof TextView,"Long row not visible");if(first instanceof TextView){TextView row=(TextView)first;check(row.getText().toString().contains("Long filename möte-"),"Long path missing");check(row.getLineCount()>1,"Long path did not wrap");for(int line=0;line<row.getLineCount();line++)check(row.getLayout().getEllipsisCount(line)==0,"Filename truncated");}current.review.entries.setSelection(120);});waitForIdleSync();
   onUi(()->{visible(current.review.cancelButton,"Scrolled Cancel");visible(current.review.sendButton,"Scrolled Send");});waitForIdleSync();sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);onUi(()->check(current.review.cancelButton.requestFocus(),"Cancel cannot receive keyboard focus"));waitForIdleSync();sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);waitForIdleSync();
   onUi(()->check(current.outcome.getText().toString().equals("Review cancelled"),"Keyboard cancellation did not run"));
   onUi(current::finish);activity=null;waitForIdleSync();
  }
  result.putString("stream","\nPASS native mobile review: long paths, 200% text, light/dark themes, pinned 48dp actions and keyboard cancellation\n");finish(-1,result);
 }catch(Throwable failure){result.putString("stream","\nFAIL "+failure+"\n");finish(0,result);}finally{if(activity!=null){ReviewCheckActivity current=activity;runOnMainSync(current::finish);}}}
}

package org.bitskim.android;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.*;
import android.view.*;
import android.widget.*;
import androidx.core.content.FileProvider;
import org.json.*;
import java.io.*;
import java.util.*;

public final class MainActivity extends Activity implements ClientService.Listener {
    private static final String LOCAL_NETWORK="android.permission.ACCESS_LOCAL_NETWORK";
    private ClientService client;
    private LinearLayout root,body,peerRows;
    private TextView status,details,diagnostics,pairing;
    private ProgressBar progress,overall;
    private AlertDialog offerDialog,reviewDialog,fileDialog;
    private long offeredSerial;
    private String peerKey="",addressDraft="",pendingKey="",deferredError="";
    private int tab,base,surface,text,muted,accent;
    private boolean pickerOpen,importing,permissionRequested,started;
    private File receivedFolder,exportSource;
    private InputPolicy.Target target;
    private WifiManager.MulticastLock multicast;
    private final Handler ui=new Handler(Looper.getMainLooper());

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);client=new ClientService(this,this);receivedFolder=client.received;
        if(state!=null){tab=state.getInt("tab",0);addressDraft=state.getString("address","");}
        render();handleSharedIntent(getIntent());
    }
    @Override protected void onSaveInstanceState(Bundle state){super.onSaveInstanceState(state);state.putInt("tab",tab);state.putString("address",addressDraft);}
    @Override public void onConfigurationChanged(android.content.res.Configuration config){super.onConfigurationChanged(config);render();}
    @Override protected void onStart(){super.onStart();started=true;client.foreground=true;render();ensureNetwork(false);if(!deferredError.isEmpty()){String message=deferredError;deferredError="";error(message);}}
    @Override protected void onStop(){started=false;client.foreground=false;if(!pickerOpen){dismissDialogs();offeredSerial=0;client.stop();releaseMulticast();}super.onStop();}
    @Override protected void onDestroy(){client.close();releaseMulticast();super.onDestroy();}
    @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);handleSharedIntent(intent);}
    private boolean networkAllowed(){return Build.VERSION.SDK_INT<37||checkSelfPermission(LOCAL_NETWORK)==PackageManager.PERMISSION_GRANTED;}
    private void ensureNetwork(boolean explicit){
        if(networkAllowed()){client.start();acquireMulticast();}
        else if(explicit||!permissionRequested){permissionRequested=true;requestPermissions(new String[]{LOCAL_NETWORK},41);}
    }
    @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){super.onRequestPermissionsResult(code,permissions,results);if(code==41){if(networkAllowed()){client.start();acquireMulticast();}else error("Local network access is needed to discover clients and transfer files. Files can still be selected locally.");render();}}
    private void acquireMulticast(){
        if(!BuildConfig.DISCOVERY||multicast!=null)return;
        try{WifiManager wifi=(WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);if(wifi!=null){multicast=wifi.createMulticastLock("BitSkim discovery");multicast.setReferenceCounted(false);multicast.acquire();}}catch(SecurityException e){error("Wi-Fi discovery lock unavailable. Manual connections remain available.");}
    }
    private void releaseMulticast(){if(multicast!=null){if(multicast.isHeld())multicast.release();multicast=null;}}
    private void dismissDialogs(){if(fileDialog!=null){fileDialog.dismiss();fileDialog=null;}if(offerDialog!=null){offerDialog.dismiss();offerDialog=null;}if(reviewDialog!=null){reviewDialog.dismiss();reviewDialog=null;}}
    private int dp(int value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private TextView label(String value,int size,int color){TextView view=new TextView(this);view.setText(value);view.setTextSize(size);view.setTextColor(color);view.setPadding(dp(10),dp(8),dp(10),dp(8));return view;}
    private Button button(String title,Runnable action){Button view=new Button(this);view.setText(title);view.setAllCaps(false);view.setTextColor(accent);view.setBackgroundTintList(ColorStateList.valueOf(surface));view.setMinHeight(dp(48));view.setOnClickListener(v->action.run());return view;}
    private EditText input(String hint){EditText view=new EditText(this);view.setHint(hint);view.setTextColor(text);view.setHintTextColor(muted);view.setSingleLine(true);view.setPadding(dp(12),dp(10),dp(12),dp(10));return view;}
    private void section(String name){body.addView(label(name,19,text));}
    private void loadTheme(){
        String id=client.preferences.getString("theme","mocha");if(id.equals("system"))id=(getResources().getConfiguration().uiMode&android.content.res.Configuration.UI_MODE_NIGHT_MASK)==android.content.res.Configuration.UI_MODE_NIGHT_YES?"mocha":"latte";
        try(InputStream in=getAssets().open("palettes.json")){JSONArray palettes=new JSONArray(readText(in));JSONObject chosen=palettes.getJSONObject(3);for(int i=0;i<palettes.length();i++)if(palettes.getJSONObject(i).getString("id").equals(id))chosen=palettes.getJSONObject(i);base=Color.parseColor("#"+chosen.getString("base"));surface=Color.parseColor("#"+chosen.getString("surface"));text=Color.parseColor("#"+chosen.getString("text"));muted=Color.parseColor("#"+chosen.getString("muted"));accent=Color.parseColor("#"+chosen.getString("accent"));}
        catch(Exception e){base=0xff1e1e2e;surface=0xff313244;text=0xffcdd6f4;muted=0xffbac2de;accent=0xffcba6f7;}
    }
    private void render(){
        loadTheme();setTheme((Color.red(base)+Color.green(base)+Color.blue(base))>500?android.R.style.Theme_Material_Light_NoActionBar:android.R.style.Theme_Material_NoActionBar);peerKey="";peerRows=null;status=null;details=null;diagnostics=null;pairing=null;progress=null;overall=null;
        root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(base);root.setPadding(dp(12),0,dp(12),0);
        root.setOnApplyWindowInsetsListener((view,insets)->{if(Build.VERSION.SDK_INT>=30){android.graphics.Insets edges=insets.getInsets(WindowInsets.Type.systemBars());view.setPadding(dp(12)+edges.left,edges.top,dp(12)+edges.right,edges.bottom);}else view.setPadding(dp(12),insets.getSystemWindowInsetTop(),dp(12),insets.getSystemWindowInsetBottom());return insets;});
        root.addView(label("BitSkim "+BuildConfig.VERSION_NAME+" · Experimental",23,text));
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);scroll.addView(body);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        String[] tabs={"Clients","Files","Received","Settings"};LinearLayout bar=new LinearLayout(this);for(int i=0;i<tabs.length;i++){final int next=i;Button b=button(tabs[i],()->{tab=next;render();});if(i==tab)b.setTextColor(text);bar.addView(b,new LinearLayout.LayoutParams(0,-2,1));}root.addView(bar);setContentView(root);
        switch(tab){case 0:clientsPage();break;case 1:filesPage();break;case 2:receivedPage();break;default:settingsPage();}
        changed(client.state);
    }
    private void clientsPage(){
        if(!networkAllowed())body.addView(button("Allow local network access",()->ensureNetwork(true)));
        body.addView(label("Keep BitSkim open to receive. Transfers stop when you leave the app.",14,muted));
        section("Nearby clients");peerRows=new LinearLayout(this);peerRows.setOrientation(LinearLayout.VERTICAL);body.addView(peerRows);
        if(BuildConfig.DISCOVERY)body.addView(button("Refresh clients",()->client.command(1,0,"")));else body.addView(label("This build uses manual connections.",14,muted));
        section("Connect by IP");EditText address=input("192.168.1.20 or 192.168.1.20:46440");address.setText(addressDraft);body.addView(address);body.addView(button("Choose client",()->{addressDraft=address.getText().toString();try{choose(InputPolicy.target(addressDraft));}catch(Exception e){error(e.getMessage());}}));
        section("Receive on this device");body.addView(label(ClientService.addresses(),14,muted));
        section("Transfer");status=label("Starting local network…",16,text);body.addView(status);details=label("",14,muted);body.addView(details);
        progress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);progress.setMax(1000);progress.setProgressTintList(ColorStateList.valueOf(accent));body.addView(progress);
        overall=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);overall.setMax(1000);overall.setProgressTintList(ColorStateList.valueOf(accent));body.addView(overall);
        pairing=label("",14,accent);pairing.setTextIsSelectable(true);body.addView(pairing);
        body.addView(button("Cancel transfer",()->client.command(2,0,"")));body.addView(button("Retry queued transfer",()->client.command(8,0,"")));
    }
    private void filesPage(){
        section("Files to send");body.addView(button(importing?"Preparing files…":"Browse Android Files",()->pickFiles(null)));
        body.addView(label("Choose local files or an enabled document provider. Files are copied locally; originals remain unchanged.",14,muted));
        List<File> files=client.selectedFiles();pendingKey=files.toString();section("Ready to send · "+files.size());for(File file:files)body.addView(label(file.getName()+" · "+size(file.length()),16,text));
        if(files.isEmpty())body.addView(label("No files selected.",14,muted));else{body.addView(button("Choose a client",()->{tab=0;render();}));body.addView(button("Clear selection",()->{if(!importing)client.discardSelection(this::render);}));}
    }
    private void choose(InputPolicy.Target selected){
        if(importing){error("Wait for the selected files to finish preparing.");return;}
        target=selected;if(client.selectedFiles().isEmpty())pickFiles(selected);else review(selected);
    }
    private void pickFiles(InputPolicy.Target selected){
        if(importing)return;target=selected;client.pauseForUi(()->{pickerOpen=true;Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("*/*");intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE,true);try{startActivityForResult(intent,11);}catch(ActivityNotFoundException e){pickerOpen=false;client.releaseUi();error("No Android document picker is installed.");}});
    }
    private void handleSharedIntent(Intent intent){
        if(intent==null)return;String action=intent.getAction();List<Uri> uris=new ArrayList<>();
        if(Intent.ACTION_SEND.equals(action)){Uri uri=intent.getParcelableExtra(Intent.EXTRA_STREAM);if(uri!=null)uris.add(uri);}
        else if(Intent.ACTION_SEND_MULTIPLE.equals(action)){ArrayList<Uri> list=intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);if(list!=null)uris.addAll(list);}else return;
        target=null;if(!uris.isEmpty())importUris(uris);setIntent(new Intent(this,MainActivity.class));
    }
    private void importUris(List<Uri> uris){
        importing=true;client.importFiles(uris,()->{importing=false;if(!started)return;if(target!=null)review(target);else{tab=1;render();}});
    }
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);pickerOpen=false;
        if(request==11){if(result==RESULT_OK&&data!=null){List<Uri> uris=new ArrayList<>();if(data.getClipData()!=null)for(int i=0;i<data.getClipData().getItemCount();i++)uris.add(data.getClipData().getItemAt(i).getUri());else if(data.getData()!=null)uris.add(data.getData());importUris(uris);}else client.releaseUi();}
        else if(request==12){File source=exportSource;exportSource=null;client.releaseUi();if(result==RESULT_OK&&data!=null&&data.getData()!=null&&source!=null){Uri uri=data.getData();new Thread(()->{try(InputStream input=new FileInputStream(source);OutputStream out=getContentResolver().openOutputStream(uri)){if(out==null)throw new IOException("Provider could not open destination.");copyStream(input,out);ui.post(()->Toast.makeText(this,"Copy handed to the document provider",Toast.LENGTH_LONG).show());}catch(Exception e){ui.post(()->error(e.getMessage()));}},"BitSkim export").start();}}
    }
    private void review(InputPolicy.Target selected){
        List<File> files=new ArrayList<>(client.selectedFiles());if(files.isEmpty()){error("Choose files first.");return;}
        client.pauseForUi(()->{try{
            long[] sizes=new long[files.size()];long total=0;StringBuilder message=new StringBuilder((selected.id.isEmpty()?"Manual client":selected.name)+"\n"+selected.endpoint()+"\n\n");for(int i=0;i<files.size();i++){File file=files.get(i);if(!file.isFile()||file.length()>InputPolicy.MAX_FILE)throw new IOException("A selected file is unavailable or too large.");sizes[i]=file.length();total+=sizes[i];message.append(file.getName()).append(" · ").append(size(sizes[i])).append('\n');}
            message.append("\nTotal: ").append(size(total)).append("\nThe recipient must approve. Keep BitSkim open.");
            int security=client.preferences.getInt("security",1);message.append(security==2?"\nEncryption required.":security==1?"\nEncryption preferred; older clients may use plaintext.":"\nEncryption disabled.");
            reviewDialog=new AlertDialog.Builder(this).setTitle("Review transfer").setMessage(message).setPositiveButton("Send",(dialog,which)->client.send(selected,files,sizes,()->{tab=0;render();})).setNegativeButton("Cancel",(dialog,which)->client.releaseUi()).create();reviewDialog.setOnCancelListener(dialog->client.releaseUi());reviewDialog.show();
        }catch(Exception e){client.releaseUi();error(e.getMessage());}});
    }
    private void receivedPage(){
        section("Received");if(!receivedFolder.equals(client.received))body.addView(button("Parent folder",()->{receivedFolder=receivedFolder.getParentFile();render();}));
        EditText search=input("Search this folder");body.addView(search);LinearLayout filesView=new LinearLayout(this);filesView.setOrientation(LinearLayout.VERTICAL);body.addView(filesView);
        Runnable reload=()->{filesView.removeAllViews();File[] listed=receivedFolder.listFiles();if(listed==null)return;Arrays.sort(listed,Comparator.comparing(File::getName,String.CASE_INSENSITIVE_ORDER));for(File file:listed){if(java.nio.file.Files.isSymbolicLink(file.toPath())||file.getName().startsWith(".")||!file.getName().toLowerCase(Locale.ROOT).contains(search.getText().toString().toLowerCase(Locale.ROOT)))continue;filesView.addView(button((file.isDirectory()?"▸ ":"")+file.getName(),()->{if(file.isDirectory()){receivedFolder=file;render();}else fileActions(file);}));}};
        search.addTextChangedListener(new android.text.TextWatcher(){public void beforeTextChanged(CharSequence s,int start,int count,int after){}public void onTextChanged(CharSequence s,int start,int before,int count){reload.run();}public void afterTextChanged(android.text.Editable e){}});reload.run();
        body.addView(label("Received files are stored privately. Use Share or Save to Files to copy them to another app or provider.",14,muted));
    }
    private void fileActions(File file){
        client.pauseForUi(()->{
            fileDialog=new AlertDialog.Builder(this).setTitle(file.getName()).setItems(new String[]{"Preview","Share","Save to Files","Rename","Delete"},(dialog,which)->{
                if(which==0||which==1){try{InputPolicy.child(client.received,file);Uri uri=FileProvider.getUriForFile(this,getPackageName()+".files",file);String mime=getContentResolver().getType(uri);if(mime==null)mime="application/octet-stream";Intent intent=new Intent(which==0?Intent.ACTION_VIEW:Intent.ACTION_SEND);if(which==0)intent.setDataAndType(uri,mime);else{intent.setType(mime);intent.putExtra(Intent.EXTRA_STREAM,uri);}intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);intent.setClipData(ClipData.newRawUri("BitSkim file",uri));startActivity(which==0?intent:Intent.createChooser(intent,"Share received file"));}catch(Exception e){error("No compatible viewer or sharing app is available.");}finally{client.releaseUi();}}
                else if(which==2){exportSource=file;pickerOpen=true;Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("application/octet-stream");intent.putExtra(Intent.EXTRA_TITLE,file.getName());try{startActivityForResult(intent,12);}catch(ActivityNotFoundException e){pickerOpen=false;client.releaseUi();error("No Android document picker is installed.");}}
                else if(which==3){EditText name=input("Filename including extension");name.setText(file.getName());fileDialog=new AlertDialog.Builder(this).setTitle("Rename").setView(name).setPositiveButton("Rename",(d,w)->{String value=name.getText().toString();client.manageFiles(()->InputPolicy.rename(client.received,file,value),this::render);}).setNegativeButton("Cancel",(d,w)->client.releaseUi()).create();fileDialog.setOnCancelListener(d->client.releaseUi());fileDialog.show();}
                else{fileDialog=new AlertDialog.Builder(this).setTitle("Delete permanently?").setMessage("This cannot be undone.").setPositiveButton("Delete",(d,w)->{client.manageFiles(()->InputPolicy.delete(client.received,file),this::render);}).setNegativeButton("Cancel",(d,w)->client.releaseUi()).create();fileDialog.setOnCancelListener(d->client.releaseUi());fileDialog.show();}
            }).create();fileDialog.setOnCancelListener(d->client.releaseUi());fileDialog.show();
        });
    }
    private void settingsPage(){
        section("Appearance");List<String> ids=new ArrayList<>(),names=new ArrayList<>();ids.add("system");names.add("Follow system");try(InputStream in=getAssets().open("palettes.json")){JSONArray palettes=new JSONArray(readText(in));for(int i=0;i<palettes.length();i++){ids.add(palettes.getJSONObject(i).getString("id"));names.add(palettes.getJSONObject(i).getString("name"));}}catch(Exception ignored){}
        Spinner themes=new Spinner(this);ArrayAdapter<String> adapter=new ArrayAdapter<>(this,android.R.layout.simple_spinner_dropdown_item,names);themes.setAdapter(adapter);themes.setSelection(Math.max(0,ids.indexOf(client.preferences.getString("theme","mocha"))));themes.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int position,long id){String choice=ids.get(position);if(!choice.equals(client.preferences.getString("theme","mocha"))){client.preferences.edit().putString("theme",choice).apply();render();}}});body.addView(themes);
        section("Encryption");String[] modes={"Disabled","Prefer encryption","Require encryption"};for(int i=0;i<modes.length;i++){final int mode=i;body.addView(button((client.preferences.getInt("security",1)==mode?"✓ ":"")+modes[i],()->{client.command(6,mode,"");ui.postDelayed(this::render,200);}));}
        body.addView(label("Prefer can use plaintext with older clients. Require rejects plaintext. Compare the full code on both devices before trusting.",14,muted));
        section("Device pairing");pairing=label("Pair during an encrypted transfer.",14,text);pairing.setTextIsSelectable(true);body.addView(pairing);EditText code=input("Code from the other device");body.addView(code);body.addView(button("Trust matching code",()->client.command(5,0,code.getText().toString().trim().toUpperCase(Locale.ROOT))));
        section("Diagnostics");diagnostics=label("",12,muted);diagnostics.setTextIsSelectable(true);body.addView(diagnostics);
        body.addView(button("Website",()->startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse("https://bitskim.retrocs.org")))));
    }
    @Override public void changed(JSONObject state){
        if(!started)return;
        if(status!=null){String value=state.optString("status","Ready on the local network");if(!networkAllowed())value="Local network permission is required.";status.setText(value);long done=state.optLong("done"),total=state.optLong("total"),rate=state.optLong("rate"),remaining=Math.max(0,total-done);String eta=!state.optBoolean("active")?"":rate>0&&remaining>0?"About "+((remaining+rate-1)/rate)+"s left":"Estimating time…";String summary=state.optString("filename")+"\n"+size(done)+" / "+size(total)+" · "+size(rate)+"/s · "+eta+"\n"+state.optString("summary");if(state.optLong("overallTotal")>0)summary+="\nOverall "+size(state.optLong("overallDone"))+" / "+size(state.optLong("overallTotal"));details.setText(summary);progress.setProgress(total>0?(int)Math.min(1000,done*1000/total):0);long all=state.optLong("overallTotal");overall.setVisibility(all>0?View.VISIBLE:View.GONE);overall.setProgress(all>0?(int)Math.min(1000,state.optLong("overallDone")*1000/all):0);}
        if(pairing!=null)pairing.setText(state.optString("code").isEmpty()?"Pair during an encrypted transfer.":"Verify: "+state.optString("code")+(state.optBoolean("trusted")?" · Saved identity matches":" · Not yet trusted"));
        if(diagnostics!=null){StringBuilder report=new StringBuilder(state.optString("diagnostics"));JSONArray history=state.optJSONArray("history");if(history!=null)for(int i=0;i<history.length();i++)report.append("\n\n").append(history.optString(i));diagnostics.setText(report);}
        if(peerRows!=null)updatePeers(state.optJSONArray("peers"));
        if(offerDialog!=null&&!state.optBoolean("awaiting")){offerDialog.dismiss();offerDialog=null;}
        if(state.optBoolean("awaiting")&&state.optLong("serial")!=offeredSerial){offeredSerial=state.optLong("serial");incoming(state);}
    }
    private void updatePeers(JSONArray peers){
        String key=peers==null?"":peers.toString();if(key.equals(peerKey))return;peerKey=key;peerRows.removeAllViews();Map<String,List<JSONObject>> groups=new LinkedHashMap<>();if(peers!=null)for(int i=0;i<peers.length();i++){JSONObject p=peers.optJSONObject(i);if(p!=null)groups.computeIfAbsent(p.optString("id"),k->new ArrayList<>()).add(p);}
        if(groups.isEmpty())peerRows.addView(label("No clients found. Check Wi-Fi, permissions and firewall, or connect by IP.",14,muted));
        for(List<JSONObject> endpoints:groups.values()){JSONObject p=endpoints.get(0);String name=p.optString("name")+" · "+p.optString("platform");peerRows.addView(button(name+(endpoints.size()>1?" · "+endpoints.size()+" addresses":"\n"+p.optString("address")+":"+p.optInt("port")),()->{if(endpoints.size()==1)choose(peerTarget(p));else{String[] items=new String[endpoints.size()];for(int i=0;i<items.length;i++)items[i]=peerTarget(endpoints.get(i)).endpoint();new AlertDialog.Builder(this).setTitle("Choose client address").setItems(items,(d,w)->choose(peerTarget(endpoints.get(w)))).show();}}));}
    }
    private InputPolicy.Target peerTarget(JSONObject p){return new InputPolicy.Target(p.optString("id"),p.optString("name"),p.optString("address"),p.optInt("port"));}
    private void incoming(JSONObject state){
        long serial=state.optLong("serial");StringBuilder message=new StringBuilder(state.optString("peer")+" wants to send "+state.optString("filename")+"\n"+size(state.optLong("total"))+"\nExisting files are preserved with safe renamed destinations.");JSONArray manifest=state.optJSONArray("manifest");if(manifest!=null&&manifest.length()>0){message.append("\nCollection: ").append(state.optInt("manifestCount")).append(" items\n");for(int i=0;i<manifest.length();i++)message.append(manifest.optString(i)).append('\n');}
        LinearLayout content=new LinearLayout(this);content.setOrientation(LinearLayout.VERTICAL);content.setPadding(dp(12),0,dp(12),0);TextView identity=label(state.optBoolean("encrypted")?(state.optString("code").isEmpty()?"Encrypted connection; verification code unavailable.":"Verify: "+state.optString("code")+ (state.optBoolean("trusted")?" · Saved identity matches":"")):"Unencrypted connection",14,text);content.addView(identity);EditText code=input("Code from the other device");content.addView(code);
        offerDialog=new AlertDialog.Builder(this).setTitle("Incoming transfer").setMessage(message).setView(content).setPositiveButton("Accept",(d,w)->client.command(3,serial,"")).setNegativeButton("Decline",(d,w)->client.command(4,serial,"")).setNeutralButton("Trust code",null).create();offerDialog.setCanceledOnTouchOutside(false);offerDialog.setOnCancelListener(d->client.command(4,serial,""));offerDialog.setOnShowListener(d->{offerDialog.getButton(AlertDialog.BUTTON_NEUTRAL).setEnabled(!state.optString("code").isEmpty());offerDialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener(v->client.command(5,0,code.getText().toString().trim().toUpperCase(Locale.ROOT)));});offerDialog.show();
    }
    private static void copyStream(InputStream in,OutputStream out) throws IOException {byte[] buffer=new byte[65536];int n;while((n=in.read(buffer))!=-1)out.write(buffer,0,n);}
    private static String readText(InputStream in) throws IOException {ByteArrayOutputStream out=new ByteArrayOutputStream();copyStream(in,out);return new String(out.toByteArray(),java.nio.charset.StandardCharsets.UTF_8);}
    private static String size(long value){if(value<1024)return value+" B";if(value<1048576)return String.format(Locale.ROOT,"%.1f KiB",value/1024.0);if(value<1073741824L)return String.format(Locale.ROOT,"%.1f MiB",value/1048576.0);return String.format(Locale.ROOT,"%.2f GiB",value/1073741824.0);}
    @Override public void error(String message){importing=false;if(!started){deferredError=message==null?"Action failed":message;return;}if(!isFinishing())new AlertDialog.Builder(this).setTitle("BitSkim").setMessage(message==null?"Action failed":message).setPositiveButton("OK",null).show();}
}

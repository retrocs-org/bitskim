"""Run the mobile folder-copy and review policies against real temporary trees."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class MobileFolders(unittest.TestCase):
 def test_android_provider_copy_boundaries_and_review_signatures(self):
  java=Path(os.environ.get('JAVA_HOME','/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home'))/'bin'
  javac=str(java/'javac') if (java/'javac').is_file() else shutil.which('javac');runtime=str(java/'java') if (java/'java').is_file() else shutil.which('java')
  if not javac or not runtime:self.skipTest('JDK required')
  with tempfile.TemporaryDirectory() as temporary:
   root=Path(temporary);source=root/'FolderChecks.java'
   source.write_text(r'''package org.bitskim.android;
import java.io.*;import java.nio.file.*;import java.util.*;
public class FolderChecks {
 interface Work {void run()throws Exception;}
 static void fail(Work work)throws Exception {try{work.run();}catch(IOException|IllegalArgumentException expected){return;}throw new AssertionError("Unsafe folder was accepted");}
 static void check(boolean value){if(!value)throw new AssertionError("Folder check failed");}
 public static void main(String[] args)throws Exception{
  File base=new File(args[0]),pending=new File(base,"pending");pending.mkdir();
  TransferFiles.Node tree=new TransferFiles.Node("root","Tree",true,0),empty=new TransferFiles.Node("empty","Empty",true,0),file=new TransferFiles.Node("file","Möte.txt",false,3,946684800000L);
  TransferFiles.Source provider=new TransferFiles.Source(){public List<TransferFiles.Node> children(TransferFiles.Node n){return n.id.equals("root")?Arrays.asList(empty,file):Collections.emptyList();}public InputStream open(TransferFiles.Node n){return new ByteArrayInputStream(new byte[]{1,2,3});}};
  File staged=TransferFiles.stage(provider,tree,pending,()->false);check(new File(staged,"Empty").isDirectory());check(Arrays.equals(Files.readAllBytes(new File(staged,"Möte.txt").toPath()),new byte[]{1,2,3}));
  check(new File(staged,"Möte.txt").lastModified()==946684800000L);
  TransferFiles.Snapshot original=TransferFiles.inspect(staged);check(original.total==3&&original.entries.size()==3);
  Files.write(new File(staged,"Möte.txt").toPath(),new byte[]{3,2,1});check(!original.matches(TransferFiles.inspect(staged)));
  TransferFiles.markQueued(pending,staged);check(new File(pending,".queued-"+staged.getParentFile().getName()).isFile());check(new File(staged,"Möte.txt").isFile());
  check(TransferFiles.batch(pending,new File(staged,"Möte.txt")).equals(staged.getParentFile()));TransferFiles.clearFinished(pending);check(pending.listFiles().length==0);
  TransferFiles.Source truncated=new TransferFiles.Source(){public List<TransferFiles.Node> children(TransferFiles.Node n){return Collections.emptyList();}public InputStream open(TransferFiles.Node n){return new ByteArrayInputStream(new byte[]{1});}};
  fail(()->TransferFiles.stage(truncated,file,pending,()->false));check(pending.listFiles().length==0);
  TransferFiles.Source loop=new TransferFiles.Source(){public List<TransferFiles.Node> children(TransferFiles.Node n){return Arrays.asList(new TransferFiles.Node("root","Again",true,0));}public InputStream open(TransferFiles.Node n){throw new AssertionError();}};
  fail(()->TransferFiles.stage(loop,tree,pending,()->false));check(pending.listFiles().length==0);
  TransferFiles.Source denied=new TransferFiles.Source(){public List<TransferFiles.Node> children(TransferFiles.Node n){return Arrays.asList(file);}public InputStream open(TransferFiles.Node n)throws IOException{throw new IOException("Denied");}};
  fail(()->TransferFiles.stage(denied,tree,pending,()->false));check(pending.listFiles().length==0);
  fail(()->TransferFiles.stage(provider,new TransferFiles.Node("bad","../escape",false,1),pending,()->false));
  fail(()->TransferFiles.stage(provider,new TransferFiles.Node("big","big",false,2147483648L),pending,()->false));
  fail(()->TransferFiles.stage(provider,tree,pending,()->true));check(pending.listFiles().length==0);
  File outside=new File(base,"outside");Files.write(outside.toPath(),new byte[]{9});File link=new File(base,"link");Files.createSymbolicLink(link.toPath(),outside.toPath());fail(()->TransferFiles.inspect(link));check(Files.readAllBytes(outside.toPath())[0]==9);
  System.out.println("PASS provider folders, empty directories, rollback, bounds, cycles, signatures and queued ownership");
 }
}''')
   src=ROOT/'clients/android/app/src/main/java/org/bitskim/android'
   subprocess.run([javac,'--release','17','-d',str(root),str(src/'InputPolicy.java'),str(src/'TransferFiles.java'),str(source)],check=True,capture_output=True,text=True)
   result=subprocess.run([runtime,'-cp',str(root),'org.bitskim.android.FolderChecks',str(root)],capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('PASS provider folders',result.stdout)

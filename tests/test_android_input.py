"""Execute Android's pure-Java input and file policy on the host JDK."""
import os,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class AndroidInputTests(unittest.TestCase):
 def test_addresses_unicode_and_received_file_boundaries(self):
  java=Path(os.environ.get('JAVA_HOME','/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home'))/'bin'
  javac=str(java/'javac') if (java/'javac').is_file() else shutil.which('javac')
  runtime=str(java/'java') if (java/'java').is_file() else shutil.which('java')
  if not javac or not runtime:self.skipTest('JDK required for executable Android input tests')
  if subprocess.run([javac,'-version'],capture_output=True).returncode:self.skipTest('Working JDK required for executable Android input tests')
  with tempfile.TemporaryDirectory() as temporary:
   root=Path(temporary);source=root/'PolicyChecks.java'
   source.write_text('''package org.bitskim.android;
import java.io.*;import java.nio.file.*;import java.nio.charset.StandardCharsets;
public final class PolicyChecks {
 interface Action {void run() throws Exception;}
 static void reject(Action action) throws Exception {try{action.run();}catch(IOException|IllegalArgumentException expected){return;}throw new AssertionError("Unsafe input accepted");}
 static void check(boolean value){if(!value)throw new AssertionError("Policy check failed");}
 public static void main(String[] args) throws Exception {
  InputPolicy.Target target=InputPolicy.target(" 192.168.001.2:65535 ");check(target.address.equals("192.168.1.2")&&target.port==65535);
  check(InputPolicy.target("127.0.0.1").port==46440);
  for(String value:new String[]{"hostname","1.2.3.4:0","1.2.3.4:65536","1.2.3.999","1.2.3.4:abc","1.2.3.4:1:2","::1","1.2.3.4\\n:4"})reject(()->InputPolicy.target(value));
  for(String value:new String[]{"..","../escape","a/b","a\\\\b",".hidden","a\\u0000b"," ","é".repeat(128)})reject(()->InputPolicy.filename(value));
  check(InputPolicy.filename("Möte 😀.txt").equals("Möte 😀.txt"));check(InputPolicy.deviceName("😀".repeat(30)).getBytes(StandardCharsets.UTF_8).length<=63);
  File base=new File(args[0],"Received");base.mkdir();File original=new File(base,"original");Files.write(original.toPath(),new byte[]{1,2,3});File collision=new File(base,"exists");Files.write(collision.toPath(),new byte[]{9});
  reject(()->InputPolicy.rename(base,original,"exists"));check(Files.readAllBytes(collision.toPath())[0]==9&&original.exists());
  InputPolicy.rename(base,original,"Möte.txt");File renamed=new File(base,"Möte.txt");check(renamed.isFile());
  File outside=new File(args[0],"outside");Files.write(outside.toPath(),new byte[]{4});File link=new File(base,"link");Files.createSymbolicLink(link.toPath(),outside.toPath());
  reject(()->InputPolicy.delete(base,link));reject(()->InputPolicy.rename(base,link,"new"));reject(()->InputPolicy.delete(base,outside));check(outside.exists());Files.delete(link.toPath());
  InputPolicy.delete(base,renamed);check(!renamed.exists());InputPolicy.delete(base,collision);System.out.println("PASS Android input and received-file policy");
 }
}''')
   subprocess.run([javac,'--release','17','-d',str(root),str(ROOT/'clients/android/app/src/main/java/org/bitskim/android/InputPolicy.java'),str(source)],check=True,capture_output=True,text=True)
   result=subprocess.run([runtime,'-cp',str(root),'org.bitskim.android.PolicyChecks',str(root)],check=True,capture_output=True,text=True)
   self.assertIn('PASS Android input',result.stdout)

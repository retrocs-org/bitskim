# Dot-sourced by the isolated installer runner after administrator approval.
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class BitSkimWizard {
 public delegate bool EnumProc(IntPtr window, IntPtr data);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr data);
 [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr data);
 [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] static extern IntPtr ReadText(IntPtr window,uint message,IntPtr capacity,StringBuilder value);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder value, int count);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
 [DllImport("user32.dll", SetLastError=true)] static extern bool PostMessage(IntPtr window,uint message,IntPtr wparam,IntPtr lparam);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wparam, string text);
 [DllImport("user32.dll", EntryPoint="SendMessageW")] public static extern IntPtr Command(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
 public static string Text(IntPtr window){var value=new StringBuilder(8192);ReadText(window,0xD,(IntPtr)value.Capacity,value);return value.ToString();}
 public static string Class(IntPtr window){var value=new StringBuilder(128);GetClassName(window,value,value.Capacity);return value.ToString();}
 public static IntPtr Window(){IntPtr found=IntPtr.Zero;EnumWindows((w,d)=>{if(IsWindowVisible(w)&&Text(w).StartsWith("BitSkimInstallerTest")){found=w;return false;}return true;},IntPtr.Zero);return found;}
 public static IntPtr[] Children(IntPtr parent){var children=new List<IntPtr>();EnumChildWindows(parent,(w,d)=>{if(IsWindowVisible(w))children.Add(w);return true;},IntPtr.Zero);return children.ToArray();}
 public static void Click(IntPtr button){if(button==IntPtr.Zero||!IsWindowEnabled(button))throw new InvalidOperationException("Wizard action is disabled");if(!PostMessage(button,0xF5,IntPtr.Zero,IntPtr.Zero))throw new InvalidOperationException("Cannot post wizard action");}
}
'@
function RunWizard([string]$file,[string]$arguments,[bool]$cancelOnly=$false) {
 if ((Get-Item -LiteralPath $file).VersionInfo.ProductName -ne 'BitSkimInstallerTest' -and (Split-Path $file -Leaf) -ne 'Uninstall.exe') { throw 'Wizard requires an isolated installer or its checked uninstaller' }
 if ([BitSkimWizard]::Window() -ne [IntPtr]::Zero) { throw 'An isolated test wizard is already open; leaving it untouched' }
 $launch=@{FilePath=$file;PassThru=$true};$options=$arguments.Replace('/S','').Trim();if($options){$launch.ArgumentList=$options};$process=Start-Process @launch
 $deadline=(Get-Date).AddSeconds(120);$seen=@{};$previous='';$opened=$false;$cancelled=$false;$finished=$false
 while ((Get-Date) -lt $deadline) {
  $window=[BitSkimWizard]::Window()
  if ($window -eq [IntPtr]::Zero) {
   $process.Refresh()
   if ($opened -and $process.HasExited) { break }
   Start-Sleep -Milliseconds 120;continue
  }
  $opened=$true
  $children=@([BitSkimWizard]::Children($window))
  $text=(@([BitSkimWizard]::Text($window))+@($children|ForEach-Object{[BitSkimWizard]::Text($_)})) -join "`n"
  if ($cancelOnly) {
   if (!$cancelled) { [BitSkimWizard]::Click([BitSkimWizard]::GetDlgItem($window,2));$cancelled=$true }
   else {
    $yes=$children|Where-Object{[BitSkimWizard]::Class($_)-eq 'Button' -and [BitSkimWizard]::Text($_) -match '^&?Yes$'}|Select-Object -First 1
    if ($yes) {[BitSkimWizard]::Click($yes)}
   }
   Start-Sleep -Milliseconds 150;continue
  }
  $button=[BitSkimWizard]::GetDlgItem($window,1)
  if ($button -eq [IntPtr]::Zero -or ![BitSkimWizard]::IsWindowEnabled($button)) {Start-Sleep -Milliseconds 120;continue}
  $caption=[BitSkimWizard]::Text($button)
  $signature=$caption+"`n"+$text
  if ($signature -eq $previous) {Start-Sleep -Milliseconds 120;continue}
  $page=if($text -match 'License Agreement'){'License'}elseif($text -match 'Choose Components'){'Components'}elseif($text -match 'Choose Install Location'){'Directory'}elseif($caption -match 'Finish|Close'){'Finish'}elseif($caption -match 'Uninstall'){'Uninstall confirmation'}elseif($text -match 'Welcome to'){'Welcome'}else{'Unknown'}
  if ($page -eq 'Unknown') {throw ('Unexpected test wizard page; no action taken: '+$caption)}
  if (!$seen.ContainsKey($page)) {Log ('PASS interactive wizard page: '+$page);$seen[$page]=$true}
  if ($page -eq 'Directory') {
   $edit=$children|Where-Object{[BitSkimWizard]::Class($_)-eq 'Edit'}|Select-Object -First 1
   if (!$edit) {throw 'Installation directory field not found'}
   $destination=$arguments -replace '^.*?/D=',''
   [void][BitSkimWizard]::SendMessage($edit,0xC,[IntPtr]::Zero,$destination)
   Check ([BitSkimWizard]::Text($edit)-eq $destination) 'wizard destination remains inside isolated test directory'
  }
  if ($page -eq 'Finish') {
   foreach($child in $children) {
    if ([BitSkimWizard]::Class($child)-eq 'Button' -and [BitSkimWizard]::Text($child)-match '^&?Run .*BitSkim') {
     if ([BitSkimWizard]::Command($child,0xF0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()-ne 0) {[BitSkimWizard]::Click($child);Start-Sleep -Milliseconds 120}
     Check ([BitSkimWizard]::Command($child,0xF0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()-eq 0) 'finish page does not launch a client'
    }
   }
   $finished=$true
  }
  $previous=$signature;[BitSkimWizard]::Click($button);Start-Sleep -Milliseconds 180
 }
 if ([BitSkimWizard]::Window()-ne [IntPtr]::Zero) {throw 'Test wizard did not complete; left open for inspection'}
 $process.WaitForExit(10000)|Out-Null;$process.Refresh()
 Check $opened 'interactive test wizard opened'
 if ($cancelOnly) {Check $cancelled 'wizard cancellation selected';return $process.ExitCode}
 Check $finished 'interactive wizard finish reached'
 if ((Split-Path $file -Leaf)-ne 'Uninstall.exe') {foreach($page in @('Welcome','License','Components','Directory','Finish')) {Check $seen.ContainsKey($page) ('wizard visited '+$page)}}
 return $process.ExitCode
}

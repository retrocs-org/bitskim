#!/usr/bin/env python3
"""Exercise isolated native key-store records; never use production accounts/profiles."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('platform',choices=['macos','windows']);a=p.parse_args();out=ROOT/'build/key-store-review';out.mkdir(parents=True,exist_ok=True)
if a.platform=='windows':
 subprocess.run([os.environ.get('WINDOWS_CC','x86_64-w64-mingw32-gcc'),'-std=c99','-Os','-Wall','-Wextra','-Werror','-static','tests/windows-pairing-store.c','-o',str(out/'KeyStoreCheck.exe'),'-lcrypt32'],cwd=ROOT,check=True)
 print('Run KeyStoreCheck.exe under the Windows test user. The test creates and removes its own temporary INI.')
else:
 source=(ROOT/'clients/macos/BitSkim.swift').read_text().split('import SwiftUI',1)[0]
 source='import Foundation\n'+source+'''
let account = "bitskim-qa-" + UUID().uuidString
func check(_ condition: Bool, _ name: String) throws { guard condition else { throw NSError(domain: name, code: 1) }; print("PASS " + name) }
do {
    defer { try? PairingKeyStore.save("", account: account) }
    try check(try PairingKeyStore.load(account) == nil, "isolated Keychain account starts empty")
    let key = String(repeating: "a", count: 64)
    try PairingKeyStore.save(key, account: account)
    try check(try PairingKeyStore.load(account) == key, "Keychain round trip")
    let changed = String(repeating: "b", count: 64)
    try PairingKeyStore.save(changed, account: account)
    try check(try PairingKeyStore.load(account) == changed, "Keychain update")
    try PairingKeyStore.save("", account: account)
    try check(try PairingKeyStore.load(account) == nil, "Keychain deletion")
} catch { print("FAIL isolated Keychain check: " + error.localizedDescription); exit(1) }
'''
 (out/'KeyStoreCheck.swift').write_text(source)
 subprocess.run(['swiftc',str(out/'KeyStoreCheck.swift'),'-o',str(out/'KeyStoreCheck'),'-framework','Security'],check=True)
 subprocess.run([str(out/'KeyStoreCheck')],check=True)

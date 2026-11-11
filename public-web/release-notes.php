<?php
declare(strict_types=1);
$pageTitle = 'BitSkim 0.1 — Release notes';
$pageDescription = 'BitSkim 0.1 features, platform requirements and release information.';
$canonicalPath = '/release-notes.php';
$isHome = false;
require __DIR__ . '/includes/header.php';
?>
<main id="main" class="wrap section release">
<p class="eyebrow">THE FIRST CONNECTION</p><h1>What's in 0.1.</h1>
<p>Direct file sharing across computer generations, with native interfaces and a small shared C engine. No account or cloud service is required. Both computers need BitSkim and a reachable IPv4 connection.</p>
<h2>The essentials</h2>
<ul>
<li>Nearby-computer discovery over UDP and manual IPv4 connections; file data travels directly over TCP.</li>
<li>Streamed files up to 2 GiB minus one byte each, with progress, cancellation and sequential disk-backed queues.</li>
<li>Negotiated folder collections preserve nested and empty directories. Browse the complete list, approve once and choose one destination; folders receive a new safe root.</li>
<li>Disk-space checks, portable filename mapping, Rename/Skip/Replace choices, CRC-32 checking, temporary receive files and no-replace finalization.</li>
<li>Transfer tools include local diagnostics, recent session results and failed-transfer retry. Files are never opened automatically.</li>
</ul>
<h2>Modern conveniences</h2>
<ul>
<li>Mac peer tiles support multiple-file selection and multi-item drops. Adding items keeps a paused queue and its existing entries.</li>
<li>Mac and modern Windows prefer TLS by default, with Disable/Prefer/Force choices. Compare verification codes to remember device public keys.</li>
<li>Verified-device-only auto-accept is enabled by default. When automatic acceptance is on, an unknown device still asks for a decision unless that restriction is turned off.</li>
<li>Modern clients can retain interrupted single-file downloads and validate their contents before resuming. Explicit cancellation removes the active checkpoint.</li>
<li>Menu-bar/tray operation, themes, completion feedback and configurable payload speed limits.</li>
</ul>
<h2>Choose the right build</h2>
<table class="release-table"><thead><tr><th scope="col">Platform</th><th scope="col">Requirements and validation</th></tr></thead><tbody>
<tr><th scope="row">macOS</th><td>Apple silicon, macOS 13+. Developer ID signed and notarized, with a stapled ticket. App and host-engine checks are recorded separately from real-network and older-OS coverage.</td></tr>
<tr><th scope="row">Windows</th><td>Windows 10 x64 target; Windows 11 ARM startup/engine checks passed. Setup and portable ZIPs are unsigned. Native Windows 10 GUI coverage remains pending.</td></tr>
<tr><th scope="row">AmigaOS</th><td>AmigaOS 3.x, 68000-compatible, Workbench at least 640×256, and an AmiTCP-compatible bsdsocket.library v4+ stack. Hardware compatibility beyond the checked configuration remains unverified.</td></tr>
<tr><th scope="row">DOS / mTCP</th><td>Experimental 16-bit 8086 build. Requires a packet driver and configured IPv4 networking. Automated transfer and discovery checks pass; physical networking hardware remains untested. Distributed under GPLv3+ with corresponding source.</td></tr>
<tr><th scope="row">RISC OS</th><td>Experimental ARM2 and StrongARM APCS-26 builds. Requires a full Internet-module-compatible TCP/IP stack. Both variants compile and link; neither has native runtime validation. Preserve Acorn ZIP filetypes when extracting.</td></tr>
</tbody></table>
<p>Atari TOS/STinG, MiNTNet 68000/68060 and Windows Classic x86 clients are available as experimental binary downloads. Atari startup has been checked; transfers and hardware coverage remain unverified. Windows Classic passes binary format and legacy import checks; runtime validation remains pending. Full project source is available in the shared source archive.</p>
<h2>Know the boundaries</h2>
<p>Retro transfers remain plaintext. Unpaired TLS encrypts a connection without verifying the device; remembered public keys require explicit code comparison. CRC-32 detects accidental corruption and is not authentication. Use a trusted local network and keep the listening port off the public Internet.</p>
<p>Queues and the eight-result history are session-only. Queue pause takes effect after the current file; active-file pause is not implemented. Folder resume, filesystem permissions, timestamps, resource forks and retro filetype metadata are not preserved. Discovery can be blocked by routers, guest Wi-Fi, VPNs and NAT; manual IP may be required.</p>
<p>Each binary package includes build information and setup notes. The source archive includes <strong>docs/VALIDATION.md</strong>, <strong>docs/AUDIT-VALIDATION.md</strong> and the current release-readiness record. Compile/link success is not a promise of hardware compatibility.</p>
<p><a href="downloads/SHA256SUMS">Verify your download with SHA-256</a> · <a href="https://github.com/retrocs-org/bitskim">Public source repository</a></p>
<a class="button primary" href="index.php#get">Choose your build ↗</a>
</main>
<?php require __DIR__ . '/includes/footer.php'; ?>

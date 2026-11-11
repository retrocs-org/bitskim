/* RX SendFile.rexx 10.0.2.2:46440 Work:example.txt */
parse arg target path
if target = '' | path = '' then do
  say 'Usage: RX SendFile.rexx address:port path'
  exit 10
end
/* Paths may contain spaces; double quotes in a path are unsupported here. */
if pos('"', path) > 0 then exit 10
options results
address BITSKIM 'SEND 'target' "'path'"'
say 'BitSkim return code:' rc

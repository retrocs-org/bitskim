/* One diagnostic pass for NovaDock integrators. No network sockets. */
options results
address BITSKIM 'CAPABILITIES'
if rc ~= 0 then do
  address BITSKIM 'STATUS'
  if rc = 0 then say result
  exit rc
end
say result
address BITSKIM 'SESSION'
if rc = 0 then say result
address BITSKIM 'STATUS'
if rc = 0 then say result
address BITSKIM 'LAST'
if rc = 0 then say result
/* Real plugins retain SESSION/LATEST and query EVENTDETAILS with their cursor. */

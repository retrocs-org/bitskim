/* One polling pass; retain LATEST for your next call. RX Events.rexx 0 */
parse arg since
if since = '' then since = 0
if datatype(since, 'W') = 0 then exit 10
options results
address BITSKIM 'EVENTS 'since
if rc = 0 then say result
else say 'BitSkim returned' rc

/* Run with RX Bin:BitSkim/ARexx/Status.rexx */
options results
address BITSKIM 'STATUS'
if rc = 0 then say result
else say 'BitSkim returned' rc

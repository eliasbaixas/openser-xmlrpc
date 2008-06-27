#!/usr/bin/python
from xmlrpclib import ServerProxy, Error

stats={"tm_stats":["received_replies","received_replies","relayed_replies"
,"local_replies","UAS_transactions","UAC_transactions"
,"2xx_transactions","3xx_transactions","4xx_transactions"
,"5xx_transactions","6xx_transactions","inuse_transactions"]
,"core_stats":["rcv_requests","rcv_replies","fwd_requests"
,"fwd_replies","drop_requests","drop_replies"
,"err_requests","err_replies","bad_URIs_rcvd"
,"unsupported_methods","bad_msg_hdr"]
,"shm_stats":["total_size","used_size","real_used_size"
,"max_used_size","free_size","fragments"]
,"dialog_stats":["active_dialogs","early_dialogs","processed_dialogs"
,"expired_dialogs","failed_dialogs"]
,"imc_stats":["active_rooms"]
,"msilo_stats":["stored_messages","dumped_messages","failed_messages"
,"dumped_reminders","failed_reminders"]
,"registrar_stats":["max_expires","max_contacts","default_expire"
,"accepted_regs","rejected_regs"]
,"siptrace_stats":["traced_requests","traced_replies"]
,"sl_stats":["1xx_replies","2xx_replies","3xx_replies"
,"4xx_replies","5xx_replies","6xx_replies"
,"sent_replies","sent_err_replies","received_ACKs"]
,"usrloc_stats":["registered_users"] }

server = ServerProxy("http://localhost:4000") # local server

print server
for k,v in stats.iteritems():
  for i in v:
    try:
      print server.get_statistics(i)
    except Error, msg:
      print "ERROR", msg 

/* User-facing recovery is separate from the raw diagnostic/wire error. */
static void set_error(BSClient *c,BSErrorCode code,const char *message){if(c){c->error_transfer_serial=c->status.active?c->transfer_serial:0;c->error_code=code;copystr(c->error,message,sizeof c->error);}}
BSErrorCode bs_error_code(const BSClient *c){return c && c->error[0]?c->error_code:BS_ERR_NONE;}
int bs_transfer_verified(const BSClient *c){return c && c->transfer_result==BS_RESULT_COMPLETE && c->verified;}
static int recovery_kind(BSErrorCode code){switch(code){
 case BS_ERR_TRUST:return 1;case BS_ERR_DESTINATION:case BS_ERR_STORAGE:return 3;
 case BS_ERR_LIMIT:return 4;case BS_ERR_SOURCE:return 5;case BS_ERR_SECURITY:return 6;
 case BS_ERR_NETWORK:return 7;case BS_ERR_TIMEOUT:case BS_ERR_INPUT:return 8;
 case BS_ERR_DECLINED:case BS_ERR_CANCELLED:return 9;case BS_ERR_REMOTE:return 11;
 default:return 10;}}
static const char *recovery_hint(int kind){switch(kind){
 case 1:return "Verify the device and compare its code before changing saved trust.";
 case 2:return "Enable receiving on the other client, then retry.";
 case 3:return "Check free space and write access, or choose another destination folder.";
 case 4:return "Check the client's limits; rename the file or send a compatible archive.";
 case 5:return "Check the original file still exists and is readable; select it again if changed.";
 case 6:return "Save compatible encryption settings on both clients, then reconnect.";
 case 7:return "Check the other app is still open; test the connection before retrying.";
 case 8:return "Check the app, IPv4 address and firewall; use Test connection.";
 case 9:return "Confirm the recipient is ready before trying again.";
 case 11:return "Check the other client and read its error details.";
 case 10:return "Review the details in Transfer tools before retrying.";
 default:return "";}}
static void recovery_format(const BSClient *c,char *out,size_t capacity){size_t used;const char *hint;if(!capacity)return;copystr(out,c?c->error:"",capacity);if(!c || !out[0])return;
    hint=recovery_hint(recovery_kind(c->error_code));used=strlen(out);if(used>110 && capacity>=240){used=110;out[used]=0;}
    strncat(out," Next: ",capacity-used-1);used=strlen(out);strncat(out,hint,capacity-used-1);
}
const char *bs_user_error(BSClient *c){if(!c)return "";recovery_format(c,c->recovery_message,sizeof c->recovery_message);return c->recovery_message;}
unsigned bs_error_actions(const BSClient *c){unsigned actions=0;BSHistoryItem h;int kind;if(!c || !c->error[0] || c->status.active)return 0;kind=recovery_kind(c->error_code);
    if(kind==7 || kind==8)actions|=BS_ERROR_TEST_CONNECTION;
    if(kind==3 && c->status.incoming && bs_last_folder(c,1)[0])actions|=BS_ERROR_OPEN_FOLDER;
    if(!bs_queue_count(c) && !bs_queue_scanning(c) && kind!=1 && kind!=4 && kind!=6 && kind!=11 && bs_history_item(c,0,&h) && !h.incoming && h.serial==c->error_transfer_serial && c->error_transfer_serial==c->transfer_serial && h.result!=BS_RESULT_COMPLETE && h.result!=BS_RESULT_SKIPPED)actions|=BS_ERROR_RETRY;
    return actions;
}
int bs_error_retry(BSClient *c){if(!(bs_error_actions(c)&BS_ERROR_RETRY) || !bs_retry_history(c,0))return 0;bs_queue_continue(c);return 1;}

const char *bs_user_status(BSClient *c){return c && c->error[0] && !strcmp(c->status.message,c->error)?bs_user_error(c):(c?c->status.message:"");}

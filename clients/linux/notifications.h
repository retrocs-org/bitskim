#ifndef BITSKIM_LINUX_NOTIFICATIONS_H
#define BITSKIM_LINUX_NOTIFICATIONS_H
/* One result notification per completed collection/paused run, never per packet. */
typedef struct {
    unsigned long seen;
    guint sent,received,folders,failed,cancelled,declined,skipped,verified;
    gint64 updated;
    char *name,*folder;
    gboolean pending;
} LinuxAlerts;
static void alerts_clear(LinuxAlerts *alerts){unsigned long seen=alerts->seen;g_free(alerts->name);g_free(alerts->folder);memset(alerts,0,sizeof *alerts);alerts->seen=seen;}
static void alerts_record(LinuxAlerts *alerts,BSClient *client,gboolean enabled,const char *destination){
    BSHistoryItem item;if(!bs_history_item(client,0,&item)||item.serial!=bs_transfer_serial(client)||item.serial==alerts->seen)return;alerts->seen=item.serial;
    if(!enabled)return;
    alerts->pending=TRUE;alerts->updated=g_get_monotonic_time();g_free(alerts->name);alerts->name=linux_display(item.filename);
    if(item.result==BS_RESULT_COMPLETE){size_t length=strlen(item.filename);if(length&&item.filename[length-1]=='/')alerts->folders++;else if(item.incoming)alerts->received++;else alerts->sent++;if(item.verified)alerts->verified++;}
    else if(item.result==BS_RESULT_FAILED)alerts->failed++;
    else if(item.result==BS_RESULT_CANCELLED)alerts->cancelled++;
    else if(item.result==BS_RESULT_DECLINED)alerts->declined++;
    else if(item.result==BS_RESULT_SKIPPED)alerts->skipped++;
    if(item.path[0]){char *folder=item.incoming?g_strdup(destination):g_path_get_dirname(item.path);if(g_path_is_absolute(folder)&&g_file_test(folder,G_FILE_TEST_IS_DIR)){if(!alerts->folder||item.incoming){g_free(alerts->folder);alerts->folder=folder;}else g_free(folder);}else g_free(folder);}
}
static gboolean alerts_ready(const LinuxAlerts *alerts,const BSClient *client){
    if(!alerts->pending||bs_status(client)->active||bs_queue_scanning(client)||g_get_monotonic_time()-alerts->updated<500000)return FALSE;
    if(bs_queue_count(client)&&!bs_queue_paused(client))return FALSE;
    if(!alerts->failed&&!alerts->cancelled&&!alerts->declined&&bs_batch_count(client)>0&&bs_batch_index(client)+1<bs_batch_count(client))return FALSE;
    return TRUE;
}
static GNotification *alerts_notification(const LinuxAlerts *alerts){
    const char *title=alerts->failed?"Transfer failed":alerts->cancelled?"Transfer cancelled":alerts->declined?"Transfer declined":alerts->skipped&&!alerts->sent&&!alerts->received&&!alerts->folders?"Transfer skipped":alerts->received&&!alerts->sent&&!alerts->folders?"File received":alerts->sent&&!alerts->received&&!alerts->folders?"File sent":"Transfer complete";
    GNotification *notice=g_notification_new(title);GString *body=g_string_new(NULL);guint count=alerts->sent+alerts->received+alerts->folders+alerts->failed+alerts->cancelled+alerts->declined+alerts->skipped;
    if(count==1&&alerts->name){char *safe=g_markup_escape_text(alerts->name,-1);g_string_append_printf(body,"%s\n",safe);g_free(safe);}
    g_string_append_printf(body,"%u sent, %u received, %u folders; %u failed, %u cancelled, %u declined, %u skipped.",alerts->sent,alerts->received,alerts->folders,alerts->failed,alerts->cancelled,alerts->declined,alerts->skipped);
    if(alerts->verified)g_string_append_printf(body," %u written files verified (CRC32).",alerts->verified);
    g_notification_set_body(notice,body->str);g_string_free(body,TRUE);g_notification_set_priority(notice,G_NOTIFICATION_PRIORITY_NORMAL);g_notification_set_default_action(notice,"app.show-transfers");
    GIcon *icon=g_themed_icon_new("org.bitskim.BitSkim");g_notification_set_icon(notice,icon);g_object_unref(icon);
    if(alerts->folder)g_notification_add_button_with_target_value(notice,"Open folder","app.open-folder",g_variant_new_uint64(alerts->seen));
    return notice;
}
#endif

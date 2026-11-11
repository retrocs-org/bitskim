/* A directory can have multiple internal entries, but only one user send. */
static int prepare_single_send(BSClient *client)
{
    if(bs_status(client)->active || bs_queue_scanning(client) ||
       (bs_queue_count(client) && !bs_queue_paused(client))) return 0;
    bs_queue_clear(client);
    return 1;
}

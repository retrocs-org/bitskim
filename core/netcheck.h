/* A bounded TCP reachability check, serviced by the ordinary event loop. */
static void network_check_finish(BSClient *c,const char *result) {
    if(c->check_pending)bsp_close(c->check_socket);
    c->check_pending=0;copystr(c->check_result,result,sizeof c->check_result);
    notify(c,BS_EVENT_STATUS);
}
int bs_network_test(BSClient *c,const char *ip,unsigned short port) {
    struct sockaddr_in address;int result;char entered[64],*colon,*end;unsigned long number;
    if(!c)return 0;
    if(c->check_pending){copystr(c->error,"A connection test is already running",256);return 0;}
    if(!ip || !*ip){
        if(c->status.peer_address[0]){ip=c->status.peer_address;if(!port)port=c->remote_port;}
        else if(c->peer_count){ip=c->peers[0].address;if(!port)port=c->peers[0].port;}
        else{copystr(c->error,"Select a computer or enter an IPv4 address to test",256);return 0;}
    }
    if(strlen(ip)>=sizeof entered){copystr(c->error,"Address is too long",256);return 0;}
    copystr(entered,ip,sizeof entered);colon=strchr(entered,':');
    if(colon){*colon++=0;number=strtoul(colon,&end,10);if(!*colon || *end || !number || number>65535){copystr(c->error,"Enter a valid port",256);return 0;}port=(unsigned short)number;}
    ip=entered;
    memset(&address,0,sizeof address);address.sin_family=AF_INET;
    address.sin_addr.s_addr=inet_addr(ip);address.sin_port=htons(port?port:BS_PORT);
    if(!address.sin_addr.s_addr || address.sin_addr.s_addr==INADDR_NONE){copystr(c->error,"Enter a valid IPv4 address",256);return 0;}
    c->check_socket=socket(AF_INET,SOCK_STREAM,0);
    if(c->check_socket==BS_INVALID_SOCKET){copystr(c->error,"Cannot create test socket",256);return 0;}
    if(bsp_nonblock(c->check_socket)<0){bsp_close(c->check_socket);copystr(c->error,"Cannot configure test socket",256);return 0;}
    c->check_pending=1;c->check_start=bsp_seconds();
    sprintf(c->check_result,"Testing TCP %.47s:%u...",ip,port?port:BS_PORT);
    result=connect(c->check_socket,(struct sockaddr *)&address,sizeof address);
    if(result<0 && !
#ifdef BS_OPENSSL
       bsp_network_wouldblock()
#else
       bsp_wouldblock()
#endif
    ){network_check_finish(c,"TCP connection failed: check the address, listener and firewall");return 1;}
    notify(c,BS_EVENT_STATUS);return 1;
}
static void network_check_tick(BSClient *c) {
    int result;if(!c->check_pending)return;
    result=bsp_connected(c->check_socket);
    if(result>0){
#if !defined(BS_DOS) && !defined(BS_ATARI)
        struct sockaddr_in address;BSSockLen size=sizeof address;
        if(!getsockname(c->check_socket,(struct sockaddr *)&address,&size))
            copystr(c->check_local,inet_ntoa(address.sin_addr),sizeof c->check_local);
#endif
        network_check_finish(c,"TCP reachable. No file sent. Discovery and encryption are checked during normal transfers.");
    }else if(result<0)network_check_finish(c,"TCP connection refused or unreachable");
    else if(bsp_seconds()-c->check_start>=6)network_check_finish(c,"TCP check timed out: check address, firewall and network route");
}

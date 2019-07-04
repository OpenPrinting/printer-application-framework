/*
 *  Author: Dheeraj(dhirajyadav135@gmail.com)
 */
#include <libudev.h>
#include <sys/poll.h>
#include <assert.h>
#include <time.h>
#if HAVE_AVAHI
  #include <avahi-client/client.h>
  #include <avahi-client/lookup.h>
  #include <avahi-common/simple-watch.h>
  #include <avahi-common/malloc.h>
  #include <avahi-common/error.h>
#endif
#include <signal.h>
#define SUBSYSTEM "usb"


void static send_signal(const char* signal, pid_t ppid,int avahi)
{
  union sigval sv;
  // signal_data_t data;
  // sv.sival_ptr = (signal_data_t*)calloc(1,sizeof(signal_data_t));
  // signal_data_t *data = sv.sival_ptr;
  if(avahi)
    if(!strncasecmp(signal,"add",3))
      sv.sival_int=AVAHI_ADD;
    else
      sv.sival_int=AVAHI_REMOVE;
  else
    if(!strncasecmp(signal,"add",3))
      sv.sival_int=USB_ADD;
    else
      sv.sival_int=USB_REMOVE;
  // time(&(data->signal_time));
  // sv.sival_ptr=(void*)(&data);
  // fprintf(stderr,"%d %s",data->val,asctime(localtime(&(data->signal_time))));
  // signal_data_t *data2 = (sv.sival_ptr);
  // fprintf(stderr,"%d %s",data2.val,asctime(localtime(&(data2.signal_time))));
  sigqueue(ppid,SIGUSR1,sv);
}

int monitor_usb_devices(pid_t ppid)
{
    struct udev* udev = udev_new();
    if(!udev) {
        fprintf(stderr,"udev_new() failed!\n");
        return -1;
    }
    struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");

    udev_monitor_filter_add_match_subsystem_devtype(mon, SUBSYSTEM, NULL);
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);

    int num_process = 1;
    struct pollfd pfds[1];
    pfds[0].fd = fd;
    pfds[0].events = POLLIN;

    while(1){
        int ret = poll(&pfds[0],(nfds_t)num_process,-1);
        if(ret>0)
        {
            struct udev_device* dev = udev_monitor_receive_device(mon);
            if(udev_device_get_devnode(dev))
            {
                const char* action = udev_device_get_action(dev);
                if(action)
                {
                    if(!strncasecmp(action,"add",3)||!strncasecmp(action,"remove",6))
                    {
                        send_signal(action,ppid,0);
                    }
                }
            }
            udev_device_unref(dev);
        }
    }
    udev_unref(udev);
    return 0;
}

#if HAVE_AVAHI
static AvahiSimplePoll *detect = NULL;
int ppid = 0;
static void resolve_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    void* userdata){
        assert(r);
        switch(event) {
          case AVAHI_RESOLVER_FOUND:
            send_signal("add",ppid,1);
        }
        avahi_service_resolver_free(r);
    }

static void browse_callback(
  AvahiServiceBrowser *b,
  AvahiIfIndex interface,
  AvahiProtocol protocol,
  AvahiBrowserEvent event,
  const char *name,
  const char *type,
  const char *domain,
  AvahiLookupResultFlags flags,
  void* userdata) {
    AvahiClient *c = userdata;
    assert(b);
    switch(event){
      case AVAHI_BROWSER_FAILURE:
        fprintf(stderr,"(BROWSER) Error: %s\n",avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
        avahi_simple_poll_quit(detect);
        return;
      case AVAHI_BROWSER_NEW:
        avahi_service_resolver_new(c,interface,protocol,name,type,
          domain,AVAHI_PROTO_UNSPEC,0,resolve_callback,c);
        break;
      case AVAHI_BROWSER_REMOVE:
        send_signal("remove",ppid,1);
        break;
    }
}

static void client_callback(AvahiClient *c,
        AvahiClientState state,
        void *userdata){
    assert(c);
    if(state==AVAHI_CLIENT_FAILURE){
      fprintf(stderr,"Server connection failure: %s\n",avahi_strerror(avahi_client_errno(c)));
      avahi_simple_poll_quit(detect);
    }
}
int monitor_avahi_devices(pid_t parent_pid)
{
  ppid = parent_pid;
  AvahiClient *client = NULL;
  AvahiServiceBrowser *sb = NULL;
  int error, ret =1;

  if(!(detect = avahi_simple_poll_new())) {
    fprintf(stderr,"Failted to create simple poll!\n");    
    goto fail;
  }

  if(!(client = avahi_client_new(avahi_simple_poll_get(detect),0,client_callback,NULL,&error))){
    fprintf(stderr,"Unable to create client object!\n");
    goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,AVAHI_PROTO_UNSPEC,"_http._tcp",NULL,0,browse_callback,client))){
   fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
   goto fail;
  }
  avahi_simple_poll_loop(detect);
  ret = 0;
fail:
    if (sb)
      avahi_service_browser_free(sb);
    if (client)
        avahi_client_free(client);
    if (detect)
        avahi_simple_poll_free(detect);
    return ret;
}
#endif

// int main()
// {
//   monitor_avahi_devices(1);
// }
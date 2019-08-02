/*
 *  Printer Application Framework.
 * 
 *  This is helper code of the server daemon. It is reponsible for detecting
 *  when a device is attached or removed.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */
#include "server.h"
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
#define NUM_PROCESS 3

void static send_signal(const char* signal, pid_t ppid,int hardware_index)
{
  // union sigval sv;
  int offset = 0;
  if(!strncasecmp(signal,"add",3))
  {
    offset = 1;
  }
  else if(!strncasecmp(signal,"remove",6))
  {
    offset = 2;
  }
  hardware_index = 2*hardware_index+offset;
  // sv.sival_int = hardware_index;
  // sigqueue(ppid,SIGUSR1,sv);
  pthread_mutex_lock(&signal_lock);
  pending_signals[hardware_index] ++;
  pthread_mutex_lock(&signal_lock);
}

int monitor_devices(pid_t ppid)
{
    struct udev* udev = udev_new();
    if(!udev) {
        fprintf(stderr,"udev_new() failed!\n");
        return -1;
    }
    
    struct udev_monitor* mon[NUM_PROCESS];
    struct pollfd pfds[NUM_PROCESS];
    char arr[NUM_PROCESS][10]={"usb","tty","parallel"};
    for(int i=0;i<NUM_PROCESS;i++)
    {
      mon[i] = udev_monitor_new_from_netlink(udev,"udev");
      udev_monitor_filter_add_match_subsystem_devtype(mon[i],arr[i],NULL);
      /*
       *  The thing is, for parallel we will have to monitor multple subsystems. [parallel,printer,lp]
       */
      if(i==2)
      {
        udev_monitor_filter_add_match_subsystem_devtype(mon[i],"printers",NULL);
        udev_monitor_filter_add_match_subsystem_devtype(mon[i],"lp",NULL);
      }
      udev_monitor_enable_receiving(mon[i]);
      pfds[i].fd = udev_monitor_get_fd(mon[i]);
      pfds[i].events = POLLIN;
    }
    while(1) {
      int ret = poll(&pfds[0],(nfds_t)NUM_PROCESS,-1);
      if(ret>0){
  for(int i=0;i<NUM_PROCESS;i++)
  {
    if(pfds[i].revents & POLLIN){
      pfds[i].revents = 0;

      struct udev_device* dev = udev_monitor_receive_device(mon[i]);
      const char *devnode;
      if(devnode = udev_device_get_devnode(dev))
      {
        const char *action = udev_device_get_action(dev);
        if(!strncasecmp(action,"add",3)||!strncasecmp(action,"remove",6))
        {
          send_signal(action,ppid,i+1);
        }
        // const char *devpath = udev_device_get_devpath(dev);
        // const char *devtype = udev_device_get_devtype(dev);
        // fprintf(stderr,"Signal Type:%s\tAction:%s\tDevpath:%s\tDevtype:%s\n",arr[i],action,devpath,devtype);
      }
      udev_device_unref(dev);
    }
  }
      }
    }
  for(int i=0;i<NUM_PROCESS;i++)
  {
    udev_monitor_unref(mon[i]);
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
            send_signal("add",ppid,0);
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
        send_signal("remove",ppid,0);
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

  if(!(client = avahi_client_new(avahi_simple_poll_get(detect)
      ,0,client_callback,NULL,&error))){
    fprintf(stderr,"Unable to create client object!\n");
    goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,
  AVAHI_PROTO_UNSPEC,"_fax-ipp._tcp",NULL,0,browse_callback,client))){
   fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
   goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,
  AVAHI_PROTO_UNSPEC,"_ipp._tcp",NULL,0,browse_callback,client))){
   fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
   goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,
  AVAHI_PROTO_UNSPEC,"_ipp-tls._tcp",NULL,0,browse_callback,client))){
   fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
   goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,
  AVAHI_PROTO_UNSPEC,"_ipps._tcp",NULL,0,browse_callback,client))){
   fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
   goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,
  AVAHI_PROTO_UNSPEC,"_pdl-datastream._tcp",NULL,0,browse_callback,client))){
   fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
   goto fail;
  }
  if(!(sb=avahi_service_browser_new(client,AVAHI_IF_UNSPEC,
  AVAHI_PROTO_UNSPEC,"_printer._tcp",NULL,0,browse_callback,client))){
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
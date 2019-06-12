#include "framework-config.h"
#include <cups/file-private.h>
#include <cups/array.h>
#include "util.h"
#include "server.h"
#include <time.h>
#include <sys/wait.h>

static int num_devices = 0;

static device_t devices[MAX_DEVICES];
static cups_array_t *con_devices;

static int compare_devices(device_t *d0,device_t *d1);

static int get_devices();
static int parse_line(process_t*);
static int		add_device(const char *device_class,
				   const char *device_make_and_model,
				   const char *device_info,
				   const char *device_uri,
				   const char *device_id,
				   const char *device_location);

static void DEBUG(char* x)
{
    static int counter =0;
    fprintf(stderr,"DEBUG[%d]: %s\n",counter++,x);
}

int main(int argc,char* argv[])
{
  con_devices = cupsArrayNew((cups_array_func_t)compare_devices,NULL);
  get_devices();
  printf("Size: %d\n",cupsArrayCount(con_devices));
  int n = (int)(cupsArrayCount(con_devices));
  for(int i=0;i<n;i++)
  {
    device_t *temp = cupsArrayIndex(con_devices,i);
    fprintf(stdout,"%d %s %s\n",i,temp->device_uri,temp->device_make_and_model);
  }
  cupsArrayDelete(con_devices);
  return 0;
}
int delay(int t)
{
    for(int i=0;i<t;i++)
    {
        for(int j=0;j<100000000;j++);
    }
}
static int 
get_devices()

{
    const char  *serverbin; // ServerBin
    char        program[2048];     // Full Path to program
    char        *argv[7];
    char        name[32],reques_id[16],limit[16],
                timeout[16],user_id[16],options[1024];
    process_t   *process;
    int         process_pid,status;

    process = calloc(1,sizeof(process_t));

    _cups_strcpy(name,"cups-deviced");
    _cups_strcpy(reques_id,"1");
    _cups_strcpy(limit,"1");
    _cups_strcpy(timeout,"100");
    _cups_strcpy(user_id,"1");
    _cups_strcpy(options,"\"\"");

    if((serverbin = getenv("SERVERBIN"))==NULL)
        serverbin = SERVERBIN;
    DEBUG(NULL);
    snprintf(program,sizeof(program),"%s/%s",serverbin,name);

    if(_cupsFileCheck(program,_CUPS_FILE_CHECK_PROGRAM,!geteuid(),
                        _cupsFileCheckFilter,NULL))
        return (-1);

    argv[0] = (char*) name;
    argv[1] = (char*) reques_id;
    argv[2] = (char*) limit;
    argv[3] = (char*) timeout;
    argv[4] = (char*) user_id;
    argv[5] = (char*) options;
    argv[6] = NULL;

    DEBUG(program);
    if((process->pipe = cupsdPipeCommand(&(process->pid),program,argv,
                            10))==NULL)
    {
        fprintf(stderr,"ERROR: Unable to execute!\n");
        delay(1000);
        return (-1);
    }
    if((process_pid = wait(&status))>0)
    {
        if(WIFEXITED(status))
        {
            do{
                parse_line(process);
            }
            while(_cupsFilePeekAhead(process->pipe,'\n'));
        }
    }
    free(process);
    return (0);
}
static int				/* O - 0 on success, -1 on error */
parse_line(process_t *backend)	/* I - Backend to read from */
{
  char	line[2048],			/* Line from backend */
	temp[2048],			/* Copy of line */
	*ptr,				/* Pointer into line */
	*dclass,			/* Device class */
	*uri,				/* Device URI */
	*make_model,			/* Make and model */
	*info,				/* Device info */
	*device_id,			/* 1284 device ID */
	*location;			/* Physical location */

if (cupsFileGets(backend->pipe, line, sizeof(line)))
  {
   /*
    * Each line is of the form:
    *
    *   class URI "make model" "name" ["1284 device ID"] ["location"]
    */

    strlcpy(temp, line, sizeof(temp));
    fprintf(stdout,"%s\n",line);
   /*
    * device-class
    */

    dclass = temp;

    for (ptr = temp; *ptr; ptr ++)
      if (isspace(*ptr & 255))
        break;

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

   /*
    * device-uri
    */

    if (!*ptr)
      goto error;

    for (uri = ptr; *ptr; ptr ++)
      if (isspace(*ptr & 255))
        break;

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

   /*
    * device-make-and-model
    */

    if (*ptr != '\"')
      goto error;

    for (ptr ++, make_model = ptr; *ptr && *ptr != '\"'; ptr ++)
    {
      if (*ptr == '\\' && ptr[1])
        _cups_strcpy(ptr, ptr + 1);
    }

    if (*ptr != '\"')
      goto error;

    for (*ptr++ = '\0'; isspace(*ptr & 255); *ptr++ = '\0');

   /*
    * device-info
    */

    if (*ptr != '\"')
      goto error;

    for (ptr ++, info = ptr; *ptr && *ptr != '\"'; ptr ++)
    {
      if (*ptr == '\\' && ptr[1])
        _cups_strcpy(ptr, ptr + 1);
    }

    if (*ptr != '\"')
      goto error;

    for (*ptr++ = '\0'; isspace(*ptr & 255); *ptr++ = '\0');

   /*
    * device-id
    */

    if (*ptr == '\"')
    {
      for (ptr ++, device_id = ptr; *ptr && *ptr != '\"'; ptr ++)
      {
	if (*ptr == '\\' && ptr[1])
	  _cups_strcpy(ptr, ptr + 1);
      }

      if (*ptr != '\"')
	goto error;

      for (*ptr++ = '\0'; isspace(*ptr & 255); *ptr++ = '\0');

     /*
      * device-location
      */

      if (*ptr == '\"')
      {
	for (ptr ++, location = ptr; *ptr && *ptr != '\"'; ptr ++)
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);
	}

	if (*ptr != '\"')
	  goto error;

	*ptr = '\0';
      }
      else
        location = NULL;
    }
    else
    {
      device_id = NULL;
      location  = NULL;
    }

   /*
    * Add the device to the array of available devices...
    */

    if (!add_device(dclass, make_model, info, uri, device_id, location))
      fprintf(stderr, "DEBUG: [cups-deviced] Found device \"%s\"...\n", uri);

    return (0);
  }

 /*
  * End of file...
  */

  cupsFileClose(backend->pipe);
  backend->pipe = NULL;

  return (-1);

 /*
  * Bad format; strip trailing newline and write an error message.
  */

  error:

  if (line[strlen(line) - 1] == '\n')
    line[strlen(line) - 1] = '\0';

  fprintf(stderr, "ERROR: [cups-deviced] Bad line from \"%s\": %s\n",
	  backend->name, line);
  return (0);
}

/*
 *  'compare_devices()' - Compare device uri.
 */
static int
compare_devices(device_t *d0,
                device_t *d1)
{
  int diff=  _cups_strcasecmp(d0->device_uri,d1->device_uri);
//  fprintf(stdout,"%s %s %d\n",d0->device_uri,d1->device_uri,diff);
  return diff;
}

static int
add_device(const char *device_class,
    const char *device_make_and_model,
    const char *device_info,
    const char *device_uri,
    const char *device_id,
    const char *device_location)
{
  device_t *device;
  if((device = calloc(1,sizeof(device_t)))==NULL)
  {
    fprintf(stderr,"Ran out of memory!\n");
    return -1;
  }

  strlcpy(device->device_uri,device_uri,sizeof(device->device_uri));
  strlcpy(device->device_class,device->device_class,sizeof(device->device_class));
  strlcpy(device->device_make_and_model,device->device_make_and_model,sizeof(device->device_make_and_model));
  strlcpy(device->device_info,device->device_info,sizeof(device->device_info));
  strlcpy(device->device_id,device->device_id,sizeof(device->device_id));
  strlcpy(device->device_location,device->device_location,sizeof(device->device_location));

  if(cupsArrayFind(con_devices,device))
    free(device);
  else
    cupsArrayAdd(con_devices,device); // Do we need device limit????
  
  return 0;
}
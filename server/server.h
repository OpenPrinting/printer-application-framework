
#define MAX_DEVICES 1000
#define DEVICED_REQ "1"
#define DEVICED_LIM "100"
#define DEVICED_TIM "500"
#define DEVICED_USE "1"
#define DEVICED_OPT "\"\""

typedef struct
{
    char name[1024];
    int pid, status;
    cups_file_t *pipe;
} process_t;

typedef struct
{
    char device_class[128],
    device_info[128],
    device_uri[1024],
    device_location[128],
    device_make_and_model[256],
    device_id[128];
    char ppd[128];
    int eve_pid;
} device_t;

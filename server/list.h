#include <cups/array.h>
typedef struct ppd_s{
    char name[2048];
    char uri[2048];
}ppd_t;

int compare_ppd(ppd_t* p0,ppd_t* p1);

cups_array_t* ppd_list;
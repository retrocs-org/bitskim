/* Host surrogate for FileSwitch's no-replace move. No RISC OS SWIs are run. */
#include <unistd.h>
int bsp_riscos_commit(const char *from,const char *to) {
    if(link(from,to)) return -1;
    return unlink(from);
}

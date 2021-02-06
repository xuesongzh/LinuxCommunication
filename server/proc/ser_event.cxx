#include <signal.h>

#include "ser_datastruct.h"
#include "ser_function.h"
#include "ser_logic_socket.h"

void ser_process_events_and_timers() {
    g_socket.ser_epoll_process_events(-1);  //-1表示卡住等待

    //以后添加

    return;
}

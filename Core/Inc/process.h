#ifndef INC_PROCESS_H_
#define INC_PROCESS_H_

#include "usart2.h"

void ProcessMsg(void);

// Responses & Handlers
void Version_Resp(CommPort_t port);
void Time_StoreData(CommPort_t port);
void Time_Resp(CommPort_t port);
void Dust_Resp(CommPort_t port);
void Dust_AsciiResp(CommPort_t port);

#endif /* INC_PROCESS_H_ */


#include <syslog.h>

#include "scope_server.h"
#include "scope_handlers.h"


void *handler_dummy(void *data);

void *handler_dummy(void *data)
{
	struct handler_data hdata = *(struct handler_data *)(data);

	syslog(LOG_INFO, "Handler: %s started (id = %d)", __func__, hdata.id);

	return NULL;
}


#ifndef SCOPE_HANDLER
	#define SCOPE_HANDLER(id, function, option) {id, function, option},
#elif
	#error "Something went wrong with handlers map!"
#endif //SCOPE_HANDLER

struct handler_data handlers[] = {
	#include "scope_handlers_map.h"
};
#define HANDLERS_CNT (sizeof(handlers)/sizeof(handlers[0]))

unsigned long int get_handlers_size(void)
{
	return  HANDLERS_CNT;
}

#undef SCOPE_HANDLER

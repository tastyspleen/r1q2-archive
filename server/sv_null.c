// sv_null.c -- this file can stub out the entire server system
// for pure net-only clients

typedef enum {false, true}	qboolean;

void SV_Init (void)
{
}

void SV_Shutdown (char *finalmsg, qboolean reconnect)
{
}

void SV_Frame (int msec)
{
}

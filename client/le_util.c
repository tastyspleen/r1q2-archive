//local entity management routines.

#include "client.h"

int	localent_count = 0;

cvar_t	*cl_lents;

void LE_RunEntity (localent_t *ent);
void V_AddEntity (entity_t *ent);

localent_t *Le_Alloc (void)
{
	int i;

	//r1: disable lents?
	if (!cl_lents->intvalue)
		return NULL;

	for (i = 0; i < MAX_LOCAL_ENTS; i++) {
		if (!cl_localents[i].inuse) {
			if (i+1 > localent_count)
				localent_count = i+1;
			memset (&cl_localents[i], 0, sizeof(localent_t));
			cl_localents[i].inuse = true;
			return &cl_localents[i];
		}
	}
	Com_Printf ("Le_Alloc: no free local entities!\n", LOG_CLIENT);
	return NULL;
}

void Le_Free (localent_t *lent)
{
	if (!lent->inuse) {
		Com_Printf ("Le_Free: freeing an unused entity.\n", LOG_CLIENT);
		return;
	}

	memset (lent, 0, sizeof(localent_t));
	return;
}

void Le_Reset (void)
{
	localent_count = 0;
	memset (&cl_localents, 0, MAX_LOCAL_ENTS * sizeof(localent_t));
}

void LE_RunLocalEnts (void)
{
	int i;

	for (i = 0; i < localent_count; i++) {
		if (cl_localents[i].inuse)
			LE_RunEntity (&cl_localents[i]);
	}
}

void CL_AddLocalEnts (void)
{
	int i;

	for (i = 0; i < localent_count; i++) {
		if (cl_localents[i].inuse)
			V_AddEntity (&cl_localents[i].ent);
	}
}

void LE_Init (void)
{
	cl_lents = Cvar_Get ("cl_lents", "0", 0);
}

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cvar.c -- dynamic variable tracking

#include "qcommon.h"

#include "redblack.h"

cvar_t			*cvar_vars;
struct rbtree	*cvartree;

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (char *s)
{
	if (strchr (s, '\\'))
		return false;
	if (strchr (s, '"'))
		return false;
	if (strchr (s, ';'))
		return false;
	return true;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	void	**data;

	//not inited yet
	if (!cvartree)
		return NULL;

	data = rbfind (var_name, cvartree);
	if (data)
	{
		var = *(cvar_t **)data;
		return var;
	}
	
	/*for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (var_name, var->name))
			return var;*/

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;

	assert (var_name != NULL);
	
	var = Cvar_FindVar (var_name);
	
	if (!var)
		return 0;

	return var->value;
}

char dateBuff[32];

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;

	assert (var_name != NULL);
	
	var = Cvar_FindVar (var_name);

	if (var)
		return var->string;

	if (!strcmp (var_name, "$timestamp"))
	{
		time_t now;
		time (&now);
		strftime(dateBuff, sizeof(dateBuff)-1, "%Y-%m-%d_%H%M", localtime((const time_t *)&now));
		return dateBuff;
	}
	else if (!strcmp (var_name, "$date"))
	{
		time_t now;
		time (&now);
		strftime(dateBuff, sizeof(dateBuff)-1, "%Y-%m-%d", localtime((const time_t *)&now));
		return dateBuff;
	}
	else if (!strcmp (var_name, "$time"))
	{
		time_t now;
		time (&now);
		strftime(dateBuff, sizeof(dateBuff)-1, "%H%M", localtime((const time_t *)&now));
		return dateBuff;
	}
	else if (!strcmp (var_name, "$random"))
	{
		return va ("%u", randomMT());
	}
	else if (!strcmp (var_name, "$inc"))
	{
		static unsigned int incNum = 0;
		return va ("%u", ++incNum);
	}

	return "";
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*cvar;
	int			len;

	assert (partial != NULL);
	
	len = strlen(partial);
	
	if (!len)
		return NULL;
		
	// check exact match (note, don't use rbtree since we want some kind of order here)
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strcmp (partial,cvar->name))
			return cvar->name;

	// check partial match
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strncmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}

cvar_t *Cvar_Add (char *var_name, char *var_value, int flags)
{
	cvar_t	*var;
	void	**data;

	var = Z_TagMalloc (sizeof(*var), TAGMALLOC_CVAR);
	var->name = CopyString (var_name, TAGMALLOC_CVAR);
	var->string = CopyString (var_value, TAGMALLOC_CVAR);
	var->modified = true;
	var->changed = NULL;

	var->value = atof (var->string);
	var->intvalue = var->value;
	var->flags = flags;

	//r1: fix 0 case
	if (!var->intvalue && var->value)
		var->intvalue = 1;

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;

	//r1: insert to btree
	data = rbsearch (var->name, cvartree);
	*data = var;

	return var;
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cvar_t * EXPORT Cvar_Get (char *var_name, char *var_value, int flags)
{
	cvar_t	*var;

	assert (var_name != NULL);
	assert (var_value != NULL);
	
	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_name))
		{
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}
	}

	var = Cvar_FindVar (var_name);
	if (var)
	{
		var->flags |= flags;
		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_value))
		{
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}
	}

	return Cvar_Add (var_name, var_value, flags);
}

/*
============
Cvar_GameGet
============
R1CH: Use as a wrapper to cvars requested by the mod. Can then apply filtering
to them such as disallowing serverinfo set by mod.
*/
cvar_t * EXPORT Cvar_GameGet (char *var_name, char *var_value, int flags)
{
	if (Cvar_VariableValue("sv_no_game_serverinfo"))
		flags &= ~CVAR_SERVERINFO;

	return Cvar_Get (var_name, var_value, flags);
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2 (char *var_name, char *value, qboolean force)
{
	cvar_t	*var;
	char *old_string;

	assert (var_name != NULL);
	assert (value != NULL);
	
	if (*var_name == '$' && !force)
	{
		Com_Printf ("%s is write protected.\n", var_name);
		return NULL;
	}

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, 0);
	}

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (value))
		{
			Com_Printf("invalid info cvar value\n");
			return var;
		}
	}

	if (!force)
	{
		if (var->flags & CVAR_NOSET && !Cvar_VariableValue ("developer"))
		{
			Com_Printf ("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
					return var;
				Z_Free (var->latched_string);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			if (Com_ServerState())
			{
				Com_Printf ("%s will be changed for next map.\n", var_name);
				var->latched_string = CopyString(value, TAGMALLOC_CVAR);
			}
			else
			{
				var->string = CopyString(value, TAGMALLOC_CVAR);
				var->value = atof (var->string);
				var->intvalue = var->value;

				//r1: fix 0 case
				if (!var->intvalue && var->value)
					var->intvalue = 1;

				if (!strcmp(var->name, "game"))
				{
					FS_SetGamedir (var->string);
					FS_ExecAutoexec ();
				}
			}
			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Z_Free (var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	old_string = var->string;
	var->string = CopyString(value, TAGMALLOC_CVAR);

	var->value = atof (var->string);
	var->intvalue = var->value;

	//r1: fix 0 case
	if (!var->intvalue && var->value)
		var->intvalue = 1;

	var->modified = true;

	if (var->changed)
		var->changed (var, old_string, value);

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity
	
	Z_Free (old_string);	// free the old value string

	return var;
}

/*
============
Cvar_ForceSet
============
*/
cvar_t * EXPORT Cvar_ForceSet (char *var_name, char *value)
{
	return Cvar_Set2 (var_name, value, true);
}

/*
============
Cvar_Set
============
*/
cvar_t * EXPORT Cvar_Set (char *var_name, char *value)
{
	return Cvar_Set2 (var_name, value, false);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (char *var_name, char *value, int flags)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, flags);
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity
	
	Z_Free (var->string);	// free the old value string
	
	var->string = CopyString(value, TAGMALLOC_CVAR);
	var->value = atof (var->string);
	var->intvalue = var->value;

	//r1: fix 0 case
	if (!var->intvalue && var->value)
		var->intvalue = 1;

	var->flags = flags;

	return var;
}

/*
============
Cvar_SetValue
============
*/
void EXPORT Cvar_SetValue (char *var_name, float value)
{
	char	val[32];

	if (value == (int)value)
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	else
		Com_sprintf (val, sizeof(val), "%f",value);
	Cvar_Set (var_name, val);
}


/*
============
Cvar_GetLatchedVars

Any variables with latched values will now be updated
============
*/
void Cvar_GetLatchedVars (void)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!var->latched_string)
			continue;

		Z_Free (var->string);

		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		var->intvalue = var->value;

		//r1: fix 0 case
		if (!var->intvalue && var->value)
			var->intvalue = 1;

		if (!strcmp(var->name, "game"))
		{
			FS_SetGamedir (var->string);
			FS_ExecAutoexec ();
		}
	}
}

//r1: check if any cvars are latched
int Cvar_GetNumLatchedVars (void)
{
	int latched = 0;
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!var->latched_string)
			continue;
		latched++;
	}
	return latched;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console
============
*/
void Cvar_Set_f (void)
{
	int		c;
	int		flags;

	c = Cmd_Argc();
	if (c != 3 && c != 4)
	{
		Com_Printf ("usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		if (!strcmp(Cmd_Argv(3), "u"))
			flags = CVAR_USERINFO;
		else if (!strcmp(Cmd_Argv(3), "s"))
			flags = CVAR_SERVERINFO;
		else
		{
			Com_Printf ("flags can only be 'u' or 's' ('%s' given)\n", Cmd_Argv(3));
			return;
		}
		Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), flags);
	}
	else
		Cvar_Set (Cmd_Argv(1), Cmd_Argv(2));
}

void Cvar_UnSet_f (void)
{
	cvar_t	*v;
	cvar_t	*last = cvar_vars;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("usage: unset <variable>\n");
		return;
	}

	for (v=cvar_vars ; v ; v=v->next)
	{
		if (!strcmp (Cmd_Argv(1), v->name))
		{
			last->next = v->next;
			Z_Free (v->name);
			Z_Free (v->string);
			if (v->latched_string)
				Z_Free (v->latched_string);
			Z_Free (v);
			Com_Printf ("Variable '%s' removed.\n", Cmd_Argv(1));
			return;
		}
		last = v;
	}

	Com_Printf ("Variable '%s' not found.\n", Cmd_Argv(1));
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (char *path)
{
	cvar_t	*var;
	char	buffer[1024];
	FILE	*f;

	f = fopen (path, "a");
	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			Com_sprintf (buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf (f, "%s", buffer);
		}
	}
	fclose (f);
}

/*
============
Cvar_List_f

============
*/
static int EXPORT cvarsort( const void *_a, const void *_b )
{
	const cvar_t	*a = (const cvar_t *)_a;
	const cvar_t	*b = (const cvar_t *)_b;

	return strcmp (a->name, b->name);
}

void Cvar_List_f (void)
{
	cvar_t	*var;
	int		i, j;
	int		len, num;
	cvar_t	*sortedList;
	int		argLen;

	argLen = strlen(Cmd_Argv(1));

	for (var = cvar_vars, i = 0; var ; var = var->next, i++);
	num = i;

	len = num * sizeof(cvar_t);
	sortedList = Z_TagMalloc (len, TAGMALLOC_CVAR);
	
	for (var = cvar_vars, i = 0; var ; var = var->next, i++)
	{
		sortedList[i] = *var;
	}

	qsort (sortedList, num, sizeof(sortedList[0]), (int (EXPORT *)(const void *, const void *))cvarsort);

	for (j = 0; j < num; j++)
	{
		var = &sortedList[j];
		if (argLen && Q_strncasecmp (var->name, Cmd_Argv(1), argLen))
			continue;

		if (!argLen)
		{
			if (var->flags & CVAR_ARCHIVE)
				Com_Printf ("*");
			else
				Com_Printf (" ");
			if (var->flags & CVAR_USERINFO)
				Com_Printf ("U");
			else
				Com_Printf (" ");
			if (var->flags & CVAR_SERVERINFO)
				Com_Printf ("S");
			else
				Com_Printf (" ");
			if (var->flags & CVAR_NOSET)
				Com_Printf ("-");
			else if (var->flags & CVAR_LATCH)
				Com_Printf ("L");
			else
				Com_Printf (" ");
			Com_Printf (" %s \"%s\"\n", var->name, var->string);
		}
		else
		{
			Com_Printf ("v %s\n", var->name, var->string);
		}
	}
	if (!argLen)
		Com_Printf ("%i cvars\n", i);

	Z_Free (sortedList);
}


qboolean userinfo_modified;


char	*Cvar_BitInfo (int bit)
{
	static char	info[MAX_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}
	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char	*Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char	*Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	cvartree = rbinit ((int (*)(const void *, const void *))strcmp, 0);

	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("unset", Cvar_UnSet_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);

	developer = Cvar_Get ("developer", "0", 0);
}

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

static struct rbtree	*cvartree;

//UGLY HACK for client locations
#ifdef DEDICATED_ONLY
static const char *CL_Get_Loc_There (void) { return ""; }
static const char *CL_Get_Loc_Here (void) { return ""; }
#else
const char *CL_Get_Loc_There (void);
const char *CL_Get_Loc_Here (void);
#endif

//Server metavars
const char *SV_GetClientID (void);
const char *SV_GetClientIP (void);
const char *SV_GetClientName (void);

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (const char *s)
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
cvar_t *Cvar_FindVar (const char *var_name)
{
	cvar_t		*var;
	const void	**data;

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
float Cvar_VariableValue (const char *var_name)
{
	const cvar_t	*var;

	Q_assert (var_name != NULL);
	
	var = Cvar_FindVar (var_name);
	
	if (!var)
		return 0;

	return var->value;
}

int Cvar_IntValue (const char *var_name)
{
	const cvar_t	*var;

	Q_assert (var_name != NULL);
	
	var = Cvar_FindVar (var_name);
	
	if (!var)
		return 0;

	return var->intvalue;
}

/*
============
Cvar_SetVar
============
*/
static const char *Cvar_GetMetaVar (const char *var_name)
{
	static char dateBuff[32];

	if (var_name[0] == '$')
	{
		/*const char	*varstring;

		varstring = Cvar_VariableString (var_name + 1);
		if (varstring[0])
			return varstring;*/

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
			static unsigned long incNum = 0;
			return va ("%lu", ++incNum);
		}
		else if (!strcmp (var_name, "$loc_here"))
		{
			//aiee
			return CL_Get_Loc_Here ();
		}
		else if (!strcmp (var_name, "$loc_there"))
		{
			//aiee
			return CL_Get_Loc_There ();
		}
		else if (!strcmp (var_name, "$client.id"))
		{
			return SV_GetClientID ();
		}
		else if (!strcmp (var_name, "$client.ip"))
		{
			return SV_GetClientIP ();
		}
		else if (!strcmp (var_name, "$client.name"))
		{
			return SV_GetClientName ();
		}
	}

	return NULL;
}

/*
============
Cvar_VariableString
============
*/
const char *Cvar_VariableString (const char *var_name)
{
	const char		*metavar;
	const cvar_t	*var;

	Q_assert (var_name != NULL);

	metavar = Cvar_GetMetaVar (var_name);
	if (metavar)
		return metavar;
	
	var = Cvar_FindVar (var_name);

	if (var)
		return var->string;

	return "";
}


/*
============
Cvar_CompleteVariable
============
*/
const char *Cvar_CompleteVariable (const char *partial)
{
	const cvar_t		*cvar;
	int			len;

	Q_assert (partial != NULL);
	
	len = (int)strlen(partial);
	
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

static cvar_t *Cvar_Add (const char *var_name, const char *var_value, int flags)
{
	cvar_t		*var;
	const void	**data;

	var = Z_TagMalloc (sizeof(cvar_t), TAGMALLOC_CVAR);

	var->name = CopyString (var_name, TAGMALLOC_CVAR);
	var->string = CopyString (var_value, TAGMALLOC_CVAR);
	var->modified = true;

	var->changed = NULL;
	var->latched_string = NULL;

	var->value = (float)atof (var->string);
	var->intvalue = (int)var->value;
	var->flags = flags;
	var->help = NULL;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;

	//r1: fix 0 case
	if (!var->intvalue && FLOAT_NE_ZERO(var->value))
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
cvar_t * EXPORT Cvar_Get (const char *var_name, const char *var_value, int flags)
{
	cvar_t	*var;

	Q_assert (var_name != NULL);
	
	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_name))
		{
			Com_Printf("invalid info cvar name\n", LOG_GENERAL);
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
			Com_Printf("invalid info cvar value\n", LOG_GENERAL);
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
cvar_t * EXPORT Cvar_GameGet (const char *var_name, const char *var_value, int flags)
{
	if (Cvar_IntValue("sv_no_game_serverinfo"))
		flags &= ~CVAR_SERVERINFO;

	return Cvar_Get (var_name, var_value, flags);
}

/*
============
Cvar_Set2
============
*/
static cvar_t *Cvar_Set2 (const char *var_name, const char *value, qboolean force)
{
	cvar_t	*var;
	char *old_string;

	Q_assert (var_name != NULL);
	Q_assert (value != NULL);
	
	if (var_name[0] == '$' && !force)
	{
		Com_Printf ("%s is write protected.\n", LOG_GENERAL, var_name);
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
			Com_Printf("invalid info cvar value\n", LOG_GENERAL);
			return var;
		}
	}

	if (!force)
	{
#ifdef _DEBUG
		if (var->flags & CVAR_NOSET && !Cvar_IntValue ("developer"))
#else
		if (var->flags & CVAR_NOSET)
#endif
		{
			Com_Printf ("%s is write protected.\n", LOG_GENERAL, var_name);
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
				Com_Printf ("%s will be changed for next map.\n", LOG_GENERAL, var_name);
				var->latched_string = CopyString(value, TAGMALLOC_CVAR);
			}
			else
			{
				//memleak fix, thanks Maniac-
				Z_Free (var->string);
				var->string = CopyString(value, TAGMALLOC_CVAR);
				var->value = (float)atof (var->string);
				var->intvalue = (int)var->value;

				//r1: fix 0 case
				if (!var->intvalue && FLOAT_NE_ZERO(var->value))
					var->intvalue = 1;

				if (!strcmp(var->name, "game"))
				{
					FS_SetGamedir (var->string);
#ifndef DEDICATED_ONLY
					if (!Cvar_IntValue ("dedicated"))
						FS_ExecConfig ("autoexec.cfg");
#endif
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

	var->value = (float)atof (var->string);
	var->intvalue = (int)var->value;

	//r1: fix 0 case
	if (!var->intvalue && FLOAT_NE_ZERO(var->value))
		var->intvalue = 1;

	var->modified = true;

	if (var->changed)
		var->changed (var, old_string, var->string);

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
cvar_t * EXPORT Cvar_ForceSet (const char *var_name, const char *value)
{
	return Cvar_Set2 (var_name, value, true);
}

/*
============
Cvar_Set
============
*/
cvar_t * EXPORT Cvar_Set (const char *var_name, const char *value)
{
	return Cvar_Set2 (var_name, value, false);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (const char *var_name, const char *value, int flags)
{
	char	*old_string;
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, flags);
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	old_string = var->string;

	var->string = CopyString(value, TAGMALLOC_CVAR);
	var->value = (float)atof (var->string);
	var->intvalue = (int)var->value;

	if (var->changed)
		var->changed (var, old_string, var->string);

	Z_Free (old_string);

	//r1: fix 0 case
	if (!var->intvalue && FLOAT_NE_ZERO(var->value))
		var->intvalue = 1;

	var->flags = flags;

	return var;
}

/*
============
Cvar_SetValue
============
*/
void EXPORT Cvar_SetValue (const char *var_name, float value)
{
	char	val[32];

	if (value == (int)value)
		Com_sprintf (val, sizeof(val), "%i", (int)value);
	else
		Com_sprintf (val, sizeof(val), "%g", value);
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
	char	*old_string;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!var->latched_string)
			continue;

		old_string = var->string;

		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = (float)atof(var->string);
		var->intvalue = (int)var->value;

		//r1: fix 0 case
		if (!var->intvalue && FLOAT_NE_ZERO(var->value))
			var->intvalue = 1;

		if (var->changed)
			var->changed (var, old_string, var->string);

		Z_Free (old_string);

		if (!strcmp(var->name, "game"))
		{
			FS_SetGamedir (var->string);
#ifndef DEDICATED_ONLY
			if (!Cvar_IntValue ("dedicated"))
				FS_ExecConfig ("autoexec.cfg");
#endif
		}
	}
}

//r1: check if any cvars are latched
int Cvar_GetNumLatchedVars (void)
{
	int latched = 0;
	const cvar_t	*var;

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
	const cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("\"%s\" is \"%s\"\n", LOG_GENERAL, v->name, v->string);
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
static void Cvar_Set_f (void)
{
	int		c;
	int		flags;

	c = Cmd_Argc();
	//if (c != 3 && c != 4)
	if (c < 3)
	{
		Com_Printf ("usage: set <variable> <value> [u / s] (line: set %s)\n", LOG_GENERAL, Cmd_Args());
		return;
	}

	//r1: fixed so that 'set variable some thing' results in variable -> "some thing"
	//so that set command can be used in aliases to set things with spaces without
	//requring quotes.

	if (!strcmp(Cmd_Argv(c-1), "u"))
		flags = CVAR_USERINFO;
	else if (!strcmp(Cmd_Argv(3), "s"))
		flags = CVAR_SERVERINFO;
	else
		flags = 0;


	if (flags)
	{
		/*char	string[2048];
		int		i;

		string[0] = 0;

		for (i=2 ; i<c-1 ; i++)
		{
			strcat (string, Cmd_Argv(i));
			if (i+1 != c-1)
				strcat (string, " ");
		}*/

		//note, we don't do the above to get the full string since userinfo vars
		//have to use same format as 3.20 (eg set undef $undef u should set undef "u")
		//for q2admin and other userinfo checking mods.
		Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), flags);
	}
	else
	{
		Cvar_Set (Cmd_Argv(1), Cmd_Args2(2));
	}
}


/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (const char *path)
{
	const cvar_t	*var;
	char	buffer[1024];
	FILE	*f;

	f = fopen (path, "a");
	if (f)
	{
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

static void Cvar_List_f (void)
{
	const cvar_t	*var;
	int		i, j;
	int		len, num;
	cvar_t	*sortedList;
	int		argLen;

	argLen = (int)strlen(Cmd_Argv(1));

	for (var = cvar_vars, i = 0; var ; var = var->next, i++);
	num = i;

	len = num * sizeof(cvar_t);
	sortedList = Z_TagMalloc (len, TAGMALLOC_CVAR);
	//sortedList = alloca(len);
	
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
				Com_Printf ("*", LOG_GENERAL);
			else
				Com_Printf (" ", LOG_GENERAL);
			if (var->flags & CVAR_USERINFO)
				Com_Printf ("U", LOG_GENERAL);
			else
				Com_Printf (" ", LOG_GENERAL);
			if (var->flags & CVAR_SERVERINFO)
				Com_Printf ("S", LOG_GENERAL);
			else
				Com_Printf (" ", LOG_GENERAL);
			if (var->flags & CVAR_NOSET)
				Com_Printf ("-", LOG_GENERAL);
			else if (var->flags & CVAR_LATCH)
				Com_Printf ("L", LOG_GENERAL);
			else
				Com_Printf (" ", LOG_GENERAL);
			Com_Printf (" %s \"%s\"\n", LOG_GENERAL, var->name, var->string);
		}
		else
		{
			Com_Printf ("v %s\n", LOG_GENERAL, var->name);
		}
	}
	if (!argLen)
		Com_Printf ("%i cvars\n", LOG_GENERAL, i);

	Z_Free (sortedList);
}


qboolean userinfo_modified;


static char *Cvar_BitInfo (int bit)
{
	static char		info[MAX_INFO_STRING];
	const cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}

	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char *Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char *Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

void Cvar_Help_f (void)
{
	cvar_t	*var;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: cvarhelp cvarname\n", LOG_GENERAL);
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var)
	{
		Com_Printf ("Cvar %s not found.\n", LOG_GENERAL, Cmd_Argv(1));
		return;
	}

	if (!var->help)
	{
		Com_Printf ("No help available for %s.\n", LOG_GENERAL, Cmd_Argv(1));
		return;
	}

	Com_Printf ("%s: %s", LOG_GENERAL, var->name, var->help);
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	cvartree = rbinit ((int (EXPORT *)(const void *, const void *))strcmp, 0);

	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("cvarhelp", Cvar_Help_f);

	developer = Cvar_Get ("developer", "0", 0);
}

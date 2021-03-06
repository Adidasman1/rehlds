/*
*
*    This program is free software; you can redistribute it and/or modify it
*    under the terms of the GNU General Public License as published by the
*    Free Software Foundation; either version 2 of the License, or (at
*    your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but
*    WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*    In addition, as a special exception, the author gives permission to
*    link the code of this program with the Half-Life Game Engine ("HL
*    Engine") and Modified Game Libraries ("MODs") developed by Valve,
*    L.L.C ("Valve").  You must obey the GNU General Public License in all
*    respects for all of the code used other than the HL Engine and MODs
*    from Valve.  If you modify this file, you may extend this exception
*    to your version of the file, but you are not obligated to do so.  If
*    you do not wish to do so, delete this exception statement from your
*    version.
*
*/

#include "precompiled.h"


texlumpinfo_t* lumpinfo; 
int nTexLumps; 
FILE* texfiles[128];
int nTexFiles;

unsigned char texgammatable[256];
texture_t * r_notexture_mip;

int nummiptex;
char miptex[512][64];



/*
 * Globals initialization
 */
#ifndef HOOK_ENGINE

cvar_t r_wadtextures = { "r_wadtextures", "0", 0, 0.0f, NULL };

#else //HOOK_ENGINE

cvar_t r_wadtextures;

#endif //HOOK_ENGINE

/* <c61e8> ../engine/textures.c:35 */
void SafeRead(FileHandle_t f, void *buffer, int count)
{
	if (FS_Read(buffer, count, 1, f) != count)
		Sys_Error("File read failure");
}

void CleanupName(char *in, char *out)
{
	int i = 0;
	while (in[i] && i < 16) {
		out[i] = toupper(in[i]);
		i++;
	}

	while (i < 16) {
		out[i++] = 0;
	}
}

/* <c616d> ../engine/textures.c:64 */
int lump_sorter(const void *lump1, const void *lump2)
{
	const texlumpinfo_t *plump1 = (const texlumpinfo_t *)lump1;
	const texlumpinfo_t *plump2 = (const texlumpinfo_t *)lump2;
	return Q_strcmp(plump1->lump.name, plump2->lump.name);
}

/* <c6153> ../engine/textures.c:72 */
void ForwardSlashes(char *pname)
{
	while (*pname) {
		if (*pname == '\\')
			*pname = '/';

		pname++;
	}
}

/* <c62da> ../engine/textures.c:87 */
qboolean TEX_InitFromWad(char *path)
{
	char *pszWadFile;
	FileHandle_t texfile;
	char szTmpPath[1024];
	char wadName[260];
	char wadPath[260];
	wadinfo_t header;

	Q_strncpy(szTmpPath, path, 1022);
	szTmpPath[1022] = 0;
	if (!Q_strchr(szTmpPath, ';'))
		Q_strcat(szTmpPath, ";");
	for (pszWadFile = strtok(szTmpPath, ";"); pszWadFile; pszWadFile = strtok(NULL, ";"))
	{
		ForwardSlashes(pszWadFile);
		COM_FileBase(pszWadFile, wadName);
		Q_snprintf(wadPath, 0x100u, "%s", wadName);
		COM_DefaultExtension(wadPath, ".wad");

		if (Q_strstr(wadName, "pldecal") || Q_strstr(wadName, "tempdecal"))
			continue;

#ifdef REHLDS_FIXES
		if (g_psv.active
		 && Q_stricmp(wadPath, "halflife.wad")
		 && Q_stricmp(wadPath, "xeno.wad")
		 && Q_stricmp(wadPath, "decals.wad"))
			PF_precache_generic_I(wadPath);
#endif // REHLDS_FIXES

		texfile = FS_Open(wadPath, "rb");
		texfiles[nTexFiles++] = texfile;
		if (!texfile)
			Sys_Error("WARNING: couldn't open %s\n", wadPath);

		Con_DPrintf("Using WAD File: %s\n", wadPath);
		SafeRead(texfile, &header, 12);
		if (Q_strncmp(header.identification, "WAD2", 4) && Q_strncmp(header.identification, "WAD3", 4))
			Sys_Error("TEX_InitFromWad: %s isn't a wadfile", wadPath);

		header.numlumps = LittleLong(header.numlumps);
		header.infotableofs = LittleLong(header.infotableofs);
		FS_Seek(texfile, header.infotableofs, FILESYSTEM_SEEK_HEAD);
		lumpinfo = (texlumpinfo_t *)Mem_Realloc(lumpinfo, sizeof(texlumpinfo_t) * (header.numlumps + nTexLumps));

		for (int i = 0; i < header.numlumps; i++, nTexLumps++)
		{
			SafeRead(texfile, &lumpinfo[nTexLumps], sizeof(lumpinfo_t));
			CleanupName(lumpinfo[nTexLumps].lump.name, lumpinfo[nTexLumps].lump.name);
			lumpinfo[nTexLumps].lump.filepos = LittleLong(lumpinfo[nTexLumps].lump.filepos);
			lumpinfo[nTexLumps].lump.disksize = LittleLong(lumpinfo[nTexLumps].lump.disksize);;
			lumpinfo[nTexLumps].iTexFile = nTexFiles - 1;
		}
		
	}
	qsort(lumpinfo, nTexLumps, sizeof(texlumpinfo_t), lump_sorter);
	return 1;
}

/* <c644f> ../engine/textures.c:178 */
void TEX_CleanupWadInfo(void)
{
	if (lumpinfo)
	{
		Mem_Free(lumpinfo);
		lumpinfo = 0;
	}

	for (int i = 0; i < nTexFiles; i++)
	{
		FS_Close(texfiles[i]);
		texfiles[i] = 0;
	}

	nTexLumps = 0;
	nTexFiles = 0;
}

/* <c6476> ../engine/textures.c:203 */
int TEX_LoadLump(char *name, unsigned char *dest)
{
	texlumpinfo_t *found;
	texlumpinfo_t key;

	CleanupName(name, key.lump.name);
	found = (texlumpinfo_t *)bsearch(&key, lumpinfo, nTexLumps, sizeof(texlumpinfo_t), lump_sorter);
	if (found)
	{
		FS_Seek(texfiles[found->iTexFile], found->lump.filepos, FILESYSTEM_SEEK_HEAD);
		SafeRead(texfiles[found->iTexFile], dest, found->lump.disksize);
		return found->lump.disksize;
	}
	else
	{
		Con_SafePrintf("WARNING: texture lump \"%s\" not found\n", name);
		return 0;
	}
}

/* <c653c> ../engine/textures.c:220 */
int FindMiptex(char *name)
{
	int i = 0;                                                        //   222
	for (i = 0; i < nummiptex; i++)
	{
		if (!Q_stricmp(name, miptex[i]))
			return i;
	}

	if (nummiptex == 512)
		Sys_Error("Exceeded MAX_MAP_TEXTURES");

	Q_strncpy(miptex[i], name, 63);
	miptex[i][63] = 0;
	++nummiptex;
	return i;
}

/* <c658a> ../engine/textures.c:241 */
void TEX_AddAnimatingTextures(void)
{
	char name[32];

	int base = nummiptex;
	for (int i = 0; i < base; i++)
	{
		if (miptex[i][0] != '+' && miptex[i][0] != '-')
			continue;


		Q_strncpy(name, miptex[i], 0x1Fu);
		name[31] = 0;

		for (int j = 0; j < 20; j++)
		{
			if (j >= 10)
				name[1] = j + 55;
			else
				name[1] = j + 48;

			for (int k = 0; k < nTexLumps; k++)
			{
				if (!Q_strcmp(name, lumpinfo[k].lump.name))
				{
					FindMiptex(name);
					break;
				}
			}
		}
	}


	if (nummiptex != base)
		Con_DPrintf("added %i texture frames\n", nummiptex - base);
}

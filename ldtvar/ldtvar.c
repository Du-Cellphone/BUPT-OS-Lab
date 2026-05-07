#include <stdio.h>
#include "apilib.h"

int g_value = 2026;

void HariMain(void)
{
	int local_value = 416;
	char s[120];
	int ds_base;
	int logical_g, logical_l;
	int physical_g, physical_l;
	int calc_g, calc_l;

	ds_base = api_getdsbase();
	logical_g = (int) &g_value;
	logical_l = (int) &local_value;
	physical_g = api_log2phy(logical_g);
	physical_l = api_log2phy(logical_l);
	calc_g = ds_base + logical_g;
	calc_l = ds_base + logical_l;

	api_putstr0("[LDT variable address demo]\n");
	sprintf(s, "DS base            : %08X\n", ds_base);
	api_putstr0(s);
	sprintf(s, "global logical addr: %08X\n", logical_g);
	api_putstr0(s);
	sprintf(s, "global physical    : %08X\n", physical_g);
	api_putstr0(s);
	sprintf(s, "check base+logical : %08X\n", calc_g);
	api_putstr0(s);

	sprintf(s, "local  logical addr: %08X\n", logical_l);
	api_putstr0(s);
	sprintf(s, "local  physical    : %08X\n", physical_l);
	api_putstr0(s);
	sprintf(s, "check base+logical : %08X\n", calc_l);
	api_putstr0(s);

	api_putstr0("formula: physical = segment_base + logical_offset\n");
	api_end();
}

#include <config.h>
#include <common.h>
#include <lcd.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SPLASH_SOURCE
#include <splash.h>

static struct splash_location splash_loc[] = {
	{
	.name = SPLASH_STORAGE_NAME,
	.storage = SPLASH_STORAGE_DEVICE,
	.flags = SPLASH_STORAGE_FLAGS,
	.devpart = SPLASH_STORAGE_DEVPART,
	},
};
#endif

#ifdef CONFIG_CMD_BMP
void draw_logo(void)
{
	int x = BMP_ALIGN_CENTER, y = BMP_ALIGN_CENTER;
	ulong addr = 0;
#ifdef CONFIG_SPLASH_SOURCE
	char *s, buff[64];
	int ret;

	s = env_get("splashimage");
	if (!s)
		return;

	if (!gd->video_bottom)
		return;

	addr = simple_strtoul(s, NULL, 16);

	/* load env_get("splashfile") */
	ret = splash_source_load(splash_loc,
			sizeof(splash_loc) / sizeof(struct splash_location));
	if (ret)
		return;

	sprintf(buff, "0x%lx", gd->video_bottom);
	env_set("fb_addr", buff);

	printf("splashimage: 0x%lx -> 0x%lx\n", addr, gd->video_bottom);
#endif
	if (!addr)
		return;

	bmp_display(addr, x, y);
}
#endif


#include <ccplib.h>

static CmdErr cmd_sys(FsContext *ctx, int argc, char **argv)
{
    (void)ctx;
    (void)argv;

    if (argc > 1)
        return cmderr_syntax(NULL);

    SysInfo si;
    if (sys_info(&si) != EOK)
        return cmderr_bdos(ctx->vol_id, EIO);

    strupr(si.platform);

    printf("CP/M Neo v%u.%u for %s\n",
           si.os_version >> 8, si.os_version & 0xFF, si.platform);
    printf("    Kernel   v%u.%u\n", si.kern_version >> 8, si.kern_version & 0xFF);
    printf("    CCP      v%u.%u\n", si.ccp_version >> 8, si.ccp_version & 0xFF);
    printf("    TPA      %uk\n", si.tpa);
    printf("    Disk     %uk\n", si.disk_size_kb);

    printf("    Vols     [");
    int mounted = 0;
    for (int v = 0; v < VOL_MAX; v++)
    {
        if (!si.vol_mounted[v])
            continue;

        printf("%s%c:", mounted ? ", " : "", 'A' + v);
        mounted = 1;
    }
    printf("]\n");

    return cmderr_ok();
}

int main(int argc, char **argv)
{
    return ccp_run_app(cmd_sys, argc, argv);
}
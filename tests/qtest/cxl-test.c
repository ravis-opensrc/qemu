/*
 * QTest testcase for CXL
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/bswap.h"
#include "libqtest-single.h"

#define QEMU_PXB_CMD \
    "-machine q35,cxl=on " \
    "-device pxb-cxl,id=cxl.0,bus=pcie.0,bus_nr=52 " \
    "-M cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.size=4G "

#define QEMU_2PXB_CMD \
    "-machine q35,cxl=on " \
    "-device pxb-cxl,id=cxl.0,bus=pcie.0,bus_nr=52 " \
    "-device pxb-cxl,id=cxl.1,bus=pcie.0,bus_nr=53 " \
    "-M cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.targets.1=cxl.1,cxl-fmw.0.size=4G "

#define QEMU_VIRT_2PXB_CMD \
    "-machine virt,cxl=on -cpu max " \
    "-device pxb-cxl,id=cxl.0,bus=pcie.0,bus_nr=52 " \
    "-device pxb-cxl,id=cxl.1,bus=pcie.0,bus_nr=53 " \
    "-M cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.targets.1=cxl.1,cxl-fmw.0.size=4G "

#define QEMU_RP \
    "-device cxl-rp,id=rp0,bus=cxl.0,chassis=0,slot=0 "

/* Dual ports on first pxb */
#define QEMU_2RP \
    "-device cxl-rp,id=rp0,bus=cxl.0,chassis=0,slot=0 " \
    "-device cxl-rp,id=rp1,bus=cxl.0,chassis=0,slot=1 "

/* Dual ports on each of the pxb instances */
#define QEMU_4RP \
    "-device cxl-rp,id=rp0,bus=cxl.0,chassis=0,slot=0 " \
    "-device cxl-rp,id=rp1,bus=cxl.0,chassis=0,slot=1 " \
    "-device cxl-rp,id=rp2,bus=cxl.1,chassis=0,slot=2 " \
    "-device cxl-rp,id=rp3,bus=cxl.1,chassis=0,slot=3 "

#define QEMU_T3D_DEPRECATED \
    "-object memory-backend-file,id=cxl-mem0,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa0,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp0,memdev=cxl-mem0,lsa=lsa0,id=cxl-pmem0 "

#define QEMU_T3D_PMEM \
    "-object memory-backend-file,id=cxl-mem0,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa0,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp0,persistent-memdev=cxl-mem0,lsa=lsa0,id=pmem0 "

#define QEMU_T3D_VMEM \
    "-object memory-backend-ram,id=cxl-mem0,size=256M " \
    "-device cxl-type3,bus=rp0,volatile-memdev=cxl-mem0,id=mem0 "

#define QEMU_T3D_VMEM_LSA \
    "-object memory-backend-ram,id=cxl-mem0,size=256M " \
    "-object memory-backend-file,id=lsa0,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp0,volatile-memdev=cxl-mem0,lsa=lsa0,id=mem0 "

#define QEMU_2T3D \
    "-object memory-backend-file,id=cxl-mem0,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa0,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp0,persistent-memdev=cxl-mem0,lsa=lsa0,id=pmem0 " \
    "-object memory-backend-file,id=cxl-mem1,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa1,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp1,persistent-memdev=cxl-mem1,lsa=lsa1,id=pmem1 "

#define QEMU_4T3D \
    "-object memory-backend-file,id=cxl-mem0,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa0,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp0,persistent-memdev=cxl-mem0,lsa=lsa0,id=pmem0 " \
    "-object memory-backend-file,id=cxl-mem1,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa1,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp1,persistent-memdev=cxl-mem1,lsa=lsa1,id=pmem1 " \
    "-object memory-backend-file,id=cxl-mem2,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa2,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp2,persistent-memdev=cxl-mem2,lsa=lsa2,id=pmem2 " \
    "-object memory-backend-file,id=cxl-mem3,mem-path=%s,size=256M " \
    "-object memory-backend-file,id=lsa3,mem-path=%s,size=256M " \
    "-device cxl-type3,bus=rp3,persistent-memdev=cxl-mem3,lsa=lsa3,id=pmem3 "

/* Single pxb + volatile type3 + one window; base for the CFMW tests. */
#define QEMU_CFMW \
    "-machine q35,cxl=on " \
    "-device pxb-cxl,id=cxl.0,bus=pcie.0,bus_nr=52 " \
    QEMU_RP QEMU_T3D_VMEM \
    "-M cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.size=4G"

static void cxl_basic_hb(void)
{
    qtest_start("-machine q35,cxl=on");
    qtest_end();
}

static void cxl_basic_pxb(void)
{
    qtest_start("-machine q35,cxl=on -device pxb-cxl,bus=pcie.0");
    qtest_end();
}

static void cxl_pxb_with_window(void)
{
    qtest_start(QEMU_PXB_CMD);
    qtest_end();
}

static void cxl_2pxb_with_window(void)
{
    qtest_start(QEMU_2PXB_CMD);
    qtest_end();
}

static void cxl_root_port(void)
{
    qtest_start(QEMU_PXB_CMD QEMU_RP);
    qtest_end();
}

static void cxl_2root_port(void)
{
    qtest_start(QEMU_PXB_CMD QEMU_2RP);
    qtest_end();
}

/*
 * CFMWS "Window Restrictions" (CXL r4.0 9.18.1.3) are configurable per
 * window; check that valid restriction sets are accepted.
 */
static void cxl_cfmw_default(void)
{
    /* No coherency flag advertises both HDM-D and HDM-H. */
    qtest_start(QEMU_CFMW);
    qtest_end();
}

static void cxl_cfmw_host_only_off(void)
{
    /* Disabling host-only leaves device-coherent (HDM-D). */
    qtest_start(QEMU_CFMW ",cxl-fmw.0.host-only=off");
    qtest_end();
}

static void cxl_cfmw_device_coherent_off(void)
{
    /* Disabling device-coherent leaves host-only (HDM-H). */
    qtest_start(QEMU_CFMW ",cxl-fmw.0.device-coherent=off");
    qtest_end();
}

static void cxl_cfmw_back_invalidate(void)
{
    /* Back-Invalidate implies device-coherent (HDM-DB). */
    qtest_start(QEMU_CFMW ",cxl-fmw.0.back-invalidate=on");
    qtest_end();
}

static void cxl_cfmw_fixed_config(void)
{
    qtest_start(QEMU_CFMW ",cxl-fmw.0.fixed-config=on");
    qtest_end();
}

static void cxl_cfmw_device_and_host(void)
{
    /*
     * device-coherent and host-only may both be advertised (the spec
     * permits multiple bits set); this must be accepted -- a regression
     * guard against reinstating a device/host-only mutual exclusion.
     */
    qtest_start(QEMU_CFMW
                ",cxl-fmw.0.device-coherent=on,cxl-fmw.0.host-only=on");
    qtest_end();
}

static void cxl_cfmw_volatile_only(void)
{
    qtest_start(QEMU_CFMW ",cxl-fmw.0.persistent=off");
    qtest_end();
}

static void cxl_cfmw_persistent_only(void)
{
    qtest_start(QEMU_CFMW ",cxl-fmw.0.volatile=off");
    qtest_end();
}

/* Two windows in one VM: a Back-Invalidate window and a host-only window. */
static void cxl_cfmw_two_windows(void)
{
    qtest_start("-machine q35,cxl=on "
                "-device pxb-cxl,id=cxl.0,bus=pcie.0,bus_nr=52 "
                "-device pxb-cxl,id=cxl.1,bus=pcie.0,bus_nr=53 "
                QEMU_RP QEMU_T3D_VMEM
                "-M cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.size=4G,"
                "cxl-fmw.0.back-invalidate=on,"
                "cxl-fmw.1.targets.0=cxl.1,cxl-fmw.1.size=4G");
    qtest_end();
}

#ifdef CONFIG_POSIX
static void cxl_t3d_deprecated(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);
    g_autofree const char *tmpfs = NULL;

    tmpfs = g_dir_make_tmp("cxl-test-XXXXXX", NULL);

    g_string_printf(cmdline, QEMU_PXB_CMD QEMU_RP QEMU_T3D_DEPRECATED,
                    tmpfs, tmpfs);

    qtest_start(cmdline->str);
    qtest_end();
    rmdir(tmpfs);
}

static void cxl_t3d_persistent(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);
    g_autofree const char *tmpfs = NULL;

    tmpfs = g_dir_make_tmp("cxl-test-XXXXXX", NULL);

    g_string_printf(cmdline, QEMU_PXB_CMD QEMU_RP QEMU_T3D_PMEM,
                    tmpfs, tmpfs);

    qtest_start(cmdline->str);
    qtest_end();
    rmdir(tmpfs);
}

static void cxl_t3d_volatile(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);

    g_string_printf(cmdline, QEMU_PXB_CMD QEMU_RP QEMU_T3D_VMEM);

    qtest_start(cmdline->str);
    qtest_end();
}

static void cxl_t3d_volatile_lsa(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);
    g_autofree const char *tmpfs = NULL;

    tmpfs = g_dir_make_tmp("cxl-test-XXXXXX", NULL);

    g_string_printf(cmdline, QEMU_PXB_CMD QEMU_RP QEMU_T3D_VMEM_LSA,
                    tmpfs);

    qtest_start(cmdline->str);
    qtest_end();
    rmdir(tmpfs);
}

static void cxl_1pxb_2rp_2t3d(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);
    g_autofree const char *tmpfs = NULL;

    tmpfs = g_dir_make_tmp("cxl-test-XXXXXX", NULL);

    g_string_printf(cmdline, QEMU_PXB_CMD QEMU_2RP QEMU_2T3D,
                    tmpfs, tmpfs, tmpfs, tmpfs);

    qtest_start(cmdline->str);
    qtest_end();
    rmdir(tmpfs);
}

static void cxl_2pxb_4rp_4t3d(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);
    g_autofree const char *tmpfs = NULL;

    tmpfs = g_dir_make_tmp("cxl-test-XXXXXX", NULL);

    g_string_printf(cmdline, QEMU_2PXB_CMD QEMU_4RP QEMU_4T3D,
                    tmpfs, tmpfs, tmpfs, tmpfs, tmpfs, tmpfs,
                    tmpfs, tmpfs);

    qtest_start(cmdline->str);
    qtest_end();
    rmdir(tmpfs);
}

static void cxl_virt_2pxb_4rp_4t3d(void)
{
    g_autoptr(GString) cmdline = g_string_new(NULL);
    g_autofree const char *tmpfs = NULL;

    tmpfs = g_dir_make_tmp("cxl-test-XXXXXX", NULL);

    g_string_printf(cmdline, QEMU_VIRT_2PXB_CMD QEMU_4RP QEMU_4T3D,
                    tmpfs, tmpfs, tmpfs, tmpfs, tmpfs, tmpfs,
                    tmpfs, tmpfs);

    qtest_start(cmdline->str);
    qtest_end();
    rmdir(tmpfs);
}

#define QEMU_T3D_DC \
    "-object memory-backend-ram,id=cxl-dc0,size=256M " \
    "-device cxl-type3,bus=rp0,volatile-dc-memdev=cxl-dc0,num-dc-regions=1," \
    "id=dcd0 "

static uint32_t cfg_addr(unsigned bus, unsigned dev, unsigned fn,
                         unsigned off)
{
    return 0x80000000 | bus << 16 | dev << 11 | fn << 8 | (off & 0xfc);
}

static uint16_t cfg_readw(QTestState *qts, unsigned bus, unsigned off)
{
    qtest_outl(qts, 0xcf8, cfg_addr(bus, 0, 0, off));
    return qtest_inw(qts, 0xcfc + (off & 2));
}

static void cfg_writel(QTestState *qts, unsigned bus, unsigned off,
                       uint32_t val)
{
    qtest_outl(qts, 0xcf8, cfg_addr(bus, 0, 0, off));
    qtest_outl(qts, 0xcfc, val);
}

#define MBOX_BAR        0xc0000000ULL
/* The device registers: the mailbox follows the capability array and status */
#define MBOX_REGS       (MBOX_BAR + 0x88)
#define MBOX_CTRL       (MBOX_REGS + 0x04)
#define MBOX_CMD        (MBOX_REGS + 0x08)
#define MBOX_STS        (MBOX_REGS + 0x10)
#define MBOX_PAYLOAD    (MBOX_REGS + 0x20)

#define RC_SUCCESS      0x0
#define RC_INVALID_INPUT 0x2
#define RC_INVALID_PA    0xf

/* Give the Type 3 device on bus 53, behind the root port, its register BAR */
static void t3_map_registers(QTestState *qts)
{
    /* Root port: bus numbers 52 -> 53, a memory window, memory enable */
    cfg_writel(qts, 52, 0x18, 0x00353534);
    cfg_writel(qts, 52, 0x20, 0xc000c000);
    cfg_writel(qts, 52, 0x04, 0x0006);
    g_assert_cmphex(cfg_readw(qts, 53, 0), !=, 0xffff);
    /* The device registers are the 64 bit BAR 2 */
    cfg_writel(qts, 53, 0x18, MBOX_BAR);
    cfg_writel(qts, 53, 0x1c, 0);
    cfg_writel(qts, 53, 0x04, 0x0006);
}

/* Run a mailbox command and return its return code */
static unsigned t3_mbox(QTestState *qts, unsigned set, unsigned cmd,
                        const void *pl, size_t len)
{
    gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;

    if (len) {
        qtest_memwrite(qts, MBOX_PAYLOAD, pl, len);
    }
    qtest_writeq(qts, MBOX_CMD, (uint64_t)len << 16 | set << 8 | cmd);
    qtest_writel(qts, MBOX_CTRL, 1);
    while (qtest_readl(qts, MBOX_CTRL) & 1) {
        g_assert_cmpint(g_get_monotonic_time(), <, deadline);
        g_usleep(1000);
    }
    return (qtest_readq(qts, MBOX_STS) >> 32) & 0xffff;
}

/* Add (4802h) or Release (4803h) Dynamic Capacity with one extent */
static unsigned dc_response(QTestState *qts, unsigned cmd, uint64_t dpa,
                            uint64_t len)
{
    uint8_t pl[8 + 24] = {};

    stl_le_p(pl, 1);
    stq_le_p(pl + 8, dpa);
    stq_le_p(pl + 16, len);
    return t3_mbox(qts, 0x48, cmd, pl, sizeof(pl));
}

static void dc_offer(QTestState *qts, uint64_t offset, uint64_t len)
{
    qtest_qmp_assert_success(qts, "{ 'execute': 'cxl-add-dynamic-capacity', "
                             "'arguments': { 'path': '/machine/peripheral/"
                             "dcd0', 'host-id': 0, "
                             "'selection-policy': 'prescriptive', "
                             "'region': 0, "
                             "'extents': [ { 'offset': %" PRIu64
                             ", 'len': %" PRIu64 " } ] } }", offset, len);
}

/* A response answers an offer: with none outstanding it is refused */
static void cxl_t3d_dc_no_offer(void)
{
    QTestState *qts = qtest_init(QEMU_PXB_CMD QEMU_RP QEMU_T3D_DC);
    uint8_t none[8] = {};

    t3_map_registers(qts);

    /* With or without entries: this used to abort QEMU */
    g_assert_cmpuint(t3_mbox(qts, 0x48, 0x02, none, sizeof(none)), ==,
                     RC_INVALID_INPUT);
    g_assert_cmpuint(dc_response(qts, 0x02, 0, 128 * MiB), ==,
                     RC_INVALID_INPUT);

    dc_offer(qts, 0, 128 * MiB);
    g_assert_cmpuint(dc_response(qts, 0x02, 0, 128 * MiB), ==, RC_SUCCESS);

    /* CXL r3.2 section 8.2.10.9.9.3 rejects an already accepted range. */
    dc_offer(qts, 128 * MiB, 128 * MiB);
    g_assert_cmpuint(dc_response(qts, 0x02, 0, 128 * MiB), ==,
                     RC_INVALID_PA);
    g_assert_cmpuint(dc_response(qts, 0x02, 128 * MiB, 128 * MiB), ==,
                     RC_SUCCESS);
    g_assert_cmpuint(t3_mbox(qts, 0x48, 0x02, none, sizeof(none)), ==,
                     RC_INVALID_INPUT);
    qtest_quit(qts);
}

/* Two fixed windows on the one host bridge */
#define QEMU_PXB_2FMW \
    "-machine q35,cxl=on " \
    "-device pxb-cxl,id=cxl.0,bus=pcie.0,bus_nr=52 " \
    "-M cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.size=1G," \
    "cxl-fmw.1.targets.0=cxl.0,cxl-fmw.1.size=1G "

/* The component registers of the Type 3 device: HDM decoder 0 */
#define COMP_BAR        (MBOX_BAR + 0x10000)
#define HDM_DEC0        (COMP_BAR + 0x1000 + 0x128 + 0x10)
#define HDM_DEC0_BASE   (HDM_DEC0 + 0x00)
#define HDM_DEC0_SIZE   (HDM_DEC0 + 0x08)
#define HDM_DEC0_CTRL   (HDM_DEC0 + 0x10)

/* Commit endpoint HDM decoder 0 for [base, base + size) */
static void t3_commit_decoder(QTestState *qts, uint64_t base, uint64_t size)
{
    cfg_writel(qts, 53, 0x10, COMP_BAR);
    cfg_writel(qts, 53, 0x14, 0);
    /* The component registers take 32 bit accesses only */
    qtest_writel(qts, HDM_DEC0_BASE, base);
    qtest_writel(qts, HDM_DEC0_BASE + 4, base >> 32);
    qtest_writel(qts, HDM_DEC0_SIZE, size);
    qtest_writel(qts, HDM_DEC0_SIZE + 4, size >> 32);
    qtest_writel(qts, HDM_DEC0_CTRL, 1 << 9);
    g_assert_cmphex(qtest_readl(qts, HDM_DEC0_CTRL) & (1 << 10), !=, 0);
}

/* The line of "info mtree" of the @n th (from 0) fixed memory window */
static const char *mtree_window_line(const char *mtree, unsigned n)
{
    const char *p = mtree;

    while ((p = strstr(p, "cxl-fixed-memory-region")) != NULL) {
        while (p > mtree && p[-1] != '\n') {
            p--;
        }
        if (n-- == 0) {
            return p;
        }
        p = strchr(p, '\n');
    }
    g_assert_not_reached();
}

static void cxl_t3d_direct_alias_window(void)
{
    QTestState *qts = qtest_init(QEMU_PXB_2FMW QEMU_RP QEMU_T3D_VMEM);
    g_autofree char *map = qtest_hmp(qts, "info mtree");
    const char *line = mtree_window_line(map, 0);
    uint64_t base = g_ascii_strtoull(line, NULL, 16);
    g_autofree char *after = NULL;
    const char *alias;

    t3_map_registers(qts);
    t3_commit_decoder(qts, base + 256 * MiB, 256 * MiB);

    after = qtest_hmp(qts, "info mtree");
    g_assert_null(strstr(after, "DETECTED OVERFLOW"));
    /* The alias is listed under the first window, before the second */
    alias = strstr(mtree_window_line(after, 0), "cxl-direct-mapping-alias-0");
    g_assert_nonnull(alias);
    g_assert_true(alias < mtree_window_line(after, 1));
    qtest_quit(qts);
}

/* Whether the memory map has the direct mapping alias of decoder 0 */
static bool has_direct_alias(QTestState *qts)
{
    g_autofree char *map = qtest_hmp(qts, "info mtree");

    return strstr(map, "cxl-direct-mapping-alias-0") != NULL;
}

/* A decoder that is uncommitted no longer maps its memory into the window */
static void cxl_t3d_direct_alias_uncommit(void)
{
    QTestState *qts = qtest_init(QEMU_PXB_CMD QEMU_RP QEMU_T3D_VMEM);
    g_autofree char *map = qtest_hmp(qts, "info mtree");
    uint64_t base = g_ascii_strtoull(mtree_window_line(map, 0), NULL, 16);

    t3_map_registers(qts);
    g_assert_false(has_direct_alias(qts));
    t3_commit_decoder(qts, base, 256 * MiB);
    g_assert_true(has_direct_alias(qts));

    qtest_writel(qts, HDM_DEC0_CTRL, 0);
    g_assert_false(has_direct_alias(qts));
    qtest_quit(qts);
}
#endif /* CONFIG_POSIX */

int main(int argc, char **argv)
{
    const char *arch = qtest_get_arch();

    g_test_init(&argc, &argv, NULL);
    if (strcmp(arch, "i386") == 0 || strcmp(arch, "x86_64") == 0) {
        qtest_add_func("/pci/cxl/basic_hostbridge", cxl_basic_hb);
        qtest_add_func("/pci/cxl/basic_pxb", cxl_basic_pxb);
        qtest_add_func("/pci/cxl/pxb_with_window", cxl_pxb_with_window);
        qtest_add_func("/pci/cxl/pxb_x2_with_window", cxl_2pxb_with_window);
        qtest_add_func("/pci/cxl/rp", cxl_root_port);
        qtest_add_func("/pci/cxl/rp_x2", cxl_2root_port);
        qtest_add_func("/pci/cxl/cfmw/default", cxl_cfmw_default);
        qtest_add_func("/pci/cxl/cfmw/host_only_off", cxl_cfmw_host_only_off);
        qtest_add_func("/pci/cxl/cfmw/device_coherent_off",
                       cxl_cfmw_device_coherent_off);
        qtest_add_func("/pci/cxl/cfmw/back_invalidate",
                       cxl_cfmw_back_invalidate);
        qtest_add_func("/pci/cxl/cfmw/fixed_config", cxl_cfmw_fixed_config);
        qtest_add_func("/pci/cxl/cfmw/device_and_host",
                       cxl_cfmw_device_and_host);
        qtest_add_func("/pci/cxl/cfmw/volatile_only", cxl_cfmw_volatile_only);
        qtest_add_func("/pci/cxl/cfmw/persistent_only",
                       cxl_cfmw_persistent_only);
        qtest_add_func("/pci/cxl/cfmw/two_windows", cxl_cfmw_two_windows);
#ifdef CONFIG_POSIX
        qtest_add_func("/pci/cxl/type3_device", cxl_t3d_deprecated);
        qtest_add_func("/pci/cxl/type3_device_pmem", cxl_t3d_persistent);
        qtest_add_func("/pci/cxl/type3_device_vmem", cxl_t3d_volatile);
        qtest_add_func("/pci/cxl/type3_device_vmem_lsa", cxl_t3d_volatile_lsa);
        qtest_add_func("/pci/cxl/rp_x2_type3_x2", cxl_1pxb_2rp_2t3d);
        qtest_add_func("/pci/cxl/pxb_x2_root_port_x4_type3_x4",
                       cxl_2pxb_4rp_4t3d);
        qtest_add_func("/pci/cxl/type3_device_dc_no_offer",
                       cxl_t3d_dc_no_offer);
        qtest_add_func("/pci/cxl/type3_device_direct_alias_window",
                       cxl_t3d_direct_alias_window);
        qtest_add_func("/pci/cxl/type3_device_direct_alias_uncommit",
                       cxl_t3d_direct_alias_uncommit);
#endif
    } else if (strcmp(arch, "aarch64") == 0) {
#ifdef CONFIG_POSIX
        qtest_add_func("/pci/cxl/virt/pxb_x2_root_port_x4_type3_x4",
                       cxl_virt_2pxb_4rp_4t3d);
#endif
    }

    return g_test_run();
}

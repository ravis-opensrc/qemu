/*
 * QTest testcase for CXL
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/bswap.h"
#include "qobject/qjson.h"
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
#define RC_RESOURCES_EXHAUSTED 0x1d

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

/* 1024 blocks of 2 MiB, twice what the device can track */
#define QEMU_T3D_DC_2G \
    "-object memory-backend-ram,id=cxl-dc0,size=2G " \
    "-device cxl-type3,bus=rp0,volatile-dc-memdev=cxl-dc0,num-dc-regions=1," \
    "id=dcd0 "

/* Offer @n extents of one block each, from @offset into region 0 */
static QDict *dc_offer_n_rsp(QTestState *qts, uint64_t offset, unsigned n)
{
    QList *ext = qlist_new();
    unsigned i;

    for (i = 0; i < n; i++) {
        qlist_append(ext, qdict_from_jsonf_nofail("{ 'offset': %" PRIu64
                                                  ", 'len': %" PRIu64 " }",
                                                  offset + i * 2 * MiB,
                                                  2 * MiB));
    }
    return qtest_qmp(qts, "{ 'execute': 'cxl-add-dynamic-capacity', "
                     "'arguments': { 'path': '/machine/peripheral/dcd0', "
                     "'host-id': 0, 'selection-policy': 'prescriptive', "
                     "'region': 0, 'extents': %p } }", ext);
}

static void dc_offer_n(QTestState *qts, uint64_t offset, unsigned n)
{
    QDict *rsp = dc_offer_n_rsp(qts, offset, n);

    g_assert_false(qdict_haskey(rsp, "error"));
    qobject_unref(rsp);
}

/* Add Dynamic Capacity Response (4802h) accepting @n blocks from @dpa */
static unsigned dc_response_n(QTestState *qts, uint64_t dpa, unsigned n)
{
    /* The payload holds 84 entries of 24 bytes after the 8 byte header */
    uint8_t pl[8 + 84 * 24] = {};
    unsigned i;

    g_assert_cmpuint(n, <=, 84);
    stl_le_p(pl, n);
    for (i = 0; i < n; i++) {
        stq_le_p(pl + 8 + i * 24, dpa + i * 2 * MiB);
        stq_le_p(pl + 16 + i * 24, 2 * MiB);
    }
    return t3_mbox(qts, 0x48, 0x02, pl, 8 + n * 24);
}

/*
 * The device tracks 512 extents. An offered extent was counted once when
 * it was queued and again when the response was checked, so a response
 * for the last offers was refused while the device tracked fewer than 512.
 */
static void cxl_t3d_dc_extent_limit(void)
{
    QTestState *qts = qtest_init(QEMU_PXB_CMD QEMU_RP QEMU_T3D_DC_2G);
    uint64_t dpa = 0;
    QDict *rsp;
    unsigned i;

    t3_map_registers(qts);

    /* 480 extents in six batches of 80: nothing near the limit yet */
    for (i = 0; i < 6; i++) {
        dc_offer_n(qts, dpa, 80);
        g_assert_cmpuint(dc_response_n(qts, dpa, 80), ==, RC_SUCCESS);
        dpa += 80 * 2 * MiB;
    }

    /* 32 more fill the device up, and the response must be accepted */
    dc_offer_n(qts, dpa, 32);
    g_assert_cmpuint(dc_response_n(qts, dpa, 32), ==, RC_SUCCESS);
    dpa += 32 * 2 * MiB;

    /* The 513th is not offered: the device could not take it */
    rsp = dc_offer_n_rsp(qts, dpa, 1);
    g_assert_true(qdict_haskey(rsp, "error"));
    qobject_unref(rsp);
    qtest_quit(qts);
}

/*
 * Get Event Records told the host there were more records when the log was
 * not empty, also when the response had all of them.
 */
static void cxl_t3d_event_more_records(void)
{
    QTestState *qts = qtest_init(QEMU_PXB_CMD QEMU_RP QEMU_T3D_VMEM);
    uint8_t log = 0; /* Informational Event Log */
    uint8_t out[32 + 15 * 0x80];
    int i;

    t3_map_registers(qts);

    for (i = 0; i < 3; i++) {
        qtest_qmp_assert_success(qts, "{ 'execute': 'cxl-inject-dram-event', "
                                 "'arguments': { 'path': '/machine/"
                                 "peripheral/mem0', 'log': 'informational', "
                                 "'flags': 1, 'dpa': %d, 'descriptor': 1, "
                                 "'type': 3, 'transaction-type': 192, "
                                 "'sub-type': 0 } }", i * 64);
    }

    g_assert_cmpuint(t3_mbox(qts, 0x01, 0x00, &log, 1), ==, RC_SUCCESS);
    qtest_memread(qts, MBOX_PAYLOAD, out, sizeof(out));
    g_assert_cmpuint(out[0], ==, 0); /* no More Event Records, no overflow */
    g_assert_cmpuint(out[20] | out[21] << 8, ==, 3);
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
        qtest_add_func("/pci/cxl/type3_device_dc_extent_limit",
                       cxl_t3d_dc_extent_limit);
        qtest_add_func("/pci/cxl/type3_device_event_more_records",
                       cxl_t3d_event_more_records);
#endif
    } else if (strcmp(arch, "aarch64") == 0) {
#ifdef CONFIG_POSIX
        qtest_add_func("/pci/cxl/virt/pxb_x2_root_port_x4_type3_x4",
                       cxl_virt_2pxb_4rp_4t3d);
#endif
    }

    return g_test_run();
}

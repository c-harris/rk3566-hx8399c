#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#include "panel-himax-hx8399c-fhdplus.h"
#define REGFLAG_DELAY			0xFFFC


struct hx8399c {
    struct device* dev;
    struct drm_panel panel;
    struct backlight_device* backlight;
    struct gpio_desc* reset_gpio;
    struct gpio_desc* enable_gpio;
    struct regulator* vcc;

    bool prepared;
    bool enabled;

    int error;
};


#define mipi_dsi_dcs_write_seq(ctx, seq...)                                         \
        ({                                                                     \
                const u8 d[] = {seq};                                          \
                BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
                                 "DCS sequence too big for stack");            \
                internal_mipi_dsi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
        })

#define mipi_dsi_dcs_write_seq_static(ctx, seq...)                                  \
        ({                                                                     \
                static const u8 d[] = {seq};                                   \
                internal_mipi_dsi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
        })

static inline struct hx8399c* panel_to_HX8399(struct drm_panel* panel)
{
    return container_of(panel, struct hx8399c, panel);
}

static void internal_mipi_dsi_dcs_write(struct hx8399c* ctx, const void* data, size_t len)
{
    struct mipi_dsi_device* dsi = to_mipi_dsi_device(ctx->dev);
    ssize_t ret;
    char* addr;

    if (ctx->error < 0)
        return;

    addr = (char*)data;
    if ((int)*addr < 0xB0)
        ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
    else
        ret = mipi_dsi_generic_write(dsi, data, len);
    if (ret < 0) {
        dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
        ctx->error = ret;
    }
}

#ifdef PANEL_SUPPORT_READBACK
static int hx8399c_dcs_read(struct hx8399c* ctx, u8 cmd, void* data, size_t len)
{
    struct mipi_dsi_device* dsi = to_mipi_dsi_device(ctx->dev);
    ssize_t ret;

    if (ctx->error < 0)
        return 0;

    ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
    if (ret < 0) {
        dev_err(ctx->dev, "error %zu reading dcs seq:(%#x)\n", ret, cmd);
        ctx->error = ret;
    }

    return ret;
}

static void hx8399c_panel_get_data(struct hx8399c* ctx)
{
    u8 buffer[3] = { 0 };
    static int ret;

    if (ret == 0) {
        ret = hx8399c_dcs_read(ctx, 0x0A, buffer, 1);
        dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
            ret, buffer[0] | (buffer[1] << 8));
    }
}
#endif

static void xf055fhd03_init_sequence(struct hx8399c* ctx)
{
    ctx->reset_gpio =
        devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
            __func__, PTR_ERR(ctx->reset_gpio));
        return;
    }
    //        gpiod_set_value(ctx->reset_gpio, 0);
    //        udelay(4);
    gpiod_set_value(ctx->reset_gpio, 1);
    mdelay(50);
    gpiod_set_value(ctx->reset_gpio, 0);
    mdelay(20);
    gpiod_set_value(ctx->reset_gpio, 1);
    mdelay(150);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    /* 6.3.7 SETEXTC: Set extension command (B9h)  Enable Extended Command Set Access*/
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETEXTC,
        0xff, 0x83, 0x99);

    /* 6.3.19 SETOFFSET (D2h)*/
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETOFFSET,
        0x77);

    /* 6.3.2 SETPOWER: Set power (B1h)	*/
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETPOWER,
        0x02, 0x04, 0x74, 0x94, 0x01, 0x32, 0x33, 0x11, 0x11, 0xAB, 0x4D, 0x56, 0x73, 0x02, 0x02);

    /* 6.3.3 SETDISP: Set display related register (B2h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETDISP,
        0x00, 0x80, 0x80, 0xAE, 0x05, 0x07, 0x5A, 0x11, 0x00, 0x00, 0x10, 0x1E, 0x70, 0x03, 0xD4);
    //b2 00 80 80 cc 05 07 5a 11 10 10 00 1e 70 03 d4

    /* 6.3.4 SETCYC: Set display waveform cycles (B4h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETCYC,
        0x00, 0xFF, 0x02, 0xC0, 0x02, 0xC0, 0x00, 0x00, 0x08, 0x00, 0x04, 0x06, 0x00, 0x32, 0x04, 0x0A, 0x08, 0x21, 0x03, 0x01, 0x00, 0x0F, 0xB8, 0x8B, 0x02, 0xC0, 0x02, 0xC0, 0x00, 0x00, 0x08, 0x00, 0x04, 0x06, 0x00, 0x32, 0x04, 0x0A, 0x08, 0x01, 0x00, 0x0F, 0xB8, 0x01);

    /* 6.3.20 SETGIP0: Set GIP Option0 (D3h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGIP0,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x10, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x05, 0x07, 0x00, 0x00, 0x00, 0x05, 0x40);


    msleep(5);


    /* 6.3.21 Set GIP Option1 (D5h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGIP1,
        0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x21, 0x20, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x2F, 0x2F, 0x30, 0x30, 0x31, 0x31, 0x18, 0x18, 0x18, 0x18);
    msleep(5);

    /* 6.3.22 Set GIP Option2 (D6h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGIP2,
        0x18, 0x18, 0x19, 0x19, 0x40, 0x40, 0x20, 0x21, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x2F, 0x2F, 0x30, 0x30, 0x31, 0x31, 0x40, 0x40,
        0x40, 0x40);
    msleep(5);


    /* 6.3.23 SETGIP3: Set GIP option3 (D8h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGIP3,
        0xA2, 0xAA, 0x02, 0xA0, 0xA2, 0xA8, 0x02, 0xA0, 0xB0, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00);


    /* 6.3.10 Set register bank (BDh) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETREGBANK,
        0x01);

    /* 6.3.23 SETGIP3: Set GIP option3 (D8h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGIP3,
        0xB0, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0xE2, 0xAA, 0x03, 0xF0, 0xE2, 0xAA, 0x03, 0xF0);

    /* 6.3.10 Set register bank (BDh) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETREGBANK,
        0x02);
    /* 6.3.23 SETGIP3: Set GIP option3 (D8h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGIP3,
        0xE2, 0xAA, 0x03, 0xF0, 0xE2, 0xAA, 0x03, 0xF0);

    /* 6.3.10 Set register bank (BDh) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETREGBANK,
        0x00);

    /* 6.3.5 SETVCOM: Set VCOM Voltage (B6h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETVCOM,
        0x87, 0x87);

    /* 6.3.28 SETGAMMA: Set gamma curve related setting (E0h) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETGAMMA,
        0x00, 0x1F, 0x24, 0x27, 0x5F, 0x63, 0x78, 0x72,
        0x7A, 0x86, 0x8C, 0x92, 0x98, 0x9F, 0xA7, 0xAA,
        0xB1, 0xB8, 0xBC, 0xC8, 0xBD, 0xBF, 0xC4, 0x6B,
        0x66, 0x72, 0x7D, 0x00, 0x1F, 0x24, 0x27, 0x5F,
        0x63, 0x78, 0x72, 0x7A, 0x86, 0x8C, 0x92, 0x98,
        0x9F, 0xA7, 0xAA, 0xB1, 0xB8, 0xBC, 0xC8, 0xBD,
        0xBF, 0xC4, 0x6B, 0x66, 0x72, 0x77);

    mdelay(5);

    mdelay(5);


    /* 6.3.17 SETPANEL: Set Panel Related Register (CCh) */
    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_SETPANEL,
        0x08);


    mipi_dsi_dcs_write_seq_static(ctx, HX8399_CMD_UNKNOWN2,
        0xFF, 0xF9);


    //mdelay(120);
    mipi_dsi_dcs_write_seq_static(ctx, 0x11, 0x00);
    mdelay(120);
    mipi_dsi_dcs_write_seq_static(ctx, 0x29, 0x00);
    mdelay(10);
}

static int HX8399_disable(struct drm_panel* panel)
{
    struct hx8399c* ctx = panel_to_HX8399(panel);

    if (!ctx->enabled)
        return 0;

    if (ctx->backlight) {
        ctx->backlight->props.power = FB_BLANK_POWERDOWN;
        backlight_update_status(ctx->backlight);
    }

    ctx->enabled = false;

    return 0;
}

static int HX8399_unprepare(struct drm_panel* panel)
{
    struct hx8399c* ctx = panel_to_HX8399(panel);

    dev_info(ctx->dev, "%s+\n", __func__);
    if (!ctx->prepared)
        return 0;
    //mipi_dsi_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x81, 0x00);
    mipi_dsi_dcs_write_seq_static(ctx, 0x28);
    msleep(50);
    mipi_dsi_dcs_write_seq_static(ctx, 0x10);
    msleep(150);

    ctx->error = 0;
    ctx->prepared = false;

    ctx->reset_gpio =
        devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
            __func__, PTR_ERR(ctx->reset_gpio));
        return PTR_ERR(ctx->reset_gpio);
    }
    gpiod_set_value(ctx->enable_gpio, 0);

    gpiod_set_value(ctx->reset_gpio, 0);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    regulator_disable(ctx->vcc);

    return 0;
}

static int HX8399_prepare(struct drm_panel* panel)
{
    struct hx8399c* ctx = panel_to_HX8399(panel);
    int ret;

    dev_info(ctx->dev, "%s+\n", __func__);
    if (ctx->prepared)
        return 0;

    gpiod_set_value_cansleep(ctx->reset_gpio, 1);

    ret = regulator_enable(ctx->vcc);
    if (ret) {
        dev_err(ctx->dev, "Failed to enable vcc supply: %d\n", ret);
        return ret;
    }
    gpiod_set_value(ctx->enable_gpio, 1);

    gpiod_set_value_cansleep(ctx->reset_gpio, 0);

    msleep(180);

    xf055fhd03_init_sequence(ctx);

    ret = ctx->error;
    if (ret < 0)
        HX8399_unprepare(panel);

    ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
    hx8399c_panel_get_data(ctx);
#endif
    dev_info(ctx->dev, "%s-\n", __func__);
    return ret;
}

static int HX8399_enable(struct drm_panel* panel)
{
    struct hx8399c* ctx = panel_to_HX8399(panel);
    dev_info(ctx->dev, "%s+\n", __func__);
    if (ctx->enabled)
        return 0;

    if (ctx->backlight) {
        ctx->backlight->props.power = FB_BLANK_UNBLANK;
        backlight_update_status(ctx->backlight);
    }

    ctx->enabled = true;
    dev_info(ctx->dev, "%s-\n", __func__);

    return 0;
}


static int HX8399_get_modes(struct drm_panel* panel,
    struct drm_connector* connector)
{
    struct drm_display_mode* mode;

    mode = drm_mode_duplicate(connector->dev, &default_mode);
    if (!mode) {
        dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
            default_mode.hdisplay, default_mode.vdisplay,
            drm_mode_vrefresh(&default_mode));
        return -ENOMEM;
    }

    drm_mode_set_name(mode);
    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(connector, mode);

    connector->display_info.width_mm = 68;
    connector->display_info.height_mm = 151;

    return 2;
}


static const struct drm_panel_funcs hx8399c_drm_funcs = {
        .disable = HX8399_disable,
        .unprepare = HX8399_unprepare,
        .prepare = HX8399_prepare,
        .enable = HX8399_enable,
        .get_modes = HX8399_get_modes,
};

static int hx8399c_probe(struct mipi_dsi_device* dsi)
{
    struct device* dev = &dsi->dev;
    struct hx8399c* ctx;
    struct device_node* backlight;
    int ret;
    struct device_node* dsi_node, * remote_node = NULL, * endpoint = NULL;

    dsi_node = of_get_parent(dev->of_node);
    if (dsi_node) {
        endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
        if (endpoint) {
            remote_node = of_graph_get_remote_port_parent(endpoint);
            if (!remote_node) {
                dev_info(dev, "No panel connected,skip probe hx8399c\n");
                return -ENODEV;
            }
            dev_info(dev, "device node name:%s\n", remote_node->name);
        }
    }
    if (remote_node != dev->of_node) {
        dev_info(dev, "%s+ skip probe due to not current hx8399c\n", __func__);
        return -ENODEV;
    }

    dev_info(dev, "%s+\n", __func__);
    ctx = devm_kzalloc(dev, sizeof(struct hx8399c), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    mipi_dsi_set_drvdata(dsi, ctx);

    ctx->dev = dev;
    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
        | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
        | MIPI_DSI_CLOCK_NON_CONTINUOUS;

    backlight = of_parse_phandle(dev->of_node, "backlight", 0);
    if (backlight) {
        ctx->backlight = of_find_backlight_by_node(backlight);
        of_node_put(backlight);

        if (!ctx->backlight)
            return -EPROBE_DEFER;
    }

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_err(dev, "%s: cannot get reset-gpios %ld\n",
            __func__, PTR_ERR(ctx->reset_gpio));
        return PTR_ERR(ctx->reset_gpio);
    }
    devm_gpiod_put(dev, ctx->reset_gpio);

    ctx->vcc = devm_regulator_get(dev, "vcc");
    if (IS_ERR(ctx->vcc))
        return dev_err_probe(dev, PTR_ERR(ctx->vcc),
            "Failed to request vcc regulator\n");


    ctx->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->enable_gpio)) {
        ret = PTR_ERR(ctx->enable_gpio);
        if (ret != -EPROBE_DEFER)
            dev_err(dev, "failed to get enable GPIO: %d\n", ret);
        return ret;
    }

    devm_gpiod_put(dev, ctx->enable_gpio);

    drm_panel_init(&ctx->panel, dev, &hx8399c_drm_funcs, DRM_MODE_CONNECTOR_DSI);

    drm_panel_add(&ctx->panel);

    ret = mipi_dsi_attach(dsi);
    if (ret < 0) {
        dev_err_probe(dev, ret, "mipi_dsi_attach failed\n");
        drm_panel_remove(&ctx->panel);
        return ret;
    }


    dev_info(ctx->dev, "%s-\n", __func__);
    return ret;
}

static int hx8399c_remove(struct mipi_dsi_device* dsi)
{
    struct hx8399c* ctx = mipi_dsi_get_drvdata(dsi);
    dev_info(ctx->dev, "%s+\n", __func__);
    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->panel);
    dev_info(ctx->dev, "%s-\n", __func__);

    return 0;
}

static const struct of_device_id hx8399c_of_match[] = {
        {.compatible = "xinsun,xf055fhd03", },
        { }
};

MODULE_DEVICE_TABLE(of, hx8399c_of_match);

static struct mipi_dsi_driver hx8399c_driver = {
        .probe = hx8399c_probe,
        .remove = hx8399c_remove,
        .driver = {
                .name = DRV_NAME,
                .owner = THIS_MODULE,
                .of_match_table = hx8399c_of_match,
        },
};

module_mipi_dsi_driver(hx8399c_driver);

MODULE_AUTHOR("Christopher Harris <c.harris@communicationinnovations.com.au>");
MODULE_DESCRIPTION("DRM driver for Himax HX8399C based MIPI DSI panels");
MODULE_LICENSE("GPL");

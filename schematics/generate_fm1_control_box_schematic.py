import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.lines import Line2D

LW = 1.4
FS_PIN = 11
FS_LABEL = 14
FS_TITLE = 22
FS_SUB = 12.5
FS_SMALL = 10.5
FS_NOTES = 12.5


def line(ax, x1, y1, x2, y2, lw=LW):
    ax.add_line(Line2D([x1, x2], [y1, y2], color='black', lw=lw, solid_capstyle='round'))


def dot(ax, x, y, r=0.06):
    ax.add_patch(patches.Circle((x, y), r, color='black', zorder=5))


def box(ax, x, y, w, h, label, sublabel=None):
    ax.add_patch(patches.Rectangle((x, y), w, h, fill=False, lw=1.6, edgecolor='black'))
    ax.text(x + w / 2, y + h + 0.3, label, ha='center', va='bottom', fontsize=FS_LABEL, fontweight='bold')
    if sublabel:
        ax.text(x + w / 2, y + h + 0.65, sublabel, ha='center', va='bottom', fontsize=FS_SMALL)


def pin_left(ax, box_x, y, stub_len, name):
    x1 = box_x - stub_len
    line(ax, x1, y, box_x, y)
    ax.text((x1 + box_x) / 2, y + 0.18, name, ha='center', va='bottom', fontsize=FS_PIN)
    return (x1, y)


def pin_right(ax, box_x, y, stub_len, name):
    x2 = box_x + stub_len
    line(ax, box_x, y, x2, y)
    ax.text((box_x + x2) / 2, y + 0.18, name, ha='center', va='bottom', fontsize=FS_PIN)
    return (x2, y)


def resistor_h(ax, x1, y, length, label):
    zz_len = length * 0.55
    lead = (length - zz_len) / 2
    x_start_zz = x1 + lead
    n = 6
    xs = [x_start_zz + i * (zz_len / n) for i in range(n + 1)]
    amp = 0.26
    ys = [y + (amp if i % 2 == 1 else -amp) for i in range(n + 1)]
    ys[0] = y
    ys[-1] = y
    line(ax, x1, y, x_start_zz, y)
    for i in range(n):
        line(ax, xs[i], ys[i], xs[i + 1], ys[i + 1])
    line(ax, x_start_zz + zz_len, y, x1 + length, y)
    ax.text(x1 + length / 2, y + 0.55, label, ha='center', va='bottom', fontsize=FS_PIN)
    return (x1 + length, y)


def ground_symbol(ax, x, y):
    line(ax, x, y, x, y - 0.35)
    widths = [0.5, 0.32, 0.14]
    for i, w in enumerate(widths):
        yy = y - 0.35 - i * 0.16
        line(ax, x - w / 2, yy, x + w / 2, yy)


def rail_tick(ax, x, y, label):
    # Short tick mark labeling a horizontal power/ground bus at its left end.
    line(ax, x, y - 0.22, x, y + 0.22, lw=1.8)
    ax.text(x - 0.3, y, label, ha='right', va='center', fontsize=FS_PIN, fontweight='bold')


def stack_group(top_y, n, spacing=1.0, margin=0.5):
    """n pins evenly spaced `spacing` apart, starting at top_y going down.
    Returns (list of pin y's, box_top, box_bottom) with `margin` clearance
    above the first pin and below the last."""
    ys = [top_y - i * spacing for i in range(n)]
    return ys, ys[0] + margin, ys[-1] - margin


# ===========================================================================
# FIGURE 1 — Schematic
#
# Right-hand column stacks three boxes (MIDI OUT, Rotary Encoder, TFT
# Display), each fed by straight horizontal wires from the ESP32-S3's right
# pins — every ESP32 pin's y EXACTLY matches its destination pin's y, so
# every signal wire is a flat horizontal line, nothing to misread. Groups
# are built with stack_group() so spacing/margins stay consistent and each
# lower box always clears the label text of the box above it.
# ===========================================================================
fig1, ax1 = plt.subplots(figsize=(20, 17))
ax1.set_xlim(-1, 29)
ax1.set_ylim(-14.5, 18)
ax1.set_aspect('equal')
ax1.axis('off')

ax1.text(0, 17.7, 'FM-1 Control Box (v2) — Schematic', fontsize=FS_TITLE, fontweight='bold', va='top')
ax1.text(0, 17.0, 'ESP32-S3 Mini  +  TRS Pedal In  +  Built-in Button  +  TRS MIDI Out (Type A)  +  Rotary Encoder  +  1.28" Round TFT  +  Mic (MAX9814, onboard)  +  SING Button', fontsize=FS_SUB, va='top', style='italic')

GND_Y = -6.0
V3_Y = -7.2
line(ax1, 0.5, GND_Y, 28.0, GND_Y, lw=1.8)
ground_symbol(ax1, 0.9, GND_Y)
line(ax1, 0.5, V3_Y, 28.0, V3_Y, lw=1.8)
rail_tick(ax1, 0.9, V3_Y, '3V3')

GROUP_GAP = 1.7  # vertical clearance between one box's bottom and the next box's top

# --- Right-column pin groups (top to bottom): MIDI (3), Encoder (5), TFT (8) ---
midi_ys, midi_top, midi_bottom = stack_group(15.0, 3)               # Tip, Ring, Sleeve
enc_ys, enc_top, enc_bottom = stack_group(midi_bottom - GROUP_GAP, 5)     # CLK, DT, SW, +, GND
tft_ys, tft_top, tft_bottom = stack_group(enc_bottom - GROUP_GAP, 8)      # CS,DC,RST,SCK,MOSI,VCC,BLK,GND

MIDI_TIP_Y, MIDI_RING_Y, MIDI_SLEEVE_Y = midi_ys
ENC_CLK_Y, ENC_DT_Y, ENC_SW_Y, ENC_VCC_Y, ENC_GND_Y = enc_ys
TFT_CS_Y, TFT_DC_Y, TFT_RST_Y, TFT_SCK_Y, TFT_MOSI_Y, TFT_VCC_Y, TFT_BLK_Y, TFT_GND_Y = tft_ys

# --- Pedal input jack (TRS) ---
PJ_X, PJ_Y, PJ_W, PJ_H = 1.0, 10.6, 2.8, 4.0
box(ax1, PJ_X, PJ_Y, PJ_W, PJ_H, 'Pedal IN', '3.5mm TRS jack')
pj_t = pin_right(ax1, PJ_X + PJ_W, 13.8, 0.9, 'Tip')
pj_r = pin_right(ax1, PJ_X + PJ_W, 12.5, 0.9, 'Ring')
pj_s = pin_right(ax1, PJ_X + PJ_W, 11.2, 0.9, 'Sleeve')

# Ring + Sleeve tied together, dropped to GND bus
line(ax1, pj_r[0], pj_r[1], pj_r[0], pj_s[1])
dot(ax1, pj_r[0], pj_r[1])
dot(ax1, pj_r[0], pj_s[1])
ground_symbol(ax1, pj_r[0], pj_s[1])

# --- ESP32-S3 Mini ---
EX, EW = 9.0, 4.4
EY = tft_ys[-1] - 0.5   # bottom margin below the lowest right-side pin (TFT GND)
E_TOP = midi_top        # top margin above the highest right-side pin (MIDI Tip)
EH = E_TOP - EY
box(ax1, EX, EY, EW, EH, 'ESP32-S3 Mini', '"Super Mini" / ESP32-S3FH4R2')
e_gpio5 = pin_left(ax1, EX, 13.8, 1.1, 'GPIO5')
e_gnd_l = pin_left(ax1, EX, 4.0, 1.1, 'GND')
e_gpio2 = pin_left(ax1, EX, 2.5, 1.1, 'GPIO2')
e_gpio1 = pin_left(ax1, EX, -0.5, 1.1, 'GPIO1 (ADC)')
e_gpio4 = pin_right(ax1, EX + EW, MIDI_TIP_Y, 1.1, 'GPIO4 (TX1)')
e_3v3 = pin_right(ax1, EX + EW, MIDI_RING_Y, 1.1, '3V3')
e_gnd_r = pin_right(ax1, EX + EW, MIDI_SLEEVE_Y, 1.1, 'GND')
e_clk = pin_right(ax1, EX + EW, ENC_CLK_Y, 1.1, 'GPIO6')
e_dt = pin_right(ax1, EX + EW, ENC_DT_Y, 1.1, 'GPIO7')
e_sw = pin_right(ax1, EX + EW, ENC_SW_Y, 1.1, 'GPIO15')
e_cs = pin_right(ax1, EX + EW, TFT_CS_Y, 1.1, 'GPIO10')
e_dc = pin_right(ax1, EX + EW, TFT_DC_Y, 1.1, 'GPIO11')
e_rst = pin_right(ax1, EX + EW, TFT_RST_Y, 1.1, 'GPIO12')
e_sck = pin_right(ax1, EX + EW, TFT_SCK_Y, 1.1, 'GPIO13')
e_mosi = pin_right(ax1, EX + EW, TFT_MOSI_Y, 1.1, 'GPIO14')
ax1.text(EX + EW / 2, EY + 1.2, 'INPUT_PULLUP on\nGPIO2, GPIO5, GPIO15\n\n3.3V logic throughout', ha='center', va='center', fontsize=FS_SMALL, style='italic', color='dimgray')

# Pedal Tip -> GPIO5 (straight wire, same y)
line(ax1, pj_t[0], pj_t[1], e_gpio5[0], e_gpio5[1])
dot(ax1, e_gpio5[0], e_gpio5[1])

# ESP32 left GND -> local ground (keeps the lower-left free for the mic and SING button)
ground_symbol(ax1, e_gnd_l[0], e_gnd_l[1])

# --- Built-in panel pushbutton (wired in parallel with the pedal jack) ---
BTN_X, BTN_Y, BTN_R = 3.6, 7.2, 0.32
ax1.add_patch(patches.Circle((BTN_X, BTN_Y), BTN_R, fill=False, lw=1.6))
ax1.text(BTN_X - BTN_R - 0.25, BTN_Y + 0.15, 'Built-in Button', ha='right', va='bottom', fontsize=FS_LABEL, fontweight='bold')
ax1.text(BTN_X - BTN_R - 0.25, BTN_Y - 0.05, '(panel momentary, normally-open)', ha='right', va='top', fontsize=FS_SMALL, style='italic', color='dimgray')

line(ax1, BTN_X, BTN_Y + 0.32, BTN_X, e_gpio5[1])
line(ax1, BTN_X, e_gpio5[1], e_gpio5[0], e_gpio5[1])
dot(ax1, BTN_X, e_gpio5[1])
ground_symbol(ax1, BTN_X, BTN_Y - 0.32)

# --- SING button (sing-on-key mode toggle) -> GPIO2, other leg to ground ---
SB_X, SB_R = 4.3, 0.32
ax1.add_patch(patches.Circle((SB_X, e_gpio2[1]), SB_R, fill=False, lw=1.6))
ax1.text(SB_X, e_gpio2[1] + SB_R + 0.2, 'SING Button', ha='center', va='bottom', fontsize=FS_LABEL, fontweight='bold')
ax1.text(SB_X, e_gpio2[1] + SB_R + 0.75, '(sing-on-key mode, momentary)', ha='center', va='bottom', fontsize=FS_SMALL, style='italic', color='dimgray')
line(ax1, SB_X + SB_R, e_gpio2[1], e_gpio2[0], e_gpio2[1])
line(ax1, SB_X - SB_R, e_gpio2[1], SB_X - 0.9, e_gpio2[1])
ground_symbol(ax1, SB_X - 0.9, e_gpio2[1])

# --- Mic: MAX9814 module with its own onboard electret mic -> GPIO1 ---
MX_X, MX_W, MX_TOP, MX_BOT = 3.2, 2.4, 0.0, -3.0
box(ax1, MX_X, MX_BOT, MX_W, MX_TOP - MX_BOT, 'MAX9814', 'mic amp module (AGC)')
# Onboard electret capsule, drawn as the usual mic symbol inside the module
MIC_CX, MIC_CY, MIC_R = MX_X + 0.75, -1.5, 0.38
ax1.add_patch(patches.Circle((MIC_CX, MIC_CY), MIC_R, fill=False, lw=1.4))
line(ax1, MIC_CX - MIC_R, MIC_CY - MIC_R, MIC_CX - MIC_R, MIC_CY + MIC_R, lw=1.8)
ax1.text(MIC_CX, MIC_CY - MIC_R - 0.2, 'onboard\nmic', ha='center', va='top', fontsize=FS_SMALL - 1)
m_out = pin_right(ax1, MX_X + MX_W, -0.5, 0.9, 'OUT')
m_vdd = pin_right(ax1, MX_X + MX_W, -1.5, 0.9, 'VDD')
m_gnd = pin_right(ax1, MX_X + MX_W, -2.5, 0.5, 'GND')
line(ax1, m_out[0], m_out[1], e_gpio1[0], e_gpio1[1])
line(ax1, m_vdd[0], m_vdd[1], m_vdd[0], V3_Y)
dot(ax1, m_vdd[0], V3_Y)
ground_symbol(ax1, m_gnd[0], m_gnd[1])

# --- MIDI OUT jack (TRS, Type A) — top-right band ---
MJ_X, MJ_W = 21.0, 2.8
MJ_Y, MJ_H = midi_bottom, midi_top - midi_bottom
box(ax1, MJ_X, MJ_Y, MJ_W, MJ_H, 'MIDI OUT', '3.5mm TRS jack (Type A)')
mj_t = pin_left(ax1, MJ_X, MIDI_TIP_Y, 0.9, 'Tip')
mj_r = pin_left(ax1, MJ_X, MIDI_RING_Y, 0.9, 'Ring')
mj_s = pin_left(ax1, MJ_X, MIDI_SLEEVE_Y, 0.9, 'Sleeve')

# GPIO4 -> R1 (220ohm) -> MIDI jack Tip (signal)
r1_end = resistor_h(ax1, e_gpio4[0], e_gpio4[1], mj_t[0] - e_gpio4[0], 'R1\n220Ω')
line(ax1, r1_end[0], r1_end[1], mj_t[0], mj_t[1])

# 3V3 -> R2 (220ohm) -> MIDI jack Ring (current source, reduced vs. a 5V circuit)
r2_end = resistor_h(ax1, e_3v3[0], e_3v3[1], mj_r[0] - e_3v3[0], 'R2\n220Ω')
line(ax1, r2_end[0], r2_end[1], mj_r[0], mj_r[1])

# GND -> MIDI jack Sleeve (direct wire, no resistor)
line(ax1, e_gnd_r[0], e_gnd_r[1], mj_s[0], mj_s[1])
dot(ax1, e_gnd_r[0], e_gnd_r[1])
line(ax1, e_gnd_r[0], e_gnd_r[1], e_gnd_r[0], GND_Y)

# 3V3 pin also taps the 3V3 bus, so the encoder/TFT can draw from the same rail
line(ax1, e_3v3[0], e_3v3[1], e_3v3[0], V3_Y)
dot(ax1, e_3v3[0], e_3v3[1])

# --- Rotary encoder module (KY-040) — mid-right band ---
KX, KW = 21.0, 2.8
KY_, KH = enc_bottom, enc_top - enc_bottom
box(ax1, KX, KY_, KW, KH, 'Rotary Encoder', 'KY-040 module (WayinTop etc.)')
k_clk = pin_left(ax1, KX, ENC_CLK_Y, 0.9, 'CLK')
k_dt = pin_left(ax1, KX, ENC_DT_Y, 0.9, 'DT')
k_sw = pin_left(ax1, KX, ENC_SW_Y, 0.9, 'SW')
k_vcc = pin_left(ax1, KX, ENC_VCC_Y, 0.9, '+')
k_gnd = pin_left(ax1, KX, ENC_GND_Y, 0.9, 'GND')

# CLK/DT/SW: straight wires, ESP pin y already matches encoder pin y exactly
line(ax1, e_clk[0], e_clk[1], k_clk[0], k_clk[1])
line(ax1, e_dt[0], e_dt[1], k_dt[0], k_dt[1])
line(ax1, e_sw[0], e_sw[1], k_sw[0], k_sw[1])

line(ax1, k_vcc[0], k_vcc[1], k_vcc[0], V3_Y)
dot(ax1, k_vcc[0], k_vcc[1])
line(ax1, k_gnd[0], k_gnd[1], k_gnd[0], GND_Y)
dot(ax1, k_gnd[0], k_gnd[1])

# --- 1.28" round TFT (GC9A01, SPI) — bottom-right band ---
TX_, TW = 21.0, 2.8
TY_, TH = tft_bottom, tft_top - tft_bottom
box(ax1, TX_, TY_, TW, TH, 'Round TFT', 'GC9A01, 1.28" 240x240 (D-FLIFE etc.)')
t_cs = pin_left(ax1, TX_, TFT_CS_Y, 0.9, 'CS')
t_dc = pin_left(ax1, TX_, TFT_DC_Y, 0.9, 'DC')
t_rst = pin_left(ax1, TX_, TFT_RST_Y, 0.9, 'RES')
t_sck = pin_left(ax1, TX_, TFT_SCK_Y, 0.9, 'SCL')
t_mosi = pin_left(ax1, TX_, TFT_MOSI_Y, 0.9, 'SDA')
t_vcc = pin_left(ax1, TX_, TFT_VCC_Y, 0.9, 'VCC')
t_blk = pin_left(ax1, TX_, TFT_BLK_Y, 0.9, 'BLK')
t_gnd = pin_left(ax1, TX_, TFT_GND_Y, 0.9, 'GND')

# CS/DC/RES/SCL/SDA: straight wires, ESP pin y already matches TFT pin y exactly
line(ax1, e_cs[0], e_cs[1], t_cs[0], t_cs[1])
line(ax1, e_dc[0], e_dc[1], t_dc[0], t_dc[1])
line(ax1, e_rst[0], e_rst[1], t_rst[0], t_rst[1])
line(ax1, e_sck[0], e_sck[1], t_sck[0], t_sck[1])
line(ax1, e_mosi[0], e_mosi[1], t_mosi[0], t_mosi[1])

# VCC + BLK both tie straight to the 3V3 rail (backlight always-on, no PWM dimming)
line(ax1, t_vcc[0], t_vcc[1], t_vcc[0], V3_Y)
dot(ax1, t_vcc[0], t_vcc[1])
line(ax1, t_blk[0], t_blk[1], t_blk[0], V3_Y)
dot(ax1, t_blk[0], t_blk[1])
line(ax1, t_gnd[0], t_gnd[1], t_gnd[0], GND_Y)
dot(ax1, t_gnd[0], t_gnd[1])

notes1 = (
    "Notes:\n"
    "• Built-in Button is wired in parallel with the pedal jack (same GPIO5/GND nodes) -- either one alone can trigger sustain, so an external pedal is optional\n"
    "• Pedal ring + sleeve are tied together so a plain mono (TS) pedal plug still grounds correctly in the TRS jack\n"
    "• GPIO5 uses INPUT_PULLUP -- the button or pedal only needs to short GPIO5 to GND when pressed (normally-open momentary)\n"
    "• Type A TRS MIDI: tip = signal, ring = +3V3 current source, sleeve = ground -- adapted from v1's 5V circuit, same 220Ω values; UNCONFIRMED on the\n"
    "  bench at 3.3V (about half the MIDI-spec opto drive current) -- try 33Ω/10kΩ or add a buffer stage if the FM-1 doesn't respond\n"
    "• GPIO4 (TX1) is a UART assigned in software (Serial1), not shared with the ESP32-S3's native USB-CDC -- less likely to need disconnecting before\n"
    "  flashing than v1's Arduino was, but disconnect it if uploads ever misbehave\n"
    "• If the FM-1 doesn't respond at all, it may expect Type B instead: swap Tip and Ring at the MIDI OUT jack (tip = GND, ring = signal, sleeve = GND)\n"
    "• Rotary Encoder (KY-040): turning it live-auditions presets with velocity-based acceleration (MIDI Program Change per step, needs FM-1 firmware\n"
    "  v14+; tap SW for LIVE/SILENT); a medium press sends a 32-voice SysEx bank dump moving the browsed preset to slot 1 (\"Assign\") -- see\n"
    "  fm1_control_box.ino header comment and the project README for what Assign does and does not do (it can't press the FM-1's own A/B/C/D\n"
    "  confirmation knob for you)\n"
    "• KY-040 module has onboard pull-ups on CLK/DT/SW already -- the sketch's own INPUT_PULLUP on GPIO15 (SW) is redundant but harmless\n"
    "• Round TFT: VCC and BLK (backlight) both tie straight to 3V3 -- module's listed \"Driving voltage: 3-5V\" makes this safe without a level shifter,\n"
    "  but confirm your specific module's spec sheet before assuming that; backlight is always-on, no PWM dimming built yet\n"
    "• Display fills with a color per FM-1 bank (A-D) so you recognize where you are while spinning through 128 presets, plus preset number/name,\n"
    "  LIVE/SILENT, sustain state, and an \"ASSIGN SENT, turn FM-1 Knob\" prompt after Assign. Hold SW 3s for WiFi mode: a phone page to load,\n"
    "  reorder and send your own .syx banks (none are included)\n"
    "• Mic: the MAX9814 module's own onboard electret mic, facing a hole in the front panel, away from the FM-1's speaker. GAIN unconnected = 60dB max\n"
    "  (tie GAIN to GND for 50dB if room noise registers). OUT sits at ~1.25V DC and must go to an ADC1 pin (GPIO1) -- ADC2 stops working with WiFi on\n"
    "• SING button toggles sing-on-key mode (tap) and cycles SONG/DRILL/FREE note lists (hold 0.6s)"
)
line(ax1, 0.5, V3_Y - 0.8, 28.0, V3_Y - 0.8, lw=0.8)
ax1.lines[-1].set_linestyle('dashed')
ax1.lines[-1].set_color('gray')
ax1.text(0, V3_Y - 1.3, notes1, fontsize=FS_NOTES, va='top', ha='left', family='sans-serif', linespacing=1.6)

plt.tight_layout()

# ===========================================================================
# FIGURE 2 — Physical layout / assembly diagram
# ===========================================================================
fig2, ax2 = plt.subplots(figsize=(15, 12))
ax2.set_xlim(-1, 20)
ax2.set_ylim(-3, 13.6)
ax2.set_aspect('equal')
ax2.axis('off')

ax2.text(0, 13.1, 'FM-1 Control Box (v2) — Layout / Assembly Diagram', fontsize=FS_TITLE - 3, fontweight='bold', va='top')
ax2.text(0, 12.45, 'Top-down view of enclosure: pedal jack, button, SING button (left panel), mic module (behind a front-panel hole), ESP32-S3 (center), MIDI out jack (right panel), encoder + round TFT (front panel)', fontsize=FS_SUB - 1, va='top', style='italic')

ENC_X, ENC_Y, ENC_W, ENC_H = 0.5, 1.0, 18.0, 10.2
ax2.add_patch(patches.FancyBboxPatch((ENC_X, ENC_Y), ENC_W, ENC_H,
                                      boxstyle="round,pad=0,rounding_size=0.25",
                                      fill=False, lw=2.0, edgecolor='black'))
ax2.text(ENC_X + ENC_W / 2, ENC_Y + ENC_H + 0.3, 'project box (top cover removed)', ha='center', fontsize=FS_SMALL, style='italic', color='dimgray')

# ESP32-S3 Mini footprint (small — Super Mini boards run roughly credit-card-sized down to ~1.2" x 1.1")
ESP_X, ESP_Y, ESP_W, ESP_H = 6.0, 4.0, 2.2, 2.6
ax2.add_patch(patches.Rectangle((ESP_X, ESP_Y), ESP_W, ESP_H, fill=True, facecolor='#eef2ff', edgecolor='black', lw=1.6))
ax2.text(ESP_X + ESP_W / 2, ESP_Y + ESP_H + 0.25, 'ESP32-S3 Mini', ha='center', fontsize=FS_LABEL, fontweight='bold')
ax2.add_patch(patches.Rectangle((ESP_X + 0.15, ESP_Y + ESP_H - 0.55), 0.9, 0.35, fill=True, facecolor='#c7d2fe', edgecolor='black', lw=1.0))
ax2.text(ESP_X + 0.15 + 0.45, ESP_Y + ESP_H - 0.375, 'USB-C', ha='center', va='center', fontsize=FS_SMALL - 1)

esp_gpio5 = (ESP_X, ESP_Y + 1.9)
esp_gnd_l = (ESP_X, ESP_Y + 1.3)
esp_tx = (ESP_X + ESP_W, ESP_Y + 2.2)
esp_v3 = (ESP_X + ESP_W, ESP_Y + 1.8)
esp_gnd_r = (ESP_X + ESP_W, ESP_Y + 1.4)
esp_clk = (ESP_X + ESP_W, ESP_Y + 1.0)
esp_dt = (ESP_X + ESP_W, ESP_Y + 0.6)
esp_sw = (ESP_X + ESP_W, ESP_Y + 0.2)
for (px, py), name, ha, dx in [
    (esp_gpio5, 'GPIO5', 'right', -0.15),
    (esp_gnd_l, 'GND', 'right', -0.15),
    (esp_tx, 'TX', 'left', 0.15),
    (esp_v3, '3V3', 'left', 0.15),
    (esp_gnd_r, 'GND', 'left', 0.15),
]:
    dot(ax2, px, py, r=0.07)
    ax2.text(px + dx, py, name, ha=ha, va='center', fontsize=FS_PIN - 1,
              bbox=dict(facecolor='white', edgecolor='none', pad=0.5))

# Pedal jack on left panel
PJACK = (ENC_X + 1.3, ESP_Y + 1.9)
ax2.add_patch(patches.Circle(PJACK, 0.35, fill=True, facecolor='#fef3c7', edgecolor='black', lw=1.6))
ax2.text(PJACK[0], PJACK[1] + 0.65, 'Pedal IN', ha='center', fontsize=FS_LABEL, fontweight='bold')
ax2.text(PJACK[0], PJACK[1] + 0.30, '3.5mm TRS', ha='center', fontsize=FS_SMALL)
ax2.text(PJACK[0], ENC_Y + 0.25, '(left panel,\nto external pedal)', ha='center', fontsize=FS_SMALL, style='italic', color='dimgray')

# Built-in button, left panel
BTN_R = 0.35
BUTTON = (ENC_X + 1.3, ESP_Y - 0.3)
ax2.add_patch(patches.Circle(BUTTON, BTN_R, fill=True, facecolor='#d1fae5', edgecolor='black', lw=1.6))
ax2.text(BUTTON[0], BUTTON[1] + BTN_R + 0.25, 'Button', ha='center', va='bottom', fontsize=FS_LABEL, fontweight='bold')
ax2.text(BUTTON[0], BUTTON[1] - BTN_R - 0.25, '(parallel with Pedal IN)', ha='center', va='top', fontsize=FS_SMALL, style='italic', color='dimgray')

# MIDI out jack, right panel
MJACK = (ENC_X + 15.5, ESP_Y + 2.1)
ax2.add_patch(patches.Circle(MJACK, 0.35, fill=True, facecolor='#fecaca', edgecolor='black', lw=1.6))
ax2.text(MJACK[0], MJACK[1] + 0.75, 'MIDI OUT', ha='center', fontsize=FS_LABEL, fontweight='bold')
ax2.text(MJACK[0], MJACK[1] + 0.55, '3.5mm TRS (Type A)', ha='center', fontsize=FS_SMALL)
ax2.text(MJACK[0], ENC_Y + 0.25, '(right panel,\ncable to FM-1 MIDI IN)', ha='center', fontsize=FS_SMALL, style='italic', color='dimgray')

# Small perfboard patch for the two resistors
PERF_X, PERF_Y, PERF_W, PERF_H = MJACK[0] - 2.6, ESP_Y + 1.5, 1.5, 1.4
ax2.add_patch(patches.Rectangle((PERF_X, PERF_Y), PERF_W, PERF_H, fill=True, facecolor='#f3f4f6', edgecolor='black', lw=1.2, linestyle='dashed'))
ax2.text(PERF_X + PERF_W / 2, PERF_Y + PERF_H / 2, 'R1, R2\n(220Ω)', ha='center', va='center', fontsize=FS_SMALL)

# Wires: pedal jack -> ESP32 GPIO5 / GND
line(ax2, PJACK[0] + 0.35, PJACK[1] + 0.15, esp_gpio5[0], esp_gpio5[1])
line(ax2, PJACK[0] + 0.35, PJACK[1] - 0.15, esp_gnd_l[0], esp_gnd_l[1])
ax2.text((PJACK[0] + esp_gpio5[0]) / 2, (PJACK[1] + 0.15 + esp_gpio5[1]) / 2 + 0.15, 'tip', ha='center', fontsize=FS_SMALL, color='dimgray')
ax2.text((PJACK[0] + esp_gnd_l[0]) / 2, (PJACK[1] - 0.15 + esp_gnd_l[1]) / 2 - 0.25, 'ring+sleeve', ha='center', fontsize=FS_SMALL, color='dimgray')

# Wires: built-in button -> same GPIO5/GND nodes (parallel with pedal jack)
line(ax2, BUTTON[0] + 0.35, BUTTON[1] + 0.15, esp_gpio5[0], esp_gpio5[1])
line(ax2, BUTTON[0] + 0.35, BUTTON[1] - 0.15, esp_gnd_l[0], esp_gnd_l[1])
dot(ax2, esp_gpio5[0], esp_gpio5[1], r=0.06)
dot(ax2, esp_gnd_l[0], esp_gnd_l[1], r=0.06)

# Wires: ESP32 -> perfboard -> MIDI jack
line(ax2, esp_tx[0], esp_tx[1], PERF_X, PERF_Y + PERF_H - 0.3)
line(ax2, esp_v3[0], esp_v3[1], PERF_X, PERF_Y + PERF_H - 0.9)
line(ax2, esp_gnd_r[0], esp_gnd_r[1], MJACK[0] - 0.35, MJACK[1] - 0.15)
line(ax2, PERF_X + PERF_W, PERF_Y + PERF_H - 0.3, MJACK[0] - 0.35, MJACK[1] + 0.15)
line(ax2, PERF_X + PERF_W, PERF_Y + PERF_H - 0.9, MJACK[0] - 0.35, MJACK[1])

# MAX9814 mic module (onboard mic behind a front-panel hole) -> ESP32 GPIO1
esp_gpio1 = (ESP_X, ESP_Y + 2.35)
esp_gpio2 = (ESP_X, ESP_Y + 0.7)
MICJ = (ENC_X + 1.3, ENC_Y + 7.6)
MAXM_X, MAXM_Y, MAXM_W, MAXM_H = MICJ[0] - 0.4, MICJ[1] - 0.45, 1.7, 0.9
ax2.add_patch(patches.Rectangle((MAXM_X, MAXM_Y), MAXM_W, MAXM_H, fill=True, facecolor='#f3e8ff', edgecolor='black', lw=1.2))
ax2.text(MAXM_X + MAXM_W / 2, MAXM_Y + MAXM_H / 2, 'MAX9814', ha='center', va='center', fontsize=FS_SMALL)
ax2.text(MAXM_X + MAXM_W / 2, MAXM_Y + MAXM_H + 0.45, 'Mic', ha='center', fontsize=FS_LABEL, fontweight='bold')
ax2.text(MAXM_X + MAXM_W / 2, MAXM_Y + MAXM_H + 0.1, '(faces a panel hole)', ha='center', fontsize=FS_SMALL - 1, style='italic', color='dimgray')
MIC_RUN_X = MAXM_X + MAXM_W + 0.5
line(ax2, MAXM_X + MAXM_W, MICJ[1], MIC_RUN_X, MICJ[1])
line(ax2, MIC_RUN_X, MICJ[1], MIC_RUN_X, esp_gpio1[1])
line(ax2, MIC_RUN_X, esp_gpio1[1], esp_gpio1[0], esp_gpio1[1])

# SING button (left panel) -> ESP32 GPIO2
SING = (ENC_X + 3.5, ESP_Y - 0.3)
ax2.add_patch(patches.Circle(SING, 0.35, fill=True, facecolor='#fde68a', edgecolor='black', lw=1.6))
ax2.text(SING[0], SING[1] + 0.6, 'SING', ha='center', va='bottom', fontsize=FS_LABEL, fontweight='bold')
line(ax2, SING[0] + 0.35, SING[1], esp_gpio2[0] - 0.9, SING[1])
line(ax2, esp_gpio2[0] - 0.9, SING[1], esp_gpio2[0] - 0.9, esp_gpio2[1])
line(ax2, esp_gpio2[0] - 0.9, esp_gpio2[1], esp_gpio2[0], esp_gpio2[1])
for (px, py), name in [(esp_gpio1, 'GPIO1'), (esp_gpio2, 'GPIO2')]:
    dot(ax2, px, py, r=0.07)
    ax2.text(px - 0.15, py, name, ha='right', va='center', fontsize=FS_PIN - 1, bbox=dict(facecolor='white', edgecolor='none', pad=0.5))

# Encoder, front panel — knob-accessible, placed above the ESP32
ENC2_W, ENC2_H = 1.7, 1.7
ENC2_XY = (ESP_X + ESP_W / 2 - ENC2_W / 2, ESP_Y + ESP_H + 1.4)
ax2.add_patch(patches.Circle((ENC2_XY[0] + ENC2_W / 2, ENC2_XY[1] + ENC2_H / 2), ENC2_W / 2, fill=True, facecolor='#bfdbfe', edgecolor='black', lw=1.6))
ax2.text(ENC2_XY[0] + ENC2_W / 2, ENC2_XY[1] + ENC2_H + 0.3, 'Rotary Encoder', ha='center', va='bottom', fontsize=FS_LABEL, fontweight='bold')
ax2.text(ENC2_XY[0] + ENC2_W / 2, ENC2_XY[1] + ENC2_H / 2, 'KY-040\n(knob thru\nfront panel)', ha='center', va='center', fontsize=FS_SMALL - 1)
line(ax2, esp_clk[0], esp_clk[1], ENC2_XY[0] - 0.6, esp_clk[1])
line(ax2, ENC2_XY[0] - 0.6, esp_clk[1], ENC2_XY[0] - 0.6, ENC2_XY[1] + ENC2_H / 2)
line(ax2, ENC2_XY[0] - 0.6, ENC2_XY[1] + ENC2_H / 2, ENC2_XY[0], ENC2_XY[1] + ENC2_H / 2)
ax2.text(ENC2_XY[0] - 0.65, ENC2_XY[1] + ENC2_H + 0.55, 'CLK/DT/SW/+/GND\n(5 wires, see schematic)', ha='right', va='center', fontsize=FS_SMALL - 1, style='italic', color='dimgray')

# Round TFT, front panel — placed to the right of the encoder, screen visible thru panel
TFT2_D = 1.9
TFT2_XY = (ENC2_XY[0] + ENC2_W + 3.4, ESP_Y + ESP_H + 1.5)
ax2.add_patch(patches.Circle((TFT2_XY[0] + TFT2_D / 2, TFT2_XY[1] + TFT2_D / 2), TFT2_D / 2, fill=True, facecolor='#111827', edgecolor='black', lw=1.6))
ax2.text(TFT2_XY[0] + TFT2_D / 2, TFT2_XY[1] + TFT2_D / 2, 'TFT', ha='center', va='center', fontsize=FS_SMALL, color='white')
ax2.text(TFT2_XY[0] + TFT2_D / 2, TFT2_XY[1] + TFT2_D + 0.25, '1.28" round GC9A01', ha='center', va='bottom', fontsize=FS_LABEL, fontweight='bold')
line(ax2, esp_dt[0], esp_dt[1], TFT2_XY[0] - 0.4, esp_dt[1])
line(ax2, TFT2_XY[0] - 0.4, esp_dt[1], TFT2_XY[0] - 0.4, TFT2_XY[1] + TFT2_D / 2)
line(ax2, TFT2_XY[0] - 0.4, TFT2_XY[1] + TFT2_D / 2, TFT2_XY[0], TFT2_XY[1] + TFT2_D / 2)
ax2.text(TFT2_XY[0] - 0.45, TFT2_XY[1] + TFT2_D / 2 + 0.2, 'CS/DC/RES/SCL/SDA/\nVCC/BLK/GND\n(8 wires, see schematic)', ha='right', va='center', fontsize=FS_SMALL - 1, style='italic', color='dimgray')

notes2 = (
    "Notes:\n"
    "• Resistors R1/R2 can live on a small offcut of perfboard, or be soldered directly in-line on the wire runs -- exact placement isn't critical\n"
    "• See the schematic for the electrical connections (which wire goes through which resistor, which pin is which) -- this diagram is for physical placement only\n"
    "• Button and Pedal IN jack are wired in parallel to the same GPIO5/GND nodes -- either one alone triggers sustain, so the pedal can be left unplugged\n"
    "• Encoder knob and TFT window both need cutouts in the front panel -- mock up the panel layout before drilling, this diagram isn't to physical scale\n"
    "• Leave slack on the USB-C cable path -- the board still needs to be reachable for re-flashing\n"
    "• v1's pitch-strip input was dropped in this rebuild (see project README History) -- no strip or its wiring here"
)
ax2.text(0, ENC_Y - 0.5, notes2, fontsize=FS_NOTES - 1, va='top', ha='left', family='sans-serif', linespacing=1.6)

plt.tight_layout()

# ===========================================================================
# Save
# ===========================================================================
import os
OUT_DIR = os.path.dirname(os.path.abspath(__file__))
fig1.savefig(os.path.join(OUT_DIR, 'fm1_control_box_schematic.pdf'))
fig1.savefig(os.path.join(OUT_DIR, 'fm1_control_box_schematic.png'), dpi=160)
fig1.savefig(os.path.join(OUT_DIR, 'fm1_control_box_schematic.svg'))
fig2.savefig(os.path.join(OUT_DIR, 'fm1_control_box_layout.pdf'))
fig2.savefig(os.path.join(OUT_DIR, 'fm1_control_box_layout.png'), dpi=160)
fig2.savefig(os.path.join(OUT_DIR, 'fm1_control_box_layout.svg'))
print('saved schematic + layout to', OUT_DIR)

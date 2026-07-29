# ZMK ファーム実装ノート（rhyn47 / split-wireless）

`wireless-rhyn-design.md` を土台に、ZMK ファームを書くための調査結果と設計判断をまとめたもの。
次のセッションでこのノートだけを見れば実装に着手できる状態にすることが目的。**まだファーム本体のファイルは未作成**。

最終更新：2026-07-29 / ステータス：**方針確定・実装前**

---

## 0. 結論サマリ（先に要点だけ）

- **左右分割・完全無線**。4行6列オーソリニア × 左右、右手の左下1キーぶんがトラックボール。**総キー数 47**（左24 + 右23）。名前 `rhyn47` の 47 と一致。
- **右手 = セントラル（PC と BLE 接続する親側）／左手 = ペリフェラル**。
  - 理由：トラボが右手にある。トラボをセントラルに直結すれば **ポインタ入力を BLE 越しに転送しなくて済む**（分割リンクで一番重いのがポインタの高頻度データなので、これを渡さずに済むのは省電力・低遅延で有利）。
  - トレードオフ：USB ケーブルは右側に挿さる／親側（右）の消費がやや大きい。→ 許容。
- ターゲットボードは ZMK の **`xiao_ble`**（新 HWMv2 名）。XIAO nRF52840 **Plus** も同ボードで扱う（コアは同一 nRF52840、Plus は端子が増えただけの上位互換）。
- マトリクスは **`diode-direction = "col2row"`**、外部抵抗なし（内蔵プルを使用）。ROW=入力、COL=出力。
- トラボは **PMW3610**、SPI **3線**（SDIO を MOSI/MISO の同一ピンに割当）。外部モジュール `zmk-pmw3610-driver` を west.yml で取り込む。
- 電池監視は設計どおり **外部分圧（A0/AIN0, 1M/470k）** を使うため、ボード標準の `vbatt`（オンボード分圧 P0.31）を **上書き**する。

---

## 1. ターゲットボード

| 項目 | 値 |
|---|---|
| ZMK ボード名 | **`xiao_ble`**（旧名 `seeeduino_xiao_ble`。現行 main は `app/boards/seeed/xiao_ble/`）|
| 実チップ | nRF52840（XIAO nRF52840 Plus）|
| ZMK リビジョン | **main を推奨**（pointing サブシステム = `CONFIG_ZMK_POINTING` が upstream 済み）|

- ZMK 標準ボード `xiao_ble` は既に以下を定義済み（`xiao_ble_zmk.dts` で確認）：
  - `chosen { zmk,battery = &vbatt; }` … オンボード分圧（`io-channels = <&adc 7>`＝P0.31、`power-gpios = <&gpio0 14 ...>`、510k/1510k）。**本設計は別分圧を使うので後述で上書きする。**
  - `&adc { status = "okay"; }` … ADC は有効済み。
  - `&qspi` … 外部フラッシュ（p25q16h）有効。UF2 ブートローダ対応。
  - `&xiao_serial { status = "disabled"; }`。
  - `&spi0` / `&i2c0` は **この dts では明示有効化されていない**（→ トラボ用に `&spi0` を使える見込み。ただし Zephyr 側 base `xiao_ble.dts` の内容は §5 の「要確認」で最終確認する）。

---

## 2. リポジトリ構成（ZMK config リポジトリ形式）

`firmware/` 配下を ZMK の user config 形式で構成する。GitHub Actions でビルドする前提。

```
firmware/
├── build.yaml                        # ビルド対象（board × shield）
├── zephyr/
│   └── module.yml                    # このリポを Zephyr モジュールとして認識させる（shields を探す用）※任意だが確実
└── config/
    ├── west.yml                      # ZMK 本体 + pmw3610 ドライバの manifest
    ├── rhyn47.keymap                 # 左右共通のキーマップ（47キー）
    ├── rhyn47_left.conf              # 左（ペリフェラル）個別設定
    ├── rhyn47_right.conf             # 右（セントラル）個別設定
    └── boards/
        └── shields/
            └── rhyn47/
                ├── Kconfig.shield
                ├── Kconfig.defconfig       # SPLIT / セントラル役割 / キーボード名
                ├── rhyn47.dtsi             # 共通：kscan・matrix-transform・電池上書き
                ├── rhyn47_left.overlay     # 左：dtsi include のみ（col-offset=0）
                ├── rhyn47_right.overlay    # 右：col-offset=6 + トラボ(SPI/PMW3610)
                └── rhyn47.zmk.yml          # メタデータ（split: true など）
```

- **左右共通の shield 名 = `rhyn47`**、side は `rhyn47_left` / `rhyn47_right`。
- キーマップ `rhyn47.keymap` は左右共通（ZMK 標準）。conf は side ごと。
- `zephyr/module.yml` を置くと、config リポ自身の `boards/shields` を確実に拾える（近年の ZMK テンプレートの推奨）。

---

## 3. ピン割当（設計 §4 → ZMK gpio 参照）

XIAO Plus の端子を、生 `&gpio0` / `&gpio1` で参照する（`xiao_d` ネクサスは D0〜D10 までで、トラボの castellation ピンを含まないため、全部 raw 参照で統一するのが安全）。

| 論理 | XIAO | nRF | ZMK 参照 |
|---|---|---|---|
| ROW0 | D1 | P0.03 | `&gpio0 3` |
| ROW1 | D2 | P0.28 | `&gpio0 28` |
| ROW2 | D3 | P0.29 | `&gpio0 29` |
| ROW3 | D4 | P0.04 | `&gpio0 4` |
| COL0 | D5 | P0.05 | `&gpio0 5` |
| COL1 | D6 | P1.11 | `&gpio1 11` |
| COL2 | D7 | P1.12 | `&gpio1 12` |
| COL3 | D8 | P1.13 | `&gpio1 13` |
| COL4 | D9 | P1.14 | `&gpio1 14` |
| COL5 | D10 | P1.15 | `&gpio1 15` |
| INPUT_VOLTAGE（電池ADC）| D0 | P0.02 (AIN0) | `&adc` channel 0 |
| SCLK（トラボ）| — | P1.07 | pinctrl `SPIM_SCK, 1, 7` |
| SDIO（トラボ）| — | P1.05 | pinctrl `SPIM_MOSI/MISO, 1, 5`（3線で共用）|
| nCS（トラボ）| — | P1.03 | `cs-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>` |
| MOTION（トラボ）| — | P0.15 | `irq-gpios = <&gpio0 15 (GPIO_ACTIVE_LOW \| GPIO_PULL_UP)>` |

- **マトリクスのピンは左右で完全に同一**（XIAO＋ドーターは左右共通基板）。→ kscan は `rhyn47.dtsi` に1回だけ書けばよい。
- トラボ関連（SPI/PMW3610）は **右 overlay のみ**。左には書かない（左メイン基板にセンサーが無い＝クロックも通信も出ず電力の無駄なし。設計 §12.7）。

---

## 4. マトリクス（kscan）

`rhyn47.dtsi` に共通で置く。ZMK の `zmk,kscan-gpio-matrix`。

```dts
/ {
    chosen {
        zmk,kscan = &kscan0;
        zmk,matrix-transform = &default_transform;
    };

    kscan0: kscan {
        compatible = "zmk,kscan-gpio-matrix";
        diode-direction = "col2row";
        wakeup-source;

        row-gpios
            = <&gpio0  3 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>   // ROW0
            , <&gpio0 28 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>   // ROW1
            , <&gpio0 29 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>   // ROW2
            , <&gpio0  4 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>   // ROW3
            ;
        col-gpios
            = <&gpio0  5 GPIO_ACTIVE_HIGH>   // COL0
            , <&gpio1 11 GPIO_ACTIVE_HIGH>   // COL1
            , <&gpio1 12 GPIO_ACTIVE_HIGH>   // COL2
            , <&gpio1 13 GPIO_ACTIVE_HIGH>   // COL3
            , <&gpio1 14 GPIO_ACTIVE_HIGH>   // COL4
            , <&gpio1 15 GPIO_ACTIVE_HIGH>   // COL5
            ;
    };
};
```

### ⚠ プル方向の重要な注意（設計書「内蔵プルアップ」との差）
- 設計 §12.7 は「XIAO 内蔵**プルアップ**」と書いているが、**`diode-direction = "col2row"` の ZMK 標準構成では ROW 入力は内蔵プル<ins>ダウン</ins>（`GPIO_PULL_DOWN`）＋ COL 出力 `GPIO_ACTIVE_HIGH`** になる（corne 等の built-in shield と同一）。
- 理由：col2row はダイオードが COL→ROW 方向に導通する向き。COL を HIGH 駆動して ROW を HIGH 読みするので、アイドル時に ROW を LOW に落とす**プルダウン**が要る。プルアップ（COL を LOW 駆動して LOW 読み）はダイオードの向き的に成立しない（それは row2col）。
- **要確認**：実基板のダイオードの向き（カソードが ROW 側を向いているか）が ZMK col2row と一致していること。もし逆向きに実装されていたら、`diode-direction = "row2col"`＋`GPIO_PULL_UP`/`GPIO_ACTIVE_LOW` 側に合わせる。設計書の「プルアップ」表記は「外部抵抗なし＝内蔵プルを使う」の意味で読み、極性は ZMK の col2row 標準（プルダウン）に合わせるのが安全。**通電・実キー検証（ブリングアップ §11 の③）で確定。**

---

## 5. matrix-transform（左右合成・トラボ欠けを表現）

12列 × 4行の論理グリッドに、左＝列0〜5／右＝列6〜11 を割り当てる。右手の左下（トラボ位置）だけ穴を開ける。

### 仕組み（corne と同一・本家で確認済み）
- 共通の `default_transform` を `rhyn47.dtsi` に定義（`columns = <12>`, `rows = <4>`）。
- **右 overlay で `&default_transform { col-offset = <6>; };`** を上書き → 右の物理列0〜5 が論理列6〜11 に写る。
- 左 overlay は col-offset を触らない（＝0）。

### トラボ位置
- 設計：右手の **4行目1列目（bottom-left）** がトラボ。右手ブロックの「1列目（最も内側＝左手に近い側）」= 論理列6。行は最下段 = row3。
- → **`RC(3,6)` をマップから省く**。これで右は 23 キー。
- **要確認**：「右手1列目」が内側（col6）か外側（col11）かは、実際の列配線・基板の向きで最終確定する（レイアウト §16 未確定のため）。内側＝`RC(3,6)` 省略で暫定。

### transform 定義（`rhyn47.dtsi`）
```dts
#include <dt-bindings/zmk/matrix_transform.h>

/ {
    default_transform: keymap_transform_0 {
        compatible = "zmk,matrix-transform";
        columns = <12>;
        rows = <4>;
        //                      左手 (col 0-5)                        右手 (col 6-11)
        map = <
RC(0,0) RC(0,1) RC(0,2) RC(0,3) RC(0,4) RC(0,5)   RC(0,6) RC(0,7) RC(0,8) RC(0,9) RC(0,10) RC(0,11)
RC(1,0) RC(1,1) RC(1,2) RC(1,3) RC(1,4) RC(1,5)   RC(1,6) RC(1,7) RC(1,8) RC(1,9) RC(1,10) RC(1,11)
RC(2,0) RC(2,1) RC(2,2) RC(2,3) RC(2,4) RC(2,5)   RC(2,6) RC(2,7) RC(2,8) RC(2,9) RC(2,10) RC(2,11)
RC(3,0) RC(3,1) RC(3,2) RC(3,3) RC(3,4) RC(3,5)           RC(3,7) RC(3,8) RC(3,9) RC(3,10) RC(3,11)
        >;
        //                                         ↑ RC(3,6) を省略 = トラボ位置
    };
};
```

### キーマップのバインド順（超重要）
- `map` の並び順 = `rhyn47.keymap` の binding の並び順。
- 行ごとの個数：**row0=12, row1=12, row2=12, row3=11**。合計 **47**。
- row3 だけ 11 個（左6 + 右5）。キーマップを書くときは row3 の右側が1個少ないことに注意。

---

## 6. 分割ロール（右セントラル）

### Kconfig.defconfig（`boards/shields/rhyn47/Kconfig.defconfig`）
```kconfig
if SHIELD_RHYN47_LEFT

config ZMK_KEYBOARD_NAME
    default "rhyn47"

endif

if SHIELD_RHYN47_RIGHT

config ZMK_KEYBOARD_NAME
    default "rhyn47"

# 右手をセントラル（PC と接続する親）にする
config ZMK_SPLIT_ROLE_CENTRAL
    default y

endif

if SHIELD_RHYN47_LEFT || SHIELD_RHYN47_RIGHT

config ZMK_SPLIT
    default y

endif
```
- corne は左が central だが、**本設計は右 central**なので `ZMK_SPLIT_ROLE_CENTRAL default y` を **`SHIELD_RHYN47_RIGHT` 側**に置く。

### Kconfig.shield（`boards/shields/rhyn47/Kconfig.shield`）
```kconfig
config SHIELD_RHYN47_LEFT
    def_bool $(shields_list_contains,rhyn47_left)

config SHIELD_RHYN47_RIGHT
    def_bool $(shields_list_contains,rhyn47_right)
```

### rhyn47.zmk.yml（メタデータ）
```yaml
file_format: "1"
id: rhyn47
name: rhyn47
type: shield
url: ""
requires: [xiao_ble]
features:
  - keys
  - pointer
siblings:
  - rhyn47_left
  - rhyn47_right
```

---

## 7. トラックボール（PMW3610・右 = セントラルに直結）

**右がセントラルなので、`zmk,input-split` によるペリフェラル→セントラル転送は不要。** 右 overlay に SPI + PMW3610 + `zmk,input-listener` を直に書けばよい。

### 7-1. west.yml にドライバモジュールを追加（`config/west.yml`）
```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: badjeff
      url-base: https://github.com/badjeff
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-pmw3610-driver
      remote: badjeff
      revision: main
  self:
    path: config
```
- ドライバ候補：**badjeff/zmk-pmw3610-driver**（`compatible = "pixart,pmw3610-alt"`、`CONFIG_PMW3610_ALT`、upstream の `CONFIG_ZMK_POINTING` 対応）を第一候補にする。
- 代替：`inorichi/zmk-pmw3610-driver`（`pixart,pmw3610` / `CONFIG_PMW3610`）、`AntoineGS/...`、`sayu-hub/...`。**採用フォークによって compatible 文字列と CONFIG 名が変わる**ので、README を最終確認してから合わせる。

### 7-2. 右 overlay の SPI / センサー定義（`rhyn47_right.overlay`）
```dts
#include "rhyn47.dtsi"
#include <dt-bindings/zmk/pointing.h>

&default_transform {
    col-offset = <6>;   // 右手を論理列 6-11 へ
};

// --- 3線SPI: SDIO を MOSI/MISO の同一ピン(P1.05)に割当 ---
&pinctrl {
    spi0_default: spi0_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK,  1, 7)>,   // SCLK = P1.07
                    <NRF_PSEL(SPIM_MOSI, 1, 5)>,   // SDIO = P1.05
                    <NRF_PSEL(SPIM_MISO, 1, 5)>;   //  〃  （同一ピン）
        };
    };
    spi0_sleep: spi0_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK,  1, 7)>,
                    <NRF_PSEL(SPIM_MOSI, 1, 5)>,
                    <NRF_PSEL(SPIM_MISO, 1, 5)>;
            low-power-enable;
        };
    };
};

&spi0 {
    status = "okay";
    compatible = "nordic,nrf-spim";
    pinctrl-0 = <&spi0_default>;
    pinctrl-1 = <&spi0_sleep>;
    pinctrl-names = "default", "sleep";
    cs-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;   // nCS = P1.03

    trackball: trackball@0 {
        compatible = "pixart,pmw3610-alt";   // ← 採用フォークに合わせる
        reg = <0>;
        spi-max-frequency = <2000000>;       // PMW3610 は 2MHz 上限
        irq-gpios = <&gpio0 15 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;  // MOTION = P0.15
        cpi = <600>;                         // 初期値。実機で調整
        // 向きは実機で調整（暫定でコメントアウト）
        // swap-xy;
        // invert-x;
        // invert-y;
        evt-type = <INPUT_EV_REL>;
        x-input-code = <INPUT_REL_X>;
        y-input-code = <INPUT_REL_Y>;
    };
};

/ {
    trackball_listener {
        compatible = "zmk,input-listener";
        device = <&trackball>;
    };
};
```

- **NRESET は外付け 10k で 3.3V にプルアップ固定**（設計 §8）＝ GPIO 消費なし。`reset-gpios` 等は不要。
- **MOTION の内蔵プルアップ**は `irq-gpios` の `GPIO_PULL_UP` フラグで対応（外部プルアップ不要か要データシート確認だが、内蔵で受ければ OK）。
- **`&spi0` の可否＝要確認**：nRF52840 は SPIM0 と TWIM0 が周辺 ID を共有する。base `xiao_ble.dts`（Zephyr 側）で `&i2c0` が有効だと衝突する。§5 で見た ZMK 側 `xiao_ble_zmk.dts` では i2c0/spi0 は明示有効化されていなかったので使える見込みだが、**Zephyr の base dts を確認**すること。衝突する場合は **`&spi3`（専用 SPIM3、TWI と共有しない）** に切替（pinctrl はどのピンでもルーティング可）。

### 7-3. 左 overlay（`rhyn47_left.overlay`）
```dts
#include "rhyn47.dtsi"
// 左は col-offset=0（既定）。トラボ無し。追加設定なし。
```

---

## 8. 電池電圧監視（外部分圧 A0/AIN0 に上書き）

本設計は **単4×1 の生電圧（1.0〜1.4V）を外部分圧 1M/470k で A0(P0.02/AIN0) に入れて監視**する（設計 §4, §7）。ボード標準 `vbatt`（オンボード P0.31 分圧）ではないので上書きする。左右共通なので `rhyn47.dtsi` に置く。

```dts
#include <zephyr/dt-bindings/adc/adc.h>
#include <zephyr/dt-bindings/adc/nrf-adc.h>

/ {
    chosen {
        zmk,battery = &vbatt_ext;
    };

    vbatt_ext: vbatt_ext {
        compatible = "zmk,battery-voltage-divider";
        io-channels = <&adc 0>;              // AIN0 = P0.02 = D0
        output-ohms = <470000>;              // 分圧下側
        full-ohms   = <(1000000 + 470000)>;  // 上側1M + 下側470k
        // power-gpios は無し（常時ONの分圧、約1µA。設計 §10）
    };
};

// ボード標準のオンボード分圧は使わないので無効化（P0.14 を掴ませない）
&vbatt {
    status = "disabled";
};

// ADC チャンネル0 の設定（AIN0）
&adc {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    channel@0 {
        reg = <0>;
        zephyr,gain = "ADC_GAIN_1_6";                 // フルスケール 0.6V*6 = 3.6V
        zephyr,reference = "ADC_REF_INTERNAL";        // 0.6V
        zephyr,acquisition-time = <ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)>;
        zephyr,input-positive = <NRF_SAADC_AIN0>;     // P0.02
        zephyr,resolution = <12>;
    };
};
```

### ⚠ NiMH 単4×1 のパーセンテージ注意
- ZMK の電池残量%曲線は **Li 系（3.0〜4.2V 前提）**。本機は分圧復元後で 1000〜1400mV になるため、標準曲線だと **常時 0% 近辺**を報告する。
- 対策は後回しでよい（第1段階は起動・BLE 優先）。必要になったら NiMH 用のカスタム曲線 or `CONFIG_ZMK_BATTERY_REPORTING` の扱いを検討。**まずは「電圧が読めること」だけ確認**すれば十分。
- **要確認**：`zmk,battery-voltage-divider` が `channel@0` 明示ノードを要求するか（ZMK 標準 vbatt は `io-channels=<&adc 7>` だけで動いていた）。上の channel@0 は安全側で明示したもの。ビルドが通らなければ ZMK 標準 vbatt の書式に合わせる。

---

## 9. 省電力・スリープ（共通 conf の下敷き）

設計 §5「ペリフェラル常時通信で消費増、interrupt-gpios・スリープ・接続間隔の詰めが重要」に対応。

- `wakeup-source;`（kscan、§4 で設定済み）でディープスリープからキー起床。
- トラボは `MOTION`(irq-gpios) 割込みで起床。
- 共通 conf（`rhyn47_left.conf` / `rhyn47_right.conf` 両方 or 共通化）に：
```conf
CONFIG_ZMK_SLEEP=y
CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=900000     # 15分でディープスリープ（値は運用で調整）
CONFIG_ZMK_EXT_POWER=y
```
- BLE 接続間隔の詰めは実測後（消費電流計測は §13 検証項目）。

---

## 10. conf ファイル

### `config/rhyn47_right.conf`（セントラル・トラボあり）
```conf
# --- ブリングアップ用ログ（設計 §13）---
CONFIG_ZMK_USB_LOGGING=y

# --- ポインティング / PMW3610 ---
CONFIG_SPI=y
CONFIG_INPUT=y
CONFIG_ZMK_POINTING=y
CONFIG_PMW3610_ALT=y                 # ← 採用フォークに合わせる
CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN=12

# --- 省電力 ---
CONFIG_ZMK_SLEEP=y
```

### `config/rhyn47_left.conf`（ペリフェラル・キーのみ）
```conf
CONFIG_ZMK_USB_LOGGING=y
CONFIG_ZMK_SLEEP=y
```
- **注意**：`CONFIG_ZMK_POINTING` はセントラル（右）に入れれば足りる。左に PMW3610 系の CONFIG は入れない。

---

## 11. ビルド & 書き込み

### build.yaml（リポジトリ直下）
```yaml
include:
  - board: xiao_ble
    shield: rhyn47_left
  - board: xiao_ble
    shield: rhyn47_right
```

### 書き込み（設計 §13）
- XIAO nRF52840 は **UF2 ブートローダ**。本体リセットボタンを**ダブルタップ**→ UF2 ドライブが出る → `.uf2` をドロップ。自作基板側にブートボタン不要。
- **要確認（ケース/レイアウト）**：直付けで **XIAO 本体のリセットボタンが隠れないか**。隠れるなら基板側にリセットアクセス（パッド/ボタン）を用意。
- USB 有線なら電波を出さないので、**技適未決着でもキー入力・キーマップ検証は可能**（設計 §13）。

---

## 12. キーマップ（後決め・配線非依存）

- 設計どおりキーマップは配線に依存しないので**後決めで OK**。まずは配置検証用に適当な `&kp` を 47 個並べれば足りる。
- バインド順は §5 のとおり（row0=12, row1=12, row2=12, **row3=11**）。
- 既に別 rhyn バリアント（`low-profile` / `standard` 等）のキーマップがあれば流用元にできる（リポジトリ他ディレクトリ参照）。→ **要確認**：流用可能なキーマップ資産の有無。

---

## 13. 段階的ブリングアップ手順（設計 §13 準拠）

1. **最小起動**：まず左右それぞれ単体で XIAO が起動するか（USB ログ / LED）。キーマップは仮でよい。
2. **BLE 左右接続**：`rhyn47_left`（ペリフェラル）＋ `rhyn47_right`（セントラル）を書き、右が PC とペア → 左が右に繋がるか（ログ/接続状態）。※技適決着後に電波を出す。
3. **キー入力**：メイン基板が来たら 47 キーのスキャン確認。ここで **col2row のプル極性/ダイオード向き**を実機確定（§4 の注意）。NKRO・ゴースト確認。
4. **トラボ**：右にセンサー実装後、PMW3610 の動作・CPI・向き（swap-xy/invert-x/invert-y）を調整。
5. **省電力**：消費電流実測 → スリープ/接続間隔を詰める。電池持ち計測。

---

## 14. 要確認・未決事項（実装/検証で潰す）

- [ ] **`&spi0` が使えるか**（Zephyr base `xiao_ble.dts` で i2c0 と衝突しないか）。ダメなら `&spi3` に切替。
- [ ] **ダイオードの物理向き**が ZMK `col2row`（ROW=プルダウン, COL=ACTIVE_HIGH）と一致するか。逆なら row2col + プルアップに。
- [ ] **トラボ位置** `RC(3,6)`（右手内側最下段）で合っているか。物理レイアウト §16 確定後に最終化。
- [ ] **PMW3610 ドライバのフォーク確定**（badjeff 前提）→ `compatible` 文字列・`CONFIG_*` 名を README で最終照合。
- [ ] **電池 divider の channel@0 明示が要るか**、NiMH% 曲線の扱い（当面は電圧が読めれば可）。
- [ ] **MOTION の外部プルアップ要否**（内蔵で受けられるかデータシート確認）。
- [ ] **XIAO リセットボタンのアクセス**（ケース設計）。
- [ ] **技適の決着**（電波を出す運用の前）。有線検証は技適前でも可。
- [ ] 既存 rhyn バリアントに**流用できるキーマップ資産**があるか。

---

## 15. 参考リンク

- ZMK 本体（corne shield が左右分割 + col-offset の実例）: https://github.com/zmkfirmware/zmk/tree/main/app/boards/shields/corne
- ZMK `xiao_ble` ボード: https://github.com/zmkfirmware/zmk/tree/main/app/boards/seeed/xiao_ble
- PMW3610 ドライバ（badjeff）: https://github.com/badjeff/zmk-pmw3610-driver
- PMW3610 ドライバ（代替・inorichi）: https://github.com/inorichi/zmk-pmw3610-driver
- ZMK Pointing ドキュメント: https://zmk.dev/docs/keymaps/pointing
- DYA Dash（同構成の頒布実績・ZMK 設定の土台）: https://github.com/cormoran/dya-dash-keyboard

---

### 確定した設計判断（このセッションで決めたこと）
- **右手 = セントラル / 左手 = ペリフェラル**（トラボをセントラル直結にして BLE ポインタ転送を回避）。
- ボードは `xiao_ble`。ZMK は main。PMW3610 は badjeff ドライバを第一候補。
- 電池監視はオンボード `vbatt` を無効化し、外部分圧 A0/AIN0 に上書き。

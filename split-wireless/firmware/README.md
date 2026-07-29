# rhyn47 split-wireless — ZMK firmware

XIAO nRF52840 Plus × 2 の完全無線分割（左右 BLE）＋右手トラックボール（PMW3610）用の ZMK config。
設計の背景は `wireless-rhyn-design.md`、実装判断は `zmk-implementation-notes.md` を参照。

## 構成

| | |
|---|---|
| board | `xiao_ble`（ZMK main） |
| shield | `rhyn47_left`（ペリフェラル）/ `rhyn47_right`（セントラル＋トラボ） |
| マトリクス | 4行6列 × 左右、`col2row`、外部抵抗なし |
| 論理グリッド | 4行12列。右は `col-offset = 6`。`RC(3,6)` はトラボ穴 → 計 **47キー** |
| 電池監視 | 外部分圧 1MΩ/470kΩ → A0 (P0.02/AIN0)。ボード標準 `vbatt` は無効化 |

```
firmware/
├── build.yaml                     ビルド対象（board × shield）
├── .github/workflows/             ※ワークフローはリポジトリルートの .github/ にある
└── config/
    ├── west.yml                   ZMK 本体 + PMW3610 ドライバ
    ├── zephyr/module.yml          config/boards を shield 探索対象にする
    ├── rhyn47.keymap              左右共通キーマップ（47キー・6レイヤ）
    ├── rhyn47_left.conf           左（ペリフェラル）
    ├── rhyn47_right.conf          右（セントラル・トラボ）
    └── boards/shields/rhyn47/
        ├── Kconfig.shield
        ├── Kconfig.defconfig      SPLIT / 右=セントラル / キーボード名
        ├── rhyn47.dtsi            kscan・matrix-transform・電池分圧（左右共通）
        ├── rhyn47_left.overlay
        ├── rhyn47_right.overlay   col-offset=6 + SPI + PMW3610 + input-listener
        └── rhyn47.zmk.yml
```

## ビルド

GitHub Actions（`.github/workflows/build-split-wireless.yml`）で自動ビルド。
Artifacts から `rhyn47-split-wireless.zip` を落とすと `rhyn47_left-xiao_ble-zmk.uf2` /
`rhyn47_right-xiao_ble-zmk.uf2` / `settings_reset-xiao_ble-zmk.uf2` が入っている。

ローカルで回す場合は通常の ZMK ワークスペースで：

```sh
west build -p -d build/left  -b xiao_ble -- -DSHIELD=rhyn47_left  -DZMK_CONFIG=<repo>/split-wireless/firmware/config
west build -p -d build/right -b xiao_ble -- -DSHIELD=rhyn47_right -DZMK_CONFIG=<repo>/split-wireless/firmware/config
```

## 書き込み

XIAO nRF52840 のリセットボタンを**ダブルタップ** → UF2 ドライブが出る → `.uf2` をドロップ。
自作基板側にブートボタンは不要。ペアリングが壊れたら **左右両方**に `settings_reset` を焼いてから
本ファームを焼き直す（左→右の順で電源を入れると再ペアリングされる）。

## キーマップ

`low-profile-with-trackball` の QMK/Vial 版（`lp_tb/v0_1/keymaps/vial/keymap.c`）からの移植。
BASE / LOWER / UPPER / FN / MOUSE / SCROLL の6レイヤ。

- QMK の `set_auto_mouse_layer(_MOUSE)` → `rhyn47_right.overlay` の `&zip_temp_layer 4 400`
- QMK の `set_scroll_layer(_SCROLL)` → 同 `scroller` ノード（`zip_xy_to_scroll_mapper`）
- QMK の `pointing_device_task_user()` の v/h 反転 → `zip_scroll_transform` の X/Y invert
- FN レイヤに BLE 操作を追加（無線化に伴う新規）。プロファイル選択 `&bt BT_SEL 0-4` は **Q W E R T**、
  `&bt BT_CLR` は誤爆防止で BSPC の位置。`&out OUT_TOG` / `&bootloader` / `&sys_reset` は右半分。

## 実機で潰す項目

- [ ] `&spi0` が使えるか（nRF52840 は SPIM0/TWIM0 が周辺 ID 共有）。衝突するなら `&spi3` へ
- [ ] ダイオードの物理向きが `col2row`（ROW=プルダウン, COL=ACTIVE_HIGH）と一致するか
- [ ] PMW3610 ドライバのフォーク確定 → `compatible` と `CONFIG_*` を README で照合（west.yml / overlay / conf の3箇所）
- [ ] トラボの向き（`swap-xy` / `invert-x` / `invert-y`）と CPI
- [ ] 電池分圧の `channel@0` 明示が要るか、NiMH の残量%曲線
- [ ] 技適の決着（電波を出す運用の前。USB 有線でのキー検証は技適前でも可）

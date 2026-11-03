# Uconfig

The `uconfig` utility, which stands for Utopia config, is a fork of [lconfig](https://github.com/EvalynGoemer/lconfig) adapted for Utopia.

It is, unlike other parts of this repository, licensed under MPL-2.0.

## For users

You most certainly want to run `make defconfig`, which will generate the default configuration, instead of `make menuconfig`, which is intended for advanced kernel configuration. Kernel releases from `beta@2.1.0` onwards use `defconfig` for their demos.

The `menuconfig` keybinds can be found at the bottom of the `menuconfig` window.

## For developers

An example config from the `lconfig` repo:

```
proj "Example Project"

opt CONFIG_MENU(bool) -> menu {
    display: "Menuery"
    default: true
}

opt CONFIG_DEPENDANT(bool) {
    display: "Dependantary"
    default: false
    depends: [CONFIG_MENU]
}

opt CONFIG_DEPENDANT2(bool) {
    display: "Dependantary 2.0"
    default: false
    depends: [CONFIG_DEPENDANT]
}

opt CONFIG_BOOLEAN(bool) {
    display: "Booleanery"
    default: false
}

opt CONFIG_STRING(string) {
    display: "Stringery"
    default: "Jarona"
}

opt CONFIG_NUMBER(int) {
    display: "Numbery"
    default: 420
}

opt CONFIG_WITHIN(bool) {
    display: "Withinery"
    default: true
    within: CONFIG_MENU
}

opt CONFIG_SELECT(select) {
    display: "Selectary"
    options: {"Jarona!", "Glue", "I'm falling"}
    values: {1, 2, 3}
    default: "Glue"
}

opt CONFIG_NULL_MENU(null) -> menu {
    display: "Null Menuery"
}

opt CONFIG_NULL(null) {
    display: "Nullery"
}
```

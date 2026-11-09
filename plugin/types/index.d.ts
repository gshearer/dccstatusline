// One run of a sample line in one style, as `--themes` or a plain run painted it.
export type Span = { text: string; fg?: string; bg?: string }

export type Preset = { name: string; spans: Span[] }

// The picker's options by name: an absent one is at its first value.
export type Choices = Record<string, string>

// current: the theme the config names. theme: the one the theme row is at,
// no theme or a preset. held: the options as the config had them when the
// picker opened. presets: --themes' rows, the presets alone. stock: the line
// with no theme, as a plain run on no config draws it.
export type Picker = {
  bin: string
  current: string
  theme: string
  held: Choices
  choices: Choices
  presets: Preset[]
  stock: Span[]
}

declare module 'claude-code' {
  interface PluginState {
    dccstatusline: { picker: Shaped<Picker | null> }
  }
}

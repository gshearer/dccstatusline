// One run of a sample line in one style, as `--themes` painted it.
export type Span = { text: string; fg?: string; bg?: string }

export type Preset = { name: string; spans: Span[] }

// The picker's options by name: an absent one is at its first value.
export type Choices = Record<string, string>

// current: the theme the config names, none for the stock line. theme: the
// one the theme row is at. held: the options as the config had them when the
// picker opened. presets: --themes' rows, none and then every preset.
export type Picker = {
  bin: string
  current: string
  theme: string
  held: Choices
  choices: Choices
  presets: Preset[]
}

declare module 'claude-code' {
  interface PluginState {
    dccstatusline: { picker: Shaped<Picker | null> }
  }
}

// dccstatusline — MIT
// register: the theme picker, a box previewing the line over a row each for the
// theme, palette, separator, profile and glyphs, which Enter moves on. The
// Apply button sends them back to the theme skill, which writes the config.

import { atom, read, update } from 'claude-code'
import type { EngineInterface, Register } from 'claude-code'

import type { Choices, Preset, Span } from '../types'

const PANE = 'theme-picker'
const SKILL = 'dccstatusline:theme'
const NO_THEME = 'no theme'   // the theme row's first stop: the stock line, no preset

const picker = atom({ plugin: 'dccstatusline', key: 'picker' } as const, null, {
  shape: 'picker-6',
})

// Each option starts at the value whose INI the config holds, and Enter moves
// it to its next value, round to the first: the default, which writes nothing.
// Any other value is the INI the skill writes below the theme line, and
// --themes lays it over every preset to preview it. The profile is always one
// of its own, with no default: basic until the config holds detailed, so every
// pick on a preset names it. Every option is a theme's: no theme is no preset
// but the stock line in the terminal's own 16 colors, which a plain run on no
// config draws, so there every row waits, see waits. The palette is how many of a
// preset's three colors the line wears beside its muted one: three, two or
// one. The glyphs are the profile's: on puts ⎇ before the branch and, under
// detailed, ↻ before both reset times, and nerdfont puts U+E0A0 and U+F021 in
// their place and the model in a pill between U+E0B6 and U+E0B4.
// U+E0B1 is the Nerd Font thin arrow, an escape since most editors can't show it. A missing font shows up here first,
// as boxes in the preview.
const OPTIONS = [
  {
    name: 'palette',
    label: 'Palette',
    values: ['tritone', 'duotone', 'monochrome'],
    ini: (v: string) => `[statusline]\npalette = ${v}\n`,
  },
  {
    name: 'sep',
    label: 'Separator',
    values: ['│', '❯', '\uE0B1', '┃', '•', '⟫', '┊', '╱', '❭', '∷', '║', '╏', '┋'],
    ini: (v: string) => `[statusline]\nseparator = " ${v} "\n`,
  },
  {
    name: 'profile',
    label: 'Profile',
    values: ['basic', 'detailed'],
    ini: (v: string) => `[statusline]\nprofile = ${v}\n`,
    always: true,
  },
  {
    name: 'glyphs',
    label: 'Glyphs',
    values: ['off', 'on', 'nerdfont'],
    ini: (v: string) => `[statusline]\nglyphs = ${v}\n`,
  },
]

// What the options wait for, or nothing while they are live. No option does
// anything under no theme, so there each waits: it is drawn dim, reading what
// it waits for, Enter on it does nothing, and it is neither sent nor named. It
// keeps its choice for when a preset comes back. --themes draws the presets
// alone, every one at once, so it never waits.
const waits = (theme: string) => (theme === NO_THEME ? 'pick a theme' : undefined)

// A private-use glyph goes to the skill by its code point. Claude can't see the
// character in tool output, so sent raw it would look like an empty `sep=`.
const spell = (v: string) =>
  /^[\uE000-\uF8FF]$/.test(v) ? `U+${v.codePointAt(0)!.toString(16).toUpperCase()}` : v

const NAMES = ['black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan', 'white']
const SGR = /\x1b\[([0-9;]*)m([^\x1b]*)/g   // a sequence and the text it paints

const hex = (v: number) => (v & 255).toString(16).padStart(2, '0')

// One sequence as color.c writes it for --themes: a reset, then a 16-color or
// 24-bit foreground and background. Nothing carries over from the last one.
function style(params: string) {
  const p = params.split(';').map(Number)
  const s: { fg?: string; bg?: string } = {}

  for (let i = 1; i < p.length; i++) {
    const n = p[i]!
    const key = (n >= 40 && n <= 48) || n >= 100 ? 'bg' : 'fg'

    if (n === 38 || n === 48) {
      s[key] = '#' + p.slice(i + 2, i + 5).map(hex).join('')
      i += 4
    } else {
      s[key] = NAMES[n % 10] + (n >= 90 ? 'Bright' : '')
    }
  }

  return s
}

// A sample line as the runs of text it paints, each in its own style.
const spans = (line: string): Span[] =>
  [...line.matchAll(SGR)].filter(s => s[2]).map(s => ({ text: s[2]!, ...style(s[1]!) }))

// --themes: one preset a line, its name, some padding, then the sample line. A
// binary that predates themes prints its one fallback line, which names
// nothing.
export function parse(stdout: string): Preset[] {
  const presets: Preset[] = []

  for (const line of stdout.split('\n')) {
    const m = /^([a-z0-9]+) *(\x1b.*)$/.exec(line)

    if (m) presets.push({ name: m[1]!, spans: spans(m[2]!) })
  }

  return presets
}

// A plain run: the line alone, with no name before it.
export function parseStock(stdout: string): Span[] {
  return spans(stdout.split('\n')[0] ?? '')
}

// --themes' sample as a payload, for a plain run to draw the stock line on:
// print_themes in src/main.c holds the same, and a change to one goes to the
// other. Each reset stands 30 seconds past the sample's, so the countdowns read
// as --themes' do while the run takes less. The sample's repository is fixed in
// the binary, out of a payload's reach, so the stock line shows no branch.
export function stockPayload(now: number): string {
  return JSON.stringify({
    cwd: '~/src/acme',
    model: { id: 'claude-opus-5-5', display_name: 'Opus' },
    effort: { level: 'high' },
    context_window: { total_input_tokens: 87333, context_window_size: 200000 },
    rate_limits: {
      five_hour: { used_percentage: 24, resets_at: now + 9000 + 30 },
      seven_day: { used_percentage: 41, resets_at: now + 302400 + 30 },
    },
  })
}

// The stock line: the binary run as the status line runs it, on stockPayload,
// with /dev/null for its config so none of the user's keys paint over it. It
// runs at /, where the sample's relative cwd names no repository.
async function drawStock($: EngineInterface, bin: string): Promise<Span[]> {
  const ran = await $.process.run([bin], {
    cwd: '/',
    env: { DCCSTATUSLINE_CONFIG: '/dev/null' },
    stdin: stockPayload(Math.floor(Date.now() / 1000)),
    timeoutMs: 5000,
  })

  return parseStock(ran.stdout)
}

// The configure skill's step 0, cut short: what the status line runs, if that
// is dccstatusline at all.
async function findBin($: EngineInterface): Promise<string | undefined> {
  const line = (await $.settings.read({ key: 'statusLine' })) as { command?: unknown } | undefined
  const word = typeof line?.command === 'string' ? (line.command.trim().split(/\s+/)[0] ?? '') : ''

  if (!/(^|\/)dccstatusline$/.test(word)) return undefined

  return word.replace(/^~(?=\/)/, (await $.env.get('HOME')) ?? '')
}

// An INI's keys as `section.key`, read as config.c reads them: a quoted value
// keeps its spaces and '#', an unquoted one ends at a '#' after whitespace,
// and the last of a key wins.
function keys(ini: string): Map<string, string> {
  const out = new Map<string, string>()
  let section = ''

  for (const line of ini.split('\n').map(l => l.trim())) {
    const eq = line.indexOf('=')

    if (line.startsWith('[')) {
      section = /^\[([^\]]*)\]/.exec(line)?.[1]?.trim() ?? section
    } else if (eq > 0 && !line.startsWith('#')) {
      const raw = line.slice(eq + 1).trim()
      const value = raw.startsWith('"')
        ? /^"((?:\\["\\]|[^"])*)/.exec(raw)![1]!.replace(/\\(["\\])/g, '$1')
        : raw.replace(/[ \t]#.*/, '').trim()

      out.set(`${section}.${line.slice(0, eq).trim()}`, value)
    }
  }

  return out
}

// The configure skill's step 1: the theme the config names, and the value of
// each option whose INI it holds whole, a default aside, which has none. No
// theme line is no theme, and so is default, v0.0.6's name for the stock line,
// which its reference config wrote.
async function inUse($: EngineInterface): Promise<{ current: string; held: Choices }> {
  const home = (await $.env.get('HOME')) ?? ''
  const xdg = (await $.env.get('XDG_CONFIG_HOME')) || `${home}/.config`
  const path = (await $.env.get('DCCSTATUSLINE_CONFIG')) || `${xdg}/dccstatusline/config`
  const ini = await $.fs.read(path).catch(() => '')   // no config: the defaults
  const config = keys(ini)
  const held: Choices = {}

  for (const o of OPTIONS) {
    const v = (o.always ? o.values : o.values.slice(1)).find(v =>
      [...keys(o.ini(v))].every(([k, x]) => config.get(k) === x),
    )

    if (v) held[o.name] = v
  }

  const named = /^[ \t]*theme[ \t]*=[ \t]*"?([a-z0-9]+)/m.exec(ini)?.[1]

  return { current: named === undefined || named === 'default' ? NO_THEME : named, held }
}

// Every row as --themes draws it with the chosen options laid over, so the
// theme row moves between them without running it again.
async function draw($: EngineInterface, bin: string, choices: Choices): Promise<Preset[]> {
  const overlay = OPTIONS.filter(o => choices[o.name])
    .map(o => o.ini(choices[o.name]!))
    .join('')
  const ran = await $.process.run([bin, '--themes', overlay], { stdin: '', timeoutMs: 5000 })

  return parse(ran.stdout)
}

// Finds the binary, runs --themes and the stock line, and opens the picker on
// what they draw. False when any of that fails, as with a binary that predates
// themes. The stock line is drawn once: under no theme the options wait, so
// nothing changes it.
async function openPicker($: EngineInterface): Promise<boolean> {
  const bin = await findBin($)

  if (bin === undefined) return false

  const { current, held } = await inUse($)
  const choices: Choices = { ...held }
  let presets: Preset[]
  let stock: Span[]

  // An option with no default starts at its first value, and is drawn there.
  for (const o of OPTIONS) if (o.always) choices[o.name] ??= o.values[0]!

  try {
    presets = await draw($, bin, choices)

    if (presets.length === 0) return false

    stock = await drawStock($, bin)
  } catch {
    return false
  }

  // No theme, or one this binary doesn't list: the row starts at no theme.
  const theme = presets.some(p => p.name === current) ? current : NO_THEME

  await update($, picker, () => ({ bin, current, held, theme, choices, presets, stock }))

  const opened = await $.ui.open({
    id: PANE,
    title: 'dccstatusline themes',
    focus: true,
    closeOnEscape: true,
    holdToasts: true,
    // Beside the options: the theme row, apply, the box's two borders, the blank
    // line under the box, the rows and apply, and ten rows for the preview and
    // the hint to wrap into at 30 columns, which an option can lengthen after
    // this one chance to ask. A pane short of its tree scrolls, and the arrows
    // then scroll it instead of moving between the rows. Spare rows cost
    // nothing: the frame fits the tree.
    rows: OPTIONS.length + 17,
  })

  if (!opened.isPlaced) await $.ui.close({ id: PANE })

  return opened.isPlaced
}

export const register: Register = on => {
  // Opened from the person's own command, the picker is placed at any width.
  // A run that names a preset, or finds no picker to open, is the skill's.
  on('command.run', { command: SKILL }, async ($, e, next) => {
    if (e.args.trim() !== '' || !(await openPicker($))) return next(e)

    return { text: 'Pick a theme: ↑↓ to move, Enter to change a row or apply, Esc to cancel.' }
  })

  on('ui.render', { component: 'Pane', requestId: PANE }, async ($, e) => {
    const { Box, Button, Text } = $.ui.resolve(e)
    const state = await read($, picker)

    if (!state) return <Text dimColor>No themes loaded.</Text>

    const { bin, held, theme, choices } = state
    const wait = waits(theme)

    // Apply runs the theme skill again with the theme, none for no theme, and
    // each option that is set or that the config held, so one moved back to its
    // first value is named too, as if the person had typed them, and the skill
    // writes the config. A waiting option goes unnamed, so none goes alone.
    const apply = async () => {
      const named = wait ? [] : OPTIONS.filter(o => choices[o.name] || held[o.name])
      const args = [
        theme === NO_THEME ? 'none' : theme,
        ...named.map(o => `${o.name}=${spell(choices[o.name] ?? o.values[0]!)}`),
      ]

      await $.ui.close({ id: PANE })
      await $.command.run({ command: SKILL, args: args.join(' ') }).catch(err => {
        $.ui.toast(`Couldn't apply ${theme}: ${String(err)}`)
      })
    }

    // No theme, then every preset, round to no theme again.
    const nextTheme = () =>
      update($, picker, s => {
        if (!s) return s

        const names = [NO_THEME, ...s.presets.map(p => p.name)]

        return { ...s, theme: names[(names.indexOf(s.theme) + 1) % names.length]! }
      })

    const change = async (o: (typeof OPTIONS)[number]) => {
      if (wait) return

      // Past the last value: the default, which is no choice, or for an option
      // with none, the first again.
      const i = o.values.indexOf(choices[o.name] ?? o.values[0]!)
      const value = o.values[i + 1] ?? (o.always ? o.values[0] : undefined)
      const { [o.name]: _, ...rest } = choices
      const changed = value === undefined ? rest : { ...rest, [o.name]: value }
      const presets = await draw($, bin, changed).catch(err => {
        $.ui.toast(`Couldn't preview ${o.label.toLowerCase()}: ${String(err)}`)
      })

      if (presets?.length) await update($, picker, s => s && { ...s, choices: changed, presets })
    }

    const rows = [
      { key: 'theme', label: 'Theme', value: theme, onPress: nextTheme, dim: undefined },
      ...OPTIONS.map(o => ({
        key: o.name,
        label: o.label,
        value: wait ?? choices[o.name] ?? o.values[0]!,
        onPress: () => change(o),
        dim: wait !== undefined || undefined,
      })),
    ]

    // Values line up past the longest label.
    const column = Math.max(...rows.map(r => r.label.length)) + 2
    const sample =
      theme === NO_THEME ? state.stock : (state.presets.find(p => p.name === theme)?.spans ?? [])

    return (
      <Box flexDirection="column" gap={1}>
        <Box borderStyle="round" borderDimColor paddingX={1}>
          <Text wrap="wrap">
            {sample.map(s => (
              <Text color={s.fg} backgroundColor={s.bg}>
                {s.text}
              </Text>
            ))}
          </Text>
        </Box>
        <Box flexDirection="column">
          {rows.map(r => (
            <Box>
              <Button
                key={r.key}
                plain
                autoFocus={r.key === 'theme' || undefined}
                dimColor={r.dim}
                onPress={r.onPress}
              >
                {r.label}
              </Button>
              <Text dimColor={r.dim}>
                {' '.repeat(column - r.label.length)}
                {r.value}
              </Text>
            </Box>
          ))}
        </Box>
        <Button key="apply" variant="primary" onPress={apply}>
          Apply
        </Button>
        <Text dimColor>
          ↑↓ move · Enter changes a row or applies · Esc cancels · in use: {state.current}
        </Text>
      </Box>
    )
  })
}

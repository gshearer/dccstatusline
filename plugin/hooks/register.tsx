// dccstatusline — MIT
// register: the theme picker, a box previewing the line over a row each for the
// theme, separator, profile and glyphs, which Enter moves on. The Apply button
// sends them back to the theme skill, which writes the config.

import { atom, read, update } from 'claude-code'
import type { EngineInterface, Register } from 'claude-code'

import type { Choices, Preset } from '../types'

const PANE = 'theme-picker'
const SKILL = 'dccstatusline:theme'

const picker = atom({ plugin: 'dccstatusline', key: 'picker' } as const, null, {
  shape: 'picker-5',
})

// Each option starts at the value whose INI the config holds, and Enter moves
// it to its next value, round to the first: the default, which writes nothing.
// Any other value is the INI the skill writes below the theme line, and
// --themes lays it over every preset to preview it. The glyphs are the
// profile's: on puts ⎇ before the branch and, under detailed, ↻ before both
// reset times, nerdfont puts U+E0A0 and U+F021 in their place and the model in a
// pill between U+E0B6 and U+E0B4, and with no profile the row waits, see enabled.
// U+E0B1 is the Nerd Font thin arrow, an escape since most editors can't show it. A missing font shows up here first,
// as boxes in the preview.
const OPTIONS = [
  {
    name: 'sep',
    label: 'Separator',
    values: ['│', '❯', '\uE0B1', '┃', '•', '⟫', '┊', '╱', '❭', '∷', '║', '╏', '┋'],
    ini: (v: string) => `[statusline]\nseparator = " ${v} "\n`,
  },
  {
    name: 'profile',
    label: 'Profile',
    values: ['none', 'basic', 'detailed'],
    ini: (v: string) => `[statusline]\nprofile = ${v}\n`,
  },
  {
    name: 'glyphs',
    label: 'Glyphs',
    values: ['off', 'on', 'nerdfont'],
    ini: (v: string) => `[statusline]\nglyphs = ${v}\n`,
    profiled: true,
  },
]

// A profiled option does nothing without a profile, so until one is chosen
// it is drawn dim, Enter on it does nothing, and it is neither sent nor named.
// It keeps its choice for when one is.
const enabled = (o: (typeof OPTIONS)[number], choices: Choices) =>
  !o.profiled || choices['profile'] !== undefined

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

// One preset a line: its name, some padding, then the sample line. A binary
// that predates themes prints its one fallback line, which names nothing.
export function parse(stdout: string): Preset[] {
  const presets: Preset[] = []

  for (const line of stdout.split('\n')) {
    const m = /^([a-z0-9]+) *(\x1b.*)$/.exec(line)

    if (!m) continue

    const spans = [...m[2]!.matchAll(SGR)]
      .filter(s => s[2])
      .map(s => ({ text: s[2]!, ...style(s[1]!) }))

    presets.push({ name: m[1]!, spans })
  }

  return presets
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
// each option whose INI it holds whole.
async function inUse($: EngineInterface): Promise<{ current: string; held: Choices }> {
  const home = (await $.env.get('HOME')) ?? ''
  const xdg = (await $.env.get('XDG_CONFIG_HOME')) || `${home}/.config`
  const path = (await $.env.get('DCCSTATUSLINE_CONFIG')) || `${xdg}/dccstatusline/config`
  const ini = await $.fs.read(path).catch(() => '')   // no config: the defaults
  const config = keys(ini)
  const held: Choices = {}

  for (const o of OPTIONS) {
    const v = o.values.slice(1).find(v => [...keys(o.ini(v))].every(([k, x]) => config.get(k) === x))

    if (v) held[o.name] = v
  }

  return { current: /^[ \t]*theme[ \t]*=[ \t]*"?([a-z0-9]+)/m.exec(ini)?.[1] ?? 'default', held }
}

// Every preset as --themes draws it with the chosen options laid over, so the
// theme row moves between them without running it again.
async function draw($: EngineInterface, bin: string, choices: Choices): Promise<Preset[]> {
  const overlay = OPTIONS.filter(o => choices[o.name] && enabled(o, choices))
    .map(o => o.ini(choices[o.name]!))
    .join('')
  const ran = await $.process.run([bin, '--themes', overlay], { stdin: '', timeoutMs: 5000 })

  return parse(ran.stdout)
}

// Finds the binary, runs --themes and opens the picker on what it lists.
// False when any of that fails, as with a binary that predates themes.
async function openPicker($: EngineInterface): Promise<boolean> {
  const bin = await findBin($)

  if (bin === undefined) return false

  const { current, held } = await inUse($)
  let presets: Preset[]

  try {
    presets = await draw($, bin, held)
  } catch {
    return false
  }

  if (presets.length === 0) return false

  // A config can name a theme this binary doesn't list: the row starts at the first.
  const theme = presets.some(p => p.name === current) ? current : presets[0]!.name

  await update($, picker, () => ({ bin, current, held, theme, choices: held, presets }))

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

    // Apply runs the theme skill again with the theme and each option that is
    // set or that the config held, so one moved back to its first value is
    // named too, as if the person had typed them, and the skill writes the
    // config. A disabled option goes unnamed.
    const apply = async () => {
      const named = OPTIONS.filter(o => (choices[o.name] || held[o.name]) && enabled(o, choices))
      const args = [theme, ...named.map(o => `${o.name}=${spell(choices[o.name] ?? o.values[0]!)}`)]

      await $.ui.close({ id: PANE })
      await $.command.run({ command: SKILL, args: args.join(' ') }).catch(err => {
        $.ui.toast(`Couldn't apply ${theme}: ${String(err)}`)
      })
    }

    const nextTheme = () =>
      update($, picker, s => s && {
        ...s,
        theme: s.presets[(s.presets.findIndex(p => p.name === s.theme) + 1) % s.presets.length]!.name,
      })

    const change = async (o: (typeof OPTIONS)[number]) => {
      if (!enabled(o, choices)) return

      const value = o.values[o.values.indexOf(choices[o.name] ?? o.values[0]!) + 1]
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
        value: enabled(o, choices) ? (choices[o.name] ?? o.values[0]!) : 'needs a profile',
        onPress: () => change(o),
        dim: !enabled(o, choices) || undefined,
      })),
    ]

    // Values line up past the longest label.
    const column = Math.max(...rows.map(r => r.label.length)) + 2
    const sample = state.presets.find(p => p.name === theme)?.spans ?? []

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
